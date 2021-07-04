#include "sylar/http/http_server.h"
#include "sylar/iomanager.h"
#include "sylar/log.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run()
{
    sylar::http::HttpServer::ptr server(new sylar::http::HttpServer);
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("0.0.0.0:8001");
    while(!server->bind(addr))
    {
        sleep(2);
    }
    auto sd = server->getServletDispatch();
    sd->addServlet("/hello/echo_header", [](sylar::http::HttpRequest::ptr req
                                , sylar::http::HttpResponse::ptr rsp
                                , sylar::http::HttpSession::ptr session)
    {
        SYLAR_LOG_INFO(g_logger) << "handler request";
        rsp->setBody(req->toString());
        return 0;
    });

    sd->addGlobServlet("/hello/*", [](sylar::http::HttpRequest::ptr req
                                , sylar::http::HttpResponse::ptr rsp
                                , sylar::http::HttpSession::ptr session)
    {
        SYLAR_LOG_INFO(g_logger) << "handler glob request";
        rsp->setBody("Glob:\r\n" + req->toString());
        return 0;
    });
    server->start();
}

int main(int argc, char** argv)
{
    sylar::IOManager iom(1);
    iom.schedule(run);
    return 0;
}
