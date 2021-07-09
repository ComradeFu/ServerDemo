#include "util.h"
#include <execinfo.h>

#include "log.h"
#include "fiber.h"
#include <sys/time.h>

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

}