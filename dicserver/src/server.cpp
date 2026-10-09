#include "server.hpp"
#include <myhead.h>
#include <time.h>
#include <thread>

// 服务器的构造函数：完成套接字创建，端口号快速重用，绑定IP地址和端口号
Server::Server(DatabaseManager &db_manage, const shared_ptr<threadPool> thread_pool, const string ip, int port) : db_manager_(db_manage), thread_pool_(thread_pool), ip_(ip), port_(port), runnning_(false)
{
    // 创建套接字
    this->sfd_ = socket(AF_INET, SOCK_STREAM, 0);
    HANDLE_INIT_ERROR(sfd_ < 0);
    // 端口号快速重用
    int opt = 1;
    HANDLE_INIT_ERROR(setsockopt(sfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0);
    // if (setsockopt(sfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    // {
    //     close(sfd_);
    //     throw runtime_error("setsockopt error");
    // }
    // 绑定端口号，IP地址
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(ip.c_str());
    HANDLE_INIT_ERROR(bind(sfd_, (sockaddr *)&sin, sizeof(sin)) < 0);
    // if (bind(sfd_, (sockaddr *)&sin, sizeof(sin)) < 0)
    // {
    //     close(sfd_);
    //     throw runtime_error("bind error");
    // }
    // cerr << "bind success" << endl;
}
// 析构函数
Server::~Server()
{
    stop();
}
// 启动服务器
bool Server::start()
{
    // 添加 listen()
    if (listen(sfd_, 128) < 0)
    {
        LOG(LogLevel::ERROR, "listen error: " << strerror(errno));
        return false;
    }

    LOG(LogLevel::INFO, "Server listening on " << ip_ << ":" << port_);
    // 设置socket为非阻塞模式，避免accept阻塞
    int flags = fcntl(sfd_, F_GETFL, 0);
    fcntl(sfd_, F_SETFL, flags | O_NONBLOCK);

    runnning_ = true;

    while (runnning_)
    {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int cfd = accept(sfd_, (sockaddr *)&client_addr, &addr_len);

        if (cfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 没有新连接，正常等待
                usleep(100000); // 100ms
                continue;
            }
            else if (errno == EINTR)
            {
                // 被信号中断，继续尝试
                continue;
            }
            else
            {
                LOG(LogLevel::ERROR, "accept error: " << strerror(errno));
                if (runnning_)
                    continue; // 严重错误也继续运行
            }
        }
        else
        {
            // 有新连接，正常处理
            handleNewConnection(cfd, client_addr);
        }
    }

    close(sfd_);
    LOG(LogLevel::INFO, "Server stopped gracefully");
    return true;
}

