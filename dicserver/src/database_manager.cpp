#include "database_manager.hpp"
// IO操作的头文件
#include <fstream>
// 时间的头文件
#include <ctime>
// 异常处理的头文件
#include <stdexcept>
#include <sstream>
// 单词文本路径
#define DIC_PATH "./dict.txt"
/*
构造函数,自动初始化连接池
*/
DatabaseManager::DatabaseManager()
{
}
// 析构函数，关闭数据库连接
DatabaseManager::~DatabaseManager() = default;

// 初始化数据库(用户表+单词表+历史表+收藏表)
bool DatabaseManager::initalizeDatabase()
{
    // 用户表和单词表都成功后，表示数据库初始化成功
    return initializeUserDB() && initializeDicDB();
}
// 初始化用户表
bool DatabaseManager::initializeUserDB()
{
    // 使用互斥锁锁定数据库操作，防止出现多线程竞态
    lock_guard<mutex> lock(db_mutex_);
    // 从连接池中获取一个数据库连接
    auto conn = getUsrConnection();
    // 准备sql语句，创建用户表和历史记录表的sql语句
    const char *createUserSql =
        "create table if not exists usr("
        "name text primary key,"
        "passwd int,"
        "stage int);";
    // 执行sql语句
    if (!executSQL(conn, createUserSql))
    {
        cerr << "用户表创建失败" << endl;
        return false;
    }
    // 创建历史表
    const char *createHistorySql =
        "create table if not exists history("
        "name text,"
        "word text,"
        "mean text,"
        "time text);";
    if (!executSQL(conn, createHistorySql))
    {
        cerr << "历史表创建失败" << endl;
        return false;
    }
    // 添加索引(如果已经存在不会重复创建)
    const char *createIndexSql = "create index if not exists index_name on usr(name);";
    if (!executSQL(conn, createIndexSql))
    {
        cerr << "索引创建失败" << endl;
    }
    // 重置所有的客户端都为离线状态
    const char *resetSql = "update usr set stage=0;";
    if (!executSQL(conn, resetSql))
    {
        cerr << "用户状态重置失败" << endl;
        return false;
    }
    // 创建收藏表
    const char *createCollectSql = "create table if not exists collect("
                                   "name text,"
                                   "word text,"
                                   "mean text,"
                                   "time text,UNIQUE(name,word));";
    if (!executSQL(conn, createCollectSql))
    {
        cerr << "收藏表创建失败" << endl;
        return false;
    }
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}
// 初始化单词表，导入词典数据
bool DatabaseManager::initializeDicDB()
{
    // 创建互斥锁，保护单词表
    lock_guard<mutex> lock(db_mutex_);
    // 准备创建单词数据库的sql语句
    const char *createDictSql = "create table if not exists dict(word text,mean text);";
    // 从连接池中获取一个数据库连接
    auto conn = getDictConnection();
    if (!executSQL(conn, createDictSql))
    {
        cerr << "单词表创建失败" << endl;
        return false;
    }
    // 添加单词表索引
    const char *createDictIndexSql =
        "create index if not exists idx_dict_word  on dict(word);";
    if (!executSQL(conn, createDictIndexSql))
    {
        cerr << "单词表索引创建失败" << endl;
        return false;
    }
    // 检查词典数据库是否为空
    // 查询数据库中的记录个数
    // 接收预处理sql的结果
    const char *sql = "select count(*) from dict;";
    sqlite3_stmt *stmt;
    /*
    将sql语句进行预编译一遍，用于后面的执行
    参数1：数据库句柄，参数2：要编译的sql语句，参数3：-1表示sql是一个字符串
    参数4：编译后的结果存放的位置，参数5：接收没有处理的剩余的sql语句
    */
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "sql准备失败" << sqlite3_errmsg(conn.get()) << endl;
        return false;
    }
    // 标识是否需要导入数据
    bool need_import = false;
    // 用step执行sql语句,如果返回SQLITE_ROW表示成功执行
    // 如果sqlite3_column_int为0，表示数据为空，标识为需要导入数据
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (sqlite3_column_int(stmt, 0) == 0)
        {
            need_import = true;
        }
    }
    // 释放stmt空间，也就是释放预编译sql语句的空间
    sqlite3_finalize(stmt);
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    // 如果需要导入，返回导入函数的结果，如果已经有了数据表的内容，则直接返回
    return need_import ? dicToDatabase() : true;
}
// 从文件导入词典数据到数据库
bool DatabaseManager::dicToDatabase()
{
    // 使用文件操作，打开文件dic.txt
    ifstream file(DIC_PATH);
    if (!file)
    {
        cerr << "无法打开词典文件：" << DIC_PATH << endl;
        return false;
    }
    // 从连接池中获取一个数据库连接
    auto conn = getDictConnection();
    // 定义用于读取文件中一行数据的变量
    string line;
    // 循环从文件中读取一行信息放入到line中
    while (getline(file, line))
    {
        // 跳过空格
        if (line.empty())
            continue;
        // 分割单词和意思（格式：单词 意思）
        // 定位到空格所在位置
        size_t pos = line.find(' ');
        // 如果没有该位置，就表示改行没有空格
        if (pos == string::npos)
        {
            cerr << "无效的词典条目（缺少空格分隔）" << line << endl;
            continue;
        }
        // 将单词和意思分别存储
        // 将当前行中的单词，意思截取出来
        string word = line.substr(0, pos);
        string mean = line.substr(pos + 1);
        // 准备sql语句，可以使用通配符表示
        const char *sql = "insert into dict values(?,?);";
        // 预编译sql语句
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "sql准备失败" << sqlite3_errmsg(conn.get()) << endl;
            file.close();
            return false;
        }
        // 预编译成功，可以给已经预编译成功的内容绑定参数
        sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, mean.c_str(), -1, SQLITE_STATIC);
        // 执行插入语句
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            cerr << "数据插入失败" << sqlite3_errmsg(conn.get()) << endl;
            sqlite3_finalize(stmt);
            file.close();
            return false;
        }
        // 程序执行至此，表示单词已经导入到数据库了,释放预编译sql语句的空间

        sqlite3_finalize(stmt);
        // 释放连接
        ConnectionPool::getInstance().releaseConnection(conn);
    }
    // 关闭文件
    file.close();
    cout << "单词库导入成功" << endl;
    return true;
}
// 执行sql语句的函数
/*
参数1：数据库句柄，参数2：要执行的sql语句，参数3：错误信息
*/
bool DatabaseManager::executSQL(std::shared_ptr<sqlite3> db, const string &sql, char **errmsg)
{
    if (sqlite3_exec(db.get(), sql.c_str(), NULL, NULL, errmsg) != SQLITE_OK)
    {
        cerr << "sql执行失败：" << *errmsg << endl;
        // 释放错误信息空间
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}
/*
注册功能：注册时，将用户输入的账号密码存放到usr数据表中，如果该用户名已经存在，则给出报错信息
*/
bool DatabaseManager::registerUser(const string &name, const string &password)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);
    // 从连接池中获取一个数据库连接
    auto conn = getUsrConnection();
    // 准备sql语句，？表示通配符，用于后面的绑定，0表示默认注册表时，所有用户都是离线的
    const char *sql = "insert into usr values(?,?,0);";
    // 编译sql语句
    sqlite3_stmt *tmt = NULL;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &tmt, NULL) != SQLITE_OK)
    {
        cerr << "sql准备失败" << sqlite3_errmsg(conn.get()) << endl;
        return false;
    }
    // 编译成功，可以给已经编译成功的内容绑定参数
    sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(tmt, 2, password.c_str(), -1, SQLITE_STATIC);
    // 执行插入操作
    int result = sqlite3_step(tmt);
    sqlite3_finalize(tmt);
    // 对执行结果进行判断，该用户已经存在（失败）用户不存在（注册成功）
    if (result == SQLITE_CONSTRAINT)
    {
        cerr << "该用户已经存在：" << name << endl;
        return false;
    }
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    // 程序执行至此，表示注册成功
    return result == SQLITE_DONE;
}
// 用户登陆操作
int DatabaseManager::loginUser(const string &name, const string &password, bool &is_online)
{
    std::lock_guard<std::mutex> lock(db_mutex_);

    // 获取数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "无效的数据库连接" << endl;
        return -1; // 数据库错误
    }

    // 1. 检查用户是否存在
    const char *checkUserSql = "SELECT COUNT(*) FROM usr WHERE name=?;";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(conn.get(), checkUserSql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "用户检查SQL准备失败: " << sqlite3_errmsg(conn.get()) << endl;
        return -1;
    }

    if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        cerr << "用户检查参数绑定失败: " << sqlite3_errmsg(conn.get()) << endl;
        sqlite3_finalize(stmt);
        return -1;
    }

    int result = sqlite3_step(stmt);
    if (result != SQLITE_ROW)
    {
        cerr << "用户检查执行失败: " << sqlite3_errmsg(conn.get()) << endl;
        sqlite3_finalize(stmt);
        return -1;
    }

    int userCount = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (userCount == 0)
    {
        return 1; // 用户不存在
    }

    // 2. 检查密码是否正确
    const char *checkPasswordSql = "SELECT passwd, stage FROM usr WHERE name=?;";
    if (sqlite3_prepare_v2(conn.get(), checkPasswordSql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "密码检查SQL准备失败: " << sqlite3_errmsg(conn.get()) << endl;
        return -1;
    }

    if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        cerr << "密码检查参数绑定失败: " << sqlite3_errmsg(conn.get()) << endl;
        sqlite3_finalize(stmt);
        return -1;
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_ROW)
    {
        cerr << "密码检查执行失败: " << sqlite3_errmsg(conn.get()) << endl;
        sqlite3_finalize(stmt);
        return -1;
    }

    const unsigned char *storedPassword = sqlite3_column_text(stmt, 0);
    if (storedPassword == nullptr || strcmp(reinterpret_cast<const char *>(storedPassword), password.c_str()) != 0)
    {
        sqlite3_finalize(stmt);
        return 2; // 密码错误
    }

    // 3. 检查在线状态
    is_online = sqlite3_column_int(stmt, 1) == 1;
    sqlite3_finalize(stmt);

    // 4. 如果用户不在线，更新状态为在线
    if (!is_online)
    {
        const char *updateSql = "UPDATE usr SET stage=1 WHERE name=?;";
        if (sqlite3_prepare_v2(conn.get(), updateSql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "更新状态SQL准备失败: " << sqlite3_errmsg(conn.get()) << endl;
            return 0; // 登录成功但状态更新失败
        }

        if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
        {
            cerr << "更新状态参数绑定失败: " << sqlite3_errmsg(conn.get()) << endl;
            sqlite3_finalize(stmt);
            return 0; // 登录成功但状态更新失败
        }

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            cerr << "更新用户状态失败: " << sqlite3_errmsg(conn.get()) << endl;
        }
        sqlite3_finalize(stmt);
    }

    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    return 0; // 登录成功
}
// 用户退出操作,将该用户对应的状态整成0即可
const char *DatabaseManager::logoutUser(const string &name)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);
    const char *result = "error";
    auto conn = getUsrConnection(); // 从连接池获取连接
    // 预编译sql语句
    sqlite3_stmt *tmt = NULL;
    // 先判断用户是否已经退出,再进行退出
    const char *check_sql = "select stage from usr where name = ?";
    const char *sql = "update usr set stage=0 where name=?;";
    if (sqlite3_prepare_v2(conn.get(), check_sql, -1, &tmt, NULL) != SQLITE_OK)
    {
        cerr << "sql准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
    sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(tmt) == SQLITE_ROW)
    {
        int user_stage = sqlite3_column_int(tmt, 0);
        if (user_stage != 1)
        {
            // 用户已经下线，后面的更改不需要进行操作
            result = "user not online";
            goto cleanup;
        }
    }
    // 此时登录用户已经存在
    sqlite3_finalize(tmt);
    tmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &tmt, NULL) != SQLITE_OK)
    {
        cerr << "sql准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
    // 绑定信息
    sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
    // 执行更新操作

    if (sqlite3_step(tmt) != SQLITE_DONE)
    {
        result = "user offline fail";
        cerr << "插入失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
    else
    {
        result = "user offline success";
        goto cleanup;
    }

cleanup:
    sqlite3_finalize(tmt);
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    return result;
}
// 查询单词
bool DatabaseManager::querryWord(const string name, const string &word, string &meaning, string &flage)
{
    bool found = false;
    auto conn = getDictConnection();
    {
        lock_guard<mutex> lock(db_mutex_);
        const char *sql = "select mean from dict where word=?;";
        sqlite3_stmt *tmt = NULL;

        if (sqlite3_prepare_v2(conn.get(), sql, -1, &tmt, NULL) != SQLITE_OK)
        {
            cerr << "sql准备失败：" << sqlite3_errmsg(conn.get()) << endl;
            ConnectionPool::getInstance().releaseConnection(conn); // 释放连接
            return false;
        }

        sqlite3_bind_text(tmt, 1, word.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(tmt) == SQLITE_ROW)
        {
            const unsigned char *result = sqlite3_column_text(tmt, 0);
            if (result)
            {
                meaning = reinterpret_cast<const char *>(result);
                found = true;
            }
        }
        sqlite3_finalize(tmt);
    }
    {
        lock_guard<mutex> lock(db_mutex_);
        sqlite3_stmt *tmt;
        // 检查这个单词是否被收藏
        const char *check_sql = "select count(*) from collect where name=? and word=?";
        if (sqlite3_prepare_v2(conn.get(), check_sql, -1, &tmt, nullptr) != SQLITE_OK)
        {
            cerr << "sql准备失败：" << sqlite3_errmsg(conn.get()) << endl;
            ConnectionPool::getInstance().releaseConnection(conn); // 释放连接
            return false;
        }
        sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(tmt, 2, word.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(tmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(tmt, 0);
            flage = (count > 0) ? "alreday collect" : "no collect";
        }
        else
        {
            flage = "no collect";
        }

        sqlite3_finalize(tmt);
    }
    ConnectionPool::getInstance().releaseConnection(conn); // 释放连接
    return found;
}
// 历史记录函数，将给定的信息插入到数据库中
bool DatabaseManager::recordHistory(const string &name, const string &word, const string &meaning, const string &time)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return false;
    }

    // 准备sql语句
    const char *sql = "insert into history values(?,?,?,?);";

    // 预编译sql语句
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        return false;
    }

    // 绑定信息
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, meaning.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, time.c_str(), -1, SQLITE_STATIC);

    // 执行插入操作
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);

    return result == SQLITE_DONE;
}

