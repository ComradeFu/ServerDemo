#include "util.h"
#include <execinfo.h>

#include "log.h"
#include "fiber.h"
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

pid_t GetThreadId() 
{
    return syscall(SYS_gettid);
}

uint64_t GetFiberId() 
{
    return sylar::Fiber::GetFiberId();
}

void Backtrace(std::vector<std::string>& bt, int size, int skip)
{
    //由于我们主要的业务，是在协程上执行的，这个函数想必肯定是协程上大量使用。
    //而协程，又是一个轻量级的、多量的东西
    //在我们可以设置协程的栈大小的前提下，我们一般不会设置的太大（不同于线程很大的栈）
    //这样一是能控制栈空间分配，二是在切换的时候，栈也会更轻量从而更有效率去切换
    //如果在栈上直接新建比较大的对象，很容易就溢出了
    //所以能用指针的地方，就用指针，而不是直接在栈上生成
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s = ::backtrace(array, size);

    char** strings = backtrace_symbols(array, s);
    if(strings == NULL)
    {
        SYLAR_LOG_ERROR(g_logger) << "backtrace_symbols error";
        free(array);
        return;
    }

    for(size_t i = skip; i < s; ++i)
    {
        bt.push_back(strings[i]);
    }

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix)
{
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;

    for(size_t i = 0; i < bt.size(); ++i)
    {
        ss << prefix << bt[i] << std::endl;
    }

    return ss.str();
}

//毫秒
uint64_t GetCurrentMS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

//纳秒
uint64_t GetCurrentUS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}

std::string Time2Str(time_t ts, const std::string& format)
{
    struct tm tm;
    localtime_r(&ts, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), format.c_str(), &tm);
    
    return buf;
}

void FSUtil::ListAllFile(std::vector<std::string>& files
                                , const std::string& path
                                , const std::string& subfix)
{
    //C++11 的支持可能还不够，所以先用linux内核提供的接口
    if(access(path.c_str(), 0) != 0)
    {
        //不存在这个文件夹路径
        return;
    }

    DIR* dir = opendir(path.c_str());
    if(dir == nullptr)
    {
        //打开失败
        return;
    }

    struct dirent* dp = nullptr;
    while((dp = readdir(dir)) != nullptr)
    {
        if(dp->d_type == DT_DIR)
        {
            //过滤掉当前跟上一层
            if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
                continue;
            ListAllFile(files, path + "/" + dp->d_name, subfix);
        }
        else if(dp->d_type == DT_REG)
        {
            //正常的文件
            std::string filename(dp->d_name);
            if(subfix.empty())
            {
                files.push_back(path + "/" + filename);
            }
            else
            {
                if(filename.size() < subfix.size())
                {
                    continue;
                }
                if(filename.substr(filename.length() - subfix.size()) == subfix)
                {
                    //简单的以这个结尾，就当作是成立了
                    files.push_back(path + "/" + filename);
                }
            }
        }
    }

    closedir(dir);
}

}