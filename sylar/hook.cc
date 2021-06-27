#include "hook.h"
#include <dlfcn.h>

#include "config.h"
#include "log.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "macro.h"
#include <stdarg.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar
{

static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout = 
    sylar::Config::Lookup("tcp.connect.timeout", 5000, "rcp connect timeout");

static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init()
{
    static bool is_inited = false;
    if(is_inited)
        return;

// ## 连接，从动态库里面，取出来这个函数（原先的函数），赋值给我们的变量
#define XX(name) name ## _f = (name ## _func)dlsym(RTLD_NEXT, #name);
    //赋值出来
    HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;

//故技重施，在 main 函数之前执行某个初始化函数
struct _HookIniter
{
    _HookIniter()
    {
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout->getValue();

        g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value)
        {
            SYLAR_LOG_INFO(g_logger) << "tcp connect timeout changed from " 
                                    << old_value << " to " << new_value;
            s_connect_timeout = new_value;
        });
    }
};

static _HookIniter s_hook_initer;

bool is_hook_enable()
{
    return t_hook_enable;
}

void set_hook_enable(bool flag)
{
    t_hook_enable = flag;
}

}//namespace sylar

//用来在条件定时器
struct timer_info
{
    int cancelled = 0;
};

// ioevent里 的 event，timeout_so 是fdmanager里超时的类型。args 是要hook的函数的匿名参数。forward 展开
template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_func_name,
        uint32_t event, int timeout_so, Args&&... args)
{
    //如果不 hook，就原参数
    if(!sylar::t_hook_enable)
    {
        //展开参数
        return fun(fd, std::forward<Args>(args)...);
    }

    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if(!ctx)
    {
        //如果不存在，就当作不是 socket，也同样得按照原来的io操作来做
        //因为我们只hook住了 socket
        return fun(fd, std::forward<Args>(args)...);
    }

    if(ctx->isClose())
    {
        //存在且关闭了
        errno = EBADF;
        return -1;
    }

    //getUserNonblock，用户设置了不阻塞
    if(!ctx->isSocket() || ctx->getUserNonblock())
    {
        return fun(fd, std::forward<Args>(args)...);
    }

    //timeout
    uint64_t to = ctx->getTimeout(timeout_so);
    // condition timer
    std::shared_ptr<timer_info> tinfo(new timer_info);

//精华部分
retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR)
    {
        //中断，重试
        n = fun(fd, std::forward<Args>(args)...);
    }

    if(n == -1 && errno == EAGAIN)
    {
        //阻塞状态，没数据了
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if(to != (uint64_t)-1)
        {
            //注意这里的回调，是可能被不同线程调度到的。所以加了很多的判断（有可能跟下面的同时进行）
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                auto t = winfo.lock();
                if(!t || t->cancelled){
                    //说明定时器失效了
                    return;
                }
                t->cancelled = ETIMEDOUT;
                //取消时间，强制唤醒。因为已经超时了
                iom->cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }
        //没有传入 cb，所以是直接用本 fiber 做为回调
        int rt = iom->addEvent(fd, (sylar::IOManager::Event)(event));
        if(rt)
        {
            SYLAR_LOG_ERROR(g_logger) << hook_func_name << " addEvent("
                    << fd << ", " << event << ")";

            if(timer)
            {
                timer->cancel();
            }
            
            //直接失败
            return -1;
        }
        else
        {
            //成功加入时间之后，就开始让出，等唤醒
            //没必要 READY
            // SYLAR_LOG_INFO(g_logger) << "do_io<" << hook_func_name << "> before hold";
            sylar::Fiber::YieldToHold();
            // SYLAR_LOG_INFO(g_logger) << "do_io<" << hook_func_name << "> after hold";
            //唤醒回来之后，如果timer 还在的话，cancel
            //唤醒有两种可能。一种是真的有事件过来了，另一种是上面的超时定时器超时了。
            if(timer)
            {
                timer->cancel();
            }

            if(tinfo->cancelled)
            {
                //说明超时了
                errno = tinfo->cancelled;
                return -1;
            }

            //唤醒之后，没有超时，说明就是真的有数据来了。那就继续做读取动作。一直到 errno 不是again

            //这里用while更好，goto 比较可怕
            goto retry;
        }
    }

    //当读到数据后，返回N
    return n;
}

