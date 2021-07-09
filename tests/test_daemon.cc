#include "sylar/daemon.h"
#include "sylar/iomanager.h"
#include "sylar/log.h"

//请用 top 观察两个进程，并且kill掉子进程看看

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

sylar::Timer::ptr timer;
int server_main(int argc, char** argv) 
{
    SYLAR_LOG_INFO(g_logger) << sylar::ProcessInfoMgr::GetInstance()->toString();

    sylar::IOManager iom(1);
    timer = iom.addTimer(1000, [](){
            static int count = 0;
            SYLAR_LOG_INFO(g_logger) << "onTimer, count" << count;
            if(++count > 10) {
                exit(1);
            }
    }, true);
    return 0;
}

int main(int argc, char** argv) 
{
    return sylar::start_daemon(argc, argv, server_main, argc != 1);
}
