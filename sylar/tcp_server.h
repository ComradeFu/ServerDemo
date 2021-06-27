#ifndef __SYLAR_TCP_SERVER_H__
#define __SYLAR_TCP_SERVER_H__

//tcpserver 是一个很高层级的封装。所以封装的难度很低，不会像之前的底层那么麻烦

#include <memory>
#include <functional> //支持一些回调函数
#include "iomanager.h" //调度
#include "socket.h"
#include "address.h"
#include "noncopyable.h"

namespace sylar
{
class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable
{
public:
    typedef std::shared_ptr<TcpServer> ptr;
    //一般情况下，我们的TcpServer是跑在某个协程里的，所以肯定会有自己的IOManager
    TcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(), sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
    virtual ~TcpServer();

    virtual bool bind(sylar::Address::ptr addr);
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails);
    virtual bool start();
    virtual void stop();

    uint64_t getRecvTimeout() const { return m_recvTimeout; }
    std::string getName() const { return m_name; }
    void setRecvTimeout(uint64_t v) { m_recvTimeout = v; }
    void setName(const std::string& v) { m_name = v; }

    bool isStop() const { return m_isStop; }
protected:
    virtual void handleClient(Socket::ptr client);
    virtual void startAccept(Socket::ptr sock);
private:
    //可以支持不同网卡，或者不是 0.0.0.0 的多个地址
    std::vector<Socket::ptr> m_socks;
    //当作工作线程。accept 产生的 socket 就丢到线程池里面去
    IOManager* m_worker;
    IOManager* m_acceptWorker; //专门负责accept的
    uint64_t m_recvTimeout;
    std::string m_name;
    bool m_isStop;
};
}

#endif
