#include "env.h"
#include "sylar/log.h"
#include <string.h>
#include <iostream>
#include <iomanip> //std 的格式

namespace sylar
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

bool Env::init(int argc, char** argv)
{
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
}
