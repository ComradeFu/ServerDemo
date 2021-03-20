#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <string>
#include <sstream> //序列化用
#include <boost/lexical_cast.hpp>
#include "log.h"
#include <yaml-cpp/yaml.h>

namespace sylar {

class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string& name, const std::string& description = "")
        :m_name(name)
        ,m_description(description) {
            // key 只有小写，没有大写
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
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

//F from_type, T to_type
template<class F, class T>
class LexicalCast {
public:
    T operator()(const F& v)
    {
        return boost::lexical_cast<T>(v);
    }
};

//常用内置复杂结构的偏特化，那么这里的命名Lexical可能不太准确，没关系了就这样吧。。
template<class T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator()(const std::string& v)
    {
        //直接用YAML吧。。
        YAML::Node node = YAML::Load(v);

        //必然是一个数组，否则直接会自己抛出一个异常，catch
        //模板在实例化之前并不知道 std::vector<T> 是什么的东西（嵌套模板的情况下），使用 typename 可以让定义确定下来。直接在编译时就知道
        //如果不用 typename，那么编译器可能会有潜在的解析二义性
        //既然都到这里了，就展开讲一下，为什么这里会有二义性吧，权当自己做个笔记，反正这代码估计也只有自己看。。
        //ClassA::foo ---> 一个类的静态变量
        //ClassA::foo a ---> 两种可能，a 是一个 ClassA::foo 的变量类型，ClassA::foo 是一个内部类；
        //或者说，也可以是 foo 是 ClassA 里的一个 typedef（或者说，编译期的“macro”操作）。
        //于是有了二义性。
        //当然了，如果不是 typedef 而是新的模板的写法，理论上来说是不会有二义性。但是旧版本的编译器都是统一这么处理。
        //所以不用 typename 的时候，新版本的编译器会聪明的给通过，但是会警告，说这种代码可能有迁移性的问题。
        typename std::Vecotr<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i)
        {
            ss.str("");
            ss << node[i];
            //下面为什么不用typename呢？因为它是模板类的子类型，所以不用，说来话又长了。。
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }

        //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
        return vec;
    }
}

template<class T>
class LexicalCast<std::vector<T>, std::string> {
public:
   std::string operator()(const typename std::vector<T>& v)
    {
        //直接用YAML吧。。
        YAML::Node node;
        for(auto& i : v)
        {
            //自己会转成sequence类型，变成YAML节点放回去
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }

        std::stringstream ss;
        ss << node; //利用node自己的 <<

        return ss.str()
    }
}

//下面是很基础很基础的类型，string 型，int 型等，用bosst库直接转换
//新增一些复杂类型。复杂类型分两种，Vector、Map 这种也算简单的复杂，还有就是自定义结构体了，所以需要
//FromStr ToStr，就是 “仿函数”类（重载了函数括号的类），序列化跟反序列化
//FromStr T operator()(const std::string&)
//ToStr std::string operator()(const T&)，得是一个模板
//下面用到的默认仿函数，是template 特例化
template<class T, 
    class FromStr = LexicalCast<std::string, T>, 
    class ToStr = LexicalCast<T, std::string>>
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
            // return boost::lexical_cast<std::string>(m_val);
            return ToStr()(m_val); //ToStr(m_val)
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
            // m_val = boost::lexical_cast<T>(val);
            setValue(FromStr()(val));
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
        if(name.find_first_not_of("abcdefghijklmopqrstuvwxyz._012345678")
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

    static void LoadFromYaml(const YAML::Node& root);
    static ConfigVarBase::ptr LookupBase(const std::string& name);

private:
    //singleton, so use s_ for prefix
    static ConfigVarMap s_datas;
};

}

#endif
