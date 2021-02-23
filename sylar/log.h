#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <stringstream>
#include <fstream>

namespace sylar {


//日志的本体
class LogEvent {
public:
	LogEvent();
	typedef std::shared_ptr<LogEvent> ptr;

	const char* getFile() const { return m_file; }
	int32_t getLine const { return m_line; }
	uint32_t getElapse() const { return m_elapse; }
	uint32_t getThread() const { return m_threadId; }
	uint32_t getFiberId() const { return m_fiberId; }
	uint32_t getTime() const { return m_time; }
	const std::string& getContent() const { return m_content; }

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
	//内容
	std:string m_content;
};

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

//日志输出器
class Logger {
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
private:
	std::string m_name;
	LogLevel::Level m_level; //本日志器的级别
	std::list<LogAppender::ptr> m_appenders; //输出到哪里的一个集合
	
};

//日志输出地
class LogAppender {
public:
	typedef std::shared_ptr<LogAppender> ptr;

	// 可以预见日志消费是多种的，所以必须将析构函数变成虚函数。
	// 否则继承会出现析构上的问题
	virtual ~LogAppender(){};

	// log 方法是很多的，比如 std 跟 file，子类必须实现这个方法
	virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;

	void setFormatter(LogFormatter::ptr val) {m_formatter = val;}
	LogFormatter::ptr getFormatter() const {return m_formatter;}

//这里不用 private 是为了让子类能拿到
protected:
	//定义是针对哪些级别的
	LogLevel::Level m_level;
	//需要怎么输出、输出什么
	LogFormatter::ptr m_formatter;
};

//输出到控制台的appender
class StdoutLogAppender : public LogAppender {
public:
	//习惯上，还是会先把智能指针定义好。后面大部分情况都是用智能指针的了
	typedef std::shared_ptr<StdoutLogAppender> ptr;
	//不用 virtual 用 oveerride 是为了更强调这个方法是从基类继承出来的重载的实现，炫技
	void log(LogLevel::Level level, LogEvent::ptr event) override;
};

//输出到文件的appender
class FileLogAppender : public LogAppender {
public:
	typedef std::shared_ptr<FileLogAppender> ptr;
	FileLogAppender(const std::string filename);
	void log(LogLevel::Level level, LogEvent::ptr event) override;

	//重新打开一次文件，文件打开成功返回 true
	bool reopen();

private:
	std::string m_name; //额外需要一个文件名
	std::ofstream m_filesteam; //文件流
};

//还有一个“轮转式”的，就是按天分割日志。。再说吧。。

//日志格式(Appender 输出)
class LogFormatter {
public:
	typedef std::shared_ptr<LogFormatter> ptr;
	LogFormatter(const std::string& pattern);

	//初始化
	void init();

	//%t	%thread_id %m %,
	std::string format(LogLevel::Level level, LogEvent::ptr event);
public:
	//解析，虚类
	class FormatItem
	{
	public:
		//第一件事
		typedef std::shared_ptr<FormatItem> ptr;
		virtual ~FormatItem() {}
		//下面用ossteam，而不是 string返回，是从流里拿性能会更强一些
		void format(std::ostream& os, LogLevel::Level level, LogEvent::ptr event) = 0;
	}

private:
	//根据模式来解析
	std::string m_pattern;
	std::vector<FormatItem::ptr> m_itmes;
};

}

#endif
