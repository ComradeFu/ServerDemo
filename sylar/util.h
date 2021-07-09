#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace sylar {

pid_t GetThreadId();

uint64_t GetFiberId();

//两种方法
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

//下面的 skip 是2，是因为它调用了 Backtrace ，所以有两层
std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

//时间ms
uint64_t GetCurrentMS();
uint64_t GetCurrentUS();

std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");
}

#endif
