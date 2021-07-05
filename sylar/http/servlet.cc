#include "servlet.h"
#include <fnmatch.h>

namespace sylar
{
namespace http
{
FunctionServlet::FunctionServlet(callback cb)
    :Servlet("FunctionServlet")
    ,m_cb(cb)
{
    
}

int32_t FunctionServlet::handle(HttpRequest::ptr request
            , HttpResponse::ptr response
            , HttpSession::ptr session)
{
    return m_cb(request, response, session);
}

ServletDispatch::ServletDispatch()
    :Servlet("ServletDispatch")
{
    m_default.reset(new NotFoundServlet());
}

int32_t ServletDispatch::handle(HttpRequest::ptr request
            , HttpResponse::ptr response
            , HttpSession::ptr session)
{
    auto slt = getMatchedServlet(request->getPath());
    if(slt)
    {
        slt->handle(request, response, session);
    }
    return 0;
}

void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri] = slt;
}

void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri].reset(new FunctionServlet(cb));
}

void ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr slt)
{
    RWMutexType::WriteLock lock(m_mutex);
    for(auto it = m_globs.begin();
            it != m_globs.end(); ++it)
    {
        if(it->first == uri)
        {
            m_globs.erase(it);
            break;
        }
    }

    m_globs.push_back(std::make_pair(uri, slt));
}

void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback cb)
{
    return addGlobServlet(uri, FunctionServlet::ptr(new FunctionServlet(cb)));
}

void ServletDispatch::delServlet(const std::string& uri)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string& uri)
{
    RWMutexType::WriteLock lock(m_mutex);
    for(auto it = m_globs.begin();
            it != m_globs.end(); ++it)
    {
        if(it->first == uri)
        {
            m_globs.erase(it);
            break;
        }
    }
}

Servlet::ptr ServletDispatch::getServlet(const std::string& uri)
{
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_datas.find(uri);
    return it == m_datas.end() ? nullptr : it->second;
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string& uri)
{
    RWMutexType::ReadLock lock(m_mutex);
    for(auto it = m_globs.begin(); it != m_globs.end(); ++it)
    {
        // if(it->first == uri)
        //模糊匹配
        if(!fnmatch(it->first.c_str(), uri.c_str(), 0))
        {
            return it->second;
        }
    }

    return nullptr;
}

//这个是综合上面两个 + default 匹配出来的
Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri)
{
    //上面的综合起来。为了性能考虑，就直接展开了
    RWMutexType::ReadLock lock(m_mutex);

    auto it = m_datas.find(uri);
    if(it != m_datas.end())
        return it->second;

    for(auto it = m_globs.begin();
        it != m_globs.end();
        it ++ )
    {
        if(!fnmatch(it->first.c_str(), uri.c_str(), 0))
        {
            return it->second;
        }
    }

    return m_default;
}

NotFoundServlet::NotFoundServlet()
    :Servlet("NotFoundServlet")
{

}

int32_t NotFoundServlet::handle(HttpRequest::ptr request
            , HttpResponse::ptr response
            , HttpSession::ptr session)
{
    static const std::string& RSP_BODY = "<html><head><title>404 Not Found"
        "</title></head><body><center><h1>404 Not Found</h1></center>"
        "<hr><center>custom-server/1.0.0</center></body></html>";

    response->setStatus(sylar::http::HttpStatus::NOT_FOUND);
    response->setHeader("Server", "custom-server/1.0.0");
    response->setHeader("Content-Type", "text/html");
    response->setBody(RSP_BODY);
    return 0;
}

}
}