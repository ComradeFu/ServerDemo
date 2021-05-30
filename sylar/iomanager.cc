#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event)
{
    switch(event)
    {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            SYLAR_ASSERT2(false, "getContext");
    }
}

void IOManager::FdContext::resetContext(EventContext& ctx)
{
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event)
{
    SYLAR_LOG_INFO(g_logger) << "trigger event fd:" << this->fd << ", event:" << event;
    //这个方法都是外面加过锁了，所以里面都不用加
    SYLAR_ASSERT(events & event);
    events = (Event)(events & ~event);
    EventContext& ctx = getContext(event);

    if(ctx.cb)
    {
        //里面会 swap 掉， cb 就会是空。智能指针的指针
        ctx.scheduler->schedule(&ctx.cb);
    }
    else
    {
        ctx.scheduler->schedule(&ctx.fiber);
    }

    //用完了
    ctx.scheduler = nullptr;
    return;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    :Scheduler(threads, use_caller, name)
{
    m_epfd = epoll_create(5000); //linux 2.8 以后被忽略了
    SYLAR_ASSERT(m_epfd > 0);

    //使用管道
    int rt = pipe(m_tickleFds);
    SYLAR_ASSERT(!rt);

    //epoll 的事件结构体
    epoll_event event;
    //归零
    memset(&event, 0, sizeof(epoll_event));
    //边缘触发儿
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0]; // 0 就是读的？还是i写的来着。。

    //修改句柄的属性，设置成不阻塞
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    SYLAR_ASSERT(!rt);

    //往句柄里面加事件
    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    SYLAR_ASSERT(!rt);

    //默认给64个大小
    // m_fdContexts.resize(64);

    contextResize(32);

    //scheduler 创建好了，就默认启动
    start();
}

IOManager::~IOManager()
{
    stop();
    close(m_epfd);

    //两个句柄也
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for(size_t i = 0; i < m_fdContexts.size(); ++i)
    {
        if(m_fdContexts[i])
        {
            //如果已经分配出去了的
            delete m_fdContexts[i];
        }
    }
}

//下面这个方法完全是为了 addEvent 的锁优化
//因为不这么做的话，addEvent 必须加写锁（因为fd可能会被创建两次）
//所以不如一开始就创建妥当，后面你的增减事件就都是直接使用了
//读锁就可以
void IOManager::contextResize(size_t size)
{
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); ++i)
    {
        if(!m_fdContexts[i])
        {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

//往epoll里面增加事件
int IOManager::addEvent(int fd, Event event, std::function<void()> cb)
{
    SYLAR_LOG_INFO(g_logger) << "add event fd:" << fd << ", event:" << event;
    FdContext* fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);

    //看看 fd 是不是超出了允许的范围（所以需要读锁）
    if((int)m_fdContexts.size() > fd)
    {
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    }
    else
    {
        //写锁，增加，动态扩容
        lock.unlock();
        RWMutexType::WriteLock lock(m_mutex);
        //std 是x2的，为什么用fd不是本身的fd的size，是因为
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    //因为要修改它，也要加锁
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(fd_ctx->events && event)
    {
        SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                        << " event=" << event
                        << " fd_ctx.event=" << fd_ctx->events;

        //如果要加的 event 已经有了，那说明是有问题。意味着至少两个线程同时对一个ctx进行了操作。危险
        //如果位操作 & 不为0，说明是同类事件
        SYLAR_ASSERT(!(fd_ctx->events & event));
    }

    //如果已经有原来的事件，那就是 mod，否则就是 add
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    //我们用的是 ET 模式，位操作加上新的 event
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx; //回调的时候，就能拿到是在哪个 fd_ctx 上触发的

    SYLAR_LOG_INFO(g_logger) << "epoll ctl add fd " << fd << ", event:" << epevent.events;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt)
    {
        //errno 是系统错误
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << "," << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";

        return -1;
    }

    ++m_pendingEventCount;
    fd_ctx->events = (Event)(fd_ctx->events | event);

    //根据event的类型选一个应用（读还是写event）
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);

    //为了安全。如果有值的话，说明上面肯定有事件了
    SYLAR_ASSERT(!event_ctx.scheduler);
    SYLAR_ASSERT(!event_ctx.fiber);
    SYLAR_ASSERT(!event_ctx.cb);

    event_ctx.scheduler = Scheduler::GetThis();
    if(cb)
    {
        event_ctx.cb.swap(cb);
    }
    else
    {
        //如果没指定，就把协程放进去
        event_ctx.fiber = Fiber::GetThis();
        //肯定是正在执行中的
        SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }

    return 0;
}

bool IOManager::delEvent(int fd, Event event)
{
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd)
    {
        //没有这个句柄
        return false;
    }

    // 默认初始化，所以肯定有
    // if(!m_fdContexts[fd])
    // {
        
    // }

    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event))
    {
        //没有事件
        return false;
    }

    //类似add，反操作
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt)
    {
        //errno 是系统错误
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << "," << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";

        return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);

    return true;
}

