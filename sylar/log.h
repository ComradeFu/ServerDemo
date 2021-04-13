#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include "util.h"
#include "singleton.h"
#include "thread.h"

//定义几个让日志好用的宏。一般就是用这种方式来简写。就不用定义 Event 了。当然用函数也可以
// stringsteam a = SYLAR_LOG_DEBUG(logger); a << "test logger debug.";wrap类被回收的时候会自动把ss写进logger
//智能指针真好用
//下面的前缀 SYLAR，是为了以后跟别的库进行整合的话，会起冲突
#define SYLAR_LOG_LEVEL(logger, level) \
	if(logger->getLevel() <= level) \
		sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
				__FILE__, __LINE__, 0, sylar::GetThreadId(), \
			sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
	if(logger->getLevel() <= level) \
		sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
						__FILE__, __LINE__, 0, sylar::GetThreadId(), \
					sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

//定义一个宏吧，这样方便一点
#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

namespace sylar {

class Logger;
class LoggerManager;

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
	static LogLevel::Level FromString(const std::string& str);
};

//日志的本体
class LogEvent {
public:
	typedef std::shared_ptr<LogEvent> ptr;
	LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, 
					const char* file, int32_t m_line, uint32_t elapse, 
					uint32_t thread_id, uint32_t fiber_id, uint64_t time,
					const std::string& thread_name);

	const char* getFile() const { return m_file; }
	int32_t getLine() const { return m_line; }
	uint32_t getElapse() const { return m_elapse; }
	uint32_t getThreadId() const { return m_threadId; }
	const std::string getThreadNmae() const { return m_threadName; }
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
	//线程名
	std::string m_threadName;
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

//日志格式(Appender 输出)，
//LogFormatter 没提供修改它自身的方法，一旦新建出来就不会变了，只能使用。所以就不需要考虑线程安全之类的了
class LogFormatter {
public:
	typedef std::shared_ptr<LogFormatter> ptr;
	LogFormatter(const std::string& pattern);

	//初始化
	void init();

	bool isError() const {return m_error;}

	const std::string getPattern() const { return m_pattern; }

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
	bool m_error = false; //用来判断这个formatter 是否有格式解析错误了
};

//还有一个“轮转式”的，就是按天分割日志。。再说吧。。
//日志输出地
class LogAppender {
friend class Logger;
public:
	typedef std::shared_ptr<LogAppender> ptr;
	typedef Mutex MutexType;
	// 可以预见日志消费是多种的，所以必须将析构函数变成虚函数。
	// 否则继承会出现析构上的问题
	virtual ~LogAppender(){};

	// log 方法是很多的，比如 std 跟 file，子类必须实现这个方法
	virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

	// toYamlString 也必须加锁，不然中途可能会有人修改formatter，导致输出不准
	virtual std::string toYamlString() = 0;

	void setFormatter(LogFormatter::ptr val);
	LogFormatter::ptr getFormatter();

	LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}

//这里不用 private 是为了让子类能拿到
protected:
	//定义是针对哪些级别的
	LogLevel::Level m_level = LogLevel::DEBUG;
	bool m_hasFormatter = false;
	//写锁就行，往xxx里面写是比较多的
	MutexType m_mutex;
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

	std::string toYamlString() override;
};

//输出到文件的appender
class FileLogAppender : public LogAppender {
public:
	typedef std::shared_ptr<FileLogAppender> ptr;
	FileLogAppender(const std::string filename);
	void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;

	//重新打开一次文件，文件打开成功返回 true
	bool reopen();

	std::string toYamlString() override;
private:
	std::string m_filename; //额外需要一个文件名
	std::ofstream m_filestream; //文件流
	uint64_t m_lastTime = 0;
};

//日志输出器，继承这个是为了能快速提取出自己的 shared_ptr
class Logger : public std::enable_shared_from_this<Logger> {
	friend class LoggerManager;
public:
	Logger(const std::string& name = "root");

	//智能指针定义
	typedef std::shared_ptr<Logger> ptr;	
	typedef Mutex MutexType;

	void log(LogLevel::Level level, LogEvent::ptr event);
	
	void debug(LogEvent::ptr event);
	void info(LogEvent::ptr event);
	void warn(LogEvent::ptr event);
	void error(LogEvent::ptr event);
	void fatal(LogEvent::ptr event);

	void addAppender(LogAppender::ptr appender);
	void delAppender(LogAppender::ptr appender);
	void clearAppenders();

	LogLevel::Level getLevel() const {return m_level;}
	void setLevel(LogLevel::Level val) {m_level = val;}

	const std::string& getName() const {return m_name;}
	void setFormatter(LogFormatter::ptr val);
	void setFormatter(const std::string& val);
	LogFormatter::ptr getFormatter();

	std::string toYamlString();
private:
	std::string m_name;
	LogLevel::Level m_level; //本日志器的级别
	MutexType m_mutex;
	std::list<LogAppender::ptr> m_appenders; //输出到哪里的一个集合
	LogFormatter::ptr m_formatter; //有些logger appender 不需要自己的formater是直接输出

	//一个比较奇怪的优化，如果自己没有 appender，就写到root去，所以需要知道 root
	Logger::ptr m_root;
};

class LoggerManager {
public:
	typedef Mutex MutexType;

	LoggerManager();
	Logger::ptr getLogger(const std::string& name);

	//快速初始化应该初始化的logger
	void init();
	Logger::ptr getRoot() const { return m_root; }

	std::string toYamlString();
private:
	MutexType m_mutex;
	std::map<std::string, Logger::ptr> m_loggers;
	Logger::ptr m_root; //默认的logger
};

typedef sylar::Singleton<LoggerManager> LoggerMgr;
}

#endif