bool DatabaseManager::getHistory(const string &name, vector<string> &historyRecords)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return false;
    }

    // 准备sql语句
    const char *sql =
        "select word,mean,time from history where name=? order by time DESC limit 300;";

    // 预编译sql语句
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        return false;
    }

    // 绑定信息
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    // 执行查询
    historyRecords.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // 获取字段值
        const unsigned char *word = sqlite3_column_text(stmt, 0);
        const unsigned char *meaning = sqlite3_column_text(stmt, 1);
        const unsigned char *time = sqlite3_column_text(stmt, 2);

        // 检查字段是否为空
        if (!word && !meaning && !time)
        {
            continue; // 跳过无效记录
        }

        // 格式化记录
        string record = string(reinterpret_cast<const char *>(word)) + ": " +
                        string(reinterpret_cast<const char *>(meaning)) + " (" +
                        string(reinterpret_cast<const char *>(time)) + ")";

        historyRecords.push_back(record);
    }

    sqlite3_finalize(stmt);
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);

    return true;
}

const char *DatabaseManager::collectWord(const string &name, const string &word, const string &meaning, const string &time)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return "error";
    }
    // 检查是否已经收藏
    const char *check_sql = "select 1 from collect where name=? and word=?;";
    sqlite3_stmt *stmt = NULL;
    const char *result = "error";
    int rc = sqlite3_prepare_v2(conn.get(), check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup; // 跳转到资源清理
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        result = "already collected";
        goto cleanup; // 跳转到资源清理
    }
    else
    {
        // 插入新纪录
        sqlite3_finalize(stmt);
        stmt = NULL;
        const char *sql = "insert into collect values(?,?,?,?)";
        rc = sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            cerr << "准备插入语句失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup; // 跳转到资源清理
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, meaning.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, time.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE)
        {
            result = "success";
        }
        else
        {
            cerr << "插入失败：" << sqlite3_errmsg(conn.get()) << endl;
            result = "error";
            goto cleanup; // 跳转到资源清理
        }
    }
