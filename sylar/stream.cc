#include "stream.h"

namespace sylar
{
//外面确保 buffer 是支持 length 长度的
int Stream::readFixSize(void* buffer, size_t length)
{
    size_t offset = 0;
    size_t left = length;
    while(left > 0)
    {
        //下面用 char* 转换一次，是 void* 不允许 + 操作（因为不知道类型，也就不知道大小了）
        size_t len = read((char*)buffer + offset, left);
        if(len < 0)
        {
            //异常
            return len;
        }

        offset += len;
        left -= len;
    }

    return length;
}

int Stream::readFixSize(ByteArray::ptr ba, size_t length)
{
    size_t left = length;
    while(left > 0)
    {
        //ba 自带了 position，每次读都是自增的了，所以简单一些
        size_t len = read(ba, left);
        if(len < 0)
        {
            //异常
            return len;
        }
        left -= len;
    }

    return length;
}

int Stream::writeFixSize(const void* buffer, size_t length)
{
    size_t offset = 0;
    size_t left = length;
    while(left > 0)
    {
        //下面用 char* 转换一次，是 void* 不允许 + 操作（因为不知道类型，也就不知道大小了）
        //由于是 write，所以是 const
        size_t len = write((const char*)buffer + offset, left);
        if(len < 0)
        {
            //异常
            return len;
        }

        offset += len;
        left -= len;
    }

    return length;
}

int Stream::writeFixSize(ByteArray::ptr ba, size_t length)
{
    size_t left = length;
    while(left > 0)
    {
        //ba 自带了 position，每次读都是自增的了，所以简单一些
        size_t len = write(ba, left);
        if(len < 0)
        {
            //异常
            return len;
        }
        left -= len;
    }

    return length;
}
}
