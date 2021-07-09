#ifndef __SYLAR_DAEMON_H__
#define __SYLAR_DAEMON_H__

// 不是真的守护进程，只是父fork子wait子，然后帮忙重启
#include <unistd.h>
#include <functional>
#include "sylar/singleton.h"

namespace sylar
{

//存在正在执行的进程的信息
struct ProcessInfo
{
    pid_t parent_id = 0;
    pid_t main_id = 0;
    uint64_t parent_start_time = 0;
    uint64_t main_start_time = 0;
    //出了故障，重启的次数
    uint32_t restart_count = 0;

    std::string toString() const;
};

typedef sylar::Singleton<ProcessInfo> ProcessInfoMgr;

//function 是真正要做的函数
int start_daemon(int argc, char** argv
                    , std::function<int(int argc, char** argv)> main_cb
                    , bool is_daemon);
}

#endif
