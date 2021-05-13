#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__

#include <memory>
#include <vector>
#include <list>
#include "fiber.h"
#include "thread.h"
#include <iostream>

namespace sylar
{
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    //use_caller 的意思是，如果是true，那么使用了 scheduler 的这条线程就会同时也被纳入这个调度器来
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    //只是个基类
    virtual ~Scheduler();

    const std::string& getName() const { return m_name; }

    //类似的，也有一个主调度器的概念
    static Scheduler* GetThis();
    //调度器的主协程
    static Fiber* GetMainFiber();

    void start();
    void stop();

    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1)
    {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(fc, thread);
        }

        if(need_tickle)
        {
            tickle();
        }
    }

    //也支持批量放进去，锁一次，就能把要放进去的全放进去
    template<class InputIterator>
    void schedule(InputIterator begin, InputIterator end)
    {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end)
            {
                need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
                ++begin;
            }
        }
        if(need_tickle)
        {
            tickle();
        }
    }
protected:
    virtual void tickle();
    void run();
    virtual bool stopping();
    virtual void idle(); //没任务做

    void setThis();

    bool hasIdleThreads() { return m_idleThreadCount > 0; }
private:
    //无锁，模板类实现构造重载，妙
    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread)
    {
        bool need_tickle = m_fibers.empty();
        FiberAndThread ft(fc, thread);
        if(ft.fiber || ft.cb)
        {
            m_fibers.push_back(ft);
        }

        //以前是没有任务的。放进去之后，就要唤醒线程了意味着
        return need_tickle;
    }
private:
    struct FiberAndThread
    {
        Fiber::ptr fiber;
        std::function<void()> cb;
        int thread; // 线程id，指定在这个线程上跑

        FiberAndThread(Fiber::ptr f, int thr)
            :fiber(f), thread(thr)
        {

        }

        FiberAndThread(Fiber::ptr* f, int thr)
            :thread(thr)
        {
            //std::move 也行，让外面的指针 -1 引用
            fiber.swap(*f);
        }

        FiberAndThread(std::function<void()> f, int thr)
            :cb(f), thread(thr)
        {

        }

        FiberAndThread(std::function<void()>* f, int thr)
            :thread(thr)
        {
            //给的如果是指针，也swap
            cb.swap(*f);
        }

        //stl 的容器一定要有默认构造函数。否则预分配出来的对象是无法初始化的
        FiberAndThread()
            :thread(-1)
        {

        }

        void reset()
        {
            fiber = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };
    
private:
    MutexType m_mutex;
    //线程池
    std::vector<Thread::ptr> m_threads;
    //正在执行的协程，未必是 fiber 对象哦。比如functional
    std::list<FiberAndThread> m_fibers;
    Fiber::ptr m_rootFiber; //主协程
    std::string m_name;

protected:
    std::vector<int> m_threadIds;
    size_t m_threadCount = 0;
    std::atomic<size_t> m_activeThreadCount = {0}; //原子量，保证线程安全
    std::atomic<size_t> m_idleThreadCount = {0};
    bool m_stopping = true;
    bool m_autoStop = false; //是否主动停止
    int m_rootThread = 0; //use_caller 的thread
};
}

#endif