void Server::handleNewConnection(int cfd, sockaddr_in client_addr)
{
    // 设置客户端socket为非阻塞
    int flags = fcntl(cfd, F_GETFL, 0);
    fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

    // 使用try-catch确保异常不会影响服务器
    try
    {
        auto client_addr_copy = client_addr;
        thread_pool_->addTask([this, cfd, client_addr_copy]()
                              {
            try {
                handleClient(cfd, client_addr_copy);
            }
            catch (const std::exception& e) {
                LOG(LogLevel::ERROR, "处理客户端时发生异常: " << e.what());
            }
            close(cfd); });
    }
    catch (const std::exception &e)
    {
        LOG(LogLevel::ERROR, "添加任务到线程池失败: " << e.what());
        close(cfd);
    }
}
// 停止服务器
bool Server::stop()
{
    if (runnning_)
    {
        runnning_ = false;
        if (sfd_ >= 0)
        {
            // 关闭监听套接字
            close(sfd_);
            sfd_ = -1; // 设置为无效套接字
        }

        LOG(LogLevel::INFO, "Server stopped gracefully");
        return true;
    }
    return false;
}
// 处理客户端函数
void Server::handleClient(int cfd, sockaddr_in client_addr)
{
    Msg msg;
    std::string client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    std::string username; // 记录当前连接的用户名

    LOG(LogLevel::INFO, "开始处理客户端 " << client_ip << ":" << client_port << " 的连接");

    while (true)
    {
        // 清空 msg 结构体
        memset(&msg, 0, sizeof(msg));

        // 接收消息 - 关键修复：立即检查返回值
        ssize_t recv_len = recv(cfd, &msg, sizeof(msg), 0);
        // 立即检查接收结果，不处理无效数据
        if (recv_len == 0)
        {
            // 客户端正常断开连接
            LOG(LogLevel::INFO, "客户端主动断开连接: " << client_ip << ":" << client_port);
            break;
        }
        else if (recv_len < 0)
        {
            // 接收错误
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 非阻塞模式下的正常情况，继续等待
                continue;
            }
            else
            {
                LOG(LogLevel::ERROR, "接收客户端 " << client_ip << ":" << client_port << " 数据错误: " << strerror(errno));
                break;
            }
        }
        else if (recv_len != sizeof(msg))
        {
            // 收到不完整的数据包
            LOG(LogLevel::INFO, "收到不完整消息: 期望" << sizeof(msg) << "字节，实际" << recv_len << "字节");
            continue; // 继续等待完整数据
        }

        // 只有收到完整消息时才继续处理
        msg.hostByteOrder();

        // 验证消息类型有效性
        if (msg.type < 1 || msg.type > 10)
        { // 根据您的消息类型范围调整
            LOG(LogLevel::INFO, "收到无效消息类型: " << static_cast<int>(msg.type));
            continue; // 跳过无效消息，继续处理下一条
        }

        // 正常处理消息
        LOG(LogLevel::DEBUG, "收到客户端消息，类型: " << static_cast<int>(msg.type));
        // 在登录成功后记录用户名
        if (msg.type == L)
        {
            username = msg.name; // 记录登录的用户名
        }
        processMessage(cfd, msg, client_addr);
    }
    // 连接关闭处理 - 新增状态更新
    if (!username.empty())
    {
        // 调用登出函数更新数据库
        const char *result = db_manager_.logoutUser(username);
        LOG(LogLevel::INFO, "更新用户状态结果: " << username << " " << result);
    }
    // 连接关闭处理
    close(cfd);
    LOG(LogLevel::INFO, "客户端连接处理结束: " << client_ip << ":" << client_port);
}

void Server::processMessage(int cfd, Msg &msg, sockaddr_in client_addr)
{
    std::string client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);

    cout << "msg.type = " << static_cast<int>(msg.type) << " msg.name = " << msg.name << endl;

    switch (msg.type)
    {
    case R: // 注册
        responseRegister(cfd, msg);
        break;
    case L: // 登录
        responseLogin(cfd, msg);
        break;
    case Q: // 退出
        responseQuit(cfd, msg);
        break;
    case S: // 查询
        responseQuery(cfd, msg);
        break;
    case H: // 历史记录
        responseHistory(cfd, msg);
        break;
    case A: // 心跳检测
        //responseHeartbeat(cfd, msg, client_addr);
        break;
    case C: // 收藏单词
        responseCollect(cfd, msg);
        break;
    case D: // 收藏记录
        responseCollectRecord(cfd, msg);
        break;
    default:
        // 这里应该不会执行，因为前面已经验证了消息类型
        LOG(LogLevel::ERROR, "未处理的消息类型: " << static_cast<int>(msg.type));
        break;
    }
}

// 提供一个获取当前系统时间的函数
string Server::getCurrentTime()
{
    // 保存查找单词的记录
    /*
    获取当前系统时间对应的参数，将以秒为单位的时间转变为时间的结构体
    保存当前系统时间的字符串中，将结构体中的年月日时分秒转变成格式串
    */
    time_t now = time(NULL);
    tm *local = localtime(&now);
    char time_str[20] = "";
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local);
    return string(time_str);
}

// 响应客户端心跳检测请求函数
void Server::responseHeartbeat(int cfd, Msg &msg, sockaddr_in client_addr)
{
    std::string client_ip = inet_ntoa(client_addr.sin_addr);

    Msg heartbeatResp;
    memset(&heartbeatResp, 0, sizeof(heartbeatResp));
    heartbeatResp.type = A;
    strncpy(heartbeatResp.text, "ALIVE", sizeof(heartbeatResp.text) - 1);
    heartbeatResp.networkByteOrder();

    if (send(cfd, &heartbeatResp, sizeof(heartbeatResp), 0) < 0)
    {
        // 发送失败，可能是连接已关闭
        LOG(LogLevel::DEBUG, "发送心跳响应失败，连接可能已关闭: " << client_ip);
        return; // 不抛出异常，由调用者处理连接状态
    }
    else
    {
        LOG(LogLevel::DEBUG, "心跳响应已发送: " << client_ip);
    }
}

