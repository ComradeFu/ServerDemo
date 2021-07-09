#include "fd_manager.h"
#include "hook.h"
#include <sys/stat.h>
#include <unistd.h>

namespace sylar
{

FdCtx::FdCtx(int fd)
    :m_isInit(false)
    ,m_isSocket(false)
    ,m_sysNonblock(false)
    ,m_userNonblock(false)
    ,m_isClosed(false)
    ,m_fd(fd)
    ,m_recvTimeout(-1)
    ,m_sendTimeout(-1)
{
    init();
}

FdCtx::~FdCtx()
{

}

bool FdCtx::init()
{
    if(m_isInit)
    {
        return true;
    }

    m_recvTimeout = -1;
    m_sendTimeout = -1;

    struct stat fd_stat;
    if(-1 == fstat(m_fd, &fd_stat))
    {
        m_isInit = false;
        m_isSocket = false;
    }
    else
    {
        m_isInit = true;
        //宏判断，是否为 socket
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }

    if(m_isSocket)
    {
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        if(!(flags & O_NONBLOCK))
        {
            // 是阻塞的，就要修改成非阻塞的。因为框架是非阻塞框架，走 hook
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNonblock = true;
    }
    else
    {
        //如果不是 socket ，就走原来的
        m_sysNonblock = false;
    }

    m_userNonblock = false;
    m_isClosed = false;
    return m_isInit;
}

void FdCtx::setTimeout(int type, uint64_t v)
{
    //复用一下 socket的标志
    if(type == SO_RCVTIMEO)
    {
        m_recvTimeout = v;
    }
    else
    {
        m_sendTimeout = v;
    }   
}

uint64_t FdCtx::getTimeout(int type)
{
    if(type == SO_RCVTIMEO)
    {
        return m_recvTimeout;
    }
    else
    {
        return m_sendTimeout;
    }
}

FdManager::FdManager()
{
    m_datas.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create)
{
    if(fd == -1)
    {
        return nullptr;
    }
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_datas.size() <= fd)
    {
        if(auto_create == false)
        {
            return nullptr;
        }
    }
    else
    {
        if(m_datas[fd] || !auto_create)
        {
            return m_datas[fd];
        }
    }

    lock.unlock();

    //换成写锁
    RWMutexType::WriteLock lock2(m_mutex);
    FdCtx::ptr ctx(new FdCtx(fd));
    if(fd >= (int)m_datas.size()) 
    {
        m_datas.resize(fd * 1.5);
    }

    m_datas[fd] = ctx;

    return ctx;
}

void FdManager::del(int fd)
{
    RWMutexType::WriteLock lock(m_mutex);
    if((int)m_datas.size() <= fd)
    {
        return;
    }

    m_datas[fd].reset();
}

}
