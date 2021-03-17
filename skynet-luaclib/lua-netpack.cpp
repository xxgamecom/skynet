/**
 * skynet luaclib - skynet.netpack
 * 
 * 
 */

#define LUA_LIB

#include "skynet.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdint>
#include <string>

// #include <assert.h>
// #include <stdlib.h>

#define QUEUESIZE       1024
#define HASHSIZE        4096
#define SMALLSTRING     2048

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
    int                     id = 0;                     // socket id
    int                     size = 0;                   // data size
    void*                   buf_ptr = nullptr;          // data
};

// 不完整tcp包结构
struct uncomplete
{
    netpack                 pack;                       // 数据块信息
    uncomplete*             next = nullptr;             // 链表，指向下一个
    int                     read = 0;                   // 已读的字节数
    int                     header = 0;                 // 第一个字节(代表数据长度的高8位)
};

// 
struct queue
{
    int                     cap = 0;                    //
    int                     head = 0;                   //
    int                     tail = 0;                   //
    uncomplete*             hash[HASHSIZE];             // 一次从内核读取多个tcp包时放入该队列里
    netpack                 queue[QUEUESIZE];           // 指针数组，数组里每个位置指向一个不完整的tcp包链表，fd hash值相同的组成一个链表
};

static void clear_list(uncomplete* uc)
{
    while (uc != nullptr)
    {
        delete[] uc->pack.buf_ptr;
        uncomplete* tmp = uc;
        uc = uc->next;
        delete tmp;
    }
}

static int l_clear(lua_State* L)
{
    queue* q = (queue*)lua_touserdata(L, 1);
    if (q == nullptr)
        return 0;

    for (int i = 0; i < HASHSIZE; i++)
    {
        clear_list(q->hash[i]);
        q->hash[i] = nullptr;
    }
    if (q->head > q->tail)
    {
        q->tail += q->cap;
    }
    for (int i = q->head; i < q->tail; i++)
    {
        netpack* np = &q->queue[i % q->cap];
        delete[] np->buf_ptr;
    }
    q->head = q->tail = 0;

    return 0;
}

static inline int hash_fd(int fd)
{
    int a = fd >> 24;
    int b = fd >> 12;
    int c = fd;
    return (int)(((uint32_t)(a + b + c)) % HASHSIZE);
}

static uncomplete* find_uncomplete(queue* q, int fd)
{
    if (q == nullptr)
        return nullptr;

    int h = hash_fd(fd);
    uncomplete* uc = q->hash[h];
    if (uc == nullptr)
        return nullptr;
    if (uc->pack.id == fd)
    {
        q->hash[h] = uc->next;
        return uc;
    }
    uncomplete* last = uc;
    while (last->next)
    {
        uc = last->next;
        if (uc->pack.id == fd)
        {
            last->next = uc->next;
            return uc;
        }
        last = uc;
    }

    return nullptr;
}

