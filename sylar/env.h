#ifndef __SYLAR_ENV_H__
#define __SYLAR_ENV_H__

#include "sylar/singleton.h"
#include "sylar/thread.h"
#include <map>
#include <vector>

namespace sylar
{

class Env
{
public:
    //解析一次就可以了，大部分是读锁。安全起见，在解析的时候会用上写锁
    typedef RWMutex RWMutexType;
    bool init(int argc, char** argv);

    void add(const std::string& key, const std::string& val);
    bool has(const std::string& key);
    void del(const std::string& key);
    std::string get(const std::string& key, const std::string& default_val = "");

    void addHelp(const std::string& key, const std::string& desc);
    void removeHelp(const std::string& key);
    void printHelp();
private:
    RWMutexType m_mutex;
    //后续还会有文件系统的api，执行的上下文
    std::map<std::string, std::string> m_args;
    std::vector<std::pair<std::string, std::string>> m_helps;

    //程序名
    std::string m_program;
};

typedef Singleton<Env> EnvMgr;

}

#endif
