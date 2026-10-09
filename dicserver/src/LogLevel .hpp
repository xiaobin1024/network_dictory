#ifndef LOG_HPP
#define LOG_HPP

// 定义日志级别
enum class LogLevel { DEBUG, INFO, ERROR };

// 设置全局日志级别
const LogLevel LOG_LEVEL = LogLevel::INFO;

// 日志宏定义
#define LOG(level, msg) \
    do { \
        if (level >= LOG_LEVEL) { \
            cerr << "[" << #level << "] " << msg << endl; \
        } \
    } while(0)  \

// 关于套接字通信的错误信息宏定义
#define HANDLE_INIT_ERROR(condition) \
    do { \
        if (condition) { \
            int saved_errno = errno; \
            string error_msg = string("Initialization failed: ") + string(#condition) + \
                               " (" + strerror(saved_errno) + ")"; \
            LOG(LogLevel::ERROR, error_msg); \
            \
            if (sfd_ >= 0) { \
                close(sfd_); \
                sfd_ = -1; \
            } \
            throw runtime_error(error_msg); \
        } \
    } while(0)

#endif
