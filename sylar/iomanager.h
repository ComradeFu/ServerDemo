#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"

namespace sylar {

//不同于一般的线程池，用信号量来做。底层使用epoll
class IOManager : public Scheduler {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    //支持的事件
    enum Event {
        NONE    = 0x0;
        READ    = 0x1;
        WRITE   = 0x2;
    };
private:
    //需要一个事件的类，来装这些事件
    //epoll 里的事件，是针对一个fd来的
    struct FdContext
    {
        //直接用互斥锁就行
        typedef Mutex MutexType;
        //类似fiber一样，加回调的 fiber 或者 callback
        struct EventContext
        {
            Scheduler* scheduler = nullptr;   //表示事件在哪个 scheduler 上执行
            Fiber::ptr fiber;       //事件的协程
            std::function<void()> cb;//事件的回调
        };
        
        EventContext& getContext(Event event);
        void resetContext(EventContext& ctx);
        void triggerEvent(Event event);

        EventContext read; //读事件
        EventContext write;//写事件
        int fd = 0;        //事件关联的句柄

        //已经注册的事件
        Event events = NONE;
        MutexType mutex;
    };
    
public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    ~IOManager();

    //1 success, -1 error //0 retry
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);     //直接删掉了
    bool cancelEvent(int fd, Event event);  //强制触发一次？

    bool cancelAll(int fd);

    //子类的静态方法可以隐藏掉父类的
    static IOManager* GetThis();

protected:
//三个虚方法
    void tickle() override;
    bool sopping() override;
    void idle() override;

    void contextResize(size_t size);
private:
    int m_epfd = 0; //epoll的fd
    //用 pipline的形式，而不是经典的信号量
    int m_tickleFds[0];

    std::atomic<size_t> m_pendingEventCount = {0}; //等待执行的事件数量
    RWMutexType m_mutex;

    //上下文数组
    std::vector<FdContext*> m_fdContexts;
}

}

#endif
