#include <iostream>
#include "../sylar/log.h"
#include "../sylar/util.h"


int main(int argc, char** argv)
{
    sylar::Logger::ptr logger(new sylar::Logger);
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    sylar::FileLogAppender::ptr log_file_appender(new sylar::FileLogAppender("./log.txt"));
    sylar::FileLogAppender::ptr error_file_appender(new sylar::FileLogAppender("./error.txt"));

    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));

    log_file_appender->setFormatter(fmt);
    log_file_appender->setLevel(sylar::LogLevel::INFO);

    error_file_appender->setFormatter(fmt);
    error_file_appender->setLevel(sylar::LogLevel::ERROR);

    logger->addAppender(log_file_appender);
    logger->addAppender(error_file_appender);

    // sylar::LogEvent::ptr event(new sylar::LogEvent(__FILE__, __LINE__, 0, sylar::GetThreadId(), sylar::GetThreadId(), time(0)));
    // event->getSS() << "hello sylar log";

    // logger->log(sylar::LogLevel::DEBUG, event);

    SYLAR_LOG_INFO(logger) << "test macro";
    SYLAR_LOG_ERROR(logger) << "test macro error";

    SYLAR_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

    return 0;
}
