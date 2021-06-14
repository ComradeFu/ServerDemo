#ifndef __SYLAR_MACRO_H__
#define __SYLAR_MACRO_H__

#include <string.h>
#include <assert.h>
#include "util.h"

//编译器优化。不是所有编译器都支持。可以减少汇编层的跳转。这个优化得比较苛刻了。。
#if defined __GNUC__ || defined __llvm__
//上面的成功几率大，下面的成功几率小
#define SYLAR_LICKLY(x)     __builtin_expect(!!(x), 1)
#define SYLAR_UNLICKLY(x)     __builtin_expect(!!(x), 0)
#else
#define SYLAR_LICKLY(x)     (x)
#define SYLAR_UNLICKLY(x)   (x)
#endif

//普通的assert库，没有栈信息，只有assert的位置。断言的时候，是很小概率触发的
#define SYLAR_ASSERT(x) \
    if(SYLAR_UNLICKLY(!(x))) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) <<" ASSERTTION: " #x \
            << "\nbacktrace:\n" \
            << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#define SYLAR_ASSERT2(x, w) \
    if(SYLAR_UNLICKLY(!(x))) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) <<" ASSERTTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif
