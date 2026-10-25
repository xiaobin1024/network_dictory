#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <cstring>
#include <cerrno>
#include <unistd.h> // close

// 定义日志级别
enum class LogLevel { DEBUG, INFO, ERROR };

// 设置全局日志级别
const LogLevel LOG_LEVEL = LogLevel::INFO;

/*
日志互斥锁
服务端是多线程程序，多个工作线程会并发写 std::cerr。
std::cerr 虽然是线程安全的（不会交错字符），但一次 LOG 调用会被拆成多次 << 写入，
多个线程交替写入时仍会出现 "[LogLevel::INFO] xxx[LogLevel::INFO] yyy" 这样的行交错。
这里用互斥锁保证单条日志的完整性。
*/
inline std::mutex &logMutex()
{
    static std::mutex mtx;
    return mtx;
}

/*
日志宏定义
先把消息流式拼接成完整字符串，再加锁一次性输出，避免多线程下日志行交错。
*/
#define LOG(level, msg) \
    do { \
        if (level >= LOG_LEVEL) { \
            std::ostringstream log_oss; \
            log_oss << "[" << #level << "] " << msg; \
            std::lock_guard<std::mutex> log_lock(logMutex()); \
            std::cerr << log_oss.str() << std::endl; \
        } \
    } while(0)

// 关于套接字通信的错误信息宏定义
#define HANDLE_INIT_ERROR(condition) \
    do { \
        if (condition) { \
            int saved_errno = errno; \
            std::string error_msg = std::string("Initialization failed: ") + std::string(#condition) + \
                               " (" + std::strerror(saved_errno) + ")"; \
            LOG(LogLevel::ERROR, error_msg); \
            \
            if (sfd_ >= 0) { \
                close(sfd_); \
                sfd_ = -1; \
            } \
            throw std::runtime_error(error_msg); \
        } \
    } while(0)

#endif
