// connectionPool.hpp
#ifndef CONNECTION_POOL_HPP
#define CONNECTION_POOL_HPP

#include <sqlite3.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cstddef>

class ConnectionPool
{
public:
    // 单例模式
    static ConnectionPool &getInstance()
    {
        static ConnectionPool instance;
        return instance;
    }

    // 初始化连接池（在main函数开始时调用一次）
    void init(const std::string &db_path,
              int init_connections = 10,
              int max_indle = -1)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_path_.empty())
        {
            throw std::runtime_error("连接池已初始化");
        }
        db_path_ = db_path;
        // 自动计算默认值(初始值数的2倍，最小10)
        this->maxIdleConnections = (max_indle == -1)
                                       ? std::max(init_connections * 2, 10)
                                       : max_indle;
        // 创建初始连接
        for (int i = 0; i < init_connections; ++i)
        {
            connections_.push(createConnection());
        }
    }

    // 获取连接（带超时机制）
    std::shared_ptr<sqlite3> getConnection()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待可用连接（最多等待5秒）
        if (task_cv_.wait_for(lock, std::chrono::seconds(5),
                              [this]
                              { return !connections_.empty(); }))
        {

            auto conn = connections_.front();
            connections_.pop();
            return conn;
        }

        throw std::runtime_error("获取连接超时");
    }

    // 释放连接
    void releaseConnection(std::shared_ptr<sqlite3> conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(conn);
        task_cv_.notify_one();
    }
    
private:
    // 禁止拷贝和移动
    ConnectionPool(const ConnectionPool &) = delete;
    ConnectionPool &operator=(const ConnectionPool &) = delete;

    ConnectionPool() = default;
    ~ConnectionPool() = default;

    // 创建单个连接
    std::shared_ptr<sqlite3> createConnection()
    {
        sqlite3 *conn = nullptr;
        if (sqlite3_open(db_path_.c_str(), &conn) != SQLITE_OK)
        {
            sqlite3_close(conn);
            throw std::runtime_error("数据库连接失败: " + db_path_);
        }

        // WAL 模式：读者不阻塞写者、写者不阻塞读者，使"一写多读"并发成为可能
        // （journal_mode 为库级持久设置，重复执行为 no-op）。
        // busy_timeout：多连接竞争同一写锁时等待而非立即返回 SQLITE_BUSY。
        // synchronous=NORMAL：WAL 下断电最多丢最近事务，正常退出不丢数据，
        // 换取远高于 FULL 的写吞吐。
        sqlite3_exec(conn, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);
        sqlite3_exec(conn, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(conn, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

        // 使用自定义删除器确保连接正确释放
        return std::shared_ptr<sqlite3>(conn, [this](sqlite3 *c)
                                        {
                                            std::lock_guard<std::mutex> lock(mutex_);
                                            sqlite3_close(c);                                              // 直接关闭数据库连接
                                        });
    }
    int maxIdleConnections = 20; // 默认最大空闲连接数
    std::queue<std::shared_ptr<sqlite3>> connections_;
    std::mutex mutex_;
    std::condition_variable task_cv_;
    std::string db_path_;
};

#endif // CONNECTION_POOL_HPP