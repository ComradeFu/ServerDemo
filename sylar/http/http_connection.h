#ifndef __SYLAR_HTTP_SESSION_H__
#define __SYLAR_HTTP_SESSION_H__

// server.accept 就是 session，需要配合 http_server(继承自tcp server)
// client.connect 就是 connection

#include "sylar/socket_stream.h"
//要返回 http 的结构体
#include "http.h"
#include "sylar/uri.h"
#include "sylar/thread.h"

#include <list>

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
        CREATE_SOCKET_ERROR = 7,
        POOL_GET_CONNECTION = 8,
        POOL_INVALID_CONNECTION = 9,
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

    std::string toString() const;
};

class HttpConnectionPool;

//继承socket stream
class HttpConnection : public SocketStream
{
friend class HttpConnectionPool;
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
    ~HttpConnection();

    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);

private:
    uint64_t m_createTime = 0;
    uint64_t m_request = 0; //已经处理的请求数
};

//一个链接池针对一个 host，仿照nginx
//像nginx是可以做到轮询、权重、QoS智能迁移等等的负载均衡，我们目前只做个基础，随便
class HttpConnectionPool
{
public:
    typedef std::shared_ptr<HttpConnectionPool> ptr;
    typedef Mutex MutexType; //读写分离的情况不多

    HttpConnectionPool(const std::string& host
                        , const std::string& vhost
                        , uint32_t port
                        , uint32_t max_size
                        , uint32_t max_alive_time
                        , uint32_t max_request);

    HttpConnection::ptr getConnection();

    //类似 connection，uri 跟上面的意义不是一致的，host部分没得改
    HttpResult::ptr doGet(const std::string& url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    HttpResult::ptr doGet(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    HttpResult::ptr doPost(const std::string& url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    HttpResult::ptr doPost(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    //前三个数都是经常用的
    HttpResult::ptr doRequest(HttpMethod method
                                    , const std::string& url
                                    , uint64_t timeout_ms
                                    //cookies 是 header 的一部分，就不抽出来了，不然太多
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");

    HttpResult::ptr doRequest(HttpMethod method
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms
                                    //cookies 是 header 的一部分，就不抽出来了，不然太多
                                    , const std::map<std::string, std::string>& headers = {}
                                    , const std::string& body = "");
    
    //特别抽出来是因为，有些场景需要用到这样的接口，比如常用在转发
    HttpResult::ptr doRequest(HttpRequest::ptr req
                                    , uint64_t timeout_ms);

private:
    static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);
private:
    std::string m_host;
    //协议里的host，http/1.1 强制要求
    std::string m_vhost;
    uint32_t m_port;
    uint32_t m_maxSize;
    uint32_t m_maxAliveTime; //最大的保持时间
    uint32_t m_maxRequest; //最多请求数，就认为应该重新建立连接了
    //失败先不定义了，因为毕竟不是 nginx 那种还需要负载均衡的
    //失败事实上影响业务（upstream 到别的地方去）
    //我们失败了就相当于链接断开就行

    MutexType m_mutex;
    //注意是指针，不是智能指针
    //这是因为我们需要利用智能指针做一件事，就是自定义析构函数。
    //能放回链接池的放回，不能放回的才进行删除
    std::list<HttpConnection*> m_conns; 
    //策略是这样：maxSize 不是死上限，万一遇到峰值就必须放行，不然是不对的
    //maxSize只是作为一个回弹保留的数量
    std::atomic<int32_t> m_total = {0}; //m_maxSize

};

}
}

#endif
