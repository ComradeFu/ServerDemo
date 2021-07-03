#ifndef __SYALR_HTTP_PARSER_H__
#define __SYALR_HTTP_PARSER_H__

#include "http.h"
//文件夹下 http11 和 client 是用的开源工具，利用有限状态机，用正则表达式来做字符解析
//编译成自己的语言，比直接汇编写都不差！
//做协议解析性能是很高的 mongrel2（ragel -G2 -C http11_parser.rl -o http11_parser.cc)
//这个指令专程的 c 文件效率贼高，基本都是 goto
// -C c语言，还可以指定别的
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace sylar
{
namespace http
{

class HttpRequestParser
{
public:
    typedef std::shared_ptr<HttpRequestParser> ptr;
    HttpRequestParser();
    size_t execute(char* data, size_t len);
    int isFinished();
    int hasError();

    HttpRequest::ptr getData() const { return m_data; }
    void setError(int v) { m_error = v; }

    uint64_t getContentLength();
    const http_parser& getParser() { return m_parser; }
public:
    static uint64_t GetHttpRequestBufferSize();
    static uint64_t GetHttpRequestMaxBodySize();
private:
    http_parser m_parser; //状态机的结构体
    HttpRequest::ptr m_data;
    //1000: invalid method
    //1001: invalid version
    //1002: invalid field
    int m_error; //判断是否有错误
};

class HttpResponseParser
{
public:
    typedef std::shared_ptr<HttpResponseParser> ptr;
    HttpResponseParser();
    size_t execute(char* data, size_t len, bool chunck);
    int isFinished();
    int hasError();

    HttpResponse::ptr getData() const { return m_data; }
    void setError(int v) { m_error = v; }

    uint64_t getContentLength();
    const httpclient_parser& getParser() { return m_parser; }
public:
    static uint64_t GetHttpResponseBufferSize();
    static uint64_t GetHttpResponseMaxBodySize();
private:
    httpclient_parser m_parser; //状态机的结构体
    HttpResponse::ptr m_data;
    
    int m_error; //判断是否有错误
};

}
}

#endif
