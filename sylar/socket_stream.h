#ifndef __SYLAR_SOCKET_STREAM_H__
#define __SYLAR_SOCKET_STREAM_H__

#include "stream.h"
#include "socket.h"

//之前的socket，只是对 c 的socket 进行了类的封装
//这一级的封装是更高层次的封装，一般逻辑使用的就是这一层，很少直接裸用socket

namespace sylar
{
class SocketStream : public Stream
{
public:
    typedef std::shared_ptr<SocketStream> ptr;
    //owner 标识是否全权管理。是的话，析构的时候会close
    SocketStream(Socket::ptr sock, bool owner = true);
    ~SocketStream();

    int read(void* buffer, size_t length) override;
    int read(ByteArray::ptr ba, size_t length) override;

    int write(const void* buffer, size_t length) override;
    int write(ByteArray::ptr ba, size_t length) override;

    void close() override;

    Socket::ptr getSocket() const { return m_socket; }
    bool isConnected() const;
protected:
    Socket::ptr m_socket;
    bool m_owner;
private:
};
}


#endif