extern "C"
{
//先初始化为空
#define XX(name) name ## _func name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds)
{
    if(!sylar::t_hook_enable)
    {
        //原来的函数
        return sleep_f(seconds);
    }

    //如果hook住了，就实现一下

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();

    //过多少秒之后换回来，就是重新 schedule 这个 fiber
    // iom->addTimer(seconds * 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
    //上面的不支持 scheduler 是模板，先用lambda 处理一下
    iom->addTimer(seconds * 1000, [iom, fiber](){
        iom->schedule(fiber);
    });

    //换出去
    sylar::Fiber::YieldToHold();

    return 0;
}

int usleep(useconds_t usec)
{
    if(!sylar::t_hook_enable)
    {
        return usleep_f(usec);
    }

    //如果hook住了，就实现一下

    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();

    //过多少秒之后换回来，就是重新 schedule 这个 fiber
    // iom->addTimer(seconds / 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
    //上面的不支持 scheduler 是模板，先用lambda 处理一下
    iom->addTimer(usec / 1000, [iom, fiber](){
        iom->schedule(fiber);
    });

    //换出去
    sylar::Fiber::YieldToHold();

    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if(!sylar::t_hook_enable)
    {
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();

    iom->addTimer(timeout_ms, [iom, fiber](){
        iom->schedule(fiber);
    });

    //换出去
    sylar::Fiber::YieldToHold();

    return 0;
}

int socket(int domain, int type, int protocol)
{
    if(!sylar::t_hook_enable)
        return socket_f(domain, type, protocol);

    int fd = socket_f(domain, type, protocol);
    //理论上是一定会成功的。。
    if(fd == -1)
    {
        return fd;
    }
    //自动创建
    sylar::FdMgr::GetInstance()->get(fd, true);

    return fd;
}

// 第一步：使用socket创建套接口；
// 第二步：设置套接口为非阻塞模式（默认为阻塞模式）;
// 第三步：调用connect进行连接；
// 第四步：判断第三步connect的返回值，如果返回值为0，表示连接立即成功，至此连接全部完成，不用再进行下面的步骤；
// 第五步：判断第三步connect的返回值，如果返回值不为0，此时有两种情况：第一种情况errno为EINPROGRESS，表示连接没有立即成功，需进行二次判断，进入第六步；第二种情况errno不为EINPROGRESS，表示连接失败，调用close关闭套接口之后，再次connect；
// 第六步：将该套接口加入epoll中，调用epoll_wait等待套接口的通知；
// 第七步：如果连接成功，正常情况下epoll触发EPOLLOUT事件，不会触发EPOLLIN事件。但有一种情况，如果connect成功之后，服务端马上发送数据，此时客户端也会立刻得到EPOLLIN事件。如果连接失败，我们会得到EPOLLIN、EPOLLOUT、EPOLLERR和EPOLLHUP事件。
int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms)
{
    if(!sylar::t_hook_enable)
    {
        return connect_f(fd, addr, addrlen);
    }
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClose())
    {
        errno = EBADF; //bad fd
        return -1;
    }

    if(!ctx->isSocket())
    {
        return connect_f(fd, addr, addrlen);
    }

    //如果呢，用户已经自己设置过了，那也不用管（用户自己负责nonblock就好）
    if(ctx->getUserNonblock())
    {
        return connect_f(fd, addr, addrlen);
    }

    //这里有一个问题，如果这里返回了，会导致IOMnagaer的epoll没有加到这个事件，cancel的时候会出现没有此fd
    //但如果fd已经设置为非阻塞，这里又是一定返回 -1 的。emmm
    int n = connect_f(fd, addr, addrlen);
    if(n == 0)
        return 0;
    else if(n != -1 || errno != EINPROGRESS)
    {
        return n;
    }

    sylar::IOManager* iom = sylar::IOManager::GetThis();
    sylar::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms != (uint64_t)-1)
    {
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom]()
        {
            auto t = winfo.lock();
            if(!t || t->cancelled)
            {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, sylar::IOManager::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, sylar::IOManager::WRITE);
    if(rt == 0)
    {
        sylar::Fiber::YieldToHold();
        if(timer)
        {
            timer->cancel();
        }
        if(tinfo->cancelled)
        {
            errno = tinfo->cancelled;
            return -1;
        }
    }
    else
    {
        if(timer)
        {
            timer->cancel();
        }
        SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    //下面是差异
    int error = 0;
    socklen_t len  = sizeof(int);
    //取出状态，看看有无错误
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len))
    {
        //取失败
        return -1;
    }
    if(!error)
    {
        return 0;
    }
    else
    {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    //相对accept 来说，有点复杂
    //不能用 do_io。尽管很像，还是重新实现
    return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    //accept 用的是 read 事件
    int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0)
    {
        sylar::FdMgr::GetInstance()->get(fd, true);//初始化
    }

    return fd;
}

//read
ssize_t read(int fd, void *buf, size_t count)
{
    //不像 accept ，因为 accept 会获得新的fd，还需要去初始化一下，这个就完全不用
    return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

//write
ssize_t write(int fd, const void *buf, size_t count)
{
    return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags)
{
    return do_io(s, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
    return do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags)
{
    return do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd)
{
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "close fd=" << fd;
    //跟 accept 类似，也有一些区别，就是要清理一下fd
    if(!sylar::t_hook_enable)
        return close_f(fd);

    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if(ctx)
    {
        auto iom = sylar::IOManager::GetThis();
        if(iom)
        {
            iom->cancelAll(fd);
        }
        sylar::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

//下面的几个也很复杂
int fcntl(int fd, int cmd, ...)
{
    //约20 cmds，四种传参类型。着重处理 nonblock 等几个重要的
    va_list va;
    va_start(va, cmd);
    switch (cmd)
    {
        // int 参数
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket())
                {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock())
                {
                    arg |= O_NONBLOCK;
                }
                else
                {
                    //去掉
                    arg &= ~O_NONBLOCK;
                }

                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket())
                {
                    return arg;
                }
                if(ctx->getUserNonblock())
                {
                    return arg | O_NONBLOCK;
                }
                else
                {
                    //虽然 hook 住了，但要给系统体现出来的是NONBLOCK的
                    return arg & ~O_NONBLOCK;
                }
            }
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        // void 参数
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;

        //flock 参数
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        //f_owner_ex 参数
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

//这个 d 命名不是很好
int ioctl(int d, unsigned long int request, ...)
{
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request)
    {
        bool user_nonblock = !!*(int*)arg;
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket())
        {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    //get 就不需要hook了
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    if(!sylar::t_hook_enable)
    {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET)
    {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)
        {
            //拿出来超时时间，同时放到自己的 fdmanager 里面管理（为什么不直接利用超时信息？不懂）
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(sockfd);
            if(ctx)
            {
                const timeval* tv = (const timeval*)optval;
                ctx->setTimeout(optname, tv->tv_sec * 1000 + tv->tv_usec / 1000);
            }
        }
    }

    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}//extern C
