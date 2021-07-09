#ifndef __SYLAR_URI_H__
#define __SYLAR_URI_H__

//url 只是 uri 的一种实现。我们用的时候就直接用 uri 就行了
//因为这会涉及到以后的协议设计。比如可以通过uri的形式实现一些服务
//可以参考 ietf 文件

//这个需求，是从 http connection 里出来的。因为有时候我们想直接根据 uri 去请求对应的资源

/*
//虽然文档里没有 user@ ，但标准其实是有的
     foo://user@sylar.com:8042/over/there?name=ferret#nose
       \_/   \______________/\_________/ \_________/ \__/
        |           |            |            |        |
     scheme     authority       path        query   fragment
*/

#include <memory>
#include <string>
//size_t
#include <stdint.h>
#include "address.h"

namespace sylar
{
class Uri
{
public:
    typedef std::shared_ptr<Uri> ptr;

    static Uri::ptr Create(const std::string& uri);
    Uri();

    const std::string& getScheme() const { return m_scheme; }
    const std::string& getUserinfo() const { return m_userinfo; }
    const std::string& getHost() const { return m_host; }
    const std::string& getPath() const;
    const std::string& getQuery() const { return m_query;}
    const std::string& getFragment() const { return m_fragment; }
    int32_t getPort() const;

    void setScheme(const std::string& v) { m_scheme = v; }
    void setUserinfo(const std::string& v) { m_userinfo = v; }
    void setHost(const std::string& v) { m_host = v; }
    void setPath(const std::string& v) { m_path = v;}
    void setQuery(const std::string& v) { m_query = v; }
    void setFragment(const std::string& v) { m_fragment = v; }
    void setPort(int32_t v) { m_port = v; }

    std::ostream& dump(std::ostream& os) const;
    std::string toString() const;

    //用host，创建出地址
    Address::ptr createAddress() const;
private:
    //不同的 scheme 有不同的默认端口
    bool isDefaultPort() const;
private:
    //跟着上面的文档定义
    std::string m_scheme;
    std::string m_userinfo;
    std::string m_host;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    int32_t m_port;
};
}

#endif
