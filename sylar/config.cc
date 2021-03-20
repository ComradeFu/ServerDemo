#include "config.h"

namespace sylar {

//static 成员变量必须在类外初始化一次
Config::ConfigVarMap Config::s_datas;

//查找当前有无这个配置项，有的话返回它
ConfigVarBase::ptr Config::LookupBase(const std::string& name)
{
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
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invalid name : " << prefix << " : " << node;
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
            std::cout << "key: " << key << ",val: " << i.second << std::endl;
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

}
