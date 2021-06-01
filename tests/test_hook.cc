#include "sylar/hook.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

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

void test_sock()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    SYLAR_LOG_INFO(g_logger) << "begin connect";
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    SYLAR_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

    if(rt)
    {
        return;
    }

    //最小http消息
    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    SYLAR_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0)
    {
        return;
    }

    while(true)
    {
        //不用 char* 的数组，因为直接在栈空间搞数组，会占用太大。导致协程切换不够快。
        //string 在超过一定长度之后会挪到堆上（看不同的编译器，比如 vs2005 就是 16bytes）
        std::string buff;
        buff.resize(4096);

        rt = recv(sock, &buff[0], buff.size(), 0);
        SYLAR_LOG_INFO(g_logger) << "rcv rt=" << rt << " errno=" << errno;

        if(rt <= 0)
        {
            return;
        }

        buff.resize(rt);

        //明文回复的html
        SYLAR_LOG_INFO(g_logger) << buff;
    }
}   

int main(int argc, char** argv)
{
    // test_sleep();
    // test_sock(); //直接这样调用，是没有走 iomanager，也就没有初始化 hook 的 set_hook_enable
    sylar::IOManager iom;
    iom.schedule(test_sock);
    return 0;
}
