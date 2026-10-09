#ifndef SERVER_H
#define SERVER_H

#include "database_manager.hpp"
#include <myhead.h>
#include <memory>
#include <time.h>
#include <unordered_map>
#include "message.hpp"
#include "threadPool.h"
#include"LogLevel .hpp"

class Server
{
public:
    // 构造函数：需要使用数据库管理对象，IP地址，端口号进行构造一个服务器
    Server(DatabaseManager& db_manage,const shared_ptr<threadPool> thread_pool,const string ip, int port);
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
    string ip_;
    // 运行权限
    bool runnning_;
    // 数据库管理器
    DatabaseManager& db_manager_;  // 新增成员变量保存引用
    //线程池管理器
    shared_ptr<threadPool> thread_pool_;

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
    static string getCurrentTime();
    // 响应客户端收藏请求函数
    void responseCollect(int cfd, Msg &msg);
    //响应客户端收藏记录请求
    void responseCollectRecord(int cfd, Msg &msg);
   
};

#endif