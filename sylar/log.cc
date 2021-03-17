#include "log.h"
#include <map>
#include <iostream>
#include <functional>
#include <time.h>
#include <string.h>
#include <stdarg.h>

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
            os << logger->getName();
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

    LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line, uint32_t elapse, uint32_t thread_id, uint32_t fiber_id, uint64_t time)
    :m_file(file)
    ,m_line(line)
    ,m_elapse(elapse)
    ,m_threadId(thread_id)
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
        m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T<%f:%l>%T%m%n"));
    }

    void Logger::addAppender(LogAppender::ptr appender)
    {
        //Logger 自带自定义的 formatter
        if(!appender->getFormatter())
        {
            appender->setFormatter(m_formatter);
        }

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
        
        //自己的智能指针
        auto self = shared_from_this();
        //输出给所有的 appenders
        for(auto& i : m_appenders)
        {
            i->log(self, level, event);
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
    void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        if(level < m_level)
            return;
        
        std::cout << m_formatter->format(logger, level, event);
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
        return !!m_filestream;
    }

    void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        if(level < m_level)
            return;
        
        m_filestream << m_formatter->format(logger, level, event);
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
                }
                else
                {
                    m_items.push_back(it->second(std::get<1>(i)));
                }
            }

            std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ") " << std::endl;
        }
    }
}
