#include "log.h"
#include <map>
#include <iostream>
#include <functional>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include "config.h"

namespace sylar {

    const char* LogLevel::ToString(LogLevel::Level level)
    {
        switch (level)
        {

        //咳咳，下面开始炫技，宏定义复写。oh man
        #define XX(name) \
            case LogLevel::name: \
                return #name; \
                break;

            XX(DEBUG);
            XX(INFO);
            XX(WARN);
            XX(ERROR);
            XX(FATAL);

        //写完，干掉
        #undef XX
        
        default:
            return "UNKNOW";
        }

        return "UNKNOW";
    }

    LogLevel::Level LogLevel::FromString(const std::string& str)
    {
        #define XX(level, v) \
        if(str == #v) \
        { \
            return LogLevel::level; \
        }

        //兼容大小写
        XX(DEBUG, debug);
        XX(INFO, info);
        XX(WARN, warn);
        XX(ERROR, error);
        XX(FATAL, fatal);

        XX(DEBUG, DEBUG);
        XX(INFO, INFO);
        XX(WARN, WARN);
        XX(ERROR, ERROR);
        XX(FATAL, FATAL);

        return LogLevel::UNKNOW;

        #undef XX
    }

    void LogEvent::format(const char* fmt, ...) {
        va_list al;
        va_start(al, fmt);
        format(fmt, al);
        va_end(al);
    }

    void LogEvent::format(const char* fmt, va_list al) {
        char* buf = nullptr;
        int len = vasprintf(&buf, fmt, al);
        if(len != -1) {
            m_ss << std::string(buf, len);
            free(buf);
        }
    }


    LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e)
    {

    }

    //在析构的时候，把LogEvent的ss写进去
    LogEventWrap::~LogEventWrap()
    {
        if(m_event) {
            m_event->getLogger()->log(m_event->getLevel(), m_event);
        }
    }

    std::stringstream& LogEventWrap::getSS()
    {
        return m_event->getSS();
    }

    // 下面是 Log4j 格式的各种零件
    class MessageFormatItem : public LogFormatter::FormatItem
    {
    public:
        MessageFormatItem(const std::string& str = "")
        {

        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getContent();
        }

    };

    class LevelFormatItem : public LogFormatter::FormatItem
    {
    public:
        LevelFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << LogLevel::ToString(level);
        }
    };

    class ElapseFormatItem : public LogFormatter::FormatItem
    {
    public:
        ElapseFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getElapse();
        }
    };

    class NameFormatItem : public LogFormatter::FormatItem
    {
    public:
        NameFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            // os << logger->getName();
            // 由于可能这个 logger 是用 root 来输出的，所以要换一种才能拿出最原始的
            os << event->getLogger()->getName();
        }
    };

    class ThreadIdFormatItem : public LogFormatter::FormatItem
    {
    public:
        ThreadIdFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getThreadId();
        }
    };

    class FiberIdFormatItem : public LogFormatter::FormatItem
    {
    public:
        FiberIdFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getFiberId();
        }
    };

    class ThreadNameFormatItem : public LogFormatter::FormatItem
    {
    public:
        ThreadNameFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getThreadName();
        }
    };

    class DateTimeFormatItem : public LogFormatter::FormatItem
    {
    public:
        DateTimeFormatItem(const std::string& format = "%Y:%m:%d %H:%M:%S") : m_format(format)
        {
            if(m_format.empty())
            {
                //默认的时间格式
                m_format = "%Y-%m-%d %H:%M:%S";
            }
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            struct tm tm;
            time_t time = event->getTime();
            localtime_r(&time, &tm);
            char buf[64];
            strftime(buf, sizeof(buf), m_format.c_str(), &tm);
            
            os << buf;
        }
    private:
        //时间还要特殊一些，有自己的格式
        std::string m_format;
    };

    class FilenameFormatItem : public LogFormatter::FormatItem
    {
    public:
        FilenameFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getFile();
        }
    };

    class LineFormatItem : public LogFormatter::FormatItem
    {
    public:
        LineFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getLine();
        }
    };

    class NewLineFormatItem : public LogFormatter::FormatItem
    {
    public:
        NewLineFormatItem(const std::string& str = "")
        {
            
        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << std::endl;
        }
    };

    class StringFormatItem : public LogFormatter::FormatItem
    {
    public:
        StringFormatItem(const std::string& str) : FormatItem(str), m_string(str)
        {

        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << m_string;
        }
    private:
        std::string m_string;
    };

    class TabFormatItem : public LogFormatter::FormatItem
    {
    public:
        TabFormatItem(const std::string& str = "")
        {

        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << "\t";
        }
    private:
        std::string m_string;
    };

    LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, 
                        const char* file, int32_t line, uint32_t elapse, 
                        uint32_t thread_id, uint32_t fiber_id, uint64_t time,
                        const std::string& thread_name)
    :m_file(file)
    ,m_line(line)
    ,m_elapse(elapse)
    ,m_threadId(thread_id)
    ,m_threadName(thread_name)
    ,m_fiberId(fiber_id)
    ,m_time(time)
    ,m_logger(logger)
    ,m_level(level)
    {

    }

