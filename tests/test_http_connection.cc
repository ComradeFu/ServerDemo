#include <iostream>
#include "sylar/http/http_connection.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include <fstream>

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run()
{
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80");
    if(!addr)
    {
        SYLAR_LOG_INFO(g_logger) << "get addr error";
        return;
    }

    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    SYLAR_LOG_INFO(g_logger) << "start connect !";
    bool rt = sock->connect(addr);
    if(!rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "connet " << *addr << "fail.";
        return;
    }

    sylar::http::HttpConnection::ptr conn(new sylar::http::HttpConnection(sock));
    sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);
    // req->setPath("/blog/");
    req->setHeader("host", "www.baidu.com");

    SYLAR_LOG_INFO(g_logger) << "req:" << std::endl
        << *req;

    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if(!rsp)
    {
        SYLAR_LOG_INFO(g_logger) << "recv response error";
        return;
    }

    SYLAR_LOG_INFO(g_logger) << "rsp:" << std::endl
        << *rsp;

    std::ofstream ofs("rsp.dat");
    ofs << *rsp;

    SYLAR_LOG_INFO(g_logger) << "------------------------------------------";

    auto rt2 = sylar::http::HttpConnection::DoGet("http://www.sylar.top/", 300);
    SYLAR_LOG_INFO(g_logger) << "result=" << rt2->result
        << " error=" << rt2->error
        << " rsp=" << (rt2->response ? rt2->response->toString() : "");
}

int main(int argc, char** argv)
{
    sylar::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
