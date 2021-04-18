#ifndef __FIBER_H__
#define __FIBER_H__

#include <memory>
#include <ucontext.h>
//C++好用的库，functional 跟 shared_ptr 必须是靠前的几个
//functional 解决了函数指针不太适合的几种场景
//比如不能动态传不同参数的回调进去。如果多几个参数，还得用类传进来啥的
//函数指针就是典型的c风格编程。functional就现代化的多
#include <functional>

//用到互斥量那些
#include "thread.h"

namespace sylar
{
class Scheduler;
//继承它，是为了拿出 this 的智能指针
//继承之后，这个 class 就不能在栈里创建了。因为需要用到智能指针的内啥
class Fiber : public std::enable_shared_from_this<Fiber>
{
friend class Scheduler;
public:
    typedef std::shared_ptr<Fiber> ptr;

    //状态枚举
    enum State
    {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };
private:
    //不允许默认构造，必须带入口函数
    Fiber();

public:
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
    ~Fiber();

    //比如，已经执行完了，就可以重新定义函数复用进去,INIT, TERM 两种状态可以合法调用
    void reset(std::function<void()> cb);

    //切换，不不提供了。这不是完整的任意切换的、可以进入的协程。
    //子协程执行完之后，就会把运行的控制权丢回去给主协程（线程下的唯一主协程）
    //子协程不允许直接切换子协程（但允许新建）。必须回到主协程。这样方便管理。
    // void call();

    void swapIn(); //进去执行了
    void swapOut(); //我不执行了，让出来，自己到后台去

    //比较特殊的存在。。
    void call();
    void back();

    uint64_t getId() const { return m_id; }
    State getState() const { return m_state; }

public:
    //设置当前协程
    static void SetThis(Fiber* f);
    //获取当前运行的协程
    static Fiber::ptr GetThis();
    //设置自己的状态
    //协程切换到后台，并且设置为ready
    static void YieldToReady();
    //协程切换到后台，并且设置为hold
    static void YieldToHold();

    //调试，总协程数
    static uint64_t TotalFibers();

    static void MainFunc();
    //为了配合 scheduler 搞的，非常 ugly
    static void CallerMainFunc();
    static uint64_t GetFiberId();

private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = INIT;

    ucontext_t m_ctx;
    void* m_stack = nullptr;

    std::function<void()> m_cb;
};

}
#endif
