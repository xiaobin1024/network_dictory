#ifndef DATABASE_MENAGER_H
#define DATABASE_MENAGER_H

// 字符串头文件
#include <string>
// 互斥锁头文件
#include <mutex>
// 智能指针头文件
#include <memory>
#include <vector>
#include <cstddef>
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

    注册/登录成功时通过 outUserId / outRole 返回用户的稳定标识（SQLite rowid）
    与角色（0=普通用户），供协议层组装 SUCCESS 响应；传 nullptr 表示不关心。
    */
    bool registerUser(const std::string &name, const std::string &password,
                      long long *outUserId = nullptr, int *outRole = nullptr);
    int  loginUser(const std::string &name, const std::string &password, bool &is_online,
                   long long *outUserId = nullptr, int *outRole = nullptr);
    const char* logoutUser(const std::string &name);
    // 注销账号：校验密码后删除用户及其关联数据（历史/收藏/生词本）
    // outUserId 在删除前输出被注销用户的 ID，供协议层回给客户端清理本地数据
    const char* unregisterUser(const std::string &name, const std::string &password,
                               long long *outUserId = nullptr);

    // 单词表操作
    /*
    查询单词，记录历史记录，查询历史记录,收藏单词，取消收藏单词，收藏单词记录

    查询：meaning 输出释义；collectFlag/vocabFlag 输出该单词对当前用户的
    收藏/生词本状态（1=已收藏/已在生词本，0=否），传 nullptr 表示不关心。
    */
    bool querryWord(const std::string name,const std::string &word, std::string &meaning,
                    int *collectFlag = nullptr, int *vocabFlag = nullptr);
    bool recordHistory(const std::string &name, const std::string &word, const std::string &meaning, const std::string &time);
    bool getHistory(const std::string &name, std::vector<std::string> &historyRecords);
    // 收藏单词：收藏完整词条（释义/音标/例句/翻译），时间戳由内部生成。
    // 已收藏同一单词时返回 "already collected"，由协议层决定取消收藏。
    const char* collectWord(const std::string &name, const std::string &word,
                            const std::string &meaning, const std::string &phonetic,
                            const std::string &example, const std::string &translation);
    const char* cancelCollectWord(const std::string &name, const std::string &word);
    bool getCollectWords(const std::string &name, std::vector<std::string> &collectWords);

    // 生词本操作
    /*
    添加生词、移出生词、查询生词本列表
    isAdd 为 true 表示加入生词本，false 表示移出
    */
    const char* updateVocabulary(const std::string &name, const std::string &word,
                                 const std::string &meaning, const std::string &phonetic,
                                 const std::string &example, const std::string &translation,
                                 bool isAdd);
    bool getVocabularyWords(const std::string &name, std::vector<std::string> &vocabularyRecords);

private:
    // 锁策略：原全局单锁串行化了全部数据库操作，使连接池的并发能力归零
    // （10 个连接同一时刻只允许一个线程操作数据库）。现按表拆分为表级锁：
    //   - dict_mutex_     dict 表（查词，最高频只读路径，不再被用户写操作阻塞）
    //   - usr_mutex_      usr 表（注册/登录/登出/注销）
    //   - history_mutex_  history 表
    //   - collect_mutex_  collect 表（收藏）
    //   - vocab_mutex_    vocabulary 表（生词本）
    //   - init_mutex_     建表与版本迁移（仅启动期，独立锁避免与业务锁竞争）
    // 跨表操作（unregisterUser 删四表、querryWord 查三表）用 std::lock
    // 一次性按固定顺序获取多把锁，避免死锁。
    std::mutex init_mutex_;
    std::mutex dict_mutex_;
    std::mutex usr_mutex_;
    std::mutex history_mutex_;
    std::mutex collect_mutex_;
    std::mutex vocab_mutex_;
    // 获取当前时间的字符串表示（用于历史/收藏/生词本的时间戳记录）
    static std::string getCurrentTimeStr();
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
    // 检查指定表是否已存在某列（用于旧库的平滑升级）
    bool tableHasColumn(std::shared_ptr<sqlite3> conn, const std::string &table,
                        const std::string &column);
    // 执行sql语句的函数
    bool executSQL(std::shared_ptr<sqlite3> db, const std::string &sql, char **errmsg = NULL);
};

#endif