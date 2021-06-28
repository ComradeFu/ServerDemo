#ifndef __SYLAR_STREAM_H__
#define __SYLAR_STREAM_H__

#include <memory>
#include "bytearray.h"

//外部使用的时候，就是一个 stream。所以我们可以用文件的stream来模拟一个 socket stream
//进而进行调试

namespace sylar
{

class Stream
{
public:
    typedef std::shared_ptr<Stream> ptr;
    //作为基类，可以用在文件或者 socket
    virtual ~Stream() {}

    //类似socket的api
    virtual int read(void* buffer, size_t length) = 0;
    //从当前position
    virtual int read(ByteArray::ptr ba, size_t length) = 0;
    virtual int readFixSize(void* buffer, size_t length);
    virtual int readFixSize(ByteArray::ptr ba, size_t length);

    virtual int write(const void* buffer, size_t length) = 0;
    //从当前position
    virtual int write(ByteArray::ptr ba, size_t length) = 0;
    virtual int writeFixSize(const void* buffer, size_t length);
    virtual int writeFixSize(ByteArray::ptr ba, size_t length);

    virtual void close() = 0;
};

}

#endif
