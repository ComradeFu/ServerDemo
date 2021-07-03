#ifndef __SYLAR_HTTP_SESSION_H__
#define __SYLAR_HTTP_SESSION_H__

// server.accept 就是 session，需要配合 http_server(继承自tcp server)
// client.connect 就是 connection

#include "sylar/socket_stream.h"
//要返回 http 的结构体
#include "http.h"

namespace sylar
{
namespace http 
{
//继承socket stream
class HttpConnection : public SocketStream
{
public:
    typedef std::shared_ptr<HttpConnection> ptr;
    HttpConnection(Socket::ptr sock, bool owner = true);
    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);
};

}
}

#endif
