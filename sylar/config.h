#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <string>
#include <sstream> //序列化用
#include <boost/lexical_cast.hpp>
#include "log.h"

namespace sylar {

class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string& name, const std::string& description = "")
        :m_name(name)
        ,m_description(description) {

    }
    //虚析构函数，让它释放
    virtual ~ConfigVarBase() {}

    const std::string& getName() const { return m_name; }
    const std::string& getDescription() const { return m_description; }

    //输出整个配置文件，是有利于调试什么的
    virtual std::string toString() = 0;
    //解析
    virtual bool fromString(const std::string& val) = 0;
protected: //基类能遗传
    std::string m_name;
    std::string m_description;
};

//下面是很基础很基础的类型，string 型，int 型
template<class T>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    ConfigVar(const std::string& name, const T& default_value, const std::string& description = "") 
        :ConfigVarBase(name, description)
        ,m_val(default_value) {

    }

    //输出整个配置文件，是有利于调试什么的 override 是让编辑器帮忙检查一次，确实是虚继承过来的方法
    virtual std::string toString() override {
        try
        {
            return boost::lexical_cast<std::string>(m_val);
        }
        catch(const std::exception& e)
        {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
                << e.what() << " convert: " << typeid(m_val).name() << " to string";
        }
        
        return "";
    }
    //解析
    virtual bool fromString(const std::string& val) override {
        try 
        {
            m_val = boost::lexical_cast<T>(val);
        }
        catch(std::exception& e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
                << e.what() << " convert string to " << typeid(m_val).name();
        }

        return false;
    }

    const T getValue() const { return m_val; }
    void setValue(const T& v) { m_val = v; }
private:
    T m_val;
};

//管理的类
class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    //两种用法

    //没有定义的时候就初始化
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,
            const T& default_value, const std::string& description = "") {
        auto tmp = Lookup<T>(name);
        if(tmp) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
            return tmp;
        }

        //判断是不是非法，也可以大写统一转换小写
        if(name.find_first_not_of("abcdefghijklmopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._012345678")
                != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        s_datas[name] = v;

        return v;
    }

    //直接查找
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
        auto it = s_datas.find(name);
        if(it == s_datas.end())
            return nullptr;
        //找到了还要去转成对应的类型
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }
private:
    //singleton, so use s_ for prefix
    static ConfigVarMap s_datas;
};

}

#endif
