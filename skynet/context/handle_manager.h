/**
 * service handle manager
 * 
 * service handle specs:
 * 1)
 * 
 * 
 */

#pragma once

#include <cstdint>
#include <shared_mutex>

namespace skynet {

// forward delcare
struct skynet_context;

/**
 * store all service handles
 * 
 * 创建一个新的skynet_context时，会往slot列表中放，当一个消息送达一个context时，
 * 其callback函数就会被调用，callback函数一般在module的init函数里指定，调用callback函数时，
 * 会传入userdata（一般是instance指针），source（发送方的服务id），type（消息类型），
 * msg和sz（数据及其大小），每个服务的callback处理各自的逻辑。这里其实可以将modules视为工厂，
 * 而skynet_context则是该工厂创建出来的实例，而这些实例，则是通过handle_storage来进行管理。
 */

// service handle manager
class handle_manager
{
private:
    static handle_manager* instance_;
public:
    static handle_manager* instance();

private:
    // service handle (corresponding relationship between handle and name)
    struct handle_name
    {
        std::string             name = "";                  // service name
        uint32_t                svc_handle = 0;             // service handle
    };

private:
    std::shared_mutex           rw_mutex_;                  // read write lock, must C++17

    uint32_t                    handle_index_;              // 创建下一个服务时，该服务的slot idx，一般会先判断该slot是否被占用, 从1开始计数, 因为0被系统保留
    int                         slot_size_;                 // 已分配的ctx的容量, 一定是2^n，初始值是4
    skynet_context**            slot_;                      // slot_size容量的数组, 每一项指向一个ctx

    // 在上层逻辑很难记住每个handle具体代表哪个服务，通常会为handle注册name（不限一个），通过name找到对应的handle，通过S->name实现。
    // S->name是一个数组，类似S->slot，动态分配内存，S->name_cap表示数组容量。
    // S->name是按handle_name->name升序排序的，通过二分查找快速地查找name对应的handle（skynet_handle_findname）。
    // 给handle注册name时，也需保证注册完S->name有序（skynet_handle_namehandle）。
    int                         name_cap_;                  // 已分配ctx名字(别名)的容量, 大小为2^n
    int                         name_count_;                // 别名数量
    handle_name*                name_;                      // 别名列表, name_cap容量的数组，每一项是一个handle_name

public:
    // initialize service handle manager
    void init();

    // register service
    uint32_t registe(skynet_context* ctx);
    // 利用ID注销一个服务
    int retire(uint32_t svc_handle);
    // 注销全部服务
    void retireall();

    // 利于ID获取服务上下文指针
    skynet_context* grab(uint32_t svc_handle);

    // 利用服务名字获取服务ID （二分法）
    uint32_t find_by_name(const char* name);
    // 赋予一个ID名字, TODO: change function name
    const char* set_handle_by_name(const char* name, uint32_t svc_handle);

private:
    //
    const char* _insert_name(const char* name, uint32_t svc_handle);
    // 把name插入到name数组中，再关联handle
    void _insert_name_before(char* name, uint32_t svc_handle, int before);
};


}

