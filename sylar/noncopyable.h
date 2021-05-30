#ifndef __SYLAR_NONCOPYABLE_H__
#define __SYlAR_NONCOPYABLE_H__

namespace sylar
{

//利用 C++ 的特性
class Noncopyable
{
public:
    Noncopyable() = default; //默认构造函数
    ~Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;

    //右值的引用没有禁止，留着可以做 move 操作
};

}

#endif
