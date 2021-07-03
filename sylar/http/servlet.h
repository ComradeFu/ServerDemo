#ifndef __SYLAR_HTTP_SERVLET_H__
#define __SYLAR_HTTP_SERVLET_H__

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include "http.h"
#include "http_session.h"
#include "sylar/thread.h"

//类似Koa
//仿照JAVA的Servlet

namespace sylar
{
namespace http
{

class Servlet
{
public:
    typedef std::shared_ptr<Servlet> ptr;
    Servlet(const std::string& name)
        :m_name(name) {}
    virtual ~Servlet() {}
    //真正处理url请求的方法。对应的 servlet 一定要重载
    virtual int32_t handle(HttpRequest::ptr request
                , HttpResponse::ptr response
                , HttpSession::ptr session) = 0;

    const std::string& getName() const { return m_name; }
protected:
    std::string m_name;
};

//使用function的通用 servlet，这样就可以不用非要继承，才能自定义 servlet 的处理方式了
class FunctionServlet : public Servlet
{
public:
    typedef std::shared_ptr<FunctionServlet> ptr;
    typedef std::function<int32_t (sylar::http::HttpRequest::ptr request
                , sylar::http::HttpResponse::ptr response
                , sylar::http::HttpSession::ptr session)> callback;

    FunctionServlet(callback cb);
    int32_t handle(sylar::http::HttpRequest::ptr request
                , sylar::http::HttpResponse::ptr response
                , sylar::http::HttpSession::ptr session) override;
private:
    callback m_cb;
};

//也是一种特殊的servlet，handle 不是具体处理，而是决定是哪个servlet进行处理
class ServletDispatch : public Servlet
{
public:
    typedef std::shared_ptr<ServletDispatch> ptr;
    typedef RWMutex RWMutexType;

    ServletDispatch();
    int32_t handle(sylar::http::HttpRequest::ptr request
                , sylar::http::HttpResponse::ptr response
                , sylar::http::HttpSession::ptr session) override;

    void addServlet(const std::string& uri, Servlet::ptr slt);
    void addServlet(const std::string& uri, FunctionServlet::callback cb);
    void addGlobServlet(const std::string& uri, Servlet::ptr slt);
    void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);

    void delServlet(const std::string& uri);
    void delGlobServlet(const std::string& uri);

    Servlet::ptr getServlet(const std::string& uri);
    Servlet::ptr getGlobServlet(const std::string& uri);

    Servlet::ptr getDefault() const { return m_default; }
    //有线程安全问题，但先认为不会在运行时变化
    void setDefault(Servlet::ptr v) { m_default = v; }

    //这个是综合上面两个 + default 匹配出来的
    Servlet::ptr getMatchedServlet(const std::string& uri);
private:
    RWMutexType m_mutex;
    //url, servlet，精准匹配，比如 /a/b/c
    std::unordered_map<std::string, Servlet::ptr> m_datas;
    //URL 模糊匹配，比如 /a/*，会优先命中上面的精准
    std::vector<std::pair<std::string, Servlet::ptr>> m_globs;
    //默认servlet，所有路径都没有匹配到的情况下，使用
    Servlet::ptr m_default;
};

//404 处理
class NotFoundServlet : public Servlet
{
public:
    typedef std::shared_ptr<NotFoundServlet> ptr;
    NotFoundServlet();
    int32_t handle(sylar::http::HttpRequest::ptr request
                , sylar::http::HttpResponse::ptr response
                , sylar::http::HttpSession::ptr session) override;
};

//以后可能还可以提供，上传、下载服务Servlet
//再比如，请求了一个HTML文件，我们把对应的文件丢过去（servlet uri ： *.html）
//请求了一个php文件，那么就要有对应的 Servlet 把请求传给cgi，渲染好再返回
//这样就很有nginx的样子了
}
}

#endif
