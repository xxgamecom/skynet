#define LUA_LIB

#include "lua-skynet_netpack_tcp.h"
#include "skynet.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdint>
#include <string>

// #include <assert.h>
// #include <stdlib.h>
// #include <arpa/inet.h>
// #include <stdio.h>

namespace skynet { namespace luaclib {

#define QUEUESIZE           1024
#define HASHSIZE            4096
#define SMALLSTRING         65536

#define TYPE_DATA           1
#define TYPE_MORE           2
#define TYPE_ERROR          3
#define TYPE_OPEN           4
#define TYPE_CLOSE          5
#define TYPE_WARNING        6

/**
 * used to assemble complete tcp package. 再交给gateserver去处理
 * 
 * netpack specs:
 * uint16 + data, uint16 (serialized in big-endian) is the data size (bytes).
 * 
 * 如果采用sproto打包方式，需附加4字节(32位)的session值。所以客户端传过来1kb数据，实际数据只有1024-2-4=1018字节。
 */
struct netpack
{
    int                         socket_id = 0;                  // socket id
    int                         size = 0;                       // data size
    void*                       buf_ptr = nullptr;              // data
};

 struct uncomplete
 {
     netpack                    pack;
     uncomplete*                next = nullptr;
     int                        read = 0;
     int                        header = 0;
     int                        header_size = 0;
 };

 struct queue
 {
     int                        cap = 0;
     int                        head = 0;
     int                        tail = 0;
     uncomplete*                hash[HASHSIZE];
     netpack                    queue[QUEUESIZE];
 };

/**
 *打印二进制流(转换为十六进制字符串)
 */
static void print_bin(const char* pHead, const char* pBuffer, int iLength, int size)
{
    FILE* fp = nullptr;
    int i;
    char tmpBuffer[16384];
    char strTemp[32];

    if (iLength <= 0 || iLength > 4096 || pBuffer == nullptr)
    {
        return;
    }

    fp = fopen("test.txt", "a");

    tmpBuffer[0] = '\0';
    for (i = 0; i < iLength; i++)
    {
        if (!(i % 16))
        {
            sprintf(strTemp, "\n%04d>    ", i / 16 + 1);
            strcat(tmpBuffer, strTemp);
        }
        sprintf(strTemp, "%02X ", (unsigned char)pBuffer[i]);
        strcat(tmpBuffer, strTemp);
    }

    strcat(tmpBuffer, "\n");
    fprintf(fp, "%s Size:%d Hex:%s size:%d\n", pHead, iLength, tmpBuffer, size);
    fclose(fp);
    return;
}

static void clear_list(struct uncomplete* uc)
{
    while (uc)
    {
        skynet_free(uc->pack.buf_ptr);
        void* tmp = uc;
        uc = uc->next;
        skynet_free(tmp);
    }
}

static int l_clear(lua_State* L)
{
    struct queue* q = (queue*)lua_touserdata(L, 1);
    if (q == nullptr)
    {
        return 0;
    }
    int i;
    for (i = 0; i < HASHSIZE; i++)
    {
        clear_list(q->hash[i]);
        q->hash[i] = nullptr;
    }
    if (q->head > q->tail)
    {
        q->tail += q->cap;
    }
    for (i = q->head; i < q->tail; i++)
    {
        struct netpack* np = &q->queue[i % q->cap];
        skynet_free(np->buf_ptr);
    }
    q->head = q->tail = 0;

    return 0;
}

static inline int hash_socket_id(int socket_id)
{
    int a = socket_id >> 24;
    int b = socket_id >> 12;
    int c = socket_id;
    return (int)(((uint32_t)(a + b + c)) % HASHSIZE);
}

static struct uncomplete* find_uncomplete(struct queue* q, int socket_id)
{
    if (q == nullptr)
        return nullptr;
    int h = hash_socket_id(socket_id);
    struct uncomplete* uc = q->hash[h];
    if (uc == nullptr)
        return nullptr;
    if (uc->pack.socket_id == socket_id)
    {
        q->hash[h] = uc->next;
        return uc;
    }
    struct uncomplete* last = uc;
    while (last->next)
    {
        uc = last->next;
        if (uc->pack.socket_id == socket_id)
        {
            last->next = uc->next;
            return uc;
        }
        last = uc;
    }
    return nullptr;
}

static struct queue* get_queue(lua_State* L)
{
    struct queue* q = (queue*)lua_touserdata(L, 1);
    if (q == nullptr)
    {
        q = (queue*)lua_newuserdatauv(L, sizeof(struct queue), 0);
        q->cap = QUEUESIZE;
        q->head = 0;
        q->tail = 0;
        int i;
        for (i = 0; i < HASHSIZE; i++)
        {
            q->hash[i] = nullptr;
        }
        lua_replace(L, 1);
    }
    return q;
}

static void expand_queue(lua_State* L, struct queue* q)
{
    struct queue* nq = (queue*)lua_newuserdatauv(L, sizeof(struct queue) + q->cap * sizeof(struct netpack), 0);
    nq->cap = q->cap + QUEUESIZE;
    nq->head = 0;
    nq->tail = q->cap;
    memcpy(nq->hash, q->hash, sizeof(nq->hash));
    memset(q->hash, 0, sizeof(q->hash));
    int i;
    for (i = 0; i < q->cap; i++)
    {
        int idx = (q->head + i) % q->cap;
        nq->queue[i] = q->queue[idx];
    }
    q->head = q->tail = 0;
    lua_replace(L, 1);
}

static void push_data(lua_State* L, int socket_id, void* buffer, int size, int clone)
{
    if (clone)
    {
        void* tmp = skynet_malloc(size);
        memcpy(tmp, buffer, size);
        buffer = tmp;
    }
    //print_bin("push_data", buffer, size, size);
    struct queue* q = get_queue(L);
    struct netpack* np = &q->queue[q->tail];
    if (++q->tail >= q->cap)
        q->tail -= q->cap;
    np->socket_id = socket_id;
    np->buf_ptr = buffer;
    np->size = size;
    if (q->head == q->tail)
    {
        expand_queue(L, q);
    }
}

static struct uncomplete* save_uncomplete(lua_State* L, int socket_id)
{
    struct queue* q = get_queue(L);
    int h = hash_socket_id(socket_id);
    uncomplete* uc = (uncomplete*)skynet_malloc(sizeof(struct uncomplete));
    memset(uc, 0, sizeof(*uc));
    uc->next = q->hash[h];
    uc->pack.socket_id = socket_id;
    q->hash[h] = uc;

