#define LUA_LIB

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <mutex>

#include <pthread.h>

#include <unistd.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#define CACHE_SIZE 0x1000

static void _block_send(lua_State* L, int fd, const char* buffer, int sz)
{
    while (sz > 0)
    {
        int r = send(fd, buffer, sz, 0);
        if (r < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            luaL_error(L, "socket error: %s", strerror(errno));
        }
        buffer += r;
        sz -= r;
    }
}

// quick and dirty none block stdin read_line

#define QUEUE_SIZE 1024

struct queue
{
    std::mutex lock;
    int head;
    int tail;
    char* queue[QUEUE_SIZE];
};

static void* _readline_stdin(void* arg)
{
    queue* q = (queue*)arg;
    char tmp[1024];
    while (!::feof(stdin))
    {
        if (::fgets(tmp, sizeof(tmp), stdin) == nullptr)
        {
            // read stdin failed
            exit(1);
        }
        int n = ::strlen(tmp) - 1;

        char* str = new char[n + 1];
        memcpy(str, tmp, n);
        str[n] = 0;

        std::lock_guard<std::mutex> lock(q->lock);

        q->queue[q->tail] = str;

        if (++q->tail >= QUEUE_SIZE)
        {
            q->tail = 0;
        }
        if (q->head == q->tail)
        {
            // queue overflow
            exit(1);
        }
    }

    return nullptr;
}

/*
	intger fd
	string last
	table result

	return 
		boolean (true: data, false: block, nil: close)
		string last
 */

struct socket_buffer
{
    void* buffer;
    int sz;
};

static int l_recv(lua_State* L)
{
    int fd = luaL_checkinteger(L, 1);

    char buffer[CACHE_SIZE];
    int r = recv(fd, buffer, CACHE_SIZE, 0);
    if (r == 0)
    {
        lua_pushliteral(L, "");
        // close
        return 1;
    }
    if (r < 0)
    {
        if (errno == EAGAIN || errno == EINTR)
        {
            return 0;
        }
        luaL_error(L, "socket error: %s", strerror(errno));
    }
    lua_pushlstring(L, buffer, r);
    return 1;
}

static int l_usleep(lua_State* L)
{
    int n = luaL_checknumber(L, 1);
    usleep(n);
    return 0;
}

static int l_connect(lua_State* L)
{
    const char* addr = luaL_checkstring(L, 1);
    int port = luaL_checkinteger(L, 2);
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in my_addr;

    my_addr.sin_addr.s_addr = inet_addr(addr);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);

    int r = ::connect(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr_in));
    if (r == -1)
    {
        return luaL_error(L, "Connect %s %d failed", addr, port);
    }

    int flag = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flag | O_NONBLOCK);

    lua_pushinteger(L, fd);

    return 1;
}

static int l_close(lua_State* L)
{
    int fd = luaL_checkinteger(L, 1);
    close(fd);

    return 0;
}


/*
	integer fd
	string message
 */
static int l_send(lua_State* L)
{
    size_t sz = 0;
    int fd = luaL_checkinteger(L, 1);
    const char* msg = luaL_checklstring(L, 2, &sz);

    _block_send(L, fd, msg, (int)sz);

    return 0;
}

static int l_read_stdin(lua_State* L)
{
    queue* q = (queue*)lua_touserdata(L, lua_upvalueindex(1));

    std::lock_guard<std::mutex> lock(q->lock);

    if (q->head == q->tail)
    {
        return 0;
    }

    char* str = q->queue[q->head];
    if (++q->head >= QUEUE_SIZE)
    {
        q->head = 0;
    }

    lua_pushstring(L, str);
    delete[] str;
    return 1;
}

#ifdef __cplusplus
extern "C" {
#endif

LUAMOD_API int luaopen_client_socket(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg l[] = {
        { "connect", l_connect },
        { "recv",    l_recv },
        { "send",    l_send },
        { "close",   l_close },
        { "usleep",  l_usleep },

        { nullptr,   nullptr },
    };
    luaL_newlib(L, l);

    auto q = (queue*)lua_newuserdata(L, sizeof(queue));
    ::memset(q, 0, sizeof(*q));

    lua_pushcclosure(L, l_read_stdin, 1);
    lua_setfield(L, -2, "readstdin");

    pthread_t pid;
    pthread_create(&pid, nullptr, _readline_stdin, q);

    return 1;
}

#ifdef __cplusplus
}
#endif
