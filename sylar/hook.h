// 主要是改写一些系统上的同步操作，这样能让同步也有异步的性能。
// 比如mysql库，就必须同步等待返回。我们可以重写成让epoll等待即可。
// 还有 sleep、usleep 函数等
#ifndef __SYLAR_HOOK_H__
#define __SYLAR_HOOK_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

namespace sylar 
{
    //不是所有的线程、所有的功能都是 hook 住的。我们这个粒度可以细化到线程这个级别去。
    bool is_hook_enagle();
    void set_hook_enable(bool flag);
}

// c 风格函数。编译器会放弃所有C++做的特殊操作，比如重载的时候，会给函数加一些奇奇怪怪的符号，来以示区分
extern "C"
{
//sleep
//签名，为了切换老的、新的
typedef unsigned int (*sleep_func)(unsigned int seconds);
extern sleep_func sleep_f;

typedef int (*usleep_func)(useconds_t usec);
extern usleep_func usleep_f;

typedef int (*nanosleep_func)(const struct timespec *req, struct timespec *rem);
extern nanosleep_func nanosleep_f;

//socket 有关
typedef int (*socket_func)(int domain, int type, int protocal);
extern socket_func socket_f;

typedef int (*connect_func)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern connect_func connect_f;

typedef int (*accept_func)(int s, struct sockaddr *addr, socklen_t *addrlen);
extern accept_func accept_f;

typedef ssize_t (*read_func)(int fd, void *buf, size_t count);
extern read_func read_f;

//类似一块链表的方式，直接读出多个块数据
typedef ssize_t (*readv_func)(int fd, const struct iovec *iov, int iovcnt);
extern readv_func readv_f;

//pread

//针对socket，上面的read针对文件
typedef ssize_t (*recv_func)(int sockfd, void *buf, size_t len, int flags);
extern recv_func recv_f;

typedef ssize_t (*recvfrom_func)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
extern recvfrom_func recvfrom_f;

typedef  ssize_t (*recvmsg_func)(int sockfd, struct msghdr *msg, int flags);
extern recvmsg_func recvmsg_f;

//write
typedef ssize_t (*write_func)(int fd, const void *buf, size_t count);
extern write_func write_f;

typedef ssize_t (*writev_func)(int fd, const struct iovec *iov, int iovcnt);
extern writev_func writev_f;

typedef ssize_t (*send_func)(int s, const void *msg, size_t len, int flags);
extern send_func send_f;

typedef ssize_t (*sendto_func)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
extern sendto_func sendto_f;

typedef ssize_t (*sendmsg_func)(int s, const struct msghdr *msg, int flags);
extern sendmsg_func sendmsg_f;

//close
typedef int (*close_func)(int fd);
extern close_func close_f;

//设置socket有关，比如转成异步socket
typedef int (*fcntl_func)(int fd, int cmd, ...);
extern fcntl_func fcntl_f;

typedef int (*ioctl_func)(int d, unsigned long int request, ...);
extern ioctl_func ioctl_f;

//还有设置 opt 的一对儿
typedef int (*getsockopt_func)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
extern getsockopt_func getsockopt_f;

typedef int (*setsockopt_func)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern setsockopt_func setsockopt_f;

}

#endif
