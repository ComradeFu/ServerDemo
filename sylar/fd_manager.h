#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__

//管理所有文件有关的东西，fd 状态

#include <memory>
#include <vector>
#include "thread.h"
#include "singleton.h"

namespace sylar
{

class FdCtx : public std::enable_shared_from_this<FdCtx>
{
public:
    typedef std::shared_ptr<FdCtx> ptr;
    FdCtx(int fd);
    ~FdCtx();

    bool init();
    bool isInit() const { return m_isInit; }
    bool isSocket() const { return m_isSocket; }
    bool isClose() const { return m_isClosed; }
    bool close();

    void setUserNonblock(bool v) { m_userNonblock = v; }
    bool getUserNonblock() const { return m_userNonblock; }

    void setSysNonblock(bool v) { m_sysNonblock = v; }
    bool getSysNonblock() const { return m_sysNonblock; }

    //type 分 read write
    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);
private:
    bool m_isInit: 1;
    bool m_isSocket: 1; //是不是 socket
    bool m_sysNonblock: 1; //是不是设定了不阻塞，系统态
    bool m_userNonblock: 1; //用户态，用户设置了自己 nonblock 的话，也不需要hook那边去左 nonblock 了（用户已经自己做了）
    bool m_isClosed: 1;
    int m_fd;

    uint64_t m_recvTimeout;
    uint64_t m_sendTimeout;
};

class FdManager
{
public:
    typedef RWMutex RWMutexType;
    FdManager();

    //auto 如果 fd 不存在的时候，会自动创建一个
    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd); //比如socket关闭

private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_datas;
};

typedef Singleton<FdManager> FdMgr;

}

#endif
