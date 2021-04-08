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

namespace sylar
{
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
    //不允许任何的拷贝、克隆
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
};
}

#endif