static queue* get_queue(lua_State* L)
{
    queue* q = (queue*)lua_touserdata(L, 1);
    if (q == nullptr)
    {
        q = (queue*)lua_newuserdatauv(L, sizeof(queue), 0);
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

static void expand_queue(lua_State* L, queue* q)
{
    queue* nq = (queue*)lua_newuserdatauv(L, sizeof(queue) + q->cap * sizeof(netpack), 0);
    nq->cap = q->cap + QUEUESIZE;
    nq->head = 0;
    nq->tail = q->cap;
    ::memcpy(nq->hash, q->hash, sizeof(nq->hash));
    ::memset(q->hash, 0, sizeof(q->hash));
    for (int i = 0; i < q->cap; i++)
    {
        int idx = (q->head + i) % q->cap;
        nq->queue[i] = q->queue[idx];
    }
    q->head = q->tail = 0;
    lua_replace(L, 1);
}

static void push_data(lua_State* L, int fd, void* buffer, int size, int clone)
{
    if (clone)
    {
        void* tmp = new char[size];
        ::memcpy(tmp, buffer, size);
        buffer = tmp;
    }
    queue* q = get_queue(L);
    netpack* np = &q->queue[q->tail];
    if (++q->tail >= q->cap)
        q->tail -= q->cap;
    np->id = fd;
    np->buf_ptr = buffer;
    np->size = size;
    if (q->head == q->tail)
    {
        expand_queue(L, q);
    }
}

static struct uncomplete* save_uncomplete(lua_State* L, int fd)
{
    queue* q = get_queue(L);
    int h = hash_fd(fd);
    uncomplete* uc = new uncomplete;
    ::memset(uc, 0, sizeof(*uc));
    uc->next = q->hash[h];
    uc->pack.id = fd;
    q->hash[h] = uc;

    return uc;
}

static inline int read_size(uint8_t* buffer)
{
    return (int)buffer[0] << 8 | (int)buffer[1];
}

static void push_more(lua_State* L, int fd, uint8_t* buffer, int size)
{
    if (size == 1)
    {
        uncomplete* uc = save_uncomplete(L, fd);
        uc->read = -1;
        uc->header = *buffer;
        return;
    }

    int pack_size = read_size(buffer);
    buffer += 2;
    size -= 2;

    if (size < pack_size)
    {
        uncomplete* uc = save_uncomplete(L, fd);
        uc->read = size;
        uc->pack.size = pack_size;
        uc->pack.buf_ptr = new char[pack_size];
        ::memcpy(uc->pack.buf_ptr, buffer, size);
        return;
    }
    push_data(L, fd, buffer, pack_size, 1);

    buffer += pack_size;
    size -= pack_size;
    if (size > 0)
    {
        push_more(L, fd, buffer, size);
    }
}

static void close_uncomplete(lua_State* L, int fd)
{
    queue* q = (queue*)lua_touserdata(L, 1);
    uncomplete* uc = find_uncomplete(q, fd);
    if (uc != nullptr)
    {
        delete[] uc->pack.buf_ptr;
        delete uc;
    }
}

// 
static int filter_data_(lua_State* L, int fd, uint8_t* buffer, int size)
{
    queue* q = (queue*)lua_touserdata(L, 1);
    uncomplete* uc = find_uncomplete(q, fd);
    if (uc != nullptr)
    {   // 之前收到该包的部分数据块
        // fill uncomplete
        if (uc->read < 0)
        {   // 之前只收到一个字节,加上该数据块的第一个字节，表示整个包的长度
            // read size
            assert(uc->read == -1);
            int pack_size = *buffer;
            pack_size |= uc->header << 8;
            ++buffer;
            --size;
            uc->pack.size = pack_size;
            uc->pack.buf_ptr = new char[pack_size];
            uc->read = 0;
        }
        int need = uc->pack.size - uc->read;    // 包还差多少字节
        if (size < need)
        {
            ::memcpy((char*)uc->pack.buf_ptr + uc->read, buffer, size);
            uc->read += size;
            int h = hash_fd(fd);
            uc->next = q->hash[h];
            q->hash[h] = uc;
            return 1;
        }
        memcpy((char*)uc->pack.buf_ptr + uc->read, buffer, need);
        buffer += need;
        size -= need;
        if (size == 0)
        {
            lua_pushvalue(L, lua_upvalueindex(TYPE_DATA));
            lua_pushinteger(L, fd);
            lua_pushlightuserdata(L, uc->pack.buf_ptr);
            lua_pushinteger(L, uc->pack.size);
            delete uc;
            return 5;
        }
        // more data
        push_data(L, fd, uc->pack.buf_ptr, uc->pack.size, 0);
        delete uc;
        push_more(L, fd, buffer, size);
        lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
        return 2;
    }
    else
    {
        if (size == 1)
        {
            struct uncomplete* uc = save_uncomplete(L, fd);
            uc->read = -1;
            uc->header = *buffer;
            return 1;
        }
        int pack_size = read_size(buffer);    // 需要数据包的字节数
        buffer += 2;
        size -= 2;

        // 说明还有未获得的数据包
        if (size < pack_size)
        {
            struct uncomplete* uc = save_uncomplete(L, fd);    // 保存这个数据包
            uc->read = size;
            uc->pack.size = pack_size;
            uc->pack.buf_ptr = new char[pack_size];
            memcpy(uc->pack.buf_ptr, buffer, size);
            return 1;
        }
        // 说明是一个完整包，把包返回给Lua层即可
        if (size == pack_size)
        {
            // just one package
            lua_pushvalue(L, lua_upvalueindex(TYPE_DATA));
            lua_pushinteger(L, fd);
            void* result = new char[pack_size];
            memcpy(result, buffer, size);
            lua_pushlightuserdata(L, result);
            lua_pushinteger(L, size);
            return 5;
        }
        // more data
        // 说明不止同一个数据包,还有额外的
        push_data(L, fd, buffer, pack_size, 1);    // 保存第一个包到q->queue中
        buffer += pack_size;
        size -= pack_size;
        push_more(L, fd, buffer, size);    // 处理余下的包
        lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
        return 2;
    }
}

static inline int filter_data(lua_State* L, int fd, uint8_t* buffer, int size)
{
    int ret = filter_data_(L, fd, buffer, size);
    // buffer is the data of socket message, it malloc at socket_server.c : function forward_message .
    // it should be free before return,
    delete[] buffer;
    return ret;
}

static void pushstring(lua_State* L, const char* msg, int size)
{
    if (msg != nullptr)
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
    skynet::skynet_socket_message* message = (skynet::skynet_socket_message*)lua_touserdata(L, 2);
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
    case skynet::SKYNET_SOCKET_EVENT_DATA:
        // ignore listen socket id (message->socket_id)
        assert(size == -1);    // never padding string
        return filter_data(L, message->socket_id, (uint8_t*)buffer, message->ud);
    case skynet::SKYNET_SOCKET_EVENT_CONNECT:
        // ignore listen fd connect
        return 1;
    case skynet::SKYNET_SOCKET_EVENT_CLOSE:
        // no more data in fd (message->socket_id)
        close_uncomplete(L, message->socket_id);
        lua_pushvalue(L, lua_upvalueindex(TYPE_CLOSE));
        lua_pushinteger(L, message->socket_id);
        return 3;
    case skynet::SKYNET_SOCKET_EVENT_ACCEPT:
        lua_pushvalue(L, lua_upvalueindex(TYPE_OPEN));
        // ignore listen id (message->socket_id);
        lua_pushinteger(L, message->ud);
        pushstring(L, buffer, size);
        return 4;
    case skynet::SKYNET_SOCKET_EVENT_ERROR:
        // no more data in fd (message->socket_id)
        close_uncomplete(L, message->socket_id);
        lua_pushvalue(L, lua_upvalueindex(TYPE_ERROR));
        lua_pushinteger(L, message->socket_id);
        pushstring(L, buffer, size);
        return 4;
    case skynet::SKYNET_SOCKET_EVENT_WARNING:
        lua_pushvalue(L, lua_upvalueindex(TYPE_WARNING));
        lua_pushinteger(L, message->socket_id);
        lua_pushinteger(L, message->ud);
        return 4;
    }

    // never get here
    return 1;
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
    queue* q = (queue*)lua_touserdata(L, 1);
    if (q == nullptr || q->head == q->tail)
        return 0;

    netpack* np = &q->queue[q->head];
    if (++q->head >= q->cap)
    {
        q->head = 0;
    }
    lua_pushinteger(L, np->id);
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
    buffer[0] = (len >> 8) & 0xff;
    buffer[1] = len & 0xff;
}

static int l_pack(lua_State* L)
{
    size_t len;
    const char* ptr = tolstring(L, &len, 1);
    if (len >= 0x10000)
    {
        return luaL_error(L, "Invalid size (too long) of data : %d", (int)len);
    }

    uint8_t* buffer = new uint8_t[len + 2];
    write_size(buffer, len);
    memcpy(buffer + 2, ptr, len);

    lua_pushlightuserdata(L, buffer);
    lua_pushinteger(L, len + 2);

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
        delete ptr;
    }

    return 1;
}

LUAMOD_API int luaopen_skynet_netpack(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg l[] = {
        { "pop", l_pop },
        { "pack", l_pack },
        { "clear", l_clear },
        { "tostring", l_tostring },

        { nullptr, nullptr },
    };
    luaL_newlib(L, l);

    // the order is same with macros : TYPE_* (defined top)
    lua_pushliteral(L, "data");
    lua_pushliteral(L, "more");
    lua_pushliteral(L, "error");
    lua_pushliteral(L, "open");
    lua_pushliteral(L, "close");
    lua_pushliteral(L, "warning");

    lua_pushcclosure(L, l_filter, 6);
    lua_setfield(L, -2, "filter");

    return 1;
}
