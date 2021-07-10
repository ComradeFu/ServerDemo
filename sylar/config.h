#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <string>
#include <sstream> //序列化用
#include <boost/lexical_cast.hpp>
#include "log.h"
#include <yaml-cpp/yaml.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional> //C++11 新增加支持

#include "thread.h"
#include "log.h"

namespace sylar
{

    class ConfigVarBase
    {
    public:
        typedef std::shared_ptr<ConfigVarBase> ptr;
        ConfigVarBase(const std::string &name, const std::string &description = "")
            : m_name(name), m_description(description)
        {
            // key 只有小写，没有大写
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }
        //虚析构函数，让它释放
        virtual ~ConfigVarBase() {}

        const std::string &getName() const { return m_name; }
        const std::string &getDescription() const { return m_description; }

        //输出整个配置文件，是有利于调试什么的
        virtual std::string toString() = 0;
        //解析
        virtual bool fromString(const std::string &val) = 0;
        virtual std::string getTypeName() const = 0; //不定义出来，派生类特有方法，基类指针将访问不到！
    protected:                                       //基类能遗传
        std::string m_name;
        std::string m_description;
    };

    //F from_type, T to_type
    template <class F, class T>
    class LexicalCast
    {
    public:
        T operator()(const F &v)
        {
            return boost::lexical_cast<T>(v);
        }
    };

