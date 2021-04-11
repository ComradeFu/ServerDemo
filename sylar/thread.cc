#include "thread.h"
#include "log.h"
#include "util.h"

namespace sylar {

//当前线程，thread_local 是 “线程局部变量”
static thread_local Thread* t_thread = nullptr;
//虽然也可以t_thread->name,但这个不会常改却常用。所以还是单独定义出来吧
//另外确实有些线程并不能作为我们的线程类，比如主线程。并不是我们生成的
static thread_local std::string t_thread_name = "UNKNOW"; 

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

Semaphore::Semaphore(uint32_t count)
{
    if(sem_init(&m_semaphore, 0, count))
    {
        //以后要增加错误处理
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore()
{
    sem_destroy(&m_semaphore);
}

void Semaphore::wait()
{
    //等到会返回0
    if(sem_wait(&m_semaphore))
    {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify()
{
    if(sem_post(&m_semaphore))
    {
        throw std::logic_error("sem_post error");
    }
}

Thread* Thread::GetThis()
{
    return t_thread;
}

const std::string& Thread::GetName()
{
    return t_thread_name;
}

void Thread::SetName(const std::string& name)
{
    if(name.empty()) 
    {
        return;
    }

    if(t_thread)
    {
        t_thread->m_name = name;
    }
    //主线程
    t_thread_name = name;
}

Thread::Thread(std::function<void()>cb, const std::string& name):m_cb(cb), m_name(name)
{
    if(name.empty())
    {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if(rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt =" << rt 
            << " name=" << name;
        throw std::logic_error("pthread_create error");
    }

    //也是测试也是功能，线程类并不知道自己的构造函数是否出去了，run才执行
    //这里用自己的信号量，让自己有确定的顺序。等所有的东西初始化好了（run 里面一堆的赋值），我再算构造完成。
    //换句话说，我初始化成功之后，run肯定是进去了的，跑起来了
    m_semaphore.wait();
}

Thread::~Thread()
{
    if(m_thread)
    {
        pthread_detach(m_thread); //或者 join，两种方法
    }
}

void* Thread::run(void* arg)
{
    //传入的参数转成线程参数。线程的标准规定，必须是 void* ，所以需要转换一次（实际传的确实是 thread 实例）
    Thread* thread = (Thread*)arg;
    //已经进入正式的线程
    t_thread = thread;
    t_thread_name = thread->m_name;

    thread->m_id = GetThreadId();

    //给线程命名（top）也会看到，最大16字符数
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    //用它跟线程的参数 swap 掉，防止参数里面有智能指针被引用。swap的话能直接少一个引用。
    //也就是 thread->m_cb 会变成NULL，这样外界就不会引用到了。
    //我一直很郁闷为何很多人都是这么写，因为我感觉：只要我用了run作为入口，就不会再使用了才对，不会引起重复进入之类的问题
    //但后来阿土讲通了。这就是出发点的问题。我不自觉从整个框架来讲，确实是不会这么用。
    //但是从 Thread 的封装来看，Thread 是不会知道外界的情况的。也就是说，它一定要保证，m_cb 不会再此调用对应的函数指针
    //所以就用 swap 来进行一次，把 m_cb 换成空的 function 对象
    //阿土同时还提到了一个解决方法，那就是用 bind 来解决这个？待研究
    cb.swap(thread->m_cb);

    //初始化完成
    thread->m_semaphore.notify();

    cb();

    return 0; //注意这里，void*
}

// 关于 detach 模式跟 join 模式 https://blog.csdn.net/wushuomin/article/details/80051295
void Thread::join()
{
    if(m_thread)
    {
        int rt = pthread_join(m_thread, nullptr);
        if(rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, rt =" << rt 
            << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }

        m_thread = 0;
    }
}

}