// Logger
    Logger::Logger(const std::string& name):m_name(name), m_level(LogLevel::DEBUG)
    {
        // m_formatter.reset(new LogFormatter("%d%T%t%T[%p]%T[%c]%T<%f:%l>%T%m%n"));
        m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T<%f:%l>%T%m%n"));
    }

    void Logger::setFormatter(LogFormatter::ptr val)
    {
        MutexType::Lock lock(m_mutex);
        m_formatter = val;

        //使用了默认的formatter appender的话，也要进行对应的修改
        for(auto& i : m_appenders)
        {
            if(!i->m_hasFormatter)
            {
                //这里没有调用 appender 的接口，是直接在友元操作了，所以也必须上把锁
                MutexType::Lock il(i->m_mutex);
                i->m_formatter = m_formatter;
            }
        }
    }

    void Logger::setFormatter(const std::string& val)
    {
         sylar::LogFormatter::ptr new_val(new sylar::LogFormatter(val));
        if(new_val->isError())
        {
            //Logger 打自己的日志，挺尴尬的
            //出错有可能死循环。。还是 std 把
            std::cout << "Logger setFormatter name=" << m_name
                        << " value=" << val << " invalid formatter"
                        << std::endl;
            return;
        }

        // m_formatter = new_val;
        setFormatter(new_val);
    }

    std::string Logger::toYamlString()
    {
        //锁的粒度可以小一点，但也不差这一点了。。就这样吧。。
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["name"] = m_name;

        if(m_level != LogLevel::UNKNOW)
            node["level"] = LogLevel::ToString(m_level);

        if(m_formatter)
            node["formatter"] = m_formatter->getPattern();
        
        for(auto& i : m_appenders)
        {
            //这里稍微有点奇怪，NODE push back 进去的是 string，而不是 node
            //算了怪就怪一点吧，反正也只是调试用
            node["appenders"].push_back(YAML::Load(i->toYamlString()));
        }
        
        std::stringstream ss;
        ss << node;

        return ss.str();
    }

    LogFormatter::ptr Logger::getFormatter()
    {
        // shared_ptr 并不线程安全，so
        MutexType::Lock lock(m_mutex);
        return m_formatter;
    }

    void Logger::addAppender(LogAppender::ptr appender)
    {
        MutexType::Lock lock(m_mutex);
        //Logger 自带自定义的 formatter
        if(!appender->getFormatter())
        {
            MutexType::Lock appender_lock(appender->m_mutex);
            appender->m_formatter = m_formatter;
            // 这种情况，不改变 m_hasFormatter ，表示是默认的 formatter，怪怪的。
            // appender->setFormatter(m_formatter);
        }

        //现在先不考虑线程安全。后面再补上
        m_appenders.push_back(appender);
    }

    void Logger::delAppender(LogAppender::ptr appender)
    {
        MutexType::Lock lock(m_mutex);
        //遍历删除
        for(auto it = m_appenders.begin();
                it != m_appenders.end(); ++it)
        {
            if(*it == appender)
            {
                m_appenders.erase(it);
                break;
            }
        }
    }

    void Logger::clearAppenders()
    {
        //clear 是一个复杂操作，所以要加
        MutexType::Lock lock(m_mutex);
        m_appenders.clear();
    }

    void Logger::log(LogLevel::Level level, LogEvent::ptr event)
    {
        //先判断level
        if(level < m_level)
            return;
        
        //有人是可能修改 appenders 的，so
        MutexType::Lock lock(m_mutex);

        //自己的智能指针
        auto self = shared_from_this();

        if(!m_appenders.empty())
        {
            //输出给所有的 appenders
            for(auto& i : m_appenders)
            {
                i->log(self, level, event);
            }
        }
        //下面是 else if 是因为此类有可能是野生的
        else if(m_root)
        {
            m_root->log(level, event);
        }
        
    }
	
	void Logger::debug(LogEvent::ptr event)
    {
        log(LogLevel::DEBUG, event);
    }

	void Logger::info(LogEvent::ptr event)
    {
        log(LogLevel::INFO, event);
    }

	void Logger::warn(LogEvent::ptr event)
    {
        log(LogLevel::WARN, event);
    }

	void Logger::error(LogEvent::ptr event)
    {
        log(LogLevel::ERROR, event);
    }

	void Logger::fatal(LogEvent::ptr event)
    {
        log(LogLevel::FATAL, event);
    }

