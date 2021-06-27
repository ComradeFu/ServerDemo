#include "address.h"
#include "log.h"
#include <sstream>
#include <stddef.h>
//dns
#include <netdb.h>
//网卡信息
#include <ifaddrs.h>

#include "endian.h"

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

//创建多少位的掩码
template<class T>
static T CreateMask(uint32_t bits)
{
    return (1 << (sizeof(T) * 8 - bits)) - 1; // 1111111111
}

template<class T>
static uint32_t CountBytes(T value)
{
    //这样计算 1 的数量效率是最快的
    uint32_t result = 0;
    for(; value; ++result)
    {
        //抹掉了一个1
        value &= value - 1;
    }
    return result;
}

//返回第一个
Address::ptr Address::LookupAny(const std::string& host,
                    int family, int type, int protocol)
{
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol))
    {
        return result[0];
    }
    return nullptr;
}

//只需要返回IPAddress
IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host,
                    int family, int type, int protocol)
{
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol))
    {
        for(auto& i : result)
        {
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            if(v)
            {
                return v;
            }
        }
    }
    return nullptr;
}

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                        int family, int type, int protocol)
{
    addrinfo hints, *results, *next;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    std::string node;
    const char* service = NULL;

    //检查 ipv6 address service， service 是getaddrinfo 的一个参数。如果在 ] 外还有“:” ，说明是service
    if(!host.empty() && host[0] == '[')
    {
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if(endipv6)
        {
            //TODO check out of range
            if(*(endipv6 + 1) == ':')
            {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    //检查 node service ，其实这里的 ： 可能跟之前的实现有关，可以 www.baidu.com:433 这样，让域名能够返回特定的协议的ip
    // www.baidu.com:80 == www.baidu.com:http, www.baidu.com:21 == www.baidu.com:ftp
    //不太懂
    if(node.empty())
    {
        service = (const char*)memchr(host.c_str(), ':', host.size());
        if(service)
        {
            if(!memchr(service + 1, ':', host.c_str() + host.size() - service -  1))
            {
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if(node.empty())
    {
        node = host;
    }

    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if(error)
    {
        SYLAR_LOG_ERROR(g_logger) << "Address::Lookup getaddress(" << host << ","
            << family << ", " << type << ") err=" << error << " errstr="
            << strerror(errno);
        return false;
    }

    //注意，我们要free掉刚刚的results，所以需要用 Create的方法给保存下来
    next = results;
    while(next)
    {
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }

    freeaddrinfo(results);
    return true;
}

 //通过网卡获取服务器里相关的信息
bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result
    ,int family)
{
    struct ifaddrs *next, *results;
    if(getifaddrs(&results) != 0)
    {
        SYLAR_LOG_ERROR(g_logger) << "Address:GetInterfaceAddresses getifaddrs "
            << " err=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    try
    {
        for(next = results; next; next = next->ifa_next)
        {
            Address::ptr addr;
            uint32_t prefix_len = ~0u;
            if(family != AF_UNSPEC && family != next->ifa_addr->sa_family)
            {
                //不是我们要找的
                continue;
            }
            switch (next->ifa_addr->sa_family)
            {
                case AF_INET:
                    {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        //算出来有多长
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        //等于1的数量
                        prefix_len = CountBytes(netmask);
                    }
                    break;
                case AF_INET6:
                    {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        //算出来有多长
                        in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        //等于1的数量
                        for(int i = 0; i < 16; ++i)
                        {
                            prefix_len += CountBytes(netmask.s6_addr[i]);
                        }
                    }
                    break;
                default:
                    break;
            }

            if(addr)
            {
                result.insert(std::make_pair(next->ifa_name,
                                std::make_pair(addr, prefix_len)));
            }
        }
    }
    catch(...)
    {
        SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return true;
}

//指定某个网卡的
bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result
    ,const std::string& iface, int family)
{
    //任意网卡
    if(iface.empty() || iface == "*")
    {
        if(family == AF_INET || family == AF_UNSPEC)
        {
            //0.0.0.0，任意地址
            result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
        }
        if(family == AF_INET6 || family == AF_UNSPEC)
        {
            result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
        }
        return true;
    }

    std::multimap<std::string
        ,std::pair<Address::ptr, uint32_t>> results;
    
    if(!GetInterfaceAddresses(results, family))
    {
        return false;
    }

    //找到区间
    auto its = results.equal_range(iface);
    for(;its.first != its.second; ++its.first)
    {
        result.push_back(its.first->second);
    }

    return true;
}

//base functions
int Address::getFamily() const
{
    return getAddr()->sa_family;
}

std::string Address::toString() const
{
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen)
{
    if(addr == nullptr)
    {
        return nullptr;
    }

    Address::ptr result;
    switch (addr->sa_family)
    {
        case AF_INET:
            //直接地址进行初始化，需要转换一下
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnknownAddress(*addr));
            break;
    }

    return result;
}

//可能要放在一些 set 里面，需要提供比较函数
bool Address::operator<(const Address& rhs) const
{
    socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(), minlen);
    //这里比较很机智
    if(result < 0)
    {
        return true;
    }
    else if (result > 0)
    {
        return false;
    }
    else if(getAddrLen() < rhs.getAddrLen())
    {
        return true;
    }

    return false;
}

bool Address::operator==(const Address& rhs) const
{
    return getAddrLen() == rhs.getAddrLen()
        && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const
{
    return !(*this == rhs);
}

//域名 lookup
IPAddress::ptr IPAddress::Create(const char* address, uint16_t port)
{
    //hints 是选项，给 getaddrinfo 一些信息。查询到的结果链表在 results 里
    addrinfo hints, *results;
    memset(&hints, 0, sizeof(addrinfo));

    //下面的选项是只限定 "127.0.0.1" 之类的数字ip
    // hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(address, NULL, &hints, &results);
    if(error == EAI_NONAME)
    {
        //多余的给一下提示，有这个特殊的错误号
        SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address
            << ", " << port << ") error=" << error
            << " errno=" << errno << " errstr=" << strerror(errno);

        return nullptr;
    }
    else if(error)
    {
        SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address
            << ", " << port << ") error=" << error
            << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }

    try
    {
        //如果是UNknow，就会 cast 失败。失败就会返回None
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
            Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen)
        );

        if(result)
            result->setPort(port);

        //这里存疑，因为free之后，确定不会造成 result 的内存无效引用吗？
        //不会，因为在构造函数中的 m_addr = address 已经发生了浅拷贝了（已经足够）
        freeaddrinfo(results);

        return result;
    }
    catch(...)
    {
        freeaddrinfo(results);
        return nullptr;
    }
    
}

//ipv4
//通过地址换算
IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port)
{
    IPv4Address::ptr rt(new IPv4Address);
    rt->m_addr.sin_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
    if(result <= 0)
    {
        SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in& address)
{
    m_addr = address;
}
IPv4Address::IPv4Address(uint32_t address, uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET; //ipv4协议族
    m_addr.sin_port = byteswapOnLittleEndian(port); //一定是小端
    m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address); //联合体，ipv4 实质是一个int
}

const sockaddr* IPv4Address::getAddr() const
{
    return (sockaddr*)&m_addr;
}

sockaddr* IPv4Address::getAddr()
{
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const
{
    return sizeof(m_addr);
}

//输出字符串阅读的形式
std::ostream& IPv4Address::insert(std::ostream& os) const
{
    uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
        << ((addr >> 16) & 0xff) << "."
        << ((addr >> 8) & 0xff) << "."
        << (addr & 0xff);

    //转换正常阅读
    os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
    return os;
}

//广播地址
IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) 
{
    //非法
    if(prefix_len > 32)
    {
        return nullptr;
    }

    sockaddr_in baddr(m_addr);
    baddr.sin_addr.s_addr |= byteswapOnLittleEndian(
        CreateMask<uint32_t>(prefix_len)
    );

    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) 
{
     //非法
    if(prefix_len > 32)
    {
        return nullptr;
    }

    sockaddr_in baddr(m_addr);
    // & | 的差别而已
    baddr.sin_addr.s_addr &= byteswapOnLittleEndian(
        CreateMask<uint32_t>(prefix_len)
    );

    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) 
{
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const 
{
    return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) 
{
    m_addr.sin_port = byteswapOnLittleEndian(v);
}

//ipV6
IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port)
{
    IPv6Address::ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
    if(result <= 0)
    {
        SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv6Address::IPv6Address()
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6; //ipv6协议族
}

IPv6Address::IPv6Address(const sockaddr_in6& address)
{
    m_addr = address;
}

// ipv6，太长了，只能是字符串
IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6; //ipv6协议族
    m_addr.sin6_port = byteswapOnLittleEndian(port); //一定是小端
    //string，直接copy
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);//固定16
}

const sockaddr* IPv6Address::getAddr() const
{
    return (sockaddr*)&m_addr;
}

sockaddr* IPv6Address::getAddr()
{
    return (sockaddr*)&m_addr;
}


socklen_t IPv6Address::getAddrLen() const
{
    return sizeof(m_addr);
}

std::ostream& IPv6Address::insert(std::ostream& os) const
{
    os << "[";
    uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    bool used_zeros = false;
    for(size_t i = 0; i < 8; ++i)
    {
        if(addr[i] == 0 && !used_zeros)
        {
            continue;
        }
        if(i && addr[i - 1] == 0 && !used_zeros)
        {
            os << ":";
            used_zeros = true;
        }
        if(i)
        {
            os << ":";
        }
        os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
    }

    if(!used_zeros && addr[7] == 0)
    {
        os << "::";
    }

    // rfc2373提出每段中前面的0可以省略，连续的0可省略为\"::\"，但只能出现一次。
    // 比如 1111:0000:0000:0000:0000:0000:0000:2222 可以简写：1111::2222

    os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) 
{
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |= 
        //一个一个字节来的，所以不用变换字节序
        //末尾的变换一下，然后每个字节来一次
        CreateMask<uint8_t>(prefix_len % 8);
    
    for(int i = prefix_len / 8 + 1; i < 16; ++i)
    {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }

    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) 
{
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] &= 
        //一个一个字节来的，所以不用变换字节序
        //末尾的变换一下，然后每个字节来一次
        CreateMask<uint8_t>(prefix_len % 8);
    
    //剩下的就是原来的
    // for(int i = prefix_len / 8 + 1; i < 16; ++i)
    // {
    //     baddr.sin6_addr.s6_addr[i] = 0xff;
    // }

    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) 
{
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len / 8] =
        ~CreateMask<uint8_t>(prefix_len % 8);
    
    for(uint32_t i = 0; i < prefix_len / 8; --i)
    {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }

    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const 
{
    return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) 
{
    m_addr.sin6_port = byteswapOnLittleEndian(v);
}

