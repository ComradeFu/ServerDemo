#ifndef __SYLAR_HTTP_SESSION_H__
#define __SYLAR_HTTP_SESSION_H__

// server.accept 就是 session
// client.connect 就是 connection

#include "sylar/socket_stream.h"
//要返回 http 的结构体
#include "http.h"

namespace sylar
{
namespace http 
{
//继承socket stream
class HttpSession : public SocketStream
{
public:
    typedef std::shared_ptr<HttpSession> ptr;
    HttpSession(Socket::ptr sock, bool owner = true);
    HttpRequest::ptr recvRequest();
    int sendResponse(HttpResponse::ptr rsp);
};

}
}

#endif
