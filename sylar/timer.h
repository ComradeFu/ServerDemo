#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include <memory>
#include <vector>
#include <set>
#include "thread.h"

namespace sylar {

class TimerManager;
class Timer : public std::enable_shared_from_this<Timer>
{
friend class TimerManager;
public:
    typedef std::shared_ptr<Timer> ptr;
    bool cancel();
    bool refresh(); //重新设置执行时间
    bool reset(uint64_t ms, bool from_now);
private:
    Timer(uint64_t ms, std::function<void()> cb,
            bool recurring, TimerManager* manager);
    Timer(uint64_t next);
private:
    bool m_recurring = false;   //run every
    uint64_t m_ms = 0;          //interval
    uint64_t m_next = 0;        //next time active
    std::function<void()> m_cb;
    TimerManager* m_manager = nullptr;
private:
    struct Comparator
    {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
    
};

class TimerManager 
{
friend class Timer;
public:
    typedef RWMutex RWMutexType;

    TimerManager();
    //因为可能是被比如 iomanager 继承过去的
    virtual ~TimerManager();

    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb
                        ,bool recurring = false);
    
    //比较特殊的、条件定时器。用弱指针来做条件有效。比如定时清理某个对象，而当这个对象已经在外界释放时，就没必要继续了。
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb
                        ,std::weak_ptr<void> weak_cond
                        ,bool recurring = false);
    
    uint64_t getNextTimer();
    void listExpiredCb(std::vector<std::function<void()>>& cbs);
protected:
    virtual void onTimerInsertedAtFront() = 0;
    void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);
    bool hasTimer();
private:
    bool detectClockRollover(uint64_t now_ms);
private:
    RWMutexType m_mutex;
    //有序 set
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    bool m_tickled = false; //用来提升效率，多次添加比较短的定时器事，不用重复触发
    uint64_t m_previouseTime = 0;
};
}

#endif