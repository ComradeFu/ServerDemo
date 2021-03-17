#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>

//定义几个让日志好用的宏。一般就是用这种方式来简写。就不用定义 Event 了。当然用函数也可以
// stringsteam a = SYLAR_LOG_DEBUG(logger); a << "test logger debug.";wrap类被回收的时候会自动把ss写进logger
//智能指针真好用
#define SYLAR_LOG_LEVEL(logger, level) \
	if(logger->getLevel() <= level) \
		sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
				__FILE__, __LINE__, 0, sylar::GetThreadId(), \
			sylar::GetFiberId(), time(0)))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
	if(logger->getLevel() <= level) \
		sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
						__FILE__, __LINE__, 0, sylar::GetThreadId(), \
					sylar::GetFiberId(), time(0)))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

namespace sylar {

class Logger;

//日志等级
class LogLevel {
public:
	enum Level {
		UNKNOW = 0,
		DEBUG = 1,
		INFO = 2,
		WARN = 3,
		ERROR = 4,
		FATAL = 5
	};

	static const char* ToString(LogLevel::Level level);
};

//日志的本体
class LogEvent {
public:
	typedef std::shared_ptr<LogEvent> ptr;
	LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t m_line, uint32_t elapse, uint32_t thread_id, uint32_t fiber_id, uint64_t time);

	const char* getFile() const { return m_file; }
	int32_t getLine() const { return m_line; }
	uint32_t getElapse() const { return m_elapse; }
	uint32_t getThreadId() const { return m_threadId; }
	uint32_t getFiberId() const { return m_fiberId; }
	uint32_t getTime() const { return m_time; }
	std::string getContent() const { return m_ss.str(); }
	std::shared_ptr<Logger> getLogger() const {return m_logger;}
	LogLevel::Level getLevel() const {return m_level;}

	std::stringstream& getSS() {return m_ss;}
	//有些人更喜欢这种方式，printf，而不是 ss 注入
	void format(const char* fmt, ...);
	void format(const char* fmt, va_list al);
private:
	// C++11 之后，可以在类里面直接这样初始化了。。
	const char* m_file = nullptr;
	//行号
	int32_t m_line = 0;
	//程序启动到现在的毫秒数
	uint32_t m_elapse = 0;
	//线程id
	int32_t m_threadId = 0;
	//协程id
	int32_t m_fiberId = 0;
	//时间
	uint64_t m_time;
	//内容，用ss stream 可以用流，会方便很多
	// std:string m_content;
	std::stringstream m_ss;

	//还要知道自己要写的logger（皆因shared_ptr，需要用到的wrap）
	std::shared_ptr<Logger> m_logger;
	LogLevel::Level m_level;
};

//这个类，是由于我们使用了 shared_ptr，所以直接用 LogEvent 是不合适的（因为 stringstream会被自动干掉）
class LogEventWrap {
public:
	LogEventWrap(LogEvent::ptr e);
	~LogEventWrap();

	std::stringstream& getSS();
	LogEvent::ptr getEvent() const { return m_event;}

private:
	LogEvent::ptr m_event;
};

//日志格式(Appender 输出)
class LogFormatter {
public:
	typedef std::shared_ptr<LogFormatter> ptr;
	LogFormatter(const std::string& pattern);

	//初始化
	void init();

	//%t	%thread_id %m %,
	std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
public:
	//解析，虚类
	class FormatItem
	{
	public:
		//第一件事
		typedef std::shared_ptr<FormatItem> ptr;
		FormatItem(const std::string& fmt = "") {}
		virtual ~FormatItem() {}
		//下面用ossteam，而不是 string返回，是从流里拿性能会更强一些
		virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
	};

private:
	//根据模式来解析
	std::string m_pattern;
	std::vector<FormatItem::ptr> m_items;
};

//还有一个“轮转式”的，就是按天分割日志。。再说吧。。
//日志输出地
class LogAppender {
public:
	typedef std::shared_ptr<LogAppender> ptr;

	// 可以预见日志消费是多种的，所以必须将析构函数变成虚函数。
	// 否则继承会出现析构上的问题
	virtual ~LogAppender(){};

	// log 方法是很多的，比如 std 跟 file，子类必须实现这个方法
	virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

	void setFormatter(LogFormatter::ptr val) {m_formatter = val;}
	LogFormatter::ptr getFormatter() const {return m_formatter;}

	LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}

//这里不用 private 是为了让子类能拿到
protected:
	//定义是针对哪些级别的
	LogLevel::Level m_level = LogLevel::DEBUG;
	//需要怎么输出、输出什么
	LogFormatter::ptr m_formatter;
};

//输出到控制台的appender
class StdoutLogAppender : public LogAppender {
public:
	//习惯上，还是会先把智能指针定义好。后面大部分情况都是用智能指针的了
	typedef std::shared_ptr<StdoutLogAppender> ptr;
	//不用 virtual 用 oveerride 是为了更强调这个方法是从基类继承出来的重载的实现，炫技
	void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
};

//输出到文件的appender
class FileLogAppender : public LogAppender {
public:
	typedef std::shared_ptr<FileLogAppender> ptr;
	FileLogAppender(const std::string filename);
	void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;

	//重新打开一次文件，文件打开成功返回 true
	bool reopen();

private:
	std::string m_filename; //额外需要一个文件名
	std::ofstream m_filestream; //文件流
};

//日志输出器，继承这个是为了能快速提取出自己的 shared_ptr
class Logger : public std::enable_shared_from_this<Logger> {
public:
	Logger(const std::string& name = "root");

	//智能指针定义
	typedef std::shared_ptr<Logger> ptr;	

	void log(LogLevel::Level level, LogEvent::ptr event);
	
	void debug(LogEvent::ptr event);
	void info(LogEvent::ptr event);
	void warn(LogEvent::ptr event);
	void error(LogEvent::ptr event);
	void fatal(LogEvent::ptr event);

	void addAppender(LogAppender::ptr appender);
	void delAppender(LogAppender::ptr appender);

	LogLevel::Level getLevel() const {return m_level;}
	void setLevel(LogLevel::Level val) {m_level = val;}

	const std::string& getName() const {return m_name;}
private:
	std::string m_name;
	LogLevel::Level m_level; //本日志器的级别
	std::list<LogAppender::ptr> m_appenders; //输出到哪里的一个集合
	LogFormatter::ptr m_formatter; //有些logger appender 不需要自己的formater是直接输出
};

}

#endif
