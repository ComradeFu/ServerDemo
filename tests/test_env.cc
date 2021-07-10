#include "sylar/env.h"
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "argc=" << argc << std::endl;
    sylar::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    sylar::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    sylar::EnvMgr::GetInstance()->addHelp("p", "print help");
    if(!sylar::EnvMgr::GetInstance()->init(argc, argv))
    {
        sylar::EnvMgr::GetInstance()->printHelp();
        return 0;
    }

    if(sylar::EnvMgr::GetInstance()->has("p"))
    {
        sylar::EnvMgr::GetInstance()->printHelp();
    }

    return 0;
}
