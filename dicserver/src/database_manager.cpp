#include "database_manager.hpp"
#include "password_hash.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>
// IO操作的头文件
#include <fstream>
// 时间的头文件
#include <ctime>
// 异常处理的头文件
#include <stdexcept>
#include <sstream>

using namespace std;

// 单词文本路径
#define DIC_PATH "./dict.txt"

/*
获取当前时间的字符串表示，格式 "YYYY-MM-DD HH:MM:SS"
用于历史/收藏/生词本记录的时间戳
*/
std::string DatabaseManager::getCurrentTimeStr()
{
    time_t now = time(NULL);
    tm local;
    // localtime_r 是线程安全版本，多线程下不会因共享静态缓冲产生竞态
    localtime_r(&now, &local);
    char time_str[20] = "";
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &local);
    return std::string(time_str);
}

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
    // 启动期建表/迁移：独立 init 锁，避免与业务表级锁相互竞争
    lock_guard<mutex> lock(init_mutex_);
    // 从连接池中获取一个数据库连接
    auto conn = getUsrConnection();
    // 准备sql语句，创建用户表和历史记录表的sql语句
    // passwd 为旧版明文字段（保留仅用于平滑迁移兼容）；新用户密码只存 passwd_hash
    const char *createUserSql =
        "create table if not exists usr("
        "name text primary key,"
        "passwd text,"
        "stage int,"
        "role integer not null default 0,"
        "passwd_hash text);";
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
    // 平滑升级：早期版本的 usr 表没有 role 列，这里补齐（0=普通用户）。
    // SQLite 的 ALTER TABLE ADD COLUMN 只能追加带默认值的普通列，不能加主键列，
    // 因此用户 ID 直接使用 SQLite 内置的 rowid 作为稳定标识，无需改表结构。
    if (!tableHasColumn(conn, "usr", "role"))
    {
        const char *addRoleSql = "alter table usr add column role integer not null default 0;";
        if (!executSQL(conn, addRoleSql))
        {
            cerr << "usr table add role column failed" << endl;
            return false;
        }
        cout << "usr table role column added (default 0 = normal user)" << endl;
    }

    // 平滑升级：新增 passwd_hash 列（SHA-256 + 随机盐的哈希值）。
    // 旧用户的 passwd_hash 为空，登录时走明文兼容路径，校验通过后立即升级。
    if (!tableHasColumn(conn, "usr", "passwd_hash"))
    {
        const char *addHashSql = "alter table usr add column passwd_hash text default '';";
        if (!executSQL(conn, addHashSql))
        {
            cerr << "usr table add passwd_hash column failed" << endl;
            return false;
        }
        cout << "usr table passwd_hash column added (legacy plaintext kept for migration)" << endl;
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
    // 创建收藏表：与客户端收藏列表协议对齐，保存完整词条字段
    const char *createCollectSql = "create table if not exists collect("
                                   "name text,"
                                   "word text,"
                                   "mean text,"
                                   "phonetic text,"
                                   "example text,"
                                   "translation text,"
                                   "time text,UNIQUE(name,word));";
    if (!executSQL(conn, createCollectSql))
    {
        cerr << "收藏表创建失败" << endl;
        return false;
    }
    // 平滑升级：早期版本的 collect 表只有 name/word/mean/time 四列，
    // 这里补齐音标/例句/翻译，保证收藏列表能按协议返回完整 6 字段
    const char *collectNewCols[] = {"phonetic", "example", "translation"};
    for (const char *col : collectNewCols)
    {
        if (!tableHasColumn(conn, "collect", col))
        {
            const std::string addColSql = "alter table collect add column " +
                                          std::string(col) + " text default '';";
            if (!executSQL(conn, addColSql))
            {
                cerr << "collect 表补齐 " << col << " 列失败" << endl;
                return false;
            }
            cout << "collect table column added: " << col << endl;
        }
    }
    // 创建生词本表：UNIQUE(name,word) 保证同一用户同一单词只记录一次
    const char *createVocabSql = "create table if not exists vocabulary("
                                 "name text,"
                                 "word text,"
                                 "mean text,"
                                 "phonetic text,"
                                 "example text,"
                                 "translation text,"
                                 "time text,"
                                 "UNIQUE(name,word));";
    if (!executSQL(conn, createVocabSql))
    {
        cerr << "生词本表创建失败" << endl;
        return false;
    }
    // 生词本按用户查询较多，建立索引加速
    const char *createVocabIndexSql = "create index if not exists idx_vocabulary_name on vocabulary(name);";
    if (!executSQL(conn, createVocabIndexSql))
    {
        cerr << "生词本索引创建失败" << endl;
    }
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}
// 初始化单词表，导入词典数据
bool DatabaseManager::initializeDicDB()
{
    // 创建互斥锁，保护单词表
    lock_guard<mutex> lock(init_mutex_);
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
检查指定表是否已存在某列：用于旧数据库的平滑升级判断。
通过 PRAGMA table_info 遍历列名，避免依赖特定 SQLite 版本的语法。
*/
bool DatabaseManager::tableHasColumn(std::shared_ptr<sqlite3> conn, const std::string &table,
                                     const std::string &column)
{
    // 注意：必须用具名 string 存活到 prepare 调用之后，临时对象的 c_str() 会悬垂
    const std::string sqlText = "PRAGMA table_info(" + table + ");";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sqlText.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *colName = sqlite3_column_text(stmt, 1);
        if (colName != nullptr && column == reinterpret_cast<const char *>(colName))
        {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

/*
注册功能：注册时，将用户输入的账号密码存放到usr数据表中，如果该用户名已经存在，则给出报错信息
注册成功时通过 outUserId/outRole 返回新用户的稳定标识（rowid）与角色
*/
bool DatabaseManager::registerUser(const string &name, const string &password,
                                   long long *outUserId, int *outRole)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(usr_mutex_);
    // 从连接池中获取一个数据库连接
    auto conn = getUsrConnection();
    // 准备sql语句，？表示通配符，用于后面的绑定，0表示默认注册表时，所有用户都是离线的
    // 显式列出列名：后续为 usr 增加列时不会因为 values 个数不匹配而出错。
    // 新用户只存哈希（passwd 置空串），明文永不落库
    const char *sql = "insert into usr(name,passwd,passwd_hash,stage) values(?,?,?,0);";
    // 编译sql语句
    sqlite3_stmt *tmt = NULL;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &tmt, NULL) != SQLITE_OK)
    {
        cerr << "sql准备失败" << sqlite3_errmsg(conn.get()) << endl;
        return false;
    }
    // 编译成功，可以给已经编译成功的内容绑定参数
    sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
    // passwd 明文列只对旧库迁移有意义，新用户留空，杜绝明文落库
    static const std::string kEmpty;
    sqlite3_bind_text(tmt, 2, kEmpty.c_str(), -1, SQLITE_STATIC);
    // 哈希由 password_hash 模块生成：16 字节随机盐 + SHA-256(salt+password)
    std::string passwdHash = pw::hashPassword(password);
    sqlite3_bind_text(tmt, 3, passwdHash.c_str(), -1, SQLITE_STATIC);
    // 执行插入操作
    int result = sqlite3_step(tmt);
    sqlite3_finalize(tmt);
    // 对执行结果进行判断，该用户已经存在（失败）用户不存在（注册成功）
    if (result == SQLITE_CONSTRAINT)
    {
        cerr << "该用户已经存在：" << name << endl;
        // 归还连接，避免连接池泄漏
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }
    if (result != SQLITE_DONE)
    {
        cerr << "注册失败：" << sqlite3_errmsg(conn.get()) << endl;
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    // 注册成功：回读新用户的稳定标识与角色，供协议层组装响应
    if (outUserId != nullptr || outRole != nullptr)
    {
        const char *querySql = "select rowid, role from usr where name=?;";
        sqlite3_stmt *qstmt = nullptr;
        if (sqlite3_prepare_v2(conn.get(), querySql, -1, &qstmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(qstmt, 1, name.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(qstmt) == SQLITE_ROW)
            {
                if (outUserId != nullptr)
                {
                    *outUserId = sqlite3_column_int64(qstmt, 0);
                }
                if (outRole != nullptr)
                {
                    *outRole = sqlite3_column_int(qstmt, 1);
                }
            }
            else
            {
                cerr << "注册后回读用户信息失败：" << name << endl;
            }
            sqlite3_finalize(qstmt);
        }
    }

    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    // 程序执行至此，表示注册成功
    return true;
}
// 用户登陆操作
// 登录成功时通过 outUserId/outRole 返回用户的稳定标识（rowid）与角色
int DatabaseManager::loginUser(const string &name, const string &password, bool &is_online,
                               long long *outUserId, int *outRole)
{
    std::lock_guard<std::mutex> lock(usr_mutex_);

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

    // 2. 检查密码是否正确，同时取出用户状态、稳定标识与角色
    // passwd_hash 为主校验路径；passwd 明文列仅服务于旧库平滑迁移
    const char *checkPasswordSql = "SELECT passwd, passwd_hash, stage, rowid, role FROM usr WHERE name=?;";
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

    // 三态密码校验：
    //  a) passwd_hash 非空且格式合法 → 哈希校验（新用户唯一路径）
    //  b) hash 为空但 passwd 明文非空 → 旧库兼容路径，比对成功后立即升级为哈希
    //  c) 两者皆空 → 拒绝（防止空密码绕过）
    const unsigned char *storedPassword = sqlite3_column_text(stmt, 0);
    const unsigned char *storedHash = sqlite3_column_text(stmt, 1);
    const std::string plainText = storedPassword ? reinterpret_cast<const char *>(storedPassword) : "";
    const std::string hashText = storedHash ? reinterpret_cast<const char *>(storedHash) : "";

    bool passwordOk = false;
    bool needsHashUpgrade = false;
    if (!hashText.empty() && pw::isHashedFormat(hashText))
    {
        passwordOk = pw::verifyPassword(password, hashText);
    }
    else if (!plainText.empty())
    {
        // 旧明文数据：恒定时间比较，成功后由调用方立即升级为哈希
        passwordOk = pw::constantTimeEqual(plainText, password);
        needsHashUpgrade = passwordOk;
    }

    if (!passwordOk)
    {
        sqlite3_finalize(stmt);
        return 2; // 密码错误
    }

    // 3. 检查在线状态，并记录用户标识与角色
    is_online = sqlite3_column_int(stmt, 2) == 1;
    long long userId = sqlite3_column_int64(stmt, 3);
    int userRole = sqlite3_column_int(stmt, 4);
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // 4. 平滑迁移：旧明文用户首次登录成功后，立即写入哈希并清空明文列，
    //    之后该用户只可能走哈希路径，明文在库中存留时间窗口最短
    if (needsHashUpgrade)
    {
        const char *upgradeSql = "update usr set passwd_hash=?, passwd='' where name=?;";
        if (sqlite3_prepare_v2(conn.get(), upgradeSql, -1, &stmt, NULL) == SQLITE_OK)
        {
            std::string newHash = pw::hashPassword(password);
            sqlite3_bind_text(stmt, 1, newHash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                cerr << "明文密码升级为哈希失败：" << sqlite3_errmsg(conn.get()) << endl;
            }
            else
            {
                cout << "legacy plaintext password upgraded to hash for user: " << name << endl;
            }
            sqlite3_finalize(stmt);
            stmt = nullptr;
        }
    }

    // 4. 如果用户不在线，更新状态为在线
    if (!is_online)
    {
        const char *updateSql = "UPDATE usr SET stage=1 WHERE name=?;";
        if (sqlite3_prepare_v2(conn.get(), updateSql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "更新状态SQL准备失败: " << sqlite3_errmsg(conn.get()) << endl;
            if (outUserId != nullptr) { *outUserId = userId; }
            if (outRole != nullptr) { *outRole = userRole; }
            ConnectionPool::getInstance().releaseConnection(conn);
            return 0; // 登录成功但状态更新失败
        }

        if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
        {
            cerr << "更新状态参数绑定失败: " << sqlite3_errmsg(conn.get()) << endl;
            sqlite3_finalize(stmt);
            if (outUserId != nullptr) { *outUserId = userId; }
            if (outRole != nullptr) { *outRole = userRole; }
            ConnectionPool::getInstance().releaseConnection(conn);
            return 0; // 登录成功但状态更新失败
        }

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            cerr << "更新用户状态失败: " << sqlite3_errmsg(conn.get()) << endl;
        }
        sqlite3_finalize(stmt);
    }

    // 输出用户标识与角色，供协议层组装 SUCCESS 响应
    if (outUserId != nullptr) { *outUserId = userId; }
    if (outRole != nullptr) { *outRole = userRole; }

    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);
    return 0; // 登录成功
}
// 用户退出操作,将该用户对应的状态整成0即可
const char *DatabaseManager::logoutUser(const string &name)
{
    // 创建互斥锁，保护用户表
    lock_guard<mutex> lock(usr_mutex_);
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
// meaning 输出释义；collectFlag/vocabFlag 输出该单词对当前用户的收藏/生词本状态。
// 标志查询失败不影响"单词已找到"的结论（默认按未收藏处理），保证查询主链路可用。
bool DatabaseManager::querryWord(const string name, const string &word, string &meaning,
                                 int *collectFlag, int *vocabFlag)
{
    bool found = false;
    auto conn = getDictConnection();
    {
        // dict 表为最高频只读路径，独占表级锁，不被任何用户写操作阻塞
        lock_guard<mutex> lock(dict_mutex_);
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

    if (collectFlag != nullptr)
    {
        *collectFlag = 0;
    }
    if (vocabFlag != nullptr)
    {
        *vocabFlag = 0;
    }

    if (found)
    {
        // 查询收藏状态
        if (collectFlag != nullptr)
        {
            lock_guard<mutex> lock(collect_mutex_);
            sqlite3_stmt *tmt = nullptr;
            const char *check_sql = "select count(*) from collect where name=? and word=?;";
            if (sqlite3_prepare_v2(conn.get(), check_sql, -1, &tmt, nullptr) == SQLITE_OK)
            {
                sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(tmt, 2, word.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(tmt) == SQLITE_ROW)
                {
                    *collectFlag = (sqlite3_column_int(tmt, 0) > 0) ? 1 : 0;
                }
                sqlite3_finalize(tmt);
            }
        }
        // 查询生词本状态
        if (vocabFlag != nullptr)
        {
            lock_guard<mutex> lock(vocab_mutex_);
            sqlite3_stmt *tmt = nullptr;
            const char *check_sql = "select count(*) from vocabulary where name=? and word=?;";
            if (sqlite3_prepare_v2(conn.get(), check_sql, -1, &tmt, nullptr) == SQLITE_OK)
            {
                sqlite3_bind_text(tmt, 1, name.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(tmt, 2, word.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(tmt) == SQLITE_ROW)
                {
                    *vocabFlag = (sqlite3_column_int(tmt, 0) > 0) ? 1 : 0;
                }
                sqlite3_finalize(tmt);
            }
        }
    }
    ConnectionPool::getInstance().releaseConnection(conn); // 释放连接
    return found;
}
// 历史记录函数，将给定的信息插入到数据库中
bool DatabaseManager::recordHistory(const string &name, const string &word, const string &meaning, const string &time)
{
    // 创建互斥锁，保护 history 表
    lock_guard<mutex> lock(history_mutex_);

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
    // 创建互斥锁，保护 history 表
    lock_guard<mutex> lock(history_mutex_);

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

const char *DatabaseManager::collectWord(const string &name, const string &word,
                                         const string &meaning, const string &phonetic,
                                         const string &example, const string &translation)
{
    // 创建互斥锁，保护 collect 表
    lock_guard<mutex> lock(collect_mutex_);

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
        // 插入新纪录（显式列出列名，时间戳由内部生成）
        sqlite3_finalize(stmt);
        stmt = NULL;
        const char *sql = "insert into collect(name,word,mean,phonetic,example,translation,time)"
                          " values(?,?,?,?,?,?,?);";
        rc = sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            cerr << "准备插入语句失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup; // 跳转到资源清理
        }
        const std::string timeStr = getCurrentTimeStr();
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, meaning.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, phonetic.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, example.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, translation.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, timeStr.c_str(), -1, SQLITE_TRANSIENT);
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
    // 创建互斥锁，保护 collect 表
    lock_guard<mutex> lock(collect_mutex_);

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
    // 创建互斥锁，保护 collect 表
    lock_guard<mutex> lock(collect_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return false;
    }

    // 准备sql语句：与客户端收藏列表协议对齐，取完整词条字段。
    // 用关联子查询一并带出生词本标志，避免 N+1 次查询。
    const char *sql =
        "select c.word,c.mean,c.phonetic,c.example,c.translation,"
        "(select count(*) from vocabulary v where v.name=c.name and v.word=c.word)"
        " from collect c where c.name=? order by c.time DESC limit 300;";

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
        // 获取字段值（NULL 视为空串，保证记录字段数稳定为 6）
        const unsigned char *word = sqlite3_column_text(stmt, 0);
        const unsigned char *meaning = sqlite3_column_text(stmt, 1);
        const unsigned char *phonetic = sqlite3_column_text(stmt, 2);
        const unsigned char *example = sqlite3_column_text(stmt, 3);
        const unsigned char *translation = sqlite3_column_text(stmt, 4);
        const int vocabFlag = sqlite3_column_int(stmt, 5) > 0 ? 1 : 0;

        if (!word || !meaning)
        {
            continue; // 跳过无效记录
        }

        // 记录格式：单词|^|释义|^|音标|^|例句|^|翻译|^|生词本标志
        string record = string(reinterpret_cast<const char *>(word)) + "|^|" +
                        string(reinterpret_cast<const char *>(meaning)) + "|^|" +
                        string(phonetic ? reinterpret_cast<const char *>(phonetic) : "") + "|^|" +
                        string(example ? reinterpret_cast<const char *>(example) : "") + "|^|" +
                        string(translation ? reinterpret_cast<const char *>(translation) : "") + "|^|" +
                        std::to_string(vocabFlag);

        collectWords.push_back(record);
    }

    sqlite3_finalize(stmt);
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);

    return true;
}

// ============================================================================
// 生词本操作
// ============================================================================

/*
生词本增删：isAdd 为 true 表示加入生词本，为 false 表示移出生词本
返回 "add success" / "remove success" / "already exists" / "not exists" / "error"
*/
const char *DatabaseManager::updateVocabulary(const string &name, const string &word,
                                              const string &meaning, const string &phonetic,
                                              const string &example, const string &translation,
                                              bool isAdd)
{
    // 创建互斥锁，保护生词本表
    lock_guard<mutex> lock(vocab_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return "error";
    }

    sqlite3_stmt *stmt = nullptr;
    const char *result = "error";

    if (isAdd)
    {
        // 先检查是否已存在，避免触发 UNIQUE 约束报错
        const char *check_sql = "select 1 from vocabulary where name=? and word=?;";
        if (sqlite3_prepare_v2(conn.get(), check_sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            // 已在生词本中，直接返回，避免重复插入
            result = "already exists";
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = nullptr;

        // 插入新记录
        const char *insert_sql = "insert into vocabulary values(?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(conn.get(), insert_sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "准备插入语句失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, meaning.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, phonetic.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, example.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, translation.c_str(), -1, SQLITE_STATIC);
        // 记录加入时间，按时间倒序展示
        sqlite3_bind_text(stmt, 7, getCurrentTimeStr().c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            result = "add success";
        }
        else
        {
            cerr << "生词本插入失败：" << sqlite3_errmsg(conn.get()) << endl;
            result = "error";
        }
    }
    else
    {
        // 移出生词本
        const char *delete_sql = "delete from vocabulary where name=? and word=?;";
        if (sqlite3_prepare_v2(conn.get(), delete_sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            // sqlite3_changes 为 0 表示该记录本来就不存在
            result = (sqlite3_changes(conn.get()) > 0) ? "remove success" : "not exists";
        }
        else
        {
            cerr << "生词本删除失败：" << sqlite3_errmsg(conn.get()) << endl;
            result = "error";
        }
    }

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);                            // 确保释放语句
    ConnectionPool::getInstance().releaseConnection(conn); // 确保释放连接
    return result;
}

/*
查询生词本列表，按加入时间倒序，最多返回 300 条
每条记录格式：单词|^|释义|^|音标|^|例句|^|翻译|^|收藏标识(1/0)
与客户端 WordVocabulary::processVocabularyListResponse 的解析格式保持一致
*/
bool DatabaseManager::getVocabularyWords(const string &name, vector<string> &vocabularyRecords)
{
    // 创建互斥锁，保护生词本表
    lock_guard<mutex> lock(vocab_mutex_);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return false;
    }

    // 联表查询生词本，同时带出该单词的收藏状态
    const char *sql =
        "select v.word,v.mean,v.phonetic,v.example,v.translation,"
        "(select count(*) from collect c where c.name=v.name and c.word=v.word) "
        "from vocabulary v where v.name=? order by v.time DESC limit 300;";

    // 预编译sql语句
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    // 绑定信息
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    // 执行查询
    vocabularyRecords.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // 获取字段值，NULL 时用空字符串兜底，避免 std::string 构造空指针崩溃
        const unsigned char *word = sqlite3_column_text(stmt, 0);
        const unsigned char *meaning = sqlite3_column_text(stmt, 1);
        const unsigned char *phonetic = sqlite3_column_text(stmt, 2);
        const unsigned char *example = sqlite3_column_text(stmt, 3);
        const unsigned char *translation = sqlite3_column_text(stmt, 4);
        int collected = sqlite3_column_int(stmt, 5);

        if (!word)
            continue; // 单词为空视为无效记录

        // 按客户端约定的 |^| 分隔格式组装，末尾附加收藏标识
        string record = string(reinterpret_cast<const char *>(word)) + "|^|" +
                        (meaning ? reinterpret_cast<const char *>(meaning) : "") + "|^|" +
                        (phonetic ? reinterpret_cast<const char *>(phonetic) : "") + "|^|" +
                        (example ? reinterpret_cast<const char *>(example) : "") + "|^|" +
                        (translation ? reinterpret_cast<const char *>(translation) : "") + "|^|" +
                        (collected > 0 ? "1" : "0");

        vocabularyRecords.push_back(record);
    }

    sqlite3_finalize(stmt);
    // 释放连接
    ConnectionPool::getInstance().releaseConnection(conn);

    return true;
}