//跟删除差不多。区别是 cancel 找到了对应的对象，把它强制触发执行。
bool IOManager::cancelEvent(int fd, Event event)
{
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd)
    {
        //没有这个句柄
        return false;
    }

    // 默认初始化，所以肯定有
    // if(!m_fdContexts[fd])
    // {
        
    // }

    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event))
    {
        //没有事件
        return false;
    }

    //类似add，反操作
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt)
    {
        //errno 是系统错误
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << "," << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";

        return false;
    }

    //区别在此
    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    
    return true;
}

bool IOManager::cancelAll(int fd)
{
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd)
    {
        //没有这个句柄
        return false;
    }

    // 默认初始化，所以肯定有
    // if(!m_fdContexts[fd])
    // {
        
    // }

    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(fd_ctx->events)
    {
        //没有事件
        return false;
    }

    int op = EPOLL_CTL_DEL;

    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt)
    {
        //errno 是系统错误
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << "," << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";

        return false;
    }

    if(fd_ctx->events & READ)
    {
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }
    if(fd_ctx->events & WRITE)
    {
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    //为什么不需要呢，因为触发一个事件的时候，就应该须消掉（在 triggerEvent里）
    //因为都触发了，所以这里必然应该是一个 0
    // fd_ctx->events = 0;
    //清理 context
    // fd_ctx->resetContext(event_ctx);

    SYLAR_ASSERT(fd_ctx->events == 0);
    return true;
}

IOManager* IOManager::GetThis()
{
    //继承自 Scheduler，所以转换一下就完成了
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

//三个继承来的，很重要的接口
void IOManager::tickle()
{
    //pipe 的另一端，0，已经放到 epoll wait 里面去了
    if(hasIdleThreads())
    {
        //没有空闲的线程，发了也没用。已经在while里面执行中
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    SYLAR_ASSERT(rt == 1); // 1 是实际写入的长度
}

bool IOManager::stopping(uint64_t next_timeout)
{
    next_timeout = getNextTimer();
    //处理完所有事件
    return m_pendingEventCount == 0
        && next_timeout == ~0ull
        && Scheduler::stopping();
}

bool IOManager::stopping()
{
    //scheduler 本身不需要这个值
    uint64_t next_timeout = 0;
    return stopping(next_timeout);
}

//core，利用 epoll
void IOManager::idle()
{
    //fiber 栈不大，所以尽量不在栈上玩
    epoll_event* events = new epoll_event[64]();

    //虽然智能指针不支持数组，但可以利用析构函数来搞
    //这个 shared_events 在真正用的时候，不用它。只是说离开了这个idle就自动析构
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
        delete[] ptr;
    });

    while(true)
    {
        uint64_t next_timeout = 0;
        if(stopping())
        {
            //为什么不用上面的 hasTimer()，是少了一把锁
            next_timeout = getNextTimer();
            if(next_timeout == ~0ull)
            {
                SYLAR_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
                break;
            }
        }

        //实际的长度（事件数）
        int rt = 0;
        do
        {
            static const int MAX_TIMEOUT = 5000;
            if(next_timeout != ~0ull)
            {
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            }
            else
            {
                next_timeout = MAX_TIMEOUT;
            }
            //没有事件回来，五秒之后也会唤醒，64 就是上面的一次返回处理的数量
            rt = epoll_wait(m_epfd, events, 64, (int)next_timeout);

            //EINTR 操作系统返回的中断，指示再去epoll一次
            if(rt < 0 && errno == EINTR)
            {

            }
            else
            {
                break;
            }
        } while(true);

        //统一先处理一次定时器
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);

        if(!cbs.empty())
        {
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        for(int i = 0; i < rt; ++i)
        {
            epoll_event& event = events[i];
            //谨慎判断一下
            if(event.data.fd == m_tickleFds[0])
            {
                //说明是内部有发消息给我们（比如idle通过 pipe）
                uint8_t dummy;
                //一定要读干净，否则 ET 触发不读干净的话就不会再通知了
                while(read(m_tickleFds[0], &dummy, 1) == 1);
                //已经唤醒了，就不用处理了
                continue;
            }

            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            //要操作它
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            if(event.events & (EPOLLERR | EPOLLHUP))
            {
                //错误或者中断，就加一些读写，让能反应
                // event.events |= EPOLLIN | EPOLLOUT;//我去，这里写错了。导致 iomanager assert了错误事件（没有监听却trigger）
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }

            //只关心两个事件
            int real_events = NONE;
            if(event.events & EPOLLIN)
            {
                real_events |= READ;
            }
            if(event.events & EPOLLOUT)
            {
                real_events |= WRITE;
            }

            //看有无交集
            if((fd_ctx->events & real_events) == NONE)
            {
                //如果没有，说明也许 fd 的 event 在其他地方已经处理了（比如删除？）
                continue;
            }

            //剩余的事件
            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            //可以复用的
            event.events = EPOLLET | left_events;

            //已经处理了一个，剩下的继续丢进去 epoll 处理
            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2)
            {
                //errno 是系统错误
                SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                    << op << "," << fd_ctx->fd << "," << event.events << "):"
                    << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            //为什么不能是else，是因为有可能两个事件同时触发了
            if(real_events & READ)
            {
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            if(real_events & WRITE)
            {
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        //处理完之后，就让出来
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        //回到调度器的 main fiber 里面去
        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront()
{
    //先唤醒，然后重新计算一个时间
    tickle();

}

} // namespace sylar

