#include "timer.c"
#include "util.h"

namespace sylar {

bool Timer::Comparator::operator()(const Timer::ptr& lhs
                            , const Timer::ptr& rhs) const 
{
    //什么时候会为空呢？
    if(!lhs && !rhs)
    {
        return false;
    }

    if(!lhs)
    {
        return true;
    }

    if(!rhs)
    {
        return false;
    }

    if(lhs->m_next < rhs->m_next)
    {
        return true;
    }

    if(rhs->m_next < lhs->m_next)
    {
        return false;
    }

    //如果都相等，就比较地址好了
    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> cb,
                bool recurring, TimerManager* manager)
        :m_recurring(recurring)
        ,m_ms(ms)
        ,m_cb(cb)
        ,m_manager(manager)
{
    //执行时间
    m_next = sylar::GetCurrentMS() + m_ms;
}

//这个构造函数纯粹是为了在set中找出对应的 timer。需要一个比较函数
Timer::Timer(uint64_t next)
    :m_next(next)
{

}

bool Timer::cancel()
{
    TimerManager::RWMutextType::WriteLock lock(m_manager->m_mutex);
    if(m_cb)
    {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);

        return true;
    }

    return false;
}

bool Timer::refresh()
{
    TimerManager::RWMutextType::WriteLock lock(m_manager->m_mutex);
    if(m_cb)
    {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        if(it == m_manager->m_timers.end())
            //这种情况？cb 应该是空的？
            return false;
        
        m_manager->m_timers.erase(it);
        m_next = sylar::GetCurrentMS() + m_ms;
        m_manager->m_timers.insert(shared_from_this());

        return true;
    }

    return false;
}

bool Timer::reset(uint64_t ms, bool from_now)
{
    TimerManager::RWMutextType::WriteLock lock(m_manager->m_mutex);
    if(m_cb)
    {
        if(ms == m_ms && !from_now)
        {
            return true;
        }

        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        if(it == m_manager->m_timers.end())
            //这种情况？cb 应该是空的？
            return false;
        
        m_manager->m_timers.erase(it);
        uint64_t = start = 0;
        if(from_now)
        {
            start = sylar::GetCurrentMS()；
        }
        else
        {
            start = m_next - m_ms;
        }
        m_ms = ms;
        m_next = start + ms;

        m_manager->addTimer(shared_from_this(), lock);
        return true;
    }

    return false;
}

TimerManager::TimerManager()
{
    m_prevoiuseTime = sylar::GetCurrentMs();
}

virtual TimerManager::~TimerManager()
{

}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb
                        ,bool recurring)
{
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock lock(m_mutex);

    addTimer(timer, lock);

    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
{
    std::shared_ptr<void> tmp = weak_cond.lock();
    //如果没失效，再进行
    if(tmp)
    {
        cb();
    }
}

    //比较特殊的、条件定时器。用弱指针来做条件有效。比如定时清理某个对象，而当这个对象已经在外界释放时，就没必要继续了。
Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb
                        ,std::weak_ptr<void> weak_cond
                        ,bool recurring)
{
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer()
{
    RWMutextType::ReadLock lock(m_mutex);
    m_tickled = false;
    if(m_timers.empty())
    {
        //0取反，最大延迟值
        return ~0ull;
    }

    const Timer::ptr& next = *m_timers.begin();
    uint64_t now_ms = sylar::GetCurrentMS();
    if(now_ms >= next->m_next)
    {
        //应该马上执行！
        return 0;
    }

    return next->m_next - now_ms;
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs)
{
    uint64_t now_ms = sylar::GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(m_mutex);
        if(m_timers.empty())
        {
            return
        }
    }
    RWMutexType::WriteLock lock(m_mutex);

    //时间前置
    bool rollover = detectClockRollover(now_ms);
    if(!rollover && (*m_timers.begin())->m_next > now_ms)
    {
        return;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    //下面是粗暴处理，如果往前调了，全部直接超时
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    while(it != m_timers.end() && (*it)->m_next == now_ms)
    {
        ++it;
    }

    //插入
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);

    //预开辟
    cbs.reserve(expired.size());

    for(auto& timer : expired)
    {
        cbs.push_back(timer->m_cb);
        if(timer->m_recurring)
        {
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        }
        else
        {
            timer->m_cb = nullptr; //为了防止回调用了智能指针之类的东西，不置空的话引用计数不会  -1
        }
    }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock)
{
    //pair, <是否成功, pos>
    auto it = m_timers.insert(val).first;
    //如果在最前，那就是最小的。需要通知唤醒，比如 epoll
    bool at_front = (it == m_timers.begin()) && !m_tickled;
    if(at_front)
    {
        m_tickled = true;
    }

    lock.unlock();

    if(at_front)
    {
        onTimerInsertedAtFront();
    }
}

//处理系统调整时间，往前拨
bool TimerManager::detectClockRollover(uint64_t now_ms)
{
    bool rollover = false;
    if(now_ms < m_previousTime &&
            now_ms < (m_previouseTime - 60 * 60 * 1000))
    {
        rollover = true;
    }

    m_prevoiuseTime = now_ms;
    return rollover;
}

//判断非空
bool TimerManager::hasTimer()
{
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

}
