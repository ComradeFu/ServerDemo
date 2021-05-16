// 主要是改写一些系统上的同步操作，这样能让同步也有异步的性能。
// 比如mysql库，就必须同步等待返回。我们可以重写成让epoll等待即可。
// 还有 sleep、usleep 函数等
#ifndef __SYLAR_HOOK_H__
#define __SYLAR_HOOK_H__

#include <unistd.h>

namespace sylar 
{
    //不是所有的线程、所有的功能都是 hook 住的。我们这个粒度可以细化到线程这个级别去。
    bool is_hook_enagle();
    void set_hook_enable(bool flag);
}

// c 风格函数。编译器会放弃所有C++做的特殊操作，比如重载的时候，会给函数加一些奇奇怪怪的符号，来以示区分
extern "C"
{
//签名，为了切换老的、新的
typedef unsigned int (*sleep_func)(unsigned int seconds);
extern sleep_func sleep_f;

typedef int (*usleep_func)(useconds_t usec);
extern usleep_func usleep_f;
}

#endif
