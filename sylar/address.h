#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__

#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <vector>
#include <map>

//IPv4 IPv6 unixaddress
//把 C 的原始的函数，oop 封装成C++。用起来更简单一点

namespace sylar 
{

class IPAddress;

class Address
{
public:
    typedef std::shared_ptr<Address> ptr;
    
    //用socket的结构体，给创建出来一个地址对象
    static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);
    //unspec 是任意类型
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                        int family = AF_UNSPEC, int type = 0, int protocol = 0);
    //返回第一个
    static Address::ptr LookupAny(const std::string& host,
                        int family = AF_UNSPEC, int type = 0, int protocol = 0);
    //只需要返回IPAddress，还不能直接使用 IPAddress::ptr ...
    static shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                        int family = AF_UNSPEC, int type = 0, int protocol = 0);


    //通过网卡获取服务器里相关的信息，ip addr 指令可验证
    static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result
        ,int family = AF_UNSPEC);

    //指定某个网卡的
    static bool GetInterfaceAddresses(std::vector<std::pair<Address:ptr, uint32_t>>& result
        ,const std::string& iface, int family = AF_UNSPEC);

    //基类，必须
    virtual ~Address() {};
    
    //哪一种类型的
    int getFamily() const;

    virtual const sockaddr* getAddr() const = 0;
    virtual socklen_t getAddrLen() const = 0;

    virtual std::ostream& insert(std::ostream& os) const = 0;
    std::string toString(); //虽然真正用的时候，一般都是用基类。但为了调试，输出自己是哪个也是有必要的。

    //可能要放在一些 set 里面，需要提供比较函数
    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;

};

class IPAddress : public Address
{
public:
    typedef std::shared_ptr<IPAddress> ptr;

    static IPAdress::ptr Create(const char* address, uint32_t port = 0);

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint32_t v) = 0;
};

class IPv4Address : public IPAddress
{
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    //根据字符串地址直接初始化
    static IPv4Address::ptr Create(const char* address, uint32_t port = 0);

    IPv4Address(const sockaddr_in& address);
    //IPv4 的地址本质上就是一个 int
    IPv4Address(uint32_t address = INADDR_ANY, uint32_t port = 0);

    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;

    virtual std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint32_t v) override;
private:
    //其实就一个地址，其他方法都是对这个地址的快捷操作
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress
{
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    static IPv6Address::ptr (const char* address, uint32_t port = 0);
    IPv6Address();
    IPv6Address(const sockaddr_in6& address);
    IPv6Address(const uint8_t addrss[16], uint32_t port = 0);

    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;

    virtual std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint32_t v) override;
private:
    //跟IPv4不同之处
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address
{
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress();
    //用的是一个路径
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;
    virtual std::ostream& insert(std::ostream& os) const override;

private:
    sockaddr_un m_addr;
    //还需要一个长度来描述它，直接这个结构体还不能知道它的长度
    socklen_t m_length;
};

//占位
class UnknowAddress : public Address
{
public:
    typedef std::shared_ptr<UnknowAddress> ptr;
    UnknowAddress(int family);
    UnknowAddress(const sockaddr& addr);
    const sockaddr* getAddr() const override;
    socklen_t getAddrLen() const override;
    virtual std::ostream& insert(std::ostream& os) const override;
private:
    sockaddr m_addr;
};

}

#endif
