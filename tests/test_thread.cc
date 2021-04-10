#include "sylar/sylar.h"
#include <unistd.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int count = 0;
// sylar::RWMutex s_mutex;
sylar::Mutex s_mutex;

void func1() {
    SYLAR_LOG_INFO(g_logger) << "name: " << sylar::Thread::GetName()
                            << " this.name: " << sylar::Thread::GetThis()->getName()
                            << " id: " << sylar::GetThreadId()
                            << " this.id: " << sylar::Thread::GetThis()->getId();

    //top -H -p xxxx 可以观察到具体线程名字
    // sleep(60);

    //不加锁的情况下，++ 并不是线程安全操作，所以最后的值是乱的，不是 500000（小概率是）
    //读锁 ReadLock 也是不行的
    for(int i = 0; i < 100000; ++i)
    {
        // sylar::RWMutext::WriteLock lock(s_mutex);
        sylar::Mutext::Lock lock(s_mutex);
        ++count;
    }
}

void func2() {
    //测试日志是否线程安全，如果不安全，日志会打印出很多二进制（可以用 106 来 grep），一堆 @@@@@…^^^^ 什么的
    while(true)
    {
        SYLAR_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void func3() {
    while(true)
    {
        SYLAR_LOG_INFO(g_logger) << "==============================================";
    }
}

int main(int argc, char** argv)
{
    SYLAR_LOG_INFO(g_logger) << "thread test begin";
    std::vector<sylar::Thread::ptr> thrs;
    // for(int i = 0; i < 5; ++i)
    // {
    //     sylar::Thread::ptr thr(new sylar::Thread(&func1, "name_" + std::to_string(i)));
    //     thrs.push_back(thr);
    // }

    // for(int i = 0; i < 5; ++i)
    // {
    //     thrs[i]->join();
    // }

    YAML::Node root = YAML::LoadFile("./bin/conf/log2.yml");
    sylar::Config::LoadFromYaml(root);

    for(int i = 0; i < 2; ++i)
    {
        sylar::Thread::ptr thr(new sylar::Thread(&func2, "name_" + std::to_string(i * 2)));
        sylar::Thread::ptr thr2(new sylar::Thread(&func3, "name_" + std::to_string(i * 2 + 1)));
        thrs.push_back(thr);
        thrs.push_back(thr2);
    }

    for(size_t i = 0; i < thrs.size(); ++i)
    {
        thrs[i]->join();
    }

    SYLAR_LOG_INFO(g_logger) << "thread test end";
    SYLAR_LOG_INFO(g_logger) << count;
    return 0;
}
