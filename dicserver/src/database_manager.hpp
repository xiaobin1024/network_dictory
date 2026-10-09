#ifndef DATABASE_MENAGER_H
#define DATABASE_MENAGER_H

#include <myhead.h>
// 字符串头文件
#include <string>
// 互斥锁头文件
#include <mutex>
// 智能制造头文件
#include <memory>
#include <vector>
// 连接池头文件
#include "connectionPool.hpp"

class DatabaseManager
{
public:
    // 单例模式的 getInstance 方法
    static DatabaseManager &getInstance()
    {
        static DatabaseManager instance;
        return instance;
    }

    // 获取数据库连接
    std::shared_ptr<sqlite3> getUsrConnection() {
        return ConnectionPool::getInstance().getConnection();
    }

    std::shared_ptr<sqlite3> getDictConnection() {
        return getUsrConnection();  // 共用同一个数据库
    }

    // 初始化数据库
    bool initalizeDatabase();

    // 用户相关操作
    /*
    用户注册操作，用户登陆操作，用户退出操作
    */
    bool registerUser(const string &name, const string &password);
    int  loginUser(const string &name, const string &password, bool &is_online);
    const char* logoutUser(const string &name);

    // 单词表操作
    /*
    查询单词，记录历史记录，查询历史记录,收藏单词，取消收藏单词，收藏单词记录
    */
    bool querryWord(const string name,const string &word, string &meaning,string &flage);
    bool recordHistory(const string &name, const string &word, const string &meaning, const string &time);
    bool getHistory(const string &name, vector<string> &historyRecords);
    const char* collectWord(const string &name, const string &word, const string &meaning, const string &time);
    const char* cancelCollectWord(const string &name, const string &word);
    bool getCollectWords(const string &name, vector<string> &collectWords);

private:
    // 用户库和单词库共用互斥锁
    std::mutex db_mutex_;
    // 私有构造函数和析构函数，防止外部直接创建或销毁实例
    DatabaseManager();
    ~DatabaseManager();
    // 禁止拷贝和赋值
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    // 定义私有函数
    /*
    初始化用户表，初始化单词表，导入词典数据
    */
    bool initializeUserDB();
    bool initializeDicDB();
    bool dicToDatabase();
    // 执行sql语句的函数
    bool executSQL(std::shared_ptr<sqlite3> db, const string &sql, char **errmsg = NULL);
};

#endif