// 响应客户端注册请求函数
void Server::responseRegister(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering Register");
    // 执行注册操作
    // 用success存储返回结果，如果注册成功回复OK，否则返回EXISTS
    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = R;
    strncpy(response.name, msg.name, sizeof(response.name));
    int success = db_manager_.registerUser(msg.name, msg.text);
    strcpy(response.text, success ? "**OK**" : "**EXISTS**");
    LOG(LogLevel::INFO, "Register attempt: " << msg.name << " - "
                                             << (success ? "SUCCESS" : "EXISTS"));
    // 发送请求
    // 转变为网络字节序
    response.networkByteOrder();

    if (send(cfd, &response, sizeof(response), 0) < 0)
    {
        LOG(LogLevel::ERROR, "注册的结果发送失败" << strerror(errno));
        return;
    }
}
// 响应客户端登录请求函数
void Server::responseLogin(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering Login");

    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = L;
    strncpy(response.name, msg.name, sizeof(response.name));
    bool is_online = false;

    // 执行登录函数
    int loginResult = db_manager_.loginUser(msg.name, msg.text, is_online);

    // 添加详细的调试信息
    LOG(LogLevel::INFO, "Login result - code: " << loginResult << ", is_online: " << is_online);

    // 根据登录结果设置响应文本
    switch (loginResult)
    {
    case 0: // 登录成功
        strcpy(response.text, is_online ? "**EXISTS**" : "**OK**");
        break;
    case 1: // 用户不存在
        strcpy(response.text, "**NOT_FOUND**");
        break;
    case 2: // 密码错误
        strcpy(response.text, "**PASSWORD_ERROR**");
        break;
    default: // 其他错误
        strcpy(response.text, "**FAIL**");
        break;
    }

    // 发送请求
    response.networkByteOrder();

    size_t send_size = send(cfd, &response, sizeof(response), 0);
    if ( send_size< 0)
    {
        LOG(LogLevel::ERROR, "登录的结果发送失败" << strerror(errno));
        return;
    }
    LOG(LogLevel::INFO,"发送数据大小: "<<send_size);
}
// 响应客户端查询请求函数
void Server::responseQuery(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering Query ");
    // 执行查询操作
    // 保存单词的含义
    // cout << "Entering query function, msg.text = " << msg.text << endl;
    LOG(LogLevel::INFO, "Query word : " << msg.text);
    string meaning;
    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = S;
    strncpy(response.name, msg.name, sizeof(response.name));
    string flage;
    if (db_manager_.querryWord(msg.name, msg.text, meaning, flage))
    {
        string collectStatus = "(" + flage + ")";
        // 成功找到把单词和含义一起返回给用户
        snprintf(response.text, sizeof(response.text), "%s,%s,%s", msg.text,
                 meaning.c_str(), collectStatus.c_str());
        LOG(LogLevel::INFO, "单词查询回包内容：" << response.text);
    }
    else
    {
        strcpy(response.text, "Not found");
    }
    // 发送请求
    // 转变为网络字节序
    response.networkByteOrder();

    if (send(cfd, &response, sizeof(response), 0) < 0)
    {
        LOG(LogLevel::ERROR, "单词查询的结果发送失败" << strerror(errno));
        return;
    }
    // 保存查找单词的记录
    string time_str = getCurrentTime();
    // 调用历史查询记录
    db_manager_.recordHistory(msg.name, msg.text, meaning, time_str);
}
// 响应客户端历史记录请求函数
void Server::responseHistory(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering History");

    // 获取历史记录列表
    std::vector<std::string> historyRecords;
    if (!db_manager_.getHistory(msg.name, historyRecords))
    {
        // 发送错误标志
        Msg errorMsg;
        memset(&errorMsg, 0, sizeof(errorMsg));
        errorMsg.type = H;
        strncpy(errorMsg.name, msg.name, sizeof(errorMsg.name));
        strcpy(errorMsg.text, "**ERROR**");
        errorMsg.networkByteOrder();
        send(cfd, &errorMsg, sizeof(errorMsg), 0);
        return;
    }

    if (historyRecords.empty())
    {
        // 发送空记录标志
        Msg emptyMsg;
        memset(&emptyMsg, 0, sizeof(emptyMsg));
        emptyMsg.type = H;
        strncpy(emptyMsg.name, msg.name, sizeof(emptyMsg.name));
        strcpy(emptyMsg.text, "**EMPTY**");
        emptyMsg.networkByteOrder();
        send(cfd, &emptyMsg, sizeof(emptyMsg), 0);
    }
    else
    {
        // 逐条发送历史记录
        for (size_t i=0;i<historyRecords.size();i++)
        {
            Msg response;
            memset(&response, 0, sizeof(response));
            response.type = H;
            strncpy(response.name, msg.name, sizeof(response.name));
            strncpy(response.text, historyRecords[i].c_str(), sizeof(response.text) - 1);
            response.text[sizeof(response.text) - 1] = '\0';
            response.networkByteOrder();

            ssize_t bytesSent = 0;
            ssize_t totalSize=sizeof(response);
            const char* data=reinterpret_cast<const char*>(&response);

            while(bytesSent<totalSize){
                ssize_t result = send(cfd,data+bytesSent,totalSize-bytesSent,MSG_DONTWAIT);
                if(result<0){
                    if(errno==EAGAIN || errno==EWOULDBLOCK){
                        //发送缓存区满，等待一段时间重试
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }else{
                        //真正的发送错误
                        LOG(LogLevel::ERROR,"发送历史记录失败："<<strerror(errno));

                        //尝试发送错误通知客户端
                        Msg errorMsg;
                        memset(&errorMsg,0,sizeof(errorMsg));
                        errorMsg.type=H;
                        strncpy(errorMsg.name,msg.name,sizeof(errorMsg.name));
                        strcpy(errorMsg.text,"**ERROR**");
                        errorMsg.networkByteOrder();
                        send(cfd,&errorMsg,sizeof(errorMsg),MSG_DONTWAIT);
                        return;
                    }
                }else{
                    bytesSent+=result;
                }
            }
            //每次发送10条记录添加一个小延迟，避免淹没接收端
            if((i+1)%10==0){
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            LOG(LogLevel::DEBUG,"已发送历史记录"<<(i+1)<<"/"<<historyRecords.size());
        }
    }

    // 确保发送结束标志
    Msg overMsg;
    memset(&overMsg, 0, sizeof(overMsg));
    overMsg.type = H;
    strncpy(overMsg.name, msg.name, sizeof(overMsg.name));
    strcpy(overMsg.text, "**OVER**");
    overMsg.networkByteOrder();

   ssize_t result = send(cfd, &overMsg, sizeof(overMsg), MSG_DONTWAIT);
    if (result < 0) {
        LOG(LogLevel::ERROR, "发送历史记录结束标志失败: " << strerror(errno));
    } else {
        LOG(LogLevel::INFO, "历史记录发送完成，共发送 " << historyRecords.size() << " 条记录");
    }
}
// 响应客户端退出请求函数
void Server::responseQuit(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering Quit");

    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = Q; // 退出响应类型
    strncpy(response.name, msg.name, sizeof(response.name));

    // 执行退出操作
    const char *result = db_manager_.logoutUser(msg.name);
    cerr << "quitResult:" << result << endl;
    if (strcmp(result, "user not online") == 0)
    {
        LOG(LogLevel::INFO, msg.name << ":" << "已经下线");
        return;
    }

    if (strcmp(result, "user offline success") == 0)
    {
        strcpy(response.text, "**OK**");
        LOG(LogLevel::INFO, "用户退出成功: " << msg.name);
    }
    else if (strcmp(result, "user offline fail") == 0)
    {
        strcpy(response.text, "**ERROR**");
        LOG(LogLevel::INFO, "用户退出失败: " << msg.name);
    }
    else
    {
        LOG(LogLevel::ERROR, "用户退出失败: " << msg.name << " 原因: " << result);
    }

    response.networkByteOrder();

    if (send(cfd, &response, sizeof(response), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送退出结果失败: " << strerror(errno));
    }
    LOG(LogLevel::INFO, msg.name << " :Logout completed");
}

// 响应客户端收藏请求函数
void Server::responseCollect(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, " 收藏单词: " << msg.name << " " << msg.text);
    Msg response;
    response.type = C;
    // 解析请求文本
    string tempText = msg.text;
    string username = msg.name;
    string word, meaning, timestamp;
    // 查找冒号的位置分割单词和释义
    size_t colonPos = tempText.find(":");
    word = tempText.substr(0, colonPos - 1); // 除去单词后面的逗号
    // 查找括号的位置分割释义和时间
    size_t openParen = tempText.find("(");
    size_t closeParen = tempText.find(")");
    if (openParen != string::npos && closeParen != string::npos && openParen < closeParen)
    {
        // 提取时间戳
        timestamp = tempText.substr(openParen + 1, closeParen - openParen - 1);
        // 提取释义
        meaning = tempText.substr(colonPos + 1, openParen - colonPos - 1);
    }
    else
    {
        strcpy(response.text, "format is incorrect");
        response.networkByteOrder();
        if (send(cfd, &response, sizeof(response), 0) < 0)
        {
            LOG(LogLevel::ERROR, "发送错误响应失败");
        }
        return;
    }
    // 调用数据库收藏接口
    const char *result = db_manager_.collectWord(username, word, meaning, timestamp);
    cout << "result:" << result << endl;
    if (strcmp(result, "already collected") == 0)
    {
        // 已经收藏,进行取消收藏
        result = db_manager_.cancelCollectWord(username, word);
        if (strcmp(result, "delete success") == 0)
        {
            LOG(LogLevel::INFO, "取消收藏成功");
            strcpy(response.text, "cancel collect success");
        }
        else if (strcmp(result, "delect error") == 0)
        {
            LOG(LogLevel::ERROR, "取消收藏失败");
            strcpy(response.text, "cancel collect error");
        }
    }
    else if (strcmp(result, "error") == 0)
    {
        strcpy(response.text, "error");
    }
    else if (strcmp(result, "success") == 0)
    {
        strcpy(response.text, "success");
    }
    else
    {
        LOG(LogLevel::ERROR, "收藏功能出现未知错误");
    }
    response.networkByteOrder();
    if (send(cfd, &response, sizeof(response), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送错误响应失败");
    }
    return;
}

void Server::responseCollectRecord(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering CollectRecord");

    // 获取历史记录列表
    std::vector<std::string> collectRecords;
    if (!db_manager_.getCollectWords(msg.name, collectRecords))
    {
        // 发送错误标志
        Msg errorMsg;
        memset(&errorMsg, 0, sizeof(errorMsg));
        errorMsg.type = D;
        strncpy(errorMsg.name, msg.name, sizeof(errorMsg.name));
        strcpy(errorMsg.text, "**ERROR**");
        errorMsg.networkByteOrder();
        send(cfd, &errorMsg, sizeof(errorMsg), 0);
        return;
    }

    if (collectRecords.empty())
    {
        // 发送空记录标志
        Msg emptyMsg;
        memset(&emptyMsg, 0, sizeof(emptyMsg));
        emptyMsg.type = D;
        strncpy(emptyMsg.name, msg.name, sizeof(emptyMsg.name));
        strcpy(emptyMsg.text, "**EMPTY**");
        emptyMsg.networkByteOrder();
        send(cfd, &emptyMsg, sizeof(emptyMsg), 0);
    }
    else
    {
        // 逐条发送历史记录
        for (const auto &record : collectRecords)
        {
            Msg response;
            memset(&response, 0, sizeof(response));
            response.type = D;
            strncpy(response.name, msg.name, sizeof(response.name));
            strncpy(response.text, record.c_str(), sizeof(response.text) - 1);
            response.text[sizeof(response.text) - 1] = '\0';
            response.networkByteOrder();

            if (send(cfd, &response, sizeof(response), 0) < 0)
            {
                LOG(LogLevel::ERROR, "发送收藏记录失败");
                return;
            }
        }
    }

    // 确保发送结束标志
    Msg overMsg;
    memset(&overMsg, 0, sizeof(overMsg));
    overMsg.type = D;
    strncpy(overMsg.name, msg.name, sizeof(overMsg.name));
    strcpy(overMsg.text, "**OVER**");
    overMsg.networkByteOrder();

    if (send(cfd, &overMsg, sizeof(overMsg), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送收藏记录结束标志失败");
    }
}