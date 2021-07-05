#include "http_connection.h"
#include "http_parser.h"
#include "sylar/log.h"
#include "sylar/uri.h"

namespace sylar
{
namespace http
{
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner)
{

}

HttpResponse::ptr HttpConnection::recvResponse()
{
    HttpResponseParser::ptr parser(new HttpResponseParser);
    //最大的buffer
    uint64_t buff_size = HttpResponseParser::GetHttpResponseBufferSize();
    // uint64_t buff_size = 100;
    //智能指针智能看错一个对象，所以要指定数组的析构方法
    // + 1 是parser的断言，放一个char放它的'\0'
    std::shared_ptr<char> buffer(new char[buff_size + 1],
        [](char* ptr){
            delete[] ptr;
        });
    //操作裸指针
    char* data = buffer.get();
    //data读出来多少的偏移
    int offset = 0;
    do
    {
        int len = read(data + offset, buff_size - offset);
        if(len <= 0)
        {
            close();
            return nullptr;
        }
        len += offset;
        data[len] = '\0';
        //返回实际解析的长度。这里的execute是简化了的，一直从0开始，否则是可以指定offset的。那样就更复杂了额
        //parser会把已经解析好的给删掉(mov)
        int nparser = parser->execute(data, len, false);
        if(parser->hasError())
        {
            close();
            return nullptr;
        }
        offset = len - nparser;
        if(offset == (int)buff_size)
        {
            close();
            //缓冲区满了，还是没有解析完（状态机结束）。也是有问题的
            return nullptr;
        }
        if(parser->isFinished())
        {
            //解析完成
            break;
        }
    } while(true);

    //对 encode 为 chunked 的处理
    auto& client_parser = parser->getParser();
    if(client_parser.chunked)
    {
        std::string body;
        //剩余的长度
        int len = offset;
        //chunked 压缩。这里就不考虑流了，全部拿齐再返回
        do
        {
            do
            {
                int rt = read(data + len, buff_size - len);
                printf("\nrecive rt : %d\n", rt);
                SYLAR_LOG_INFO(g_logger) << "recive rt=" << rt << ", buff_size:" << buff_size
                    << ", len:" << len;
                if(rt <= 0)
                {
                    close();
                    return nullptr;
                }
                len += rt;
                //parser 要求
                data[len] = '\0';
                size_t nparse = parser->execute(data, len, true);
                if(parser->hasError())
                {
                    close();
                    return nullptr;
                }
                len -= nparse;
                if(len == (int) buff_size)
                {
                    close();
                    //一个都没解，有数据，但parse读不了
                    return nullptr;
                }
            } 
            //确保一定能拿到它的长度
            while(!parser->isFinished());

            //头部减去 \r\n 两个字符
            len -= 2;
            SYLAR_LOG_INFO(g_logger) << "content_len=" << client_parser.content_len << ", len=" << len;
            if(client_parser.content_len <= len)
            {
                //缓存里面还有多的，说明读到了下一个chunk，回退一下
                body.append(data, client_parser.content_len);
                memmove(data, data + client_parser.content_len
                    , len - client_parser.content_len);
                
                len -= client_parser.content_len;
            }
            else
            {
                body.append(data, len);
                int left = client_parser.content_len - len;
                while(left > 0)
                {
                    //数据都都走了，所以data直接顶头写了
                    int rt = read(data, left > (int)buff_size ? (int)buff_size : left);
                    if(rt <= 0)
                    {
                        close();
                        return nullptr;
                    }
                    body.append(data, rt);
                    left -= rt;
                }
                len = 0; //进入到下一个chunk去读
            }
            //进入到下一个chunk去读
        } while(!client_parser.chunks_done);

        parser->getData()->setBody(body);
    }
    else
    {
        //正常路径，读取length长度字节
        int64_t length = parser->getContentLength();
        if(length > 0)
        {
            std::string body;
            //保留大小
            //reserve 在readfixsize 时就不行了
            // body.reserve(length);
            body.resize(length);
            int len = 0;
            if(length >= offset)
            {
                //放进去解析剩下的 body
                // body.append(data, offset);
                memcpy(&body[0], data, offset);
                len = offset;
            }
            else
            {
                // body.append(data, length);
                memcpy(&body[0], data, length);
                len = length;
            }

            length -= offset;
            if(length > 0)
            {
                //没读完指定长度，继续读
                if(readFixSize(&body[len], length) <= 0)
                {
                    close();
                    return nullptr;
                }
            }
            //把body放进去
            parser->getData()->setBody(body);
        }
    }
    return parser->getData();
}

int HttpConnection::sendRequest(HttpRequest::ptr req)
{
    std::stringstream ss;
    ss << *req;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

//get 一般不加body，但严格意义上也没有说一定不能加
HttpResult::ptr HttpConnection::DoGet(const std::string& url
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body)
{
    Uri::ptr uri = Uri::Create(url);
    if(!uri)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                                            , nullptr
                                            , "invalid url: " + url);
    }

