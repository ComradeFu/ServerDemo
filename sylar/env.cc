#include "env.h"
#include "sylar/log.h"
#include <string.h>
#include <iostream>
#include <iomanip> //std 的格式

//读取软连接
#include <unistd.h>
//环境变量
#include <stdlib.h>

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

bool Env::init(int argc, char** argv)
{
    char link[1024] = {0};
    char path[1024] = {0};
    // exe 是软链接，所以需要用到 readlink，来读到它真实的路径
    // /proc/pid/ 下还有一些有用的信息，比如 cmdline，就可以看到启动的完整命令行了
    // 这样，就可以在 main 函数之前，就能够获取到完整的命令，不用等main传递 argc argv
    sprintf(link, "/proc/%d/exe", getpid());
    readlink(link, path, sizeof(path));

    // /path/xxx/xxx/exe
    m_exe = path; //readlink 返回的是实际大小，但是由于前面的数组都补0了，所以在转成 string 的时候自动的搞定了，所以就不用了

    // /proc 下的cwd是终端调用时的文件夹路径，我们不用。我用要的是exe的文件路径
    auto pos = m_exe.find_last_of("/");
    m_cwd = m_exe.substr(0, pos) + "/";

    m_program = argv[0];
    // -config /path/to/config -f xxxx -d，默认支持这种最简单的方式
    const char* now_key = nullptr;
    for(int i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            if(strlen(argv[i]) > 1)
            {
                if(now_key)
                {
                    //如果上一个参数没有值，又紧接着遇到了下一个key，那么说明上一个key是不带val的
                    add(now_key, "");
                }
                now_key = argv[i] + 1; //跳过 -
            }
            else
            {
                //只是一个 - 就没意义
                SYLAR_LOG_ERROR(g_logger) << "invalid arg idx=" << i
                    << " val=" << argv[i];
                return false;
            }
        }
        else
        {
            if(now_key)
            {
                //说明是值
                if(now_key)
                {
                    add(now_key, argv[i]);
                    now_key = nullptr;
                }
            }
            else
            {
                SYLAR_LOG_ERROR(g_logger) << "invalid arg idx=" << i
                    << " val=" << argv[i];
                return false;
            }
        }
    }
    if(now_key)
    {
        add(now_key, "");
    }
    return true;
}

void Env::add(const std::string& key, const std::string& val)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_args[key] = val;
}

bool Env::has(const std::string& key)
{
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return it != m_args.end();
}

std::string Env::get(const std::string& key, const std::string& default_val)
{
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return it != m_args.end() ? it->second : default_val;
}

void Env::del(const std::string& key)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_args.erase(key);
}

void Env::addHelp(const std::string& key, const std::string& desc)
{
    removeHelp(key);
    RWMutexType::WriteLock lock(m_mutex);
    //用vec是能让help有序输出，重要的在前面
    m_helps.push_back(std::make_pair(key, desc));
}

void Env::removeHelp(const std::string& key)
{
    RWMutexType::WriteLock lock(m_mutex);
    for(auto it = m_helps.begin(); it != m_helps.end();)
    {
        if(it->first == key)
        {
            m_helps.erase(it); //vec 也有 erase
            return;
        }
        else
        {
            ++it;
        }
    }
}

void Env::printHelp()
{
    RWMutexType::ReadLock lock(m_mutex);
    std::cout << "Usage: " << m_program << " [options]" << std::endl;
    for(auto& i : m_helps)
    {
        std::cout << std::setw(5) << "-" << i.first << " : " << i.second << std::endl;
    }
}

//一般不用，不过为了一对儿接口，就这么设计先给着了
bool Env::setEnv(const std::string& key, const std::string& val)
{
    return setenv(key.c_str(), val.c_str(), 1);
}

std::string Env::getEnv(const std::string& key, const std::string& default_val)
{
    const char* v = getenv(key.c_str());
    if(v == nullptr)
    {
        return default_val;
    }
    else
    {
        return v;
    }
}
}
