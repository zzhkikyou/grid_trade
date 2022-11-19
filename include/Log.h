/**
 示例代码:
     // 需要在main.cpp 中加上：
        thread_local std::string Log::threadname_;
        thread_local uint32_t Log::tid_ = 0;


     // 设置onLogChange回调函数
     Log::GetInstance()->onLogChange = [=](Log::LOG_LEVEL level, std::string logText) -> void
     {
        std::cout << level << " " << logText;
     };

     LOG_DEBUG("i'm %s", "hello world");     // 输出 DEBUG (会打印所在行)
     Log::GetInstance()->WriteLog(Log::LOG_DEBUG, "i'm %s", "hello world");

 // 上面代码的执行结果
 2 [xxx 11:10:54:21:32:10 LOG_INFO ] i'm hello world
 4 [xxx 11:10:54:21:32:10 LOG_ERROR] I'm hello world (/Projects/Log/Log/main.cpp : main : 20 )
 1 [xxx 11:10:54:21:32:10 LOG_DEBUG] i'm hello world (/Projects/Log/Log/main.cpp : main : 21 )
 1 [xxx 11:10:54:21:32:10 LOG_DEBUG] i'm hello world

 */

#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <functional>
#include <iomanip>
#include <stdarg.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <locale.h>

#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>

#define LOG_PRE_DEFINE                         \
    thread_local std::string Log::threadname_; \
    thread_local uint32_t Log::tid_ = 0;

#define gettidv() syscall(__NR_gettid)

//  注册线程信息
#define REGTHREADINFO(name)                          \
    do                                               \
    {                                                \
        if (Log::GetInstance()->threadname_.empty()) \
            Log::GetInstance()->threadname_ = name;  \
        if (Log::GetInstance()->tid_ == 0)           \
            Log::GetInstance()->tid_ = gettidv();    \
    } while (0);

#ifndef LOG_FILE_NAME
#define LOG_FILE_NAME "MyLog.log" /** 日志的文件名 */
#endif

#ifndef LOG_LINE_BUFF_SIZE
#define LOG_LINE_BUFF_SIZE (1024 * 80) /** 一行的最大缓冲 */
#endif

#ifndef LOG_DISABLE_LOG
#define LOG_DISABLE_LOG 0 /** 非0表示禁用LOG - 非0情况下依然会触发onLogChange */
#endif

