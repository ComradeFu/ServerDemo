#include "http_parser.h"
#include "sylar/log.h"
#include "sylar/config.h"
#include <string.h>

namespace sylar
{
namespace http
{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
//防止收到过长的恶意头、包体，默认头4k，包体64M
static sylar::ConfigVar<uint64_t>::ptr g_http_request_buffer_size = 
    sylar::Config::Lookup("http.request.buffer_size", (uint64_t)(4 * 1024), "http request buffer size");
static sylar::ConfigVar<uint64_t>::ptr g_http_request_max_body_size = 
    sylar::Config::Lookup("http.request.max_body_size", (uint64_t)(64 * 1024 * 1024), "http request max body size");

static sylar::ConfigVar<uint64_t>::ptr g_http_response_buffer_size = 
    sylar::Config::Lookup("http.response.buffer_size", (uint64_t)(4 * 1024), "http response buffer size");
static sylar::ConfigVar<uint64_t>::ptr g_http_response_max_body_size = 
    sylar::Config::Lookup("http.response.max_body_size", (uint64_t)(64 * 1024 * 1024), "http response max body size");

//直接get value有锁的损耗，提前拿出来。这里不太需要线程安全
static uint64_t s_http_request_buffer_size = 0;     //head
static uint64_t s_http_request_max_body_size = 0;   //body
static uint64_t s_http_response_buffer_size = 0;     //head
static uint64_t s_http_response_max_body_size = 0;   //body

uint64_t HttpRequestParser::GetHttpRequestBufferSize()
{
    return s_http_request_buffer_size;
}

uint64_t HttpRequestParser::GetHttpRequestMaxBodySize()
{
    return s_http_request_max_body_size;
}

uint64_t HttpResponseParser::GetHttpResponseBufferSize()
{
    return s_http_response_buffer_size;
}

uint64_t HttpResponseParser::GetHttpResponseMaxBodySize()
{
    return s_http_response_max_body_size;
}

namespace 
{
//不污染命名空间
struct _RequestSizeIniter {
    _RequestSizeIniter()
    {
        s_http_request_buffer_size = g_http_request_buffer_size->getValue();
        s_http_request_max_body_size = g_http_request_max_body_size->getValue();
        s_http_response_buffer_size = g_http_response_buffer_size->getValue();
        s_http_response_max_body_size = g_http_response_max_body_size->getValue();

        g_http_request_buffer_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv)
            {
                s_http_request_buffer_size = nv;
            }
        );

        g_http_request_max_body_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv)
            {
                s_http_request_max_body_size = nv;
            }
        );

        g_http_response_buffer_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv)
            {
                s_http_response_buffer_size = nv;
            }
        );

        g_http_response_max_body_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv)
            {
                s_http_response_max_body_size = nv;
            }
        );
    }
};

static _RequestSizeIniter _init;
}

//element_cb
void on_reqeust_method(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    //这就是为什么要提供 char* 方法，因为这里如果统一是 string，还得复制一份
    HttpMethod m = CharsToHttpMethod(at);

    if(m == HttpMethod::INVALID_METHOD)
    {
        SYLAR_LOG_WARN(g_logger) << "invalid http request method: "
            << std::string(at, length);
        parser->setError(1000);
        return;
    }
    parser->getData()->setMethod(m);
}
void on_request_uri(void *data, const char *at, size_t length)
{
    //这个方法可以不做，因为其实是下面三个部分的组合
}
void on_request_fragment(void *data, const char *at, size_t length)
{
    //现代浏览器都会不发 fragment 其实，留作安全的纯客户端行为
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setFragment(std::string(at, length));
}
void on_request_path(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setPath(std::string(at, length));
}
void on_request_query_string(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setQuery(std::string(at, length));
}
void on_request_http_version(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    uint8_t v = 0;
    if(strncmp(at, "HTTP/1.1", length) == 0)
    {
        v = 0x11;
    }
    else if(strncmp(at, "HTTP/1.0", length) == 0) 
    {
        v = 0x10;
    }
    else
    {
        SYLAR_LOG_WARN(g_logger) << "invalid http request version: "
            << std::string(at, length);
        parser->setError(1001);
        return;
    }
    parser->getData()->setVersion(v);
}
void on_request_header_done(void *data, const char *at, size_t length)
{
    //暂时不做处理，也是下面的集合
    //HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
}

