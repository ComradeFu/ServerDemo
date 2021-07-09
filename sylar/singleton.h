#ifndef __SYLAR_SINGLETON_H__
#define __SYLAR_SINGLETON_H__

#include <memory>

namespace sylar {

//N 是为了防止，少数情况下，确实是一个类，但有两个实例。算是单例的一种变通
//一旦 X、N 不同，模板就会重新构建
template<class T, class X = void, int N = 0>
class Singleton {
public:
    static T* GetInstance() {
        static T v;
        return &v;
    }
};

// 智能指针的方式
template<class T, class X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        return v;
    }
};

}

#endif