// ============================================================================
// 账号注销
// ============================================================================

/*
注销账号：校验密码后删除用户，并级联删除其历史/收藏/生词本记录
返回 "unregister success" / "user not found" / "password error" / "error"
删除前通过 outUserId 输出被注销用户的稳定标识（rowid），供客户端清理本地数据
*/
const char *DatabaseManager::unregisterUser(const string &name, const string &password,
                                            long long *outUserId)
{
    // 跨表删除需保证原子性：按固定顺序（usr -> history -> collect -> vocab）
    // 用 std::lock 一次性获取四把表级锁并 adopt_lock 接管。
    // 固定顺序是全项目锁序约定，任何跨表路径都必须遵守，否则可能死锁。
    std::lock(usr_mutex_, history_mutex_, collect_mutex_, vocab_mutex_);
    std::lock_guard<std::mutex> lockUsr(usr_mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> lockHistory(history_mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> lockCollect(collect_mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> lockVocab(vocab_mutex_, std::adopt_lock);

    // 从连接池获取连接 - 使用用户数据库连接
    auto conn = getUsrConnection();
    if (!conn)
    {
        cerr << "获取数据库连接失败" << endl;
        return "error";
    }

    sqlite3_stmt *stmt = nullptr;
    const char *result = "error";
    // 注意：C++ 不允许 goto 跨越变量初始化，因此这里把后面需要用到的指针
    // 统一提前声明，保证 goto cleanup 可以安全跳转
    const char *cleanupSqls[] = {
        "delete from history where name=?;",
        "delete from collect where name=?;",
        "delete from vocabulary where name=?;"};
    const char *deleteSql = "delete from usr where name=?;";

    // 1. 查询用户是否存在并取出存储的密码，同时记录用户标识（删除前取出）
    const char *check_sql = "select passwd, passwd_hash, rowid from usr where name=?;";
    if (sqlite3_prepare_v2(conn.get(), check_sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        // 用户不存在
        result = "user not found";
        goto cleanup;
    }
    // 密码三态校验（与 loginUser 一致）：哈希优先，明文仅兼容旧库；
    // 注销是终态操作，明文校验通过后也无需升级哈希
    {
        const unsigned char *storedPassword = sqlite3_column_text(stmt, 0);
        const unsigned char *storedHash = sqlite3_column_text(stmt, 1);
        const std::string plainText = storedPassword ? reinterpret_cast<const char *>(storedPassword) : "";
        const std::string hashText = storedHash ? reinterpret_cast<const char *>(storedHash) : "";
        bool passwordOk = false;
        if (!hashText.empty() && pw::isHashedFormat(hashText))
        {
            passwordOk = pw::verifyPassword(password, hashText);
        }
        else if (!plainText.empty())
        {
            passwordOk = pw::constantTimeEqual(plainText, password);
        }
        if (!passwordOk)
        {
            // 密码错误，不允许注销
            result = "password error";
            goto cleanup;
        }
    }
    if (outUserId != nullptr)
    {
        *outUserId = sqlite3_column_int64(stmt, 2);
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // 2. 删除用户关联的历史/收藏/生词本记录
    for (const char *cleanupSql : cleanupSqls)
    {
        if (sqlite3_prepare_v2(conn.get(), cleanupSql, -1, &stmt, NULL) != SQLITE_OK)
        {
            cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            cerr << "清理关联数据失败：" << sqlite3_errmsg(conn.get()) << endl;
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = nullptr;
    }

    // 3. 删除用户本身
    if (sqlite3_prepare_v2(conn.get(), deleteSql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "SQL准备失败：" << sqlite3_errmsg(conn.get()) << endl;
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        result = (sqlite3_changes(conn.get()) > 0) ? "unregister success" : "user not found";
    }
    else
    {
        cerr << "注销失败：" << sqlite3_errmsg(conn.get()) << endl;
        result = "error";
    }

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);                            // 确保释放语句
    ConnectionPool::getInstance().releaseConnection(conn); // 确保释放连接
    return result;
}