    return uc;
}

static inline uint32_t read_size(uint8_t* buffer)
{
    uint32_t r = *((uint32_t*)buffer);
    uint32_t size = ntohl(r);

    //print_bin("read_size", buffer, 4, size);

    if (size > SMALLSTRING)
    {
        size = SMALLSTRING;
    }

    return size;
}

static void push_more(lua_State* L, int socket_id, uint8_t* buffer, int size)
{
    if (size <= 3)
    {
        struct uncomplete* uc = save_uncomplete(L, socket_id);
        uc->read = -1;
        memcpy(&(uc->header), buffer, size);
        uc->header_size = size;
        return;
    }
    //print_bin("push_more", buffer, size, size);
    int pack_size = read_size(buffer);
    buffer += 4;
    size -= 4;

    if (pack_size > SMALLSTRING)
    {
        pack_size = SMALLSTRING;
    }

    if (size < pack_size)
    {
        struct uncomplete* uc = save_uncomplete(L, socket_id);
        uc->read = size;
        uc->pack.size = pack_size;
        uc->pack.buf_ptr = skynet_malloc(pack_size);
        memcpy(uc->pack.buf_ptr, buffer, size);
        return;
    }
    push_data(L, socket_id, buffer, pack_size, 1);

    buffer += pack_size;
    size -= pack_size;
    if (size > 0)
    {
        push_more(L, socket_id, buffer, size);
    }
}

static void close_uncomplete(lua_State* L, int socket_id)
{
    struct queue* q = (queue*)lua_touserdata(L, 1);
    struct uncomplete* uc = find_uncomplete(q, socket_id);
    if (uc)
    {
        skynet_free(uc->pack.buf_ptr);
        skynet_free(uc);
    }
}

static int filter_data_(lua_State* L, int socket_id, uint8_t* buffer, int size)
{
    struct queue* q = (queue*)lua_touserdata(L, 1);
    struct uncomplete* uc = find_uncomplete(q, socket_id);
    //print_bin("filter_data_", buffer, size, size);
    if (uc)
    {
        // fill uncomplete
        if (uc->read < 0)
        {
            // read size
            int pack_size = -1;
            int index = 0;
            char* p = (char*)(&(uc->header));
            while (size > 0)
            {
                p[uc->header_size] = buffer[index];
                index += 1;
                uc->header_size += 1;
                if (uc->header_size == 4)
                {
                    pack_size = read_size((uint8_t*)p);
                }
                if (pack_size >= 0 || index >= size)
                {
                    size -= index;
                    buffer += index;
                    break;
                }
            }

            if (pack_size == -1)
            {
                int h = hash_socket_id(socket_id);
                uc->next = q->hash[h];
                q->hash[h] = uc;
                return 1;
            }

            if (pack_size > SMALLSTRING)
            {
                pack_size = SMALLSTRING;
            }

            //取得包头长度以后开始生成新包
            uc->pack.buf_ptr = skynet_malloc(pack_size);
            uc->pack.size = pack_size;
            uc->read = 0;
        }
        int need = uc->pack.size - uc->read;
        if (size < need)
        {
            ::memcpy((char*)uc->pack.buf_ptr + uc->read, buffer, size);
            uc->read += size;
            int h = hash_socket_id(socket_id);
            uc->next = q->hash[h];
            q->hash[h] = uc;
            return 1;
        }
        //print_bin("filter_data_1", buffer, size, size);
        ::memcpy((char*)uc->pack.buf_ptr + uc->read, buffer, need);
        buffer += need;
        size -= need;
        if (size == 0)
        {
            lua_pushvalue(L, lua_upvalueindex(TYPE_DATA));
            lua_pushinteger(L, socket_id);
            lua_pushlightuserdata(L, uc->pack.buf_ptr);
            lua_pushinteger(L, uc->pack.size);
            skynet_free(uc);
            //print_bin("filter_data_4", buffer, size, size);
            return 5;
        }
        // more data
        push_data(L, socket_id, uc->pack.buf_ptr, uc->pack.size, 0);
        skynet_free(uc);
        push_more(L, socket_id, buffer, size);
        lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
        return 2;
    }
    else
    {
        if (size <= 3)
        {
            uncomplete* uc = save_uncomplete(L, socket_id);
            uc->read = -1;
            ::memcpy(&(uc->header), buffer, size);
            uc->header_size = size;
            return 1;
        }
        //print_bin("filter_data_2", buffer, size, size);
        int pack_size = read_size(buffer);
        buffer += 4;
        size -= 4;

        if (pack_size > SMALLSTRING)
        {
            pack_size = SMALLSTRING;
        }

        if (size < pack_size)
        {
            struct uncomplete* uc = save_uncomplete(L, socket_id);
            uc->read = size;
            uc->pack.size = pack_size;
            uc->pack.buf_ptr = skynet_malloc(pack_size);
            if (size > 0)
            {
                memcpy(uc->pack.buf_ptr, buffer, size);
            }
            //print_bin("filter_data_3", buffer, size, pack_size);
            return 1;
        }
        if (size == pack_size)
        {
            // just one package
            lua_pushvalue(L, lua_upvalueindex(TYPE_DATA));
            lua_pushinteger(L, socket_id);
            void* result = skynet_malloc(pack_size);
            memcpy(result, buffer, size);
            lua_pushlightuserdata(L, result);
            lua_pushinteger(L, size);
            return 5;
        }
        // more data
        push_data(L, socket_id, buffer, pack_size, 1);
        buffer += pack_size;
        size -= pack_size;
        push_more(L, socket_id, buffer, size);
        lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
        return 2;
    }
}

static inline int
filter_data(lua_State* L, int socket_id, uint8_t* buffer, int size)
{
    int ret = filter_data_(L, socket_id, buffer, size);
    // buffer is the data of socket message, it malloc at socket_server.c : function forward_message .
    // it should be free before return,
    skynet_free(buffer);
    return ret;
}

static void pushstring(lua_State* L, const char* msg, int size)
{
    if (msg)
    {
        lua_pushlstring(L, msg, size);
    }
    else
    {
        lua_pushliteral(L, "");
    }
}

/*
    userdata queue
    lightuserdata msg
    integer size
    return
        userdata queue
        integer type
        integer fd
        string msg | lightuserdata/integer
 */
static int l_filter(lua_State* L)
{
    skynet_socket_message* message = (skynet_socket_message*)lua_touserdata(L, 2);
    int size = luaL_checkinteger(L, 3);
    char* buffer = message->buffer;
    if (buffer == nullptr)
    {
        buffer = (char*)(message + 1);
        size -= sizeof(*message);
    }
    else
    {
        size = -1;
    }

    lua_settop(L, 1);

    switch (message->socket_event)
    {
    case SKYNET_SOCKET_EVENT_DATA:
        // ignore listen id (message->id)
        assert(size == -1);    // never padding string
        return filter_data(L, message->socket_id, (uint8_t*)buffer, message->ud);
    case SKYNET_SOCKET_EVENT_CONNECT:
        // ignore listen fd connect
        return 1;
    case SKYNET_SOCKET_EVENT_CLOSE:
        // no more data in fd (message->id)
        close_uncomplete(L, message->socket_id);
        lua_pushvalue(L, lua_upvalueindex(TYPE_CLOSE));
        lua_pushinteger(L, message->socket_id);
        return 3;
    case SKYNET_SOCKET_EVENT_ACCEPT:
        lua_pushvalue(L, lua_upvalueindex(TYPE_OPEN));
        // ignore listen id (message->id);
        lua_pushinteger(L, message->ud);
        pushstring(L, buffer, size);
        return 4;
    case SKYNET_SOCKET_EVENT_ERROR:
        // no more data in fd (message->id)
        close_uncomplete(L, message->socket_id);
        lua_pushvalue(L, lua_upvalueindex(TYPE_ERROR));
        lua_pushinteger(L, message->socket_id);
        pushstring(L, buffer, size);
        return 4;
    case SKYNET_SOCKET_EVENT_WARNING:
        lua_pushvalue(L, lua_upvalueindex(TYPE_WARNING));
        lua_pushinteger(L, message->socket_id);
        lua_pushinteger(L, message->ud);
        return 4;
    default:
        // never get here
        return 1;
    }
}

/*
    userdata queue
    return
        integer fd
        lightuserdata msg
        integer size
 */
static int l_pop(lua_State* L)
{
    struct queue* q = (queue*)lua_touserdata(L, 1);
    if (q == nullptr || q->head == q->tail)
        return 0;
    struct netpack* np = &q->queue[q->head];
    if (++q->head >= q->cap)
    {
        q->head = 0;
    }
    lua_pushinteger(L, np->socket_id);
    lua_pushlightuserdata(L, np->buf_ptr);
    lua_pushinteger(L, np->size);

    return 3;
}

/*
    string msg | lightuserdata/integer

    lightuserdata/integer
 */

static const char* tolstring(lua_State* L, size_t* sz, int index)
{
    const char* ptr;
    if (lua_isuserdata(L, index))
    {
        ptr = (const char*)lua_touserdata(L, index);
        *sz = (size_t)luaL_checkinteger(L, index + 1);
    }
    else
    {
        ptr = luaL_checklstring(L, index, sz);
    }
    return ptr;
}

static inline void write_size(uint8_t* buffer, int len)
{
    uint32_t net_len = htonl(len);
    memcpy(buffer, &net_len, sizeof(uint32_t));
}

static int l_pack(lua_State* L)
{
    size_t len;
    const char* ptr = tolstring(L, &len, 1);
    if (len >= 0x200000)
    {
        return luaL_error(L, "Invalid size (too long) of data : %d", (int)len);
    }

    uint8_t* buffer = (uint8_t*)skynet_malloc(len + 4);
    write_size(buffer, len);
    memcpy(buffer + 4, ptr, len);

    lua_pushlightuserdata(L, buffer);
    lua_pushinteger(L, len + 4);

    return 2;
}

static int l_tostring(lua_State* L)
{
    void* ptr = lua_touserdata(L, 1);
    int size = luaL_checkinteger(L, 2);
    if (ptr == nullptr)
    {
        lua_pushliteral(L, "");
    }
    else
    {
        lua_pushlstring(L, (const char*)ptr, size);
        skynet_free(ptr);
    }
    return 1;
}

} }

/**
 * skynet luaclib - skynet.netpack.tcp
 */

static const luaL_Reg netpack_funcs[] = {
    { "pop",      skynet::luaclib::l_pop },
    { "pack",     skynet::luaclib::l_pack },
    { "clear",    skynet::luaclib::l_clear },
    { "tostring", skynet::luaclib::l_tostring },

    { nullptr, nullptr },
};

LUAMOD_API int luaopen_skynet_netpack_tcp(lua_State* L)
{
    luaL_checkversion(L);

    luaL_newlib(L, netpack_funcs);

     // the order is same with macros : TYPE_* (defined top)
     lua_pushliteral(L, "data");
     lua_pushliteral(L, "more");
     lua_pushliteral(L, "error");
     lua_pushliteral(L, "open");
     lua_pushliteral(L, "close");
     lua_pushliteral(L, "warning");

     //
     lua_pushcclosure(L, skynet::luaclib::l_filter, 6);
     lua_setfield(L, -2, "filter");

    return 1;
}