//LogAppender
    void LogAppender::setFormatter(LogFormatter::ptr val) 
    {
        // get set 锁，锁到函数结束吧，因为 shared_ptr 不是线程安全的。另外 m_hasFormatter 的设置也不安全
        MutexType::Lock lock(m_mutex);
        m_formatter = val;
        if(val)
        {
            m_hasFormatter = true;
        }
        else
        {
            m_hasFormatter = false;
        }
    }

    //于是下面就不能用 const 方法了，因为会改动 m_mutex
    LogFormatter::ptr LogAppender::getFormatter()
    {
        MutexType::Lock lock(m_mutex);
        return m_formatter;
    }

    void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        if(level < m_level)
            return;

        MutexType::Lock lock(m_mutex);
        std::cout << m_formatter->format(logger, level, event);
    }

    std::string StdoutLogAppender::toYamlString()
    {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["type"] = "StdoutLogAppender";

        if(m_level != LogLevel::UNKNOW)
            node["level"] = LogLevel::ToString(m_level);

        if(m_hasFormatter && m_formatter)
        {
            node["formatter"] = m_formatter->getPattern();
        }

        std::stringstream ss;
        ss << node;

        return ss.str();
    }
//
    FileLogAppender::FileLogAppender(std::string filename):m_filename(filename)
    {
        reopen();
    }

    bool FileLogAppender::reopen()
    {
        MutexType::Lock lock(m_mutex);
        if(m_filestream)
        {
            m_filestream.close();
        }

        m_filestream.open(m_filename, std::ios::app);

        //两个感叹号是为了转成 boolean
        return !!m_filestream;
    }

    void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        if(level < m_level)
            return;
        
        //为了防止意外删除出错，牺牲一点点不太影响的性能
        //每秒钟都重新打开一次（被强删掉之后，可能会有瞬间的log丢失）
        //在外部删除文件的时候，fd 还是在内存里生效的，并且磁盘也不会释放（只是从系统里“标识删除掉”了）
        //这样reopen既可以及时释放失效的fd，也可以继续产生日志
        uint64_t now = time(0);
        if(now != m_lastTime)
        {
            reopen();
            m_lastTime = now;
        }

        MutexType::Lock lock(m_mutex);
        m_filestream << m_formatter->format(logger, level, event);
    }

    std::string FileLogAppender::toYamlString()
    {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["type"] = "FileLogAppender";
        node["file"] = m_filename;

        if(m_level != LogLevel::UNKNOW)
            node["level"] = LogLevel::ToString(m_level);

        if(m_hasFormatter && m_formatter)
        {
            node["formatter"] = m_formatter->getPattern();
        }

        std::stringstream ss;
        ss << node;

        return ss.str();
    }