//unix address
static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;
UnixAddress::UnixAddress()
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX; //协议族
    m_length = path.size() + 1;

    if(!path.empty() && path[0] == '\0')
    {
        --m_length;
    }

    if(m_length > sizeof(m_addr.sun_path))
    {
        throw std::logic_error("path too long");
    }

    memcpy(m_addr.sun_path, path.c_str(), m_length);
    //真实长度是，之前的结构体再加上真实的路径长度
    m_length += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::getAddr() const 
{
    return (sockaddr*)&m_addr;
}

sockaddr* UnixAddress::getAddr() 
{
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const 
{
    return m_length;
}

void UnixAddress::setAddrLen(uint32_t v)
{
    m_length = v;
}

std::ostream& UnixAddress::insert(std::ostream& os) const 
{
    if(m_length > offsetof(sockaddr_un, sun_path)
            //"\0" 会报错，因为 string 表示的是一个字符串的首地址，而不是一个整数
            && m_addr.sun_path[0] == '\0')
    {
        return os << "\\0" << std::string(m_addr.sun_path + 1, 
            m_length - offsetof(sockaddr_un, sun_path) - 1);
    }

    return os << m_addr.sun_path;
}

//unknow
UnknownAddress::UnknownAddress(int family)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr)
{
    m_addr = addr;
}

const sockaddr* UnknownAddress::getAddr() const 
{
    return &m_addr;
}

sockaddr* UnknownAddress::getAddr() 
{
    return &m_addr;
}

socklen_t UnknownAddress::getAddrLen() const 
{
    return sizeof(m_addr);
}

std::ostream& UnknownAddress::insert(std::ostream& os) const 
{
    os << "[UnknownAddress family=" << m_addr.sa_family << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr)
{
    return addr.insert(os);
}

} // namespace sylar
