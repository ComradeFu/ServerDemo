#include "socket.h"
#include "fd_manager.h"
#include "log.h"
#include "macro.h"
#include "hook.h"
#include "iomanager.h"
#include <netinet/tcp.h>
#include <limits.h>

//msg headr(hdr)
#include <sys/types.h>
#include <sys/socket.h>

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

//便捷函数，根据address 来创建一个 socket。因为 address 里面就含有family这些信息了
Socket::ptr Socket::CreateTCP(sylar::Address::ptr address)
{
    //ipv4、ipv6、unixsocket
    Socket::ptr sock(newSocket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(sylar::Address::ptr address)
{
    Socket::ptr sock(newSocket(address->getFamily(), UDP, 0));
    return sock;
}

//IPv4
Socket::ptr Socket::CreateTCPSocket()
{
    Socket::ptr sock(newSocket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket()
{
    Socket::ptr sock(newSocket(IPv4, UDP, 0));
    return sock;
}

//IPv6
Socket::ptr Socket::CreateTCPSocket6()
{
    Socket::ptr sock(newSocket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6()
{
    Socket::ptr sock(newSocket(IPv6, UDP, 0));
    return sock;
}

//Unix socket
Socket::ptr Socket::CreateUnixTCPSocket()
{
    Socket::ptr sock(newSocket(UNIX, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket()
{
    Socket::ptr sock(newSocket(UNIX, UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    :m_sock(-1)
    ,m_family(family)
    ,m_type(type)
    ,m_protocol(protocol)
    ,m_isConnected(false)
{

}

Socket::~Socket()
{
    close();
}

int64_t Socket::getSendTimeout()
{
    //之前封装过
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if(ctx)
    {
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(uint64_t v)
{
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    //自己封装的
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout()
{
    //之前封装过
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if(ctx)
    {
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(uint64_t v)
{
     struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    //自己封装的
    return setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, size_t* len)
{
    // hook 住的方法，在这里封装一下就行
    int rt = getsockopt(m_sock, level, option, result, (socklen_t*)len);
    if(rt)
    {
        //取失败没什么大不了
        SYLAR_LOG_DEBUG(g_logger) << "getOption sock=" << m_sock
            << " level=" << level << " option=" << option
            << " errno =" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void* value, size_t len)
{
    if(setsockopt(m_sock, level, option, result, (socklen_t)len))
    {
        SYLAR_LOG_DEBUG(g_logger) << "setOption sock=" << m_sock
            << " level=" << level << " option=" << option
            << " errno =" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

//accept 回来是一个新的socket
Socket::ptr Socket::accept()
{
    //跟自己是一样的
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
    //两个冒号是一位内这是全局的，那个被hook住的函数，后两个参数是 accept 的远端地址，暂时不需要，因为一般不会拒绝
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if(newsock == -1)
    {
        SYLAR_LOG_ERROR(g_logger) << "accept(" << m_sock << ") errno="
            << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    //初始化
    if(sock->init(newsock))
    {
        return sock;
    }
    return nullptr;
}

//用一个fd来初始化 socket 类
bool Socket::init(int sock)
{
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
    if(ctx && ctx->isSocket() && !ctx->isClose())
    {
        m_sock = sock;
        m_isConnected = true;
        initSock();//设计一下 option 等，延时的东西
        //下面两个是给自己初始化一下
        getLocalAddress();
        getRemoteAddress();
        return true;
    }

    return false;
}

bool Socket::bind(const Address::ptr addr)
{
    if(!isValid())
    {
        //如果不是有效状态，就new 一个出来。newSock 也会失败，小概率先不管他
        newSock();
        if(SYLAR_UNLICKLY(!isValid()))
        {
            return false;
        }
    }

    if(SYLAR_UNLICKLY(addr->getFamily() != m_family))
    {
        //说明类型不符合
        SYLAR_LOG_ERROR(g_logger) << "bind sock.family("
            << m_family << ") addr.family(" << addr->getFamily()
            << ") not equal, addr=" << addr->toString();
        return false;
    }

    if(::bind(m_sock, addr->getAddr(), addr->getAddrLen()))
    {
        SYLAR_LOG_ERROR(g_logger) << "bind error errno =" << errno
            << " strerr=" << strerror(errno);
        return false;
    }
    //初始化一下本地得地址，服务器没有 remote 地址（都是accept 别人）
    getLocalAddress();
    return true;
}

//先加上超时，其实没用。以后有好的实现再实现
bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms)
{
    if(!isValid())
    {
        //如果不是有效状态，就new 一个出来。newSock 也会失败，小概率先不管他
        newSock();
        if(SYLAR_UNLICKLY(!isValid()))
        {
            return false;
        }
    }

    if(SYLAR_UNLICKLY(addr->getFamily() != m_family))
    {
        //说明类型不符合
        SYLAR_LOG_ERROR(g_logger) << "connect sock.family("
            << m_family << ") addr.family(" << addr->getFamily()
            << ") not equal, addr=" << addr->toString();
        return false;
    }

    if(timeout_ms == (uint64_t)-1)
    {
        //我们实现了一个timeout的hook方法
        if(::connect(m_sock, addr->getAddr(), addr->getAddrLen()))
        {
            SYLAR_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                << ") error errno=" << errno << " errstr=" << strerror(errno) ;
            //释放这个句柄
            close();
            return false;
        }
    }
    else
    {
        if(::connect_with_timeout(m_sock, addr->getAddr(), addr->getAddrLen(), timeout_ms))
        {
            SYLAR_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                << ") timeout= " << timeout_ms << "error errno=" << errno << " errstr=" << strerror(errno) ;
            //释放这个句柄
            close();
            return false;
        }
    }
    m_isConnected = true;
    //初始化两个地址
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog)
{
    if(!isValid())
    {
        SYLAR_LOG_ERROR(g_logger) << "listen error sock= -1";
        return false;
    }
    if(::listen(m_sock, backlog))
    {
        SYLAR_LOG_ERROR(g_logger) << "listen error errno=" << errno
            << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close()
{
    //可重复关闭
    if(!m_isConnected && m_sock == -1)
    {
        return true;
    }
    m_isConnected = false;
    if(m_sock != -1)
    {
        ::close(m_sock);
        m_sock = -1;
    }
    return false;
}

//length 是 vec 的元素的个数，而不是长度。用到 iovec 的地方同
int Socket::send(const void* buffer, size_t length, int flags)
{
    if(isConnected())
    {
        return ::send(m_sock, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags)
{
    if(isConnected())
    {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

//udp
int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags)
{
    if(isConnected())
    {
        return ::sendTo(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags)
{
    if(isConnected())
    {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        //下面不同的地方
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::recv(void* buffer, size_t length, int flags)
{
    if(isConnected())
    {
        return ::recv(m_sock, buffer, length, flags);
    }
    return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags)
{
    if(isConnected())
    {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;;
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

//udp，去掉const，就是因为收到消息的时候，知道是从哪儿来的，赋值给from
int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags)
{
    if(isConnected())
    {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sock, buffer, length, flags, from->getName(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags)
{
    if(isConnected())
    {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;;
        //下面不同的地方
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress()
{
    if(m_remoteAddress)
    {
        //说明已经被初始化好了
        return m_remoteAddress;
    }
    
    Address::ptr result;
    switch(m_family)
    {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }

    socklen_t addrlen = result->getAddrLen();
    if(getpeername(m_sock, result->getAddr(), &addrlen))
    {
        SYLAR_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock
            << " errno=" << errno << " errstr=" strerror(errno);
        return Addreess::ptr(new UnknownAddress(m_family));
    }
    if(m_family == AF_UNIX)
    {
        //unix 的地址长度是不一样的，要设置一下长度。
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    m_remoteAddress = result;
    return result;
}

Address::ptr Socket::getLocalAddress()
{
    if(m_localAddress)
    {
        //说明已经被初始化好了
        return m_localAddress;
    }
    
    Address::ptr result;
    switch(m_family)
    {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }

    socklen_t addrlen = result->getAddrLen();
    if(getsockname(m_sock, result->getAddr(), &addrlen))
    {
        SYLAR_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sock
            << " errno=" << errno << " errstr=" strerror(errno);
        return Addreess::ptr(new UnknownAddress(m_family));
    }
    if(m_family == AF_UNIX)
    {
        //unix 的地址长度是不一样的，要设置一下长度。
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    m_localAddress = result;
    return result;
}

int Socket::getError()
{
    //错误也是一种属性
    int error = 0;
    size_t len = sizeof(error);
    if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len))
    {
        return -1;
    }
    return error;
}

//调试
std::ostream& Socket::dump(std::ostream& os) const
{
    os << "[Socket sock=" << m_sock
        << " is_connected =" << m_isConnected
        << " family=" << m_family
        << " type=" << m_type
        << " protocol=" << m_protocol;

    if(m_localAddress)
    {
        os << " local_address=" << m_localAddress->toString();
    }
    if(m_remoteAddress)
    {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

//cancel event， iomanager 批量cancel触发。
bool Socket::cancelRead()
{
    return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
}

bool Socket::cancelWrite()
{
    return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::WRITE);
}

bool Socket::cancelAccept()
{
    //也是一种read
    return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
}

bool Socket::cancelAll()
{
    return IOManager::GetThis()->cancelAll(m_sock);
}

void Socket::initSock()
{
    int val = 1;

    //端口释放后立即使用（不设置的话，一般来说是两分钟）
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    //tcp
    if(m_type == SOCK_STREAM)
    {
        //仅用了 Nagle 算法，也就是合并。对延迟敏感的话，都应该禁止
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }

}

void Socket::newSock()
{
    m_sock = socket(m_family, m_type, m_protocol);
    if(SYLAR_LICKLY(m_sock != -1))
    {
        initSock();
    }
    else
    {
        SYLAR_LOG_ERROR(g_logger) << "socket(" << m_family
            << ", " << m_type << ", " << m_protocol << ") errno="
            << errno << " errstr=" << strerror(errno);
    }
}

}
