#include "daemon.h"
#include "sylar/log.h"
#include "sylar/config.h"
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
static sylar::ConfigVar<uint32_t>::ptr g_daemon_restart_interval
    = sylar::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

std::string ProcessInfo::toString() const
{
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" << parent_id
        << " main_id=" << main_id
        << " parent_start_time=" << sylar::Time2Str(parent_start_time)
        << " main_start_time=" << sylar::Time2Str(main_start_time)
        << " restart_count=" << restart_count << "]";
    return ss.str();
}

static int real_start(int argc, char** argv
                        , std::function<int(int argc, char** argv)> main_cb)
{
    return main_cb(argc, argv);
}

static int real_daemon(int argc, char** argv
                        , std::function<int(int argc, char** argv)> main_cb)
{
    //库函数，第二个参数是标准输出输入流是否要关闭，一般是要关闭的
    daemon(1, 0);

    //记录自己的信息
    ProcessInfoMgr::GetInstance()->parent_id = getpid();
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
    while(true)
    {
        pid_t pid = fork();
        if(pid == 0)
        {
            //子进程，pid = 0
            ProcessInfoMgr::GetInstance()->main_id = getpid();
            ProcessInfoMgr::GetInstance()->main_start_time = time(0);

            SYLAR_LOG_INFO(g_logger) << "process start, pid " << getpid();
            return real_start(argc, argv, main_cb);
        }
        else if(pid < 0)
        {
            //有问题
            SYLAR_LOG_ERROR(g_logger) << "for fail return=" << pid
                << " errno=" << errno << " errstr=" << strerror(errno);
            return -1;
        }
        else
        {
            //父进程返回，好耶。等待子进程返回
            int status = 0;
            waitpid(pid, &status, 0);
            if(status)
            {
                //exit 不为 0 ，子进程异常。又回到while循环
                SYLAR_LOG_ERROR(g_logger) << "child crash pid=" << pid
                    << " status=" << status;
            }
            else
            {
                //正常完成逻辑退出
                SYLAR_LOG_INFO(g_logger) << "child finished, pid=" << pid;
                break;
            }

            ProcessInfoMgr::GetInstance()->restart_count += 1;
            //上一次的进程资源可能还没有释放，所以这里需要等一会儿，这样成功率高
            sleep(g_daemon_restart_interval->getValue());
        }
    }
    return 0;
}

int start_daemon(int argc, char** argv
                    , std::function<int(int argc, char** argv)> main_cb
                    , bool is_daemon)
{
    if(!is_daemon)
    {
        return real_start(argc, argv, main_cb);
    }

    return real_daemon(argc, argv, main_cb);
}

}
