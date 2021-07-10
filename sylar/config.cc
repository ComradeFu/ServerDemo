#include "config.h"
#include "sylar/env.h"
#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

//static 成员变量必须在类外初始化一次
//见头文件的注释
// Config::ConfigVarMap Config::s_datas;

//查找当前有无这个配置项，有的话返回它，这个方法特殊在，不需要T，直接Base指针
ConfigVarBase::ptr Config::LookupBase(const std::string& name)
{
    RWMutexType::ReadLock lock(GetMutex());

    auto& s_datas = GetDatas();
    auto it = s_datas.find(name);
    return it == s_datas.end() ? nullptr : it->second;
}

//"A.B", 10
//A:
//  B: 10
//  C: str
//所以转换一层，让可以直接进行访问
static void ListAllMember(const std::string& prefix,
                            const YAML::Node& node,
                            std::list<std::pair<std::string, const YAML::Node>>& output)
{
        if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._012345678")
                != std::string::npos) {
            SYLAR_LOG_ERROR(g_logger) << "Config invalid name : " << prefix << " : " << node;
            return;
        }

        output.push_back(std::make_pair(prefix, node));
        if(node.IsMap()) {
            for(auto it = node.begin();
                    it != node.end(); ++ it)
            {
                ListAllMember(prefix.empty() ? it->first.Scalar() 
                    : prefix + "." + it->first.Scalar(), it->second, output);
            }
        }
        
}

//不用加锁，因为已经在 fromstring 做好。不会出现内存的问题了（但是可能出现两个配置交错的问题，这个就交给上层决定了）
void Config::LoadFromYaml(const YAML::Node& root) {
    //配合打平了
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);

    //检查并录入
    for(auto& i : all_nodes) 
    {
        std::string key = i.first;
        if(key.empty())
            continue;
        
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);
        
        //约定高于配置。所以是，配置去修改代码里的“约定”（也可以理解是内置配置了。。）
        //约定里没有的，配置都懒得解析的
        if(var)
        {
            if(i.second.IsScalar())
            {
                //录入
                var->fromString(i.second.Scalar());
            }
            else
            {
                //如果不是标量，就转到 ss 里面去（相当于 toString）
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}

static std::map<std::string, uint64_t> s_file2modifytime;
//线程安全
static sylar::Mutex s_mutex;

void Config::LoadFromConfDir(const std::string &path)
{
    std::string absoulte_path = sylar::EnvMgr::GetInstance()->getAbsolutePath(path);
    std::vector<std::string> files;
    
    FSUtil::ListAllFile(files, absoulte_path, ".yml");
    for(auto& i : files)
    {
        struct stat st;
        lstat(i.c_str(), &st);
        {
            sylar::Mutex::Lock lock(s_mutex);
            if(s_file2modifytime[i] == (uint64_t)st.st_mtime)
            {
                //优化，如果文件没有被修改过（通过修改时间来判断），就没必要做了
                continue;
            }
            s_file2modifytime[i] = st.st_mtime;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(i);
            LoadFromYaml(root);
            SYLAR_LOG_INFO(g_logger) << "LoadConfFile file="
                << i << " ok";
        }
        catch(...)
        {
            SYLAR_LOG_INFO(g_logger) << "LoadConfFile file="
                << i << " failed";
        }
    }
}

//调试、暴露内部访问的接口
void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb)
{
    RWMutexType::ReadLock lock(GetMutex());

    ConfigVarMap& m = GetDatas();
    for(auto it = m.begin(); it != m.end(); ++it)
    {
        cb(it->second);
    }
}

}
