#include "sylar/tcp_server.h"
#include "sylar/iomanager.h"
#include "sylar/log.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run()
{
    //验证一下地址有无问题， netstat -ap | grep test_ , netstat -tnalp | grep test_
    auto addr = sylar::Address::LookupAny("0.0.0.0:8001");
    // ls /tmp/unix_addr -lh 验证
    // auto addr2 = sylar::UnixAddress::ptr (new sylar::UnixAddress("/tmp/unix_addr"));
    // SYLAR_LOG_INFO(g_logger) << *addr;

    std::vector<sylar::Address::ptr> addrs;
    addrs.push_back(addr);
    // addrs.push_back(addr2);

    sylar::TcpServer::ptr tcp_server(new sylar::TcpServer);
    std::vector<sylar::Address::ptr> fails;
    while(!tcp_server->bind(addrs, fails))
    {
        sleep(2);
    }
    tcp_server->start();
}

int main(int argc, char** argv)
{
    sylar::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