#define WRITE_LOG(LEVEL, FMT, ...)                                                   \
    {                                                                                \
        if (LEVEL >= Log::GetInstance()->GetLogLevel())                              \
        {                                                                            \
            std::stringstream ss;                                                    \
            ss << FMT;                                                               \
            ss << " (" << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << ")"; \
            Log::GetInstance()->WriteLog(LEVEL, ss.str().c_str(), ##__VA_ARGS__);    \
        }                                                                            \
    }

//! 快速宏
#define LOG_TRACE(FMT, ...) WRITE_LOG(Log::TRACE, FMT, ##__VA_ARGS__)
#define LOG_DEBUG(FMT, ...) WRITE_LOG(Log::DEBUG, FMT, ##__VA_ARGS__)
#define LOG_INFO(FMT, ...) WRITE_LOG(Log::INFO, FMT, ##__VA_ARGS__)
#define LOG_WARN(FMT, ...) WRITE_LOG(Log::WARN, FMT, ##__VA_ARGS__)
#define LOG_ERROR(FMT, ...) WRITE_LOG(Log::ERROR, FMT, ##__VA_ARGS__)
#define LOG_ALARM(FMT, ...) WRITE_LOG(Log::ALARM, FMT, ##__VA_ARGS__)
#define LOG_FATAL(FMT, ...) WRITE_LOG(Log::FATAL, FMT, ##__VA_ARGS__)

#define FORMAT(FMT, ...) Log::GetInstance()->Format(FMT, ##__VA_ARGS__)

class Log
{
public:
    /** 日志级别*/
    enum LEVEL
    {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        ALARM,
        FATAL
    };

public:
    /** 单例模式 */
    static Log *GetInstance()
    {
        static Log g_Log;
        return &g_Log;
    }

    void SetLogLevel(LEVEL level) { level_ = level; }

    LEVEL GetLogLevel() { return level_; }

    void SetLogDisable(bool bWriteFile) { logdisable_ = bWriteFile; }

public:
    static void DefPrint(LEVEL level, const std::string &logText)
    {
        switch (level)
        {
        case TRACE:
            std::cout << " \033[37;1m" << logText << "\033[0m\n"; // 白色
            break;
        case DEBUG:
            std::cout << " \033[36;1m" << logText << "\033[0m\n"; // 青色
            break;
        case INFO:
            std::cout << " \033[32;1m" << logText << "\033[0m\n"; // 绿色
            break;
        case WARN:
            std::cout << " \033[33;1m" << logText << "\033[0m\n"; // 黄色
            break;
        case ERROR:
            std::cout << " \033[31;1m" << logText << "\033[0m\n"; // 红色
            break;
        case ALARM:
            std::cout << " \033[35;1m" << logText << "\033[0m\n"; // 紫色
            break;
        case FATAL:
            std::cout << " \033[30;1m" << logText << "\033[0m\n"; // 黑色
            break;
        default:
            break;
        }
    };
    std::function<void(LEVEL, const std::string &)> onLogChange = DefPrint;

public:
    /** 写日志操作 */
    void WriteLog(LEVEL level, const char *pLogText, ...)
    {
        va_list args;
        static thread_local char logText[LOG_LINE_BUFF_SIZE];
        va_start(args, pLogText);
        vsnprintf(logText, LOG_LINE_BUFF_SIZE - 1, pLogText, args);
        va_end(args);
        logText[LOG_LINE_BUFF_SIZE - 1] = '\0';
        WriteLog(logText, level);
    }

    /** 格式化 */
    std::string Format(const char *pLogText, ...)
    {
        va_list args;
        static thread_local char logText2[LOG_LINE_BUFF_SIZE];
        va_start(args, pLogText);
        vsnprintf(logText2, LOG_LINE_BUFF_SIZE - 1, pLogText, args);
        va_end(args);
        logText2[LOG_LINE_BUFF_SIZE - 1] = '\0';
        return logText2;
    }

    void WriteLog(const std::string &logText, LEVEL level = ERROR)
    {
        static const char *const LOG_STRING[] =
            {
                "TRACE ",
                "DEBUG ",
                "INFO  ",
                "WARN  ",
                "ERROR ",
                "ALARM ",
                "FATAL ",
            };

        try
        {
            // 添加时间信息
            std::time_t t = std::time(NULL);
            std::tm tm = *std::localtime(&t);

            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;

            // 生成一行LOG字符串
            std::stringstream szLogLine;
            szLogLine << "[" << std::put_time(&tm, "%m%d %H%M%S") << ":"
                      << std::setw(3) << std::setfill('0') << ms.count() << ":"
                      << std::setw(3) << std::setfill('0') << us.count() % 1000 << ":"
                      << std::setw(3) << std::setfill('0') << ns.count() % 1000 << " " << LOG_STRING[level] << "] " << logText
                      << " (" << (threadname_.empty() ? "Unknown" : threadname_) << " " << tid_ << ")";

            /* 使用CallBack方式调用回显 */
            if (onLogChange)
            {
                onLogChange(level, szLogLine.str());
            }

            szLogLine << "\n";

#if defined LOG_DISABLE_LOG && LOG_DISABLE_LOG == 0

            if (logdisable_)
            {
                /* 输出LOG字符串 - 文件打开不成功的情况下按照标准输出 */
                if (file_.is_open())
                {
                    file_.write(szLogLine.str().c_str(), szLogLine.str().size());
                    file_.flush();
                }
                else
                {
                    std::cout << szLogLine.str() << std::endl;
                }
            }
#endif
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
        }
    }

private:
    Log(void)
    {
        std::time_t t = std::time(NULL);
        std::tm tm = *std::localtime(&t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y%m%d");

        char szPath[1024] = {0};
        if (readlink("/proc/self/exe", szPath, 1024) != -1)
        {
            char *lpName = strrchr(szPath, '/');
            filename_ = ss.str() + "-" + ++lpName + ".log";
        }
        else
        {
            filename_ = ss.str() + "-" + LOG_FILE_NAME;
        }

        file_.open(filename_, std::ofstream::app);
        WriteLog("------------------ LOG SYSTEM START ------------------ ", Log::INFO);
    }
    virtual ~Log(void)
    {
        WriteLog("------------------ LOG SYSTEM END ------------------ ", Log::INFO);
        if (file_.is_open())
            file_.close();
    }

public:
    static thread_local std::string threadname_;
    static thread_local uint32_t tid_;

private:
    /** 写文件 */
    std::ofstream file_;
    std::string filename_;

    volatile LEVEL level_ = LEVEL::INFO;
    volatile bool logdisable_ = true;
};
