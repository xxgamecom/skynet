#define LUA_LIB

#include "skynet.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdint>
#include <string>
#include <cassert>

#include <arpa/inet.h>

namespace skynet { namespace luaclib {

#define QUEUE_SIZE          1024
#define HASH_SIZE           4096
#define SMALLSTRING         65536

#define TYPE_DATA           1
#define TYPE_MORE           2
#define TYPE_ERROR          3
#define TYPE_OPEN           4
#define TYPE_CLOSE          5
#define TYPE_WARNING        6

// used to assamble complete package. then handle by gateserver.
// netpack specs:
// uint16 + data, uint16 (serialized in big-endian) is the data size (bytes).
// 如果采用sproto打包方式，需附加4字节(32位)的session值。所以客户端传过来1kb数据，实际数据只有1024-2-4=1018字节。
struct netpack
{
    int                     socket_id = 0;              // socket id
    int                     size = 0;                   // data size
    void*                   buf_ptr = nullptr;          // data
};

// uncomplete package
struct uncomplete
{
    netpack                 pack;                       // 数据块信息
    uncomplete*             next = nullptr;             // 链表，指向下一个
    int                     read = 0;                   // 已读的字节数
    int                     header = 0;                 // 第一个字节(代表数据长度的高8位)
    int                     header_size = 0;
};

//
struct msg_queue
{
    int                     cap = 0;
    int                     head = 0;
    int                     tail = 0;
    uncomplete*             hash[HASH_SIZE];            // 一次从内核读取多个tcp包时放入该队列里
    netpack                 queue[QUEUE_SIZE];          // 指针数组，数组里每个位置指向一个不完整的tcp包链表，fd hash值相同的组成一个链表
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

static void clear_list(uncomplete* uc)
{
    while (uc != nullptr)
    {
        skynet_free(uc->pack.buf_ptr);
        uncomplete* tmp = uc;
        uc = uc->next;
        skynet_free(tmp);
    }
}

static inline int hash_socket_id(int socket_id)
{
    int a = socket_id >> 24;
    int b = socket_id >> 12;
    int c = socket_id;
    return (int)(((uint32_t)(a + b + c)) % HASH_SIZE);
}

static uncomplete* _find_uncomplete(msg_queue* q, int socket_id)
{
    if (q == nullptr)
        return nullptr;

    int h = hash_socket_id(socket_id);
    uncomplete* uc = q->hash[h];
    if (uc == nullptr)
        return nullptr;

    if (uc->pack.socket_id == socket_id)
    {
        q->hash[h] = uc->next;
        return uc;
    }

    uncomplete* last = uc;
    while (last->next != nullptr)
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

static msg_queue* get_queue(lua_State* L)
{
    auto q = (msg_queue*)lua_touserdata(L, 1);
    if (q == nullptr)
    {
        q = (msg_queue*)lua_newuserdata(L, sizeof(msg_queue));
        q->cap = QUEUE_SIZE;
        q->head = 0;
        q->tail = 0;
        int i;
        for (i = 0; i < HASH_SIZE; i++)
        {
            q->hash[i] = nullptr;
        }
        lua_replace(L, 1);
    }

    return q;
}

static void expand_queue(lua_State* L, msg_queue* q)
{
    auto nq = (msg_queue*)lua_newuserdata(L, sizeof(msg_queue) + q->cap * sizeof(netpack));
    nq->cap = q->cap + QUEUE_SIZE;
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

static void push_data(lua_State* L, int socket_id, void* buffer, int size, int clone)
{
    if (clone)
    {
        void* tmp = skynet_malloc(size);
        ::memcpy(tmp, buffer, size);
        buffer = tmp;
    }
    //print_bin("push_data", buffer, size, size);
    msg_queue* q = get_queue(L);
    netpack* np = &q->queue[q->tail];
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
    msg_queue* q = get_queue(L);
    int h = hash_socket_id(socket_id);
    auto uc = (uncomplete*)skynet_malloc(sizeof(uncomplete));
    ::memset(uc, 0, sizeof(*uc));
    uc->next = q->hash[h];
    uc->pack.socket_id = socket_id;
    q->hash[h] = uc;

    return uc;
}

static inline uint32_t _read_size(uint8_t* buffer)
{
    uint32_t r = *((uint32_t*)buffer);
    uint32_t size = ntohl(r);

    //print_bin("_read_size", buffer, 4, size);

    if (size > SMALLSTRING)
    {
        size = SMALLSTRING;
    }

    return size;
}

static void _push_more(lua_State* L, int socket_id, uint8_t* buffer, int size)
{
    if (size <= 3)
    {
        uncomplete* uc = save_uncomplete(L, socket_id);
        uc->read = -1;
        ::memcpy(&(uc->header), buffer, size);
        uc->header_size = size;
        return;
    }

    //print_bin("_push_more", buffer, size, size);
    int pack_size = _read_size(buffer);
    buffer += 4;
    size -= 4;

    if (pack_size > SMALLSTRING)
    {
        pack_size = SMALLSTRING;
    }

    if (size < pack_size)
    {
        uncomplete* uc = save_uncomplete(L, socket_id);
        uc->read = size;
        uc->pack.size = pack_size;
        uc->pack.buf_ptr = skynet_malloc(pack_size);
        ::memcpy(uc->pack.buf_ptr, buffer, size);
        return;
    }
    push_data(L, socket_id, buffer, pack_size, 1);

    buffer += pack_size;
    size -= pack_size;
    if (size > 0)
    {
        _push_more(L, socket_id, buffer, size);
    }
}

static void _close_uncomplete(lua_State* L, int socket_id)
{
    auto q = (msg_queue*)lua_touserdata(L, 1);
    uncomplete* uc = _find_uncomplete(q, socket_id);
    if (uc != nullptr)
    {
        skynet_free(uc->pack.buf_ptr);
        skynet_free(uc);
    }
}

//
static int filter_data_(lua_State* L, int socket_id, uint8_t* buffer, int size)
{
    auto q = (msg_queue*)lua_touserdata(L, 1);
    uncomplete* uc = _find_uncomplete(q, socket_id);
    //print_bin("filter_data_", buffer, size, size);
    if (uc != nullptr)
    {
        // fill uncomplete
        if (uc->read < 0)
        {
            // 之前只收到一个字节, 加上该数据块的第一个字节，表示整个包的长度
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
                    pack_size = _read_size((uint8_t*)p);
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

            // 取得包头长度以后开始生成新包
            uc->pack.buf_ptr = skynet_malloc(pack_size);
            uc->pack.size = pack_size;
            uc->read = 0;
        }
        int need = uc->pack.size - uc->read;    // 包还差多少字节
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
        _push_more(L, socket_id, buffer, size);
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
        int pack_size = _read_size(buffer);    // 需要数据包的字节数
        buffer += 4;
        size -= 4;

        if (pack_size > SMALLSTRING)
        {
            pack_size = SMALLSTRING;
        }

        // 说明还有未获得的数据包
        if (size < pack_size)
        {
            uncomplete* uc = save_uncomplete(L, socket_id);    // 保存这个数据包
            uc->read = size;
            uc->pack.size = pack_size;
            uc->pack.buf_ptr = skynet_malloc(pack_size);
            if (size > 0)
            {
                ::memcpy(uc->pack.buf_ptr, buffer, size);
            }
            //print_bin("filter_data_3", buffer, size, pack_size);
            return 1;
        }
        // 说明是一个完整包，把包返回给Lua层即可
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
        // 说明不止同一个数据包,还有额外的
        push_data(L, socket_id, buffer, pack_size, 1);    // 保存第一个包到q->queue中
        buffer += pack_size;
        size -= pack_size;
        _push_more(L, socket_id, buffer, size);    // 处理余下的包
        lua_pushvalue(L, lua_upvalueindex(TYPE_MORE));
        return 2;
    }
}

//
static inline int _filter_data(lua_State* L, int socket_id, uint8_t* buffer, int size)
{
    int ret = filter_data_(L, socket_id, buffer, size);
    // buffer is the data of socket message, it malloc at socket_server.c : function forward_message .
    // it should be free before return,
    skynet_free(buffer);
    return ret;
}

static void _pushstring(lua_State* L, const char* msg, int size)
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
    string msg | lightuserdata/integer

    lightuserdata/integer
 */

static const char* _tolstring(lua_State* L, size_t* sz, int index)
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

/**
 *
 * arguments:
 * 1 message queue          - userdata (msg_queue)
 *
 * outputs:
 * 1 socket id              - integer
 * 2 buffer                 - lightuserdata
 * 3 buffer size            - integer
 *
 * lua examples:
 * local fd, msg, sz = netpack.pop(msg_queue)
 */
static int l_pop(lua_State* L)
{
    // get msg_queue
    auto q = (msg_queue*)lua_touserdata(L, 1);
    // msg_queue is empty
    if (q == nullptr || q->head == q->tail)
        return 0;

    //
    netpack* np = &q->queue[q->head];
    if (++q->head >= q->cap)
    {
        q->head = 0;
    }
    lua_pushinteger(L, np->socket_id);
    lua_pushlightuserdata(L, np->buf_ptr);
    lua_pushinteger(L, np->size);

    return 3;
}

static inline void write_size(uint8_t* buffer, int len)
{
    uint32_t net_len = htonl(len);
    memcpy(buffer, &net_len, sizeof(uint32_t));
}

/**
 * pack message
 *
 * arguments:
 * 1 message                - string | lightuserdata
 *
 * outputs:
 * 1 packed buffer          - lightuserdata
 * 2 packed size            - integer
 *
 * lua examples:
 * socket_core.send(fd, netpack.pack(result))
 */
static int l_pack(lua_State* L)
{
    //
    size_t len;
    const char* ptr = _tolstring(L, &len, 1);
    if (len >= 0x200000)
    {
        return luaL_error(L, "Invalid size (too long) of data : %d", (int)len);
    }

    uint8_t* buffer = (uint8_t*)skynet_malloc(len + 4);

    // size
    write_size(buffer, len);

    // data
    ::memcpy(buffer + 4, ptr, len);

    // return packed buffer
    lua_pushlightuserdata(L, buffer);
    // return packed buffer size
    lua_pushinteger(L, len + 4);

    return 2;
}

/**
 * clear message queue
 *
 * arguments:
 * 1 message queue              - userdata (msg_queue)
 *
 * lua examples:
 * local CMD = setmetatable({}, { __gc = function()
 *     netpack.clear(msg_queue)
 * end })
 */
static int l_clear(lua_State* L)
{
    auto q = (msg_queue*)lua_touserdata(L, 1);
    if (q == nullptr)
        return 0;

    for (int i = 0; i < HASH_SIZE; i++)
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
        skynet_free(np->buf_ptr);
    }
    q->head = q->tail = 0;

    return 0;
}

/**
 * convert to string message
 *
 * arguments:
 * 1 message                - userdata
 * 2 message size           - integer
 *
 * outputs:
 * 1 string message         - string
 *
 * lua examples:
 * local message = netpack.tostring(msg, sz)
 */
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

/**
 * filter skynet socket message (skynet_socket_message)
 *
 * arguments:
 * 1 skynet socket message      - userdata (skynet_socket_message)
 * 2 message                    - lightuserdata
 * 3 message size               - integer
 *
 * outputs:
 * 1 queue                      - userdata
 * 2 type                       - integer
 * 3 fd                         - integer
 * 4 msg                        - string | lightuserdata
 *
 * lua examples:
 * netpack.filter( msg_queue, msg, sz)
 */
static int l_filter(lua_State* L)
{
    // message
    auto msg_ptr = (skynet_socket_message*)lua_touserdata(L, 2);

    // message size
    int size = luaL_checkinteger(L, 3);

    char* data_ptr = msg_ptr->buffer;
    if (data_ptr == nullptr)
    {
        data_ptr = (char*)(msg_ptr + 1);
        size -= sizeof(*msg_ptr);
    }
    else
    {
        size = -1;
    }

    // clean
    lua_settop(L, 1);

    switch (msg_ptr->socket_event)
    {
    case SKYNET_SOCKET_EVENT_DATA:
        // ignore listen socket id (message->socket_id)
        assert(size == -1);    // never padding string
        return _filter_data(L, msg_ptr->socket_id, (uint8_t*)data_ptr, msg_ptr->ud);
    case SKYNET_SOCKET_EVENT_CONNECT:
        // ignore listen socket id connect
        return 1;
    case SKYNET_SOCKET_EVENT_CLOSE:
        // no more data in fd (message->socket_id)
        _close_uncomplete(L, msg_ptr->socket_id);
        lua_pushvalue(L, lua_upvalueindex(TYPE_CLOSE));
        lua_pushinteger(L, msg_ptr->socket_id);
        return 3;
    case SKYNET_SOCKET_EVENT_ACCEPT:
        lua_pushvalue(L, lua_upvalueindex(TYPE_OPEN));
        // ignore listen id (message->socket_id);
        lua_pushinteger(L, msg_ptr->ud);
        _pushstring(L, data_ptr, size);
        return 4;
    case SKYNET_SOCKET_EVENT_ERROR:
        // no more data in fd (message->socket_id)
        _close_uncomplete(L, msg_ptr->socket_id);
        lua_pushvalue(L, lua_upvalueindex(TYPE_ERROR));
        lua_pushinteger(L, msg_ptr->socket_id);
        _pushstring(L, data_ptr, size);
        return 4;
    case SKYNET_SOCKET_EVENT_WARNING:
        lua_pushvalue(L, lua_upvalueindex(TYPE_WARNING));
        lua_pushinteger(L, msg_ptr->socket_id);
        lua_pushinteger(L, msg_ptr->ud);
        return 4;
    }

    // never get here
    return 1;
}

} }

/**
 * skynet luaclib - skynet.netpack.tcp
 */

#if __cplusplus
extern "C" {
#endif

static const luaL_Reg netpack_funcs[] = {
    { "pop",      skynet::luaclib::l_pop },
    { "pack",     skynet::luaclib::l_pack },
    { "clear",    skynet::luaclib::l_clear },
    { "tostring", skynet::luaclib::l_tostring },

    { nullptr,    nullptr },
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

#if __cplusplus
}
#endif