//LogFormatter
    LogFormatter::LogFormatter(const std::string& pattern):m_pattern(pattern)
    {
        this->init();
    }

    std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        std::stringstream ss;
        for(auto& i : m_items)
        {
            i->format(ss, logger, level, event);
        }
        
        return ss.str();
    }

    void LogFormatter::init()
    {
        //仿照 log4j 格式 %号，%xxx %xxx{xxx} %%
        //str, format, type tuple 三连
        std::vector<std::tuple<std::string, std::string, int>> vec;
        std::string nstr;
        for(size_t i = 0; i< m_pattern.size(); ++i)
        {
            if(m_pattern[i] != '%')
            {
                nstr.append(1, m_pattern[i]);
                continue;
            }

            //末尾说明真的是一个百分号
            if((i + 1) < m_pattern.size())
            {
                if(m_pattern[i + 1] == '%')
                {
                    nstr.append(1, '%');
                    continue;
                }
            }

            size_t n = i + 1;
            int fmt_state = 0;
            size_t fmt_begin = 0;

            std::string str;
            std::string fmt;
            while(n < m_pattern.size())
            {
                //只接受字母
                if(!fmt_state &&(!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}'))
                {
                    //单字母
                    str = m_pattern.substr(i + 1, n - i - 1);
                    break;
                }

                if(fmt_state == 0)
                {
                    if(m_pattern[n] == '{')
                    {
                        str = m_pattern.substr(i + 1, n - i - 1);
                        //解析格式
                        fmt_state = 1;
                        fmt_begin = n;
                        ++n;
                        continue;
                    }
                }
                else if(fmt_state == 1)
                {
                    if(m_pattern[n] == '}')
                    {
                        fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                        fmt_state = 0; //end

                        n ++;
                        break;
                    }
                }

                n ++;
                if(n == m_pattern.size()) {
                    if(str.empty()) {
                        str = m_pattern.substr(i + 1);
                    }
                }
            }

            if(fmt_state == 0)
            {
                if(!nstr.empty())
                {
                    vec.push_back(std::make_tuple(nstr, "", 0));
                    nstr.clear();
                }
                vec.push_back(std::make_tuple(str, fmt, 1));
                i = n - 1;
            }
            else if(fmt_state == 1)
            {
                // 打开可调试
                // std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
                vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
                m_error = true;
            }
        }

        //说明真的是一个纯字符串
        if(!nstr.empty())
        {
            vec.push_back(std::make_tuple(nstr, "", 0));
        }

        //%m -- 消息体
        //%p-- level
        //%r -- 启动后的时间
        //%c -- 日志名称
        //%t -- 线程号
        //%n -- 回车
        //%d -- 时间格式
        //%f -- 文件名
        //%l -- 行号
        static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> s_format_items = {
            //匿名函数指针
            #define XX(str, C) \
            {#str, [](const std::string& fmt) {return FormatItem::ptr(new C(fmt));}}

            XX(m, MessageFormatItem),
            XX(p, LevelFormatItem),
            XX(r, ElapseFormatItem),
            XX(c, NameFormatItem),
            XX(t, ThreadIdFormatItem),
            XX(n, NewLineFormatItem),
            XX(d, DateTimeFormatItem),
            XX(f, FilenameFormatItem),
            XX(l, LineFormatItem),
            XX(T, TabFormatItem),
            XX(F, FiberIdFormatItem),
            XX(N, FilenameFormatItem),
            #undef XX
        };

        for(auto& i : vec){
            //说明是纯字符串
            if(std::get<2>(i) == 0) {
                m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
            }
            else
            {
                auto it = s_format_items.find(std::get<0>(i));
                if(it == s_format_items.end())
                {
                    m_items.push_back(FormatItem::ptr(new StringFormatItem("<< error_format %" + std::get<0>(i) + ">>")));
                    //没有找到对应的格式Item
                    m_error = true;
                }
                else
                {
                    m_items.push_back(it->second(std::get<1>(i)));
                }
            }

            // std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ") " << std::endl;
        }
    }

    LoggerManager::LoggerManager() {
        m_root.reset(new Logger);
        m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));

        m_loggers[m_root->m_name] = m_root;
        init();
    }

    Logger::ptr LoggerManager::getLogger(const std::string& name) {
        MutexType::Lock lock(m_mutex);
        auto it = m_loggers.find(name);
        if(it != m_loggers.end())
        {
            return it->second;
        }

        //不存在就创建一个logger（原本是返回 root，但是那样子定义logger就不够方便了）
        //为了达成一样的默认输出的目标，所以把root的appender给过来

        Logger::ptr logger(new Logger(name));
        logger->m_root = m_root;
        m_loggers[name] = logger;

        return logger;
    }

    std::string LoggerManager::toYamlString()
    {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        for(auto& i : m_loggers)
        {
            node.push_back(YAML::Load(i.second->toYamlString()));
        }
        
        std::stringstream ss;
        ss << node;

        return ss.str();
    }

    //读取不同logger的配置的定义
    struct LogAppenderDefine 
    {
        int type = 0; //1 File, 2 Stdout
        LogLevel::Level level = LogLevel::UNKNOW;
        std::string formatter;
        std::string file; //如果是文件

        //配置事件那里有等于号的判断
        bool operator==(const LogAppenderDefine& oth) const 
        {
            return type == oth.type
                && level == oth.level
                && formatter == oth.formatter
                && file == oth.file;
        }

        
    };
    
    struct LogDefine
    {
        std::string name;
        LogLevel::Level level = LogLevel::UNKNOW;
        std::string formatter;
        std::vector<LogAppenderDefine> appenders;

        bool operator==(const LogDefine& oth) const
        {
            return name == oth.name
                && level == oth.level
                && formatter == oth.formatter
                && appenders == oth.appenders;
        }

        //set结构还需要重载一个 < 号，红黑树需要这个才能知道位置
        bool operator<(const LogDefine& oth) const
        {
            return name < oth.name;
        }
    };

    //Logdefine 配置的偏特化
    template<>
    class LexicalCast<std::string, std::set<LogDefine>>
    {
    public:
        std::set<LogDefine> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            std::set<LogDefine> vec;
            // node["name"].IsDefined();

            std::stringstream ss;
            for(size_t i = 0; i < node.size(); ++i)
            {
                auto n = node[i];
                if(!n["name"].IsDefined())
                {
                    //防止自己循环报错
                    std::cout << "log config error: name is null, " << n << std::endl;  
                    continue;
                }

                LogDefine ld;
                ld.name = n["name"].as<std::string>();
                ld.level = LogLevel::FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
                if(n["formatter"].IsDefined())
                {
                    ld.formatter = n["formatter"].as<std::string>();
                }

                if(n["appenders"].IsDefined())
                {
                    for(size_t x = 0; x < n["appenders"].size(); ++x)
                    {
                        auto a = n["appenders"][x];
                        if(!a["type"].IsDefined())
                        {
                            std::cout << "log config error: appender type is null, " << a << std::endl;
                            continue;
                        }
                        std::string type = a["type"].as<std::string>();
                        LogAppenderDefine lad;
                        if(type == "FileLogAppender")
                        {
                            lad.type = 1;
                            if(!a["file"].IsDefined())
                            {
                                std::cout << "log config error: file appender file is null, " << a << std::endl;
                                continue;
                            }
                            lad.file = a["file"].as<std::string>();

                            if(a["formatter"].IsDefined())
                            {
                                lad.formatter = a["formatter"].as<std::string>();
                            }
                        }
                        else if(type == "StdoutLogAppender")
                        {
                            lad.type = 2;
                        }
                        else
                        {
                            std::cout << "log config error: appender type is invalid, " << a << std::endl;
                            continue;
                        }

                        ld.appenders.push_back(lad);
                    }
                }

                vec.insert(ld);
            }

            return vec;
        }
    };

    template<>
    class LexicalCast<std::set<LogDefine>, std::string>
    {
    public:
        std::string operator()(const std::set<LogDefine>& v)
        {
            YAML::Node node;
            for(auto& i : v)
            {
                YAML::Node n;
                n["name"] = i.name;

                if(i.level != LogLevel::UNKNOW)
                    n["level"] = LogLevel::ToString(i.level);

                if(i.formatter.empty())
                {
                    //有可能是空的，进而本配置用到别的地方，也能准确传达（用默认的）
                    n["formatter"] = i.formatter;
                }

                for(auto& a : i.appenders)
                {
                    YAML::Node na;
                    if(a.type == 1)
                    {
                        na["type"] = "FileLogAppender";
                        na["file"] = a.file;
                    }
                    else if(a.type == 2)
                    {
                        na["type"] = "StdoutLoaAppender";
                    }

                    if(a.level != LogLevel::UNKNOW)
                        na["level"] = LogLevel::ToString(a.level);

                    if(!a.formatter.empty())
                    {
                        na["formatter"] = a.formatter;
                    }

                    n["appenders"].push_back(na);
                }

                node.push_back(n);
            }

            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    sylar::ConfigVar<std::set<LogDefine>>::ptr g_log_defines =
        sylar::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

    //用来做初始化入口，before main function.
    struct LogIniter
    {
        LogIniter()
        {
            g_log_defines->addListener([](const std::set<LogDefine>& old_value,
                            const std::set<LogDefine>& new_value){
                for(auto& i : new_value )
                {
                    auto it = old_value.find(i);
                    sylar::Logger::ptr logger;
                    if(it == old_value.end())
                    {
                        //没有就是新的
                        logger = SYLAR_LOG_NAME(i.name);
                    }
                    else
                    {
                        if(!(i == *it))
                        {
                            //修改的logger
                            logger = SYLAR_LOG_NAME(i.name);
                        }
                        else
                        {
                            //无变化
                            continue;
                        }
                    }

                    logger->setLevel(i.level);
                    if(!i.formatter.empty())
                    {
                        //字符型
                        logger->setFormatter(i.formatter);
                    }

                    //全干掉，重新来（但也可以判断type先的。。）
                    logger->clearAppenders();
                    for(auto& a : i.appenders)
                    {
                        sylar::LogAppender::ptr ap;
                        if(a.type == 1)
                        {
                            ap.reset(new FileLogAppender(a.file));
                        }
                        else if(a.type == 2)
                        {
                            ap.reset(new StdoutLogAppender);
                        }

                        ap->setLevel(a.level);
                        if(!a.formatter.empty())
                        {
                            LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                            if(fmt->isError())
                            {
                                ap->setFormatter(fmt);
                            }
                            else
                            {
                                std::cout << "log.name=" << i.name << " appender type=" << a.type << " formatter=" << a.formatter << " is invalid" << std::endl;
                            }
                        }
                        logger->addAppender(ap);
                    }
                }
                
                //删除
                for(auto& i : old_value)
                {
                    auto it = new_value.find(i);
                    if(it == new_value.end())
                    {
                        //这里的删除不是真的删除，否则一些已有的代码会出问题。。
                        //比如又删又增，那可能持有的就不一样了
                        //所以只是类似 disable 之类的类似删除
                        auto logger = SYLAR_LOG_NAME(i.name);
                        logger->setLevel((LogLevel::Level)100);
                        logger->clearAppenders(); //会转root 的 appender
                    }
                }
            });
        }
    };
    
    //利用全局结构体，在main函数之前初始化，并在构造函数里面做好逻辑（比如增加一些事件）
    static LogIniter __log_init;

    void LoggerManager::init()
    {

    }
}
