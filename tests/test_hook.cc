#include "sylar/hook.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep()
{
    //1是为了能更直观测试。因为测试方式会启动两个协程，一个 sleep 2 秒，一个 sleep 3 秒。
    //如果是5秒钟后才触发，说明是阻塞的。如果分别2、3秒触发，那就是正常
    //两条线程就做不到这一点了，因为有可能是两个fiber分别在两条线程上阻塞的
    sylar::IOManager iom(1);
    iom.schedule([]()
    {
        sleep(2);
        SYLAR_LOG_INFO(g_logger) << "sleep 2";
    });

    iom.schedule([]()
    {
        sleep(3);
        SYLAR_LOG_INFO(g_logger) << "sleep 3";
    });

    SYLAR_LOG_INFO(g_logger) << "test_sleep";
}

int main(int argc, char** argv)
{
    test_sleep();
    return 0;
}