cleanup:
    if (stmt)
        sqlite3_finalize(stmt);                            // 确保释放语句
    ConnectionPool::getInstance().releaseConnection(conn); // 确保释放连接
    return result;
}

const char *DatabaseManager::cancelCollectWord(const string &name, const string &word)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return "error";
    }
    sqlite3_stmt *stmt = nullptr;
    const char *result = "delect error";
    // 开始取消收藏
    const char *sql = "delete from collect where name =? and word =?";
    int rc = sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        result = "delete success";
    }
    else
    {
        cerr << "取消收藏失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
cleanup:
    if (stmt)
        sqlite3_finalize(stmt);                            // 确保释放语句
    ConnectionPool::getInstance().releaseConnection(conn); // 确保释放连接
    return result;
}

bool DatabaseManager::getCollectWords(const string &name, vector<string> &collectWords)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(db_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return false;
    }

    // 准备sql语句
    const char *sql =
        "select word,mean,time from collect where name=? order by time DESC limit 300;";

    // 预编译sql语句
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        return false;
    }

    // 绑定信息
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    // 执行查询
    collectWords.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // 获取字段值
        const unsigned char *word = sqlite3_column_text(stmt, 0);
        const unsigned char *meaning = sqlite3_column_text(stmt, 1);
        const unsigned char *time = sqlite3_column_text(stmt, 2);

        // 检查字段是否为空
        if (!word || !meaning || !time)
        {
            continue; // 跳过无效记录
        }

        // 格式化记录
        string record = string(reinterpret_cast<const char *>(word)) + ": " +
                        string(reinterpret_cast<const char *>(meaning)) + " (" +
                        string(reinterpret_cast<const char *>(time)) + ")";

        collectWords.push_back(record);
    }

    sqlite3_finalize(stmt);
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);

    return true;
}