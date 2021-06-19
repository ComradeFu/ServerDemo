#ifndef __SYLAR_SOCKET_H__
#define __SYLAR_SOCKET_H__

#include <memory> //shared ptr 肯定要用的
#include "address.h" //就是要用它
#include "noncopyable.h" //socket 一般也不让它再去复制

namespace sylar
{

//因为可能要自己用到自己
class Socket : public std::enable_shared_from_this<Socket>, Noncopyable
{
public:
    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr; //传递的时候用
    
    //用自己的枚举来翻译一次，也少点儿长度，好提示一些。。
    enum Type
    {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM,
    };

    enum Family
    {
        IPv4 = AF_INET, 
        IPv6 = AF_INET6,
        UNIX = AF_UNIX,
    };

    //便捷函数，根据address 来创建一个 socket。因为 address 里面就含有family这些信息了
    static Socket::ptr CreateTCP(sylar::Address::ptr address);
    static Socket::ptr CreateUDP(sylar::Address::ptr address);

    //直接创建
    static Socket::ptr CreateTCPSocket();
    static Socket::ptr CreateUDPSocket();

    static Socket::ptr CreateTCPSocket6();
    static Socket::ptr CreateUDPSocket6();

    static Socket::ptr CreateUnixTCPSocket();
    static Socket::ptr CreateUnixUDPSocket();

    Socket(int family, int type, int protocol = 0);
    ~Socket();

    int64_t getSendTimeout();
    void setSendTimeout(uint64_t v);

    int64_t getRecvTimeout();
    void setRecvTimeout(uint64_t v);

    bool getOption(int level, int option, void* result, size_t* len);

    //稍微把套接字的c稍微封装一下，oop，用起来舒服一些
    template<class T>
    bool getOption(int level, int option, T& result)
    {
        size_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    bool setOption(int level, int option, const void* value, size_t len);
    template<class T>
    bool setOption(int level, int option, const T& value)
    {
        return setOption(level, option, &value, sizeof(T));
    }

    //accept 回来是一个新的socket
    Socket::ptr accept();

    bool bind(const Address::ptr addr);
    //先加上超时，其实没用。以后有好的实现再实现
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    bool listen(int backlog = SOMAXCONN);
    bool close();

    //length 是 vec 的元素的个数，而不是长度。用到 iovec 的地方同
    int send(const void* buffer, size_t length, int flags = 0);
    int send(const iovec* buffers, size_t length, int flags = 0);
    //udp
    int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
    int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

    int recv(void* buffer, size_t length, int flags = 0);
    int recv(iovec* buffers, size_t length, int flags = 0);
    //udp，去掉const，就是因为收到消息的时候，知道是从哪儿来的，赋值给from
    int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
    int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();

    int getFamily() const { return m_family; }
    int getType() const { return m_type; }
    int getProtocol() const { return m_protocol; }

    bool isConnected() const { return m_isConnected; }
    bool isValid() const { return m_sock != -1; }
    int getError();

    //调试
    std::ostream& dump(std::ostream& os) const;
    int getSocket() const { return m_sock; } 

    //cancel event
    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelAll();
private:
    //用一个fd来初始化 socket 类，private 就行了
    bool init(int sock);
    void initSock();
    void newSock();
private:
    int m_sock;
    int m_family;
    int m_type;
    int m_protocol;
    bool m_isConnected;

    Address::ptr m_localAddress;
    Address::ptr m_remoteAddress;
};

} // namespace sylar


#endif
