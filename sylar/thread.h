#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__

//框架是协程框架，协程之间的交互是更重要的。线程交互反而就要简略一些的
//pthread_xxx
//std::thread c++11  其实还是用的 pthread ，编译的时候需要链接它
//std 库有一个问题，互斥量不提供读写分开的锁。如果是对于一些高读低写的数据，没有读写锁是很不效率的。
//所以在互斥量上，还是使用 pthread 来进行

#include <thread>
#include <functional> //线程用方法 ，更灵活
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

namespace sylar
{

//信号量
class Semaphore
{
public:
    //默认信号量是0
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    //常用比如消息队列，工作线程去wait
    void wait(); // -1
    void notify(); // +1

private:
    //同样禁止，c++11 方式，直接删除掉把
    Semaphore(const Semaphore&) = delete;
    Semaphore(const Semaphore&&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

private:

    sem_t m_semaphore;
}

//互斥量，LockGuard。一般用在局部（所以栈上的超出作用域会自动销毁）
//也就是“锁”。为什么是一个结构体，是因为我们常用的锁的情境下，一般都会有锁的互相对应，有加锁就会有解锁
//所以一般也就利用类或者结构体啥的，的构造函数来加锁，它的析构函数来解锁。防止漏掉。
//为什么要模板，是因为锁的种类很多。但用法一致。比如 stream lock 、 pthread mutex、pthread lock什么的。我们提供锁跟解锁就行了
//下面的 Read Write Impl 也是
template<class T>
struct ScopedLockImpl
{
public:
    ScopedLockImpl(T& mutext)
        :m_mutext(mutex)
    {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl()
    {
        unlock();
    }

    void lock()
    {
        //防止死锁
        if(!m_locked)
        {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if(m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

//读写锁的 LockGuard
template<class T>
struct ReadScopedLockImpl
{
public:
    ReadScopedLockImpl(T& mutext)
        :m_mutext(mutex)
    {
        m_mutex.rdlock();
        m_locked = true;
    }

    ~ReadScopedLockImpl()
    {
        unlock();
    }

    void lock()
    {
        //防止死锁
        if(!m_locked)
        {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if(m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

template<class T>
struct WriteScopedLockImpl
{
public:
    WriteScopedLockImpl(T& mutext)
        :m_mutext(mutex)
    {
        m_mutex.wrlock();
        m_locked = true;
    }

    ~WriteScopedLockImpl()
    {
        unlock();
    }

    void lock()
    {
        //防止死锁
        if(!m_locked)
        {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if(m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

//不分读写的锁，try 方法短时间内还不需要，就不加了
class Mutex {
public:
    //没有 rw，只有lock
    typedef ScopedLockImpl<Mutex> Lock;
    Mutex()
    {
        //第二个参数还能设定属性，可以做进程之间的互斥
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock()
    {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock()
    {
        pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutext_t m_mutex;
};

//空的锁，什么都不干。用来测试（就不用在加好锁的地方注释来测试了）
class NullMutex
{
public:
    typedef ScopedLockImpl<NullMutex> Lock;
    NullMutex(){}
    ~NullMutex(){}
    void lock(){}
    void unlock(){}
}

//读写锁
class RWMutex {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;
    RWMutex()
    {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex()
    {
        pthread_rwlock_destroy(&m_lock);
    }

    void rdlock()
    {
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock()
    {
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock()
    {
        pthread_rwlock_unlock(&m_lock);
    }
private:
    pthread_rwlock_t m_lock;
};

//空的锁，什么都不干。用来测试（就不用在加好锁的地方注释来测试了）
class NullRWMutex
{
public:
    typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
    typedef WriteScopedLockImpl<NullRWMutex> WriteLock;

    NullRWMutex(){}
    ~NullRWMutex(){}
    void rdlock(){}
    void wrlock(){}
    void unlock(){}
};

//下面这个锁是更轻量级的。
//提出是因为在测试日志的时候发现，加锁跟不加锁，性能差距有10倍的差距（看单位时间产生的日志容量就知道）
//这把锁在遇到资源等待的时候，并不会马上切换到内核态。而是在 cpu 上空跑一段时间。
//也就是说，会导致 cpu 占用比较高，但是它的恢复会更快。使用竞争很短的场景。日志就是了
//也就是大名鼎鼎的“自旋锁”。linux说，只有非常清楚自己在干什么，并且锁阻塞时间能够准确预估的时候才使用。
class Spinlock
{
public:
    typedef ScopedLockImpl<Spinlock> Lock;
    Spinlock()
    {
        pthread_spin_init(&m_mutex, 0);
    }

    ~Spinlock()
    {
        pthread_spin_destroy(&m_mutex);
    }

    void lock()
    {
        pthread_spin_lock(&m_mutex);
    }

    void unlock()
    {
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex;
};

//上面的自旋锁测试之后，性能差距还是不够乐观
//于是想到了CAS锁，原子锁。这个更快。c++11 有直接的提供
//其实 spin lock 底层也是这个实现。
//不同的是 spin lock 还是会有一个尝试几次之后依然会锁（保险起见）
//我们自己实现可以 while true 一直等待
//最后测试发现确实两边性能也差不了太多。。淦。。那还是用 spin lock 吧
class CASLock
{
public:
    typedef ScopedLockImpl<CASLock> Lock;
    CASLock()
    {
        m_mutex.clear();
    }
    ~CASLock()
    {

    }

    void lock()
    {
        //一直尝试取得值
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    void unlock()
    {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    volatile std::atomic_flag m_mutex;
};

class Thread{
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    pid_t getId() const { return m_id; }
    const std::string& getName() const { return m_name; }

    void join();

    //两个静态方法（为的是不用拿到实际的线程类，也能正常进行调用）
    //拿到当前运行的线程
    static Thread* GetThis();
    //获取当前线程的名称
    static const std::string& GetName();
    //因为不是所有线程（主线程）都是我们创建的，所以还要提供设定
    static void SetName(const std::string& name);
private:
    //不允许任何的拷贝、克隆。& 是左值，&& 是右值。都不可以。
    Thread(const Thread&) = delete; //C++11 的新写法，禁用的意思
    Thread(const Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;

    static void* run(void* arg);
private:
    pid_t m_id = -1;
    //还是用回 pthread 吧，考虑到互斥的统一
    //以及一个细节很重要。std 的线程的id是自己的，我们的id可以是linux里top看到的，保持线程id一致也有助于排查问题
    pthread_t m_thread = 0;
    std::function<void()> m_cb;
    std::string m_name;

    Semaphore m_semaphore;
};
}

#endif
