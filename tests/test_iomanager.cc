#include "sylar/sylar.h"
#include "sylar/iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_fiber()
{
    SYLAR_LOG_INFO(g_logger) << "test_fiber";
}

void test1()
{
    sylar::IOManager iom(2);
    iom.schedule(&test_fiber);

    //ipv4 tcp
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    connect(sock, (const sockaddr*)&addr, sizeof(addr));
    iom.addEvent(sock, sylar::IOManager::READ, [sock](){
        SYLAR_LOG_INFO(g_logger) << "read call back.";
    });

    iom.addEvent(sock, sylar::IOManager::WRITE, [sock](){
        SYLAR_LOG_INFO(g_logger) << "write call back.";
        sylar::IOManager::GetThis()->cancelEvent(sock, sylar::IOManager::READ);
        close(sock);
    });
}

// void test_timer()
// {
//     sylar::IOManager iom(2);
//     //lambda
//     auto timer = iom.addTimer(1000, [&timer](){
//         SYLAR_LOG_INFO(g_logger) << "hello timer";
//         static int i = 0;
//         if(++i == 5)
//         {
//             timer->cancel(i);
//         }
//     },true);
// }

int main(int argc, char** argv)
{
    test1();
    // test_timer();
    return 0;
}
