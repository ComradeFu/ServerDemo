#ifndef __SYLAR_SYLAR_H__
#define __SYLAR_SYLAR_H__

//这种统一的头文件的引用法，在写代码的时候是方便的
//但是弊端也是很大的
//一旦下面的头文件会经常修改，那么所有引用这个头文件的地方都会引起重编译
//说白了就是粒度划分的问题
//但头文件不经常改的话，还是很爽的

#include "sylar/config.h"
#include "sylar/log.h"
#include "sylar/singleton.h"
#include "sylar/thread.h"
#include "sylar/util.h"

#endif