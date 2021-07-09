#include "http_server.h"
#include "sylar/log.h"

namespace sylar
{
namespace http
{
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, sylar::IOManager* worker ,sylar::IOManager* accept_worker)
            :TcpServer(worker, accept_worker)
            ,m_isKeepalive(keepalive)
{
    m_dispatch.reset(new ServletDispatch);
}

//重点实现HandleClient
void HttpServer::handleClient(Socket::ptr client)
{
    HttpSession::ptr session(new HttpSession(client));
    do
    {
        auto req = session->recvRequest();
        if(!req)
        {
            //恶意攻击的比较多的话，也不至于是错误先
            SYLAR_LOG_WARN(g_logger) << "recv http request fail, errno="
                    << errno << " errorstr=" << strerror(errno)
                    << " client:" << *client << " keep_alive=" << m_isKeepalive;
            break;
        }

        //就算请求的是 keep alive，服务器不支持，也不行
        HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClose() || !m_isKeepalive));
        m_dispatch->handle(req, rsp, session);

        // rsp->setBody("hello !!!!");

        // SYLAR_LOG_INFO(g_logger) << "request:" << std::endl
        //     << * req;
        // SYLAR_LOG_INFO(g_logger) << "response:" << std::endl
        //     << *rsp;

        //不在 servlet 里发送，因为可能 servlet 会有多层的处理
        //处理好之后再统一发送
        //传入 session 只是用来做上下文的判断，比如cookie，或者session（http意义上的）
        session->sendResponse(rsp);

        if(!m_isKeepalive || req->isClose()) {
            break;
        }

    } while(true);

    session->close();
}

}

}