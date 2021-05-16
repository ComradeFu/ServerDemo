#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace sylar 
{
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

//当前调度器
static thread_local Scheduler* t_scheduler = nullptr;
//本协程的主函数
static thread_local Fiber* t_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    :m_name(name) {
    SYLAR_ASSERT(threads > 0);

    if(use_caller)
    {
        //如果没有就新建一个主协程
        sylar::Fiber::GetThis();
        //就没必要多分配一个线程了，因为可以直接用当前的
        --threads;

        //因为，一个线程，必然是只有一个协程调度器的！
        //如果在初始化之前已经有了一个，那必然是有问题的
        SYLAR_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

        //新的线程的主协程，并不会参与调度。因为主协程的入口不是run
        //所以调度器必须有一个自己的主协程，来做调度
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        sylar::Thread::SetName(m_name);

        t_fiber = m_rootFiber.get();
        m_rootThread = sylar::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    }
    else
    {
        //说明
        m_rootThread = -1;
    }

    //数量
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    SYLAR_ASSERT(m_stopping);
    if(GetThis() == this)
    {
        t_scheduler = nullptr;
    }
}

//类似的，也有一个主调度器的概念
Scheduler* Scheduler::GetThis()
{
    return t_scheduler;
}

//调度器的主协程
Fiber* Scheduler::GetMainFiber()
{
    return t_fiber;
}

//真正开始，核心方法！
void Scheduler::start()
{
    MutexType::Lock lock(m_mutex);
    if(!m_stopping)
    {
        return;
    }

    m_stopping = false;
    SYLAR_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++i)
    {
        //开始跑
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this)
                            , m_name + "_" + std::to_string(i)));

        //这里就对应上，为何 thread 的时候要放一个信号量（可以翻过去看一下）
        //因为能保证，构建返回的时候，同时 thread 已经开始跑起来之后，id是有了的
        m_threadIds.push_back(m_threads[i]->getId());
    }
    
    //不然的话，run 还会锁一次导致死锁。。
    lock.unlock();

    // if(m_rootFiber)
    // {
    //     //如果用了 caller，那么手动进来
    //     SYLAR_LOG_INFO(g_logger) << "m_rootFiber call";
    //     m_rootFiber->call();
    // }
}

