#ifndef SERVER_H
#define SERVER_H

#include "database_manager.hpp"
#include <memory>
#include <string>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <netinet/in.h> // sockaddr_in
#include "message.hpp"
#include "threadPool.h"
#include"LogLevel .hpp"

class Server
{
public:
    // 构造函数：需要使用数据库管理对象，IP地址，端口号进行构造一个服务器
    Server(DatabaseManager& db_manage,const std::shared_ptr<threadPool> thread_pool,const std::string ip, int port);
    ~Server();

    // 启动服务器
    bool start();
    // 停止服务器
    bool stop();

    void processMessage(int cfd, Msg &msg, sockaddr_in client_addr);
private:
    // 服务器套接字文件描述符
    int sfd_;
    // 端口号
    int port_;
    // ip地址
    std::string ip_;
    // 运行权限
    bool runnning_;
    // 数据库管理器
    DatabaseManager& db_manager_;  // 新增成员变量保存引用
    //线程池管理器
    std::shared_ptr<threadPool> thread_pool_;

    // 处理客户端函数
    void handleClient(int cfd, sockaddr_in client_addr);
    //新的连接继续处理
     // 有新连接，正常处理
    void  handleNewConnection(int  cfd, sockaddr_in client_addr);
    // 响应客户端登录请求函数
    void responseLogin(int cfd, Msg &msg);
    // 响应客户端注册请求函数
    void responseRegister(int cfd, Msg &msg);
    // 响应客户端查询请求函数
    void responseQuery(int cfd, Msg &msg);
    // 响应客户端历史记录请求函数
    void responseHistory(int cfd, Msg &msg);
    // 响应客户端退出请求函数
    void responseQuit(int cfd, Msg &msg);
    // 响应客户端心跳请求函数
    void responseHeartbeat(int cfd, Msg &msg,sockaddr_in client_addr);
    // 提供一个获取当前系统时间的函数
    static std::string getCurrentTime();
    // 响应客户端收藏请求函数
    void responseCollect(int cfd, Msg &msg);
    //响应客户端收藏记录请求
    void responseCollectRecord(int cfd, Msg &msg);
    // 响应客户端注销（删除账号）请求函数
    void responseUnregister(int cfd, Msg &msg);
    // 响应客户端生词本增删请求函数
    void responseVocabulary(int cfd, Msg &msg);
    // 响应客户端生词本列表请求函数
    void responseVocabularyList(int cfd, Msg &msg);
    // 安全拷贝：确保目标缓冲区以 '\0' 结尾，避免 strncpy 写满时缺少终止符导致越界
    static void safeStrcpy(char *dst, size_t dst_size, const char *src);
    // 安全拷贝（std::string 版本）
    static void safeStrcpy(std::string &dst, const std::string &src, size_t max_len);

    // ---- 协议版本与统一发送 ----
    // 每条连接按首帧判定协议版本（v1 定长 / v2 变长帧），后续收发都按该版本，
    // 同一个端口可以同时服务新旧客户端。
    void setProtocolVersion(int cfd, int version);
    // 取连接协议版本，未知时按 v1 兜底（旧客户端行为不变）
    int protocolVersion(int cfd) const;
    void eraseProtocolVersion(int cfd);
    // 统一发送出口：按连接版本序列化，循环 send 直到发完（处理 EAGAIN 重试）
    bool sendMsg(int cfd, const Msg &msg, int flags);

    // cfd -> ProtocolFrame::Version。连接数有限（每连接一个工作线程），
    // 读写都很少，用一把互斥锁保护足够，不必引入无锁结构。
    mutable std::mutex version_mutex_;
    std::unordered_map<int, int> conn_version_;
};

#endif