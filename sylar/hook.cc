#include "hook.h"
#include <dlfcn.h>
#include "fiber.h"
#include "iomanager.h"

namespace sylar
{
static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \

void hook_init()
{
    static bool is_inited = false;
    if(is_inited)
        return;

// ## 连接，从动态库里面，取出来这个函数（原先的函数），赋值给我们的变量
#define XX(name) name ## _f = (name ## _func)dlsym(RTLD_NEXT, #name);
    //赋值出来
    HOOK_FUN(XX);
#undef XX
}

//故技重施，在 main 函数之前执行某个初始化函数
struct _HookIniter
{
    _HookIniter()
    {
        hook_init();
    }
};

static _HookIniter s_hook_initer;

bool is_hook_enable()
{
    return t_hook_enable;
}

void set_hook_enable(bool flag)
{
    t_hook_enable = flag;
}

}//namespace sylar

extern "C"
{
//先初始化为空
#define XX(name) name ## _func name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds)
{
    if(!sylar::t_hook_enable)
    {
        //原来的函数
        return sleep_f(seconds);
    }

    //如果hook住了，就实现一下

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();

    //过多少秒之后换回来，就是重新 schedule 这个 fiber
    // iom->addTimer(seconds * 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
    //上面的不支持 scheduler 是模板，先用lambda 处理一下
    iom->addTimer(seconds * 1000, [iom, fiber](){
        iom->schedule(fiber);
    });

    //换出去
    sylar::Fiber::YieldToHold();

    return 0;
}

int usleep(useconds_t usec)
{
    if(!sylar::t_hook_enable)
    {
        return usleep_f(usec);
    }

    //如果hook住了，就实现一下

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();

    //过多少秒之后换回来，就是重新 schedule 这个 fiber
    // iom->addTimer(seconds / 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
    //上面的不支持 scheduler 是模板，先用lambda 处理一下
    iom->addTimer(usec / 1000, [iom, fiber](){
        iom->schedule(fiber);
    });

    //换出去
    sylar::Fiber::YieldToHold();

    return 0;
}

}//extern C
