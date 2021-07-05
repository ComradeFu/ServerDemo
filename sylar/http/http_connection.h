#ifndef __SYLAR_HTTP_SESSION_H__
#define __SYLAR_HTTP_SESSION_H__

// server.accept 就是 session，需要配合 http_server(继承自tcp server)
// client.connect 就是 connection

#include "sylar/socket_stream.h"
//要返回 http 的结构体
#include "http.h"
#include "sylar/uri.h"

namespace sylar
{
namespace http 
{

//下面的使用，还需要创建 Request 类，比较麻烦，所以不如写几个静态方法，简化用法
struct HttpResult
{
    typedef std::shared_ptr<HttpResult> ptr;
    enum class Error
    {
        OK = 0,
        INVALID_URL = 1,
        INVALID_HOST = 2,
        CONNECT_FAIL = 3,
        SEND_CLOSE_BY_PEER = 4,
        SEND_SOCKET_ERROR = 5,
        TIMEOUT = 6,
    };
    //简化的结构体
    HttpResult(int _result, HttpResponse::ptr _response
                ,const std::string& _error)
            :result(_result)
            ,response(_response)
            ,error(_error) {}
    
    int result;
    HttpResponse::ptr response;
    std::string error;
};

//继承socket stream
class HttpConnection : public SocketStream
{
public:
    typedef std::shared_ptr<HttpConnection> ptr;

    //get 一般不加body，但严格意义上也没有说一定不能加
    static HttpResult::ptr DoGet(const std::string& url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    //uri 很可能在请求开始的时候，就检测有无问题了。所以真正请求的时候，很可能已经有了现成的uri
    //这样就不必再用string转换一次
    static HttpResult::ptr DoGet(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    static HttpResult::ptr DoPost(const std::string& url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    static HttpResult::ptr DoPost(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    //前三个数都是经常用的
    static HttpResult::ptr DoRequest(HttpMethod method
                                    , const std::string& url
                                    , uint64_t timeout_ms
                                    //cookies 是 header 的一部分，就不抽出来了，不然太多
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    static HttpResult::ptr DoRequest(HttpMethod method
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms
                                    //cookies 是 header 的一部分，就不抽出来了，不然太多
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");
    
    //特别抽出来是因为，有些场景需要用到这样的接口，比如常用在转发
    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                                    , Uri::ptr uri //只提取出地址信息
                                    , uint64_t timeout_ms);

    HttpConnection(Socket::ptr sock, bool owner = true);
    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);
};

}
}

#endif