//field_cb
void on_request_http_field(void  *data, const char *field, size_t flen, const char *value, size_t vlen)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    if(flen == 0)
    {
        //key都没有
        SYLAR_LOG_WARN(g_logger) << "invalid http request field length == 0";
        // 设置了的话，会返回nullptr出去了，没有这么严重的必要
        // parser->setError(1002);
        return;
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}

HttpRequestParser::HttpRequestParser()
            :m_error(0)
{
    m_data.reset(new sylar::http::HttpRequest);
    //init 也就是重置
    http_parser_init(&m_parser);
    //一些回调的函数
    m_parser.request_method = on_reqeust_method;
    m_parser.request_uri = on_request_uri;
    m_parser.fragment = on_request_fragment;
    m_parser.request_path = on_request_path;
    m_parser.query_string = on_request_query_string;
    m_parser.http_version = on_request_http_version;
    m_parser.header_done = on_request_header_done;
    m_parser.http_field = on_request_http_field;
    m_parser.data = this; //c风格，回调的时候能在回调里能拿到
}

uint64_t HttpRequestParser::getContentLength()
{
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}

size_t HttpRequestParser::execute(char* data, size_t len)
{
    //真正的做解析，永远从0开始，简单粗暴
    size_t offset = http_parser_execute(&m_parser, data, len, 0);
    //没有解析完，把解析过的给 mov，剩下的未解析部分留着
    memmove(data, data + offset, (len - offset));
    return offset;
}

//http 可能要tcp来回几次的报文才会接收完整，所以并不会马上能完成解析
//可能是网络问题，可能是数据很大
//所以一般都是一种有限状态机去做，顺序解析，看看是否完成
int HttpRequestParser::isFinished()
{
    return http_parser_finish(&m_parser);
}

int HttpRequestParser::hasError()
{
    return m_error || http_parser_has_error(&m_parser);
}

// ResponseParser
void on_response_reason(void *data, const char *at, size_t length)
{
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    parser->getData()->setReason(std::string(at, length));
}
void on_response_status(void *data, const char *at, size_t length)
{
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    HttpStatus status = (HttpStatus)(atoi(at)); //int 简单转换
    parser->getData()->setStatus(status);
}
void on_response_chunk(void *data, const char *at, size_t length)
{
    
}
void on_response_version(void *data, const char *at, size_t length)
{
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    uint8_t v = 0;
    if(strncmp(at, "HTTP/1.1", length) == 0)
    {
        v = 0x11;
    }
    else if(strncmp(at, "HTTP/1.0", length) == 0) 
    {
        v = 0x10;
    }
    else
    {
        SYLAR_LOG_WARN(g_logger) << "invalid http response version: "
            << std::string(at, length);
        parser->setError(1001);
        return;
    }
    parser->getData()->setVersion(v);
}
void on_response_header_done(void *data, const char *at, size_t length)
{
    
}
void on_response_last_chunk(void *data, const char *at, size_t length)
{
    
}
//field_cb
void on_response_http_field(void  *data, const char *field, size_t flen, const char *value, size_t vlen)
{
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(data);
    if(flen == 0)
    {
        //key都没有
        SYLAR_LOG_WARN(g_logger) << "invalid http response field length == 0";
        // 设置了的话，会返回null出去了。
        // parser->setError(1002);
        return;
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}
HttpResponseParser::HttpResponseParser()
    :m_error(0)
{
    m_data.reset(new sylar::http::HttpResponse);
    httpclient_parser_init(&m_parser);
    m_parser.reason_phrase = on_response_reason;
    m_parser.status_code = on_response_status;
    m_parser.chunk_size = on_response_chunk;
    m_parser.http_version = on_response_version;
    m_parser.header_done = on_response_header_done;
    m_parser.last_chunk = on_response_last_chunk;
    m_parser.http_field = on_response_http_field;
    m_parser.data = this;
}

uint64_t HttpResponseParser::getContentLength()
{
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}

size_t HttpResponseParser::execute(char* data, size_t len, bool chunck)
{
    if(chunck)
    {
        //重新初始化一次它的数据，全部清空
        httpclient_parser_init(&m_parser);
    }
    //真正的做解析，永远从0开始，简单粗暴
    size_t offset = httpclient_parser_execute(&m_parser, data, len, 0);
    //没有解析完，把解析过的给 mov，剩下的未解析部分留着
    memmove(data, data + offset, (len - offset));
    return offset;
}

int HttpResponseParser::isFinished()
{   
    return httpclient_parser_finish(&m_parser);
}

int HttpResponseParser::hasError()
{
    return m_error || httpclient_parser_has_error(&m_parser);
}

}
}
