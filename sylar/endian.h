#ifndef __SYLAR_ENDIAN_H__
#define __SYLAR_ENDIAN_H__

//字节序，不同机器用的是不一样的
// BYTE_ORDER

//小端
#define SYLAR_LITTLE_ENDIAN 1 
//大端
#define SYLAR_BIG_ENDIAN 2

#include <byteswap.h>
#include <stdint.h>

namespace sylar
{

//其实下面用重载也能实现。炫技

//64
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value)
{
    return (T)bswap_64((uint64_t)value);
}

//32
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value)
{
    return (T)bswap_32((uint64_t)value);
}

//16
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value)
{
    return (T)bswap_16((uint64_t)value);
}

//内置宏
#if BYTE_ORDER == BIG_ENDIAN
#define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
#else
#define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN

#if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN
template<class T>
T byteswapOnLittleEndian(T t)
{
    return t;
}

template<class T>
T byteswapOnBigEndian(T t)
{
    return byteswap(t);
}
#else
template<class T>
T byteswapOnLittleEndian(T t)
{
    return byteswap(t);
}

template<class T>
T byteswapOnBigEndian(T t)
{
    return t;
}

}

#endif
