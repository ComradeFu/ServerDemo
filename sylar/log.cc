#include "log.h"

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

// Logger
    Logger::Logger(const std::string& name):m_name(name)
    {

    }

    void Logger::addAppender(LogAppender::ptr appender)
    {
        //现在先不考虑线程安全。后面再补上
        m_appenders.push_back(appender);
    }

    void Logger::delAppender(LogAppender::ptr appender)
    {
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

    void Logger::log(LogLevel::Level level, LogEvent::ptr event)
    {
        //先判断level
        if(level < m_level)
            return;
        
        //输出给所有的 appenders
        for(auto& i : m_appenders)
        {
            i->log(level, event);
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
    StdoutLogAppender::log(LogLevel::Level level, LogEvent::ptr event)
    {
        if(level < m_level)
            return;
        
        std::cout << m_formatter.format(event);
    }
//
    FileLogAppender::FileLogAppender(std::string filename):m_filename(filename)
    {

    }

    bool FileLogAppender::reopen()
    {
        if(m_filestream)
        {
            m_filestream.close();
        }

        m_filestream.open(m_filename);

        //两个感叹号是为了转成 boolean
        return !!m_filesteam;
    }

    FileLogAppender::log(LogLevel::Level level, LogEvent::ptr event)
    {
        if(level < m_level)
            return;
        
        m_filesteam << m_formatter.format(event);
    }

//LogFormatter
    LogFormatter::LogFormatter(const std::string& pattern):m_pattern(pattern)
    {

    }

    std::string LogFormatter::format(LogLevel::Level level, LogEvent::ptr event)
    {
        std::stringsteam ss;
        for(auto& i : m_items)
        {
            i->format(ss, level, event);
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
            if(m_pattern[i] != "%")
            {
                nstr.append(1, m_pattern[i]);
                continue
            }

            //末尾说明真的是一个百分号
            if((i + 1) < m_pattern.size())
            {
                if(m_pattern[i = 1] == '%')
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
                if(isspace(m_pattern[n]))
                {
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

                if(fmt_state == 1)
                {
                    if(m_parttern[n] == '}')
                    {
                        fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                        fmt_state = 2; //end
                        break;
                    }
                }

                n ++;
            }

            if(fmt_state == 0)
            {
                if(!nstr.empty())
                {
                    vec.push_back(std::make_tuple(nstr, "", 0));
                }
                str = m_pattern.substr(i + 1, n - i - 1);
                vec.push_back(std::make_tuple(str, fmt, 1);
                i = n;
            }
            else if(fmt_state == 1)
            {
                std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
                vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
            }
            else if (fmt_state == 2)
            {
                if(!nstr.empty())
                {
                    vec.push_back(std::make_tuple(nstr, "", 0));
                }
                vec.push_back(std::make_tuple(str, fmt, 1))
                i = n;
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
    }

    class MeassageFormatItem : public LogFormatter::FormatItem
    {
    public:
        void format(std::ostream& os, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getContent();
        }

    }

    class LevelFormatItem : public LogFormatter::FormatItem
    {
    public:
        void format(std::ostream& os, LogLevel::Level level, LogEvent::ptr event) override {
            os << LogLevel::Tostring(level);
        }
    }
}