    //常用内置复杂结构的偏特化，那么这里的命名Lexical可能不太准确，没关系了就这样吧。。
    template <class T>
    class LexicalCast<std::string, std::vector<T>>
    {
    public:
        std::vector<T> operator()(const std::string &v)
        {
            //直接用YAML吧。。
            YAML::Node node = YAML::Load(v);

            //必然是一个数组，否则直接会自己抛出一个异常，catch
            std::vector<T> vec;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); ++i)
            {
                ss.str("");
                ss << node[i];
                //下面为什么不用typename呢？因为它是模板类的子类型，所以不用，说来话又长了。。
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));
            }

            //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
            return vec;
        }
    };

    template <class T>
    class LexicalCast<std::vector<T>, std::string>
    {
    public:
        std::string operator()(const std::vector<T> &v)
        {
            //直接用YAML吧。。
            YAML::Node node;
            for (auto &i : v)
            {
                //自己会转成sequence类型，变成YAML节点放回去
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }

            std::stringstream ss;
            ss << node; //利用node自己的 <<

            return ss.str();
        }
    };

    //常用内置复杂结构的偏特化，那么这里的命名Lexical可能不太准确，没关系了就这样吧。。
    template <class T>
    class LexicalCast<std::string, std::list<T>>
    {
    public:
        std::list<T> operator()(const std::string &v)
        {
            //直接用YAML吧。。
            YAML::Node node = YAML::Load(v);

            //必然是一个数组，否则直接会自己抛出一个异常，catch
            std::list<T> vec;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); ++i)
            {
                ss.str("");
                ss << node[i];
                //下面为什么不用typename呢？因为它是模板类的子类型，所以不用，说来话又长了。。
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));
            }

            //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
            return vec;
        }
    };

    template <class T>
    class LexicalCast<std::list<T>, std::string>
    {
    public:
        std::string operator()(const std::list<T> &v)
        {
            //直接用YAML吧。。
            YAML::Node node;
            for (auto &i : v)
            {
                //自己会转成sequence类型，变成YAML节点放回去
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }

            std::stringstream ss;
            ss << node; //利用node自己的 <<

            return ss.str();
        }
    };

    //常用内置复杂结构的偏特化，那么这里的命名Lexical可能不太准确，没关系了就这样吧。。
    template <class T>
    class LexicalCast<std::string, std::set<T>>
    {
    public:
        std::set<T> operator()(const std::string &v)
        {
            //直接用YAML吧。。
            YAML::Node node = YAML::Load(v);

            std::set<T> vec;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); ++i)
            {
                ss.str("");
                ss << node[i];
                //下面为什么不用typename呢？因为它是模板类的子类型，所以不用，说来话又长了。。
                vec.insert(LexicalCast<std::string, T>()(ss.str()));
            }

            //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
            return vec;
        }
    };

    template <class T>
    class LexicalCast<std::set<T>, std::string>
    {
    public:
        std::string operator()(const std::set<T> &v)
        {
            //直接用YAML吧。。
            YAML::Node node;
            for (auto &i : v)
            {
                //自己会转成sequence类型，变成YAML节点放回去
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }

            std::stringstream ss;
            ss << node; //利用node自己的 <<

            return ss.str();
        }
    };

    //常用内置复杂结构的偏特化，那么这里的命名Lexical可能不太准确，没关系了就这样吧。。
    template <class T>
    class LexicalCast<std::string, std::unordered_set<T>>
    {
    public:
        std::unordered_set<T> operator()(const std::string &v)
        {
            //直接用YAML吧。。
            YAML::Node node = YAML::Load(v);

            std::unordered_set<T> vec;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); ++i)
            {
                ss.str("");
                ss << node[i];
                //下面为什么不用typename呢？因为它是模板类的子类型，所以不用，说来话又长了。。
                vec.insert(LexicalCast<std::string, T>()(ss.str()));
            }

            //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
            return vec;
        }
    };

    template <class T>
    class LexicalCast<std::unordered_set<T>, std::string>
    {
    public:
        std::string operator()(const std::unordered_set<T> &v)
        {
            //直接用YAML吧。。
            YAML::Node node;
            for (auto &i : v)
            {
                //自己会转成sequence类型，变成YAML节点放回去
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }

            std::stringstream ss;
            ss << node; //利用node自己的 <<

            return ss.str();
        }
    };

    //map 稍微复杂许多。。
    template <class T>
    class LexicalCast<std::string, std::map<std::string, T>>
    {
    public:
        std::map<std::string, T> operator()(const std::string &v)
        {
            //直接用YAML吧。。
            YAML::Node node = YAML::Load(v);

            std::map<std::string, T> vec;
            std::stringstream ss;
            for (auto it = node.begin();
                 it != node.end(); ++it)
            {
                ss.str(""); //重置
                ss << it->second;
                //目前暂且只支持简单的 字符串key
                vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
            }

            //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
            return vec;
        }
    };

    template <class T>
    class LexicalCast<std::map<std::string, T>, std::string>
    {
    public:
        std::string operator()(const std::map<std::string, T> &v)
        {
            //直接用YAML吧。。
            YAML::Node node;
            for (auto &i : v)
            {
                //自己会转成Map类型，变成YAML节点放回去
                node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
            }

            std::stringstream ss;
            ss << node; //利用node自己的 <<

            return ss.str();
        }
    };

    //unordered_map
    template <class T>
    class LexicalCast<std::string, std::unordered_map<std::string, T>>
    {
    public:
        std::unordered_map<std::string, T> operator()(const std::string &v)
        {
            //直接用YAML吧。。
            YAML::Node node = YAML::Load(v);

            std::unordered_map<std::string, T> vec;
            std::stringstream ss;
            for (auto it = node.begin();
                 it != node.end(); ++it)
            {
                ss.str(""); //重置
                ss << it->second;
                //目前暂且只支持简单的 字符串key
                vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
            }

            //std::move(vec); 让vec不要这么快销毁，直接传到外面，不用拷贝。不过c++11应该是自己优化了，不需要了
            return vec;
        }
    };

    template <class T>
    class LexicalCast<std::unordered_map<std::string, T>, std::string>
    {
    public:
        std::string operator()(const std::unordered_map<std::string, T> &v)
        {
            //直接用YAML吧。。
            YAML::Node node;
            for (auto &i : v)
            {
                //自己会转成Map类型，变成YAML节点放回去
                node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
            }

            std::stringstream ss;
            ss << node; //利用node自己的 <<

            return ss.str();
        }
    };

    //下面是很基础很基础的类型，string 型，int 型等，用bosst库直接转换
    //新增一些复杂类型。复杂类型分两种，Vector、Map 这种也算简单的复杂，还有就是自定义结构体了，所以需要
    //FromStr ToStr，就是 “仿函数”类（重载了函数括号的类），序列化跟反序列化
    //FromStr T operator()(const std::string&)
    //ToStr std::string operator()(const T&)，得是一个模板
    //下面用到的默认仿函数，是template 特例化
    template <class T,
              class FromStr = LexicalCast<std::string, T>,
              class ToStr = LexicalCast<T, std::string>>
    class ConfigVar : public ConfigVarBase
    {
    public:
        //配置是典型的写少读多
        typedef RWMutex RWMutexType;
        typedef std::shared_ptr<ConfigVar> ptr;
        //使用类似智能指针，变化回调
        typedef std::function<void(const T &old_value, const T &new_value)> on_change_cb;

        ConfigVar(const std::string &name, const T &default_value, const std::string &description = "")
            : ConfigVarBase(name, description), m_val(default_value)
        {
        }

        //输出整个配置文件，是有利于调试什么的 override 是让编辑器帮忙检查一次，确实是虚继承过来的方法
        virtual std::string toString() override
        {
            try
            {
                RWMutexType::ReadLock lock(m_mutex);

                // return boost::lexical_cast<std::string>(m_val);
                return ToStr()(m_val); //ToStr(m_val)
            }
            catch (const std::exception &e)
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
                                                  << e.what() << " convert: " << typeid(m_val).name() << " to string";
            }

            return "";
        }
        //解析
        virtual bool fromString(const std::string &val) override
        {
            try
            {
                // m_val = boost::lexical_cast<T>(val);
                // set value 已经加锁
                setValue(FromStr()(val));
            }
            catch (std::exception &e)
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromString exception"
                                                  << e.what() << " convert string to " << typeid(m_val).name()
                                                  << " - " << val;
            }

            return false;
        }

        //once use lock, no more const
        const T getValue() 
        { 
            RWMutexType::ReadLock lock(m_mutex);
            return m_val; 
        }

        void setValue(const T &v)
        {
            {
                RWMutexType::ReadLock lock(m_mutex);
                if (v == m_val)
                {
                    return;
                }

                for (auto &i : m_cbs)
                {
                    i.second(m_val, v);
                }
            }
            
            RWMutexType::WriteLock lock(m_mutex);
            m_val = v;
        }

        std::string getTypeName() const override { return typeid(T).name(); }

        uint64_t addListener(on_change_cb cb)
        {
            static uint64_t s_fun_id = 0;
            RWMutexType::WriteLock lock(m_mutex);

            ++s_fun_id;

            m_cbs[s_fun_id] = cb;

            return s_fun_id;
        }

        void delListener(uint64_t key)
        {
            RWMutexType::WriteLock lock(m_mutex);
            m_cbs.erase(key);
        }

        on_change_cb getListener(uint64_t key)
        {
            RWMutexType::ReadLock lock(m_mutex);
            auto it = m_cbs.find(key);
            return it == m_cbs.end() ? nullptr : it->second;
        }

        void clearListener()
        {
            RWMutexType::WriteLock lock(m_mutex);
            m_cbs.clear();
        }

    private:
        RWMutexType m_mutex;
        T m_val;

        //变更回调函数组；为什么用map，是functional没有比较函数，因此它放在容器里面是没有办法被找出来的
        //因此需要一个int来进行 id 包装一层 uint64_t key, 要求唯一，一般可以用hash
        std::map<uint64_t, on_change_cb> m_cbs;
    };

    //管理的类
    class Config
    {
    public:
        // typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
        //写到了 unordered_map ，干脆就换了算了。。
        typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
        //也是读多写少的情况
        typedef RWMutex RWMutexType;

        //两种用法

        //没有定义的时候就初始化
        //模板在实例化之前并不知道类下面的是什么的东西（嵌套模板的情况下），使用 typename 可以让定义确定下来。直接在编译时就知道
        //如果不用 typename，那么编译器可能会有潜在的解析二义性
        //既然都到这里了，就展开讲一下，为什么这里会有二义性吧，权当自己做个笔记，反正这代码估计也只有自己看。。
        //ClassA::foo ---> 一个类的静态变量
        //ClassA::foo a ---> 两种可能，a 是一个 ClassA::foo 的变量类型，ClassA::foo 是一个内部类；
        //或者说，也可以是 foo 是 ClassA 里的一个 typedef（或者说，编译期的“macro”操作）。
        //于是有了二义性。
        template <class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string &name,
                                                 const T &default_value, const std::string &description = "")
        {
            //粗暴一点加个写锁吧
            RWMutexType::WriteLock lock(GetMutex());

            //tmp 是空的的话，两种情况。一种是真的是空，另一种是类型不一样。
            // auto tmp = Lookup<T>(name);
            // if(tmp) {
            //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
            //     return tmp;
            // }
            auto& s_datas = GetDatas();
            auto it = s_datas.find(name);
            // 这个才是真的没有
            if (it != s_datas.end())
            {
                auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
                if (tmp)
                {
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
                    return tmp;
                }
                else
                {
                    SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name=" << name << "exists but type not "
                                                      << typeid(T).name() << " real_type=" << it->second->getTypeName()
                                                      << " " << it->second->toString();
                    return nullptr;
                }
            }

            //判断是不是非法，也可以大写统一转换小写
            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._012345678") != std::string::npos)
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
                throw std::invalid_argument(name);
            }

            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
            s_datas[name] = v;

            return v;
        }

        //直接查找
        template <class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string &name)
        {
            RWMutexType::ReadLock lock(GetMutex());

            auto& s_datas = GetDatas();
            auto it = s_datas.find(name);
            if (it == s_datas.end())
                return nullptr;
            //找到了还要去转成对应的类型（从base类型指针）
            return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
        }

        static void LoadFromYaml(const YAML::Node &root);
        static void LoadFromConfDir(const std::string &path);

        //不需要T的Base方法
        static ConfigVarBase::ptr LookupBase(const std::string &name);

        //调试方法，查看某个配置
        static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
    private:
        //singleton, so use s_ for prefix
        // static ConfigVarMap s_datas;
        //下面改用成一个统一的函数进行返回
        //原因是静态变量的初始化顺序，不同编译器是没有直接决定的
        //所以在外界的init struct 的构造函数里使用 Lookup 的静态方法的时候
        //s_datas 未必就已经初始化好了
        //所以改成再用一层函数返回一个在函数中再定义的一个 “局部静态变量”，这样顺序就是固定的了
        static ConfigVarMap &GetDatas()
        {
            static ConfigVarMap s_datas;
            return s_datas;
        }

        //这么做的理由同上。全局静态变量的初始化，是没有严格的顺序的
        static RWMutexType& GetMutex()
        {
            static RWMutexType s_mutex;
            return s_mutex;
        }
    };

}

#endif