    return DoGet(uri, timeout_ms, headers, body);
}

//uri 很可能在请求开始的时候，就检测有无问题了。所以真正请求的时候，很可能已经有了现成的uri
//这样就不必再用string转换一次
HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body)
{
    return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(const std::string& url
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body)
{
    Uri::ptr uri = Uri::Create(url);
    if(!uri)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                                            , nullptr
                                            , "invalid url: " + url);
    }

    return DoPost(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body)
{
    return DoRequest(HttpMethod::POST, uri, timeout_ms, headers, body);
}

//前三个数都是经常用的
HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                                , const std::string& url
                                , uint64_t timeout_ms
                                //cookies 是 header 的一部分，就不抽出来了，不然太多
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body)
{
    Uri::ptr uri = Uri::Create(url);
    if(!uri)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                                            , nullptr
                                            , "invalid url: " + url);
    }

    return DoRequest(method, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                                , Uri::ptr uri
                                , uint64_t timeout_ms
                                //cookies 是 header 的一部分，就不抽出来了，不然太多
                                , const std::map<std::string, std::string>& headers
                                , const std::string& body)
{
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setPath(uri->getPath());
    req->setMethod(method);
    bool has_host = false;
    for(auto& i : headers)
    {
        if(strcasecmp(i.first.c_str(), "connection") == 0)
        {
            if(strcasecmp(i.second.c_str(), "keep-alive") == 0)
            {
                req->setClose(false);
            }
            continue;
        }

        if(!has_host && strcasecmp(i.first.c_str(), "host") == 0)
        {
            has_host = !i.second.empty();
        }

        req->setHeader(i.first, i.second);
    }
    if(has_host)
    {
        req->setHeader("Host", uri->getHost());
    }

    req->setBody(body);
    return DoRequest(req, uri, timeout_ms);
}

HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req
                                    , Uri::ptr uri //只提取出地址信息
                                    , uint64_t timeout_ms)
{
    Address::ptr addr = uri->createAddress();
    if(!addr)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST, nullptr
                                            , "invalid host: " + uri->getHost());
    }

    Socket::ptr sock = Socket::CreateTCP(addr);
    if(!sock)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL, nullptr
                                            , "create fail: " + addr->toString()
                                            + " errno=" + std::to_string(errno)
                                            + " errstr=" + std::string(strerror(errno)));
    }
    
    //先connect！
    if(!sock->connect(addr))
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL, nullptr
                                            , "connect fail: " + addr->toString());
    }

    sock->setRecvTimeout(timeout_ms);
    HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);

    int rt = conn->sendRequest(req);
    //没写进去，所以是远端关闭的
    if(rt == 0)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER, nullptr
                                            , "send request close by peer: " + addr->toString());
    }
    else if(rt < 0)
    {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr
                                            , "send request socket error, errno= " + std::to_string(errno)
                                                + " errstr=" + std::string(strerror(errno)));
    }

    auto rsp = conn->recvResponse();
    if(!rsp)
    {
        //先简单的认为都是超时。但其实还是要区分一下，是超时，还是被远端关闭的等等，很多错误类型
        return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT, nullptr
                                            , "recv response timeout: " + addr->toString()
                                                + " timeout_ms:" + std::to_string(timeout_ms));
    }

    return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
}

}
}