void Scheduler::stop()
{
    SYLAR_LOG_INFO(g_logger) << "scheduler stop!";
    m_autoStop = true;
    // root fiber ，就是那个执行 scheduler 的 run 的协程
    if(m_rootFiber 
            && m_threadCount == 0 
            && (m_rootFiber->getState() == Fiber::TERM
                || m_rootFiber->getState() == Fiber::INIT))
    {
        //没跑起来就结束了
        SYLAR_LOG_INFO(g_logger) << this << "stopped";
        m_stopping = true;

        //留给子类去实现，让子类有清理自己的任务的机会
        if(stopping())
        {
            return;
        }
    }

    // bool exit_on_this_fiber = false;
    if(m_rootThread != -1)
    {
        //说明是 use caller 的线程，这个 stop 就必须在这个协程上执行
        SYLAR_ASSERT(GetThis() == this);
    }
    else
    {
        //就可以在任意的协程上执行这个 stop
        SYLAR_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for(size_t i = 0; i < m_threadCount; ++i)
    {
        //线程唤醒，有多少个线程就唤醒多少次，这样线程醒来就会自行退出了
        tickle();
    }

    //线程数 - 1 了，所以要补回来
    if(m_rootFiber)
    {
        tickle();
    }

    if(m_rootFiber)
    {
        // //使用了当前线程，那就必须等待全部处理完才能退出
        // while(!stopping())
        // {
        //     if(m_rootFiber->getState() == Fiber::TERM
        //                         || m_rootFiber->getState() == Fiber::EXCEPT)
        //     {
        //         //如果主协程已经是执行完的状态，必须重启一次
        //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, this));
        //         SYLAR_LOG_INFO(g_logger) << " root fiber is term, reset.";

        //         //必须要重新赋值
        //         t_fiber = m_rootFiber.get();
        //     }
        //     //如果结束的时候，还没stop，就唤醒来处理没有处理完的任务。这写法非常奇怪。。
        //     m_rootFiber->call();
        // })
        
        //放弃挣扎了，直接简单处理，居然是在这里才实现的主线程复用
        //哎，暂时不知道有甚么意义
        if(!stopping())
        {
            m_rootFiber->call();
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    for(auto& i : thrs)
    {
        i->join();
    }
    // if(exit_on_this_fiber)
    // {

    // }
}

void Scheduler::setThis()
{
    t_scheduler = this;
}

//核心中的核心
void Scheduler::run()
{
    SYLAR_LOG_INFO(g_logger) << "scheculer run.";
    // scheduler 一定要 hook，不然就失去意义了
    set_hook_enable(true);
    setThis();

    if(sylar::GetThreadId() != m_rootThread)
    {
        //use caller 已经赋值过了
        t_fiber = Fiber::GetThis().get();
    }
    
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::ptr cb_fiber;

    FiberAndThread ft;

    //如果用 stopping 的话，会让 idle 没办法判定自己的 stopping 进而正确退出
    // while(!stopping())
    while(true)
    {
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {
            //在队列取出一个协程，要加锁
            MutexType::Lock lock(m_mutex);
            auto it = m_fibers.begin();

            while(it != m_fibers.end())
            {
                //说明是指定线程的，而且现在也不在这个线程上
                if(it->thread != -1 && it->thread != sylar::GetThreadId())
                {
                    ++it;
                    //虽然不处理，但要留下信号，让后续的线程能唤醒拿到它
                    //但比较无序，简单点处理就行
                    tickle_me = true;
                    continue;
                }

                SYLAR_ASSERT(it->fiber || it->cb);
                //这种情况在。。
                if(it->fiber && it->fiber->getState() == Fiber::EXEC)
                {
                    //正在执行的，也不需要处理
                    ++it;
                    continue;
                }

                ft = *it;
                m_fibers.erase(it++);
                //从下面挪到这里的原因，是有可能还没执行到下面+1的时候，另一个线程的 stopping 状态判断为成立
                //导致 idle 函数提起那退出了
                //原则上只要有一个任务在执行，schedule 的线程们都不应该退出去
                ++m_activeThreadCount;

                //active 是做的更绝，直接不让空闲的线程到 idle 去了
                is_active = true;

                break;
            }
        }

        if(tickle_me)
        {
            tickle();
        }

        if(ft.fiber && ft.fiber->getState() != Fiber::TERM
                        && ft.fiber->getState() != Fiber::EXCEPT)
        {
            // ++m_activeThreadCount;
            ft.fiber->swapIn();
            --m_activeThreadCount;

            if(ft.fiber->getState() == Fiber::READY)
            {
                //是yield to ready出去的，那就再次执行
                schedule(ft.fiber);
            }
            else if(ft.fiber->getState() != Fiber::TERM
                    && ft.fiber->getState() != Fiber::EXCEPT)
            {
                //不等于两个结束的状态，说明是挂起？
                ft.fiber->m_state = Fiber::HOLD;
            }
            ft.reset();
        }
        else if(ft.cb)
        {
            if(cb_fiber)
            {
                cb_fiber->reset(ft.cb);
            }
            else
            {
                cb_fiber.reset(new Fiber(ft.cb));
                ft.cb = nullptr;
            }
            ft.reset();

            // ++m_activeThreadCount;
            cb_fiber->swapIn();
            --m_activeThreadCount;

            //下面是执行完的情况
            if(cb_fiber->getState() == Fiber::READY)
            {
                // yield to ready
                schedule(cb_fiber);
                cb_fiber.reset();
            }
            else if(cb_fiber->getState() == Fiber::EXCEPT
                    || cb_fiber->getState() == Fiber::TERM)
            {
                // already done
                SYLAR_LOG_DEBUG(g_logger) << "cb fiber FINISH!";
                cb_fiber->reset(nullptr);
            }
            else
            {
                //其他的状态没处理的话，统一为 HOLD
                cb_fiber->m_state = Fiber::HOLD;
                cb_fiber.reset();
            }
        }
        else
        {
            if(is_active)
            {
                --m_activeThreadCount;
                continue;
            }
            //直到这个队列里没有任务的时候，我们再执行 idle
            if(idle_fiber->getState() == Fiber::TERM)
            {
                SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                //说明已经退出
                break;
            }

            //这个 ++ 是原子量
            ++m_idleThreadCount;
            idle_fiber->swapIn();
            --m_idleThreadCount;

            if(idle_fiber->getState() != Fiber::TERM
                    && idle_fiber->getState() != Fiber::EXCEPT)
            {
                idle_fiber->m_state = Fiber::HOLD;
            }
        }
    }
}

void Scheduler::tickle()
{
    SYLAR_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping()
{
    // m_fibers 需要锁起来
    MutexType::Lock lock(m_mutex);

    // SYLAR_LOG_INFO(g_logger) << "stopping ::" 
    //         << m_autoStop << ", " 
    //         << m_stopping << ", " 
    //         << m_fibers.empty() << ", "
    //         << m_activeThreadCount;
    return m_autoStop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle()
{
    SYLAR_LOG_INFO(g_logger) << "idle";
    while(!stopping())
    {
        sylar::Fiber::YieldToHold();
    }
}
    
}
