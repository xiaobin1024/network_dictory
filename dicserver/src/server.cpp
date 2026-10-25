#include "server.hpp"

#include <iostream>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <chrono>
#include <thread>
#include <vector>
#include <cerrno>
#include <unistd.h>      // close / usleep
#include <fcntl.h>       // fcntl / O_NONBLOCK
#include <sys/time.h>    // struct timeval
#include <sys/socket.h>  // socket / bind / listen / accept / recv / send
#include <netinet/in.h>  // sockaddr_in / htons / ntohs
#include <arpa/inet.h>   // inet_addr / inet_ntoa

using namespace std;

// 单连接接收循环的参数
namespace
{
    constexpr int RECV_CHUNK_SIZE = 4096;       // 单次 recv 读取上限
    constexpr int RECV_TIMEOUT_SEC = 10;        // 收发超时，recv 周期醒来检查心跳
    constexpr int64_t HEARTBEAT_KICK_SEC = 90; // 连续 90s 无任何报文判定为死连接
    constexpr size_t NAME_BUF_SIZE = 32;        // Msg::name 缓冲区大小（需容纳 20 字符用户名 + '\0'）
    constexpr size_t TEXT_BUF_SIZE = 1280;      // Msg::text 缓冲区大小
    // 生词本数据的字段分隔符，与客户端 WordVocabulary 约定的格式一致
    constexpr const char *VOCAB_FIELD_SEP = "|^|";
    // 生词本记录的字段数量：单词/释义/音标/例句/翻译/收藏标识
    constexpr int VOCAB_FIELD_COUNT = 6;

    // 按分隔符拆分字符串：空字段同样保留，保证各协议字段数稳定可预期
    std::vector<std::string> splitFields(const std::string &text, const std::string &sep)
    {
        std::vector<std::string> fields;
        size_t start = 0;
        while (true)
        {
            size_t pos = text.find(sep, start);
            if (pos == std::string::npos)
            {
                fields.push_back(text.substr(start));
                break;
            }
            fields.push_back(text.substr(start, pos - start));
            start = pos + sep.size();
        }
        return fields;
    }
}

// 安全拷贝：确保目标缓冲区以 '\0' 结尾
// strncpy 在源串长度 >= n 时不会写入终止符，直接用会导致后续 strlen/strcmp 越界读取
void Server::safeStrcpy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
    {
        return;
    }
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    size_t len = strnlen(src, dst_size - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
}

// 安全拷贝（std::string 版本）：按最大长度截断
void Server::safeStrcpy(std::string &dst, const std::string &src, size_t max_len)
{
    dst = src.length() > max_len ? src.substr(0, max_len) : src;
}

// 服务器的构造函数：完成套接字创建，端口号快速重用，绑定IP地址和端口号
// 初始化列表顺序与 server.hpp 中的成员声明顺序保持一致，避免 -Wreorder 告警
Server::Server(DatabaseManager &db_manage, const shared_ptr<threadPool> thread_pool, const string ip, int port)
    : sfd_(-1), port_(port), ip_(ip), runnning_(false), db_manager_(db_manage), thread_pool_(thread_pool)
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
    // 客户端 socket 使用阻塞模式 + 收发超时：
    // 既避免非阻塞 EAGAIN 空转占满 CPU，又能周期性醒来检查心跳超时
    struct timeval rcvTimeout {RECV_TIMEOUT_SEC, 0};
    struct timeval sndTimeout {RECV_TIMEOUT_SEC, 0};
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout));
    setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));

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
            close(cfd); // 连接 fd 统一在此关闭，handleClient 内不再 close
        });
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
    std::string client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    std::string username; // 记录当前连接的用户名

    LOG(LogLevel::INFO, "开始处理客户端 " << client_ip << ":" << client_port << " 的连接");

    // TCP 是字节流，recv 一次可能返回任意长度，必须先缓冲再按定长 Msg 切包
    std::vector<char> readBuf;
    readBuf.reserve(sizeof(Msg) * 2);
    char chunk[RECV_CHUNK_SIZE];
    // 最近一次收到报文的时间，用于心跳超时判定
    auto lastActive = std::chrono::steady_clock::now();

    while (runnning_)
    {
        ssize_t recv_len = recv(cfd, chunk, sizeof(chunk), 0);

        if (recv_len < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 收超时：检查是否已长时间无任何报文（客户端崩溃/掉网且没有发 FIN）
                int64_t idleSec = std::chrono::duration_cast<std::chrono::seconds>(
                                      std::chrono::steady_clock::now() - lastActive)
                                      .count();
                if (idleSec >= HEARTBEAT_KICK_SEC)
                {
                    LOG(LogLevel::INFO, "心跳超时(" << idleSec << "s)，关闭连接: " << client_ip << ":" << client_port);
                    break;
                }
                continue;
            }
            if (errno == EINTR)
            {
                // 被信号中断，继续接收
                continue;
            }
            LOG(LogLevel::ERROR, "接收客户端 " << client_ip << ":" << client_port << " 数据错误: " << strerror(errno));
            break;
        }

        if (recv_len == 0)
        {
            // 客户端正常断开连接（收到 FIN）
            LOG(LogLevel::INFO, "客户端主动断开连接: " << client_ip << ":" << client_port);
            break;
        }

        // 收到数据，刷新活跃时间并追加到拼包缓冲
        lastActive = std::chrono::steady_clock::now();
        readBuf.insert(readBuf.end(), chunk, chunk + recv_len);

        // 按定长结构体切出完整消息，不足一个 Msg 的字节留在缓冲中等待下次 recv
        while (readBuf.size() >= sizeof(Msg))
        {
            Msg msg;
            memset(&msg, 0, sizeof(msg));
            memcpy(&msg, readBuf.data(), sizeof(Msg));
            readBuf.erase(readBuf.begin(), readBuf.begin() + sizeof(Msg));

            // 只有收到完整消息时才继续处理
            msg.hostByteOrder();

            // 验证消息类型有效性（1~11，与客户端 protocol/message.h 的枚举保持一致）
            if (msg.type < 1 || msg.type > MAX_MSG_TYPE)
            {
                LOG(LogLevel::INFO, "收到无效消息类型: " << static_cast<int>(msg.type));
                continue; // 跳过无效消息，继续处理下一条
            }

            LOG(LogLevel::DEBUG, "收到客户端消息，类型: " << static_cast<int>(msg.type));
            // 在登录成功后记录用户名
            if (msg.type == L)
            {
                username = msg.name; // 记录登录的用户名
            }
            processMessage(cfd, msg, client_addr);
        }
    }

    // 连接关闭兜底：已登录用户置为离线，避免残留在线状态
    if (!username.empty())
    {
        const char *result = db_manager_.logoutUser(username);
        LOG(LogLevel::INFO, "更新用户状态结果: " << username << " " << result);
    }
    // fd 由 handleNewConnection 投递任务的 lambda 统一 close，这里不再重复关闭
    LOG(LogLevel::INFO, "客户端连接处理结束: " << client_ip << ":" << client_port);
}

void Server::processMessage(int cfd, Msg &msg, sockaddr_in client_addr)
{
    std::string client_ip = inet_ntoa(client_addr.sin_addr);

    LOG(LogLevel::DEBUG, "收到消息，类型: " << static_cast<int>(msg.type)
                       << " 用户名: " << msg.name);

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
    case A: // 心跳检测：收到客户端心跳，立即回 ACK
        responseHeartbeat(cfd, msg, client_addr);
        break;
    case C: // 收藏单词
        responseCollect(cfd, msg);
        break;
    case D: // 收藏记录
        responseCollectRecord(cfd, msg);
        break;
    case U: // 注销账号：删除用户及其历史/收藏/生词本数据
        responseUnregister(cfd, msg);
        break;
    case V: // 生词本增删
        responseVocabulary(cfd, msg);
        break;
    case W: // 生词本记录列表
        responseVocabularyList(cfd, msg);
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
    tm local;
    // 服务端是多线程环境，使用 localtime_r 避免共享静态缓冲区产生竞态
    localtime_r(&now, &local);
    char time_str[20] = "";
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &local);
    return string(time_str);
}

// 响应客户端心跳检测请求函数
void Server::responseHeartbeat(int cfd, Msg &msg, sockaddr_in client_addr)
{
    // 心跳报文不携带业务数据，此处仅需回送 ALIVE
    (void)msg;
    std::string client_ip = inet_ntoa(client_addr.sin_addr);

    Msg heartbeatResp;
    memset(&heartbeatResp, 0, sizeof(heartbeatResp));
    heartbeatResp.type = A;
    safeStrcpy(heartbeatResp.text, TEXT_BUF_SIZE, "ALIVE");
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
    safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);
    // 注册结果通过出参回传新用户的稳定标识与角色。
    // 协议格式与客户端 UserSession::processRegisterResponse 约定：
    //   成功：SUCCESS:<用户名>:<密码>:<用户ID>:<角色>
    //   失败：ERROR:<失败原因>
    long long userId = 0;
    int role = 0;
    int success = db_manager_.registerUser(msg.name, msg.text, &userId, &role);
    if (success)
    {
        // 精度限定符为各字段设定上限，保证拼接结果必然小于 TEXT_BUF_SIZE：
        // 8(SUCCESS:) + 31(用户名) + 1 + 1000(密码) + 1 + 19(用户ID) + 1 + 10(角色) < 1280
        snprintf(response.text, TEXT_BUF_SIZE, "SUCCESS:%.31s:%.1000s:%lld:%d",
                 msg.name, msg.text, userId, role);
        LOG(LogLevel::INFO, "Register attempt: " << msg.name << " - SUCCESS, userId=" << userId);
    }
    else
    {
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:用户名已存在或注册失败");
        LOG(LogLevel::INFO, "Register attempt: " << msg.name << " - FAILED");
    }
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
    safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);
    bool is_online = false;
    long long userId = 0;
    int role = 0;

    // 执行登录函数，成功时回传用户标识与角色
    int loginResult = db_manager_.loginUser(msg.name, msg.text, is_online, &userId, &role);

    // 添加详细的调试信息
    LOG(LogLevel::INFO, "Login result - code: " << loginResult << ", is_online: " << is_online);

    // 根据登录结果设置响应文本。
    // 协议格式与客户端 UserSession::processLoginResponse 约定：
    //   成功：SUCCESS:<用户名>:<密码>:<用户ID>:<角色>
    //   失败：ERROR:<失败原因>
    switch (loginResult)
    {
    case 0: // 登录成功
        if (is_online)
        {
            safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:用户已在线");
        }
        else
        {
            // 精度限定符为各字段设定上限，保证拼接结果必然小于 TEXT_BUF_SIZE（同上）
            snprintf(response.text, TEXT_BUF_SIZE, "SUCCESS:%.31s:%.1000s:%lld:%d",
                     msg.name, msg.text, userId, role);
        }
        break;
    case 1: // 用户不存在
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:用户不存在");
        break;
    case 2: // 密码错误
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:密码错误");
        break;
    default: // 其他错误
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:登录失败");
        break;
    }

    // 发送请求
    response.networkByteOrder();

    // 注意：send 返回 ssize_t，用 size_t 接收会导致 < 0 判断恒为假，发送失败无法被发现
    ssize_t send_size = send(cfd, &response, sizeof(response), 0);
    if (send_size < 0)
    {
        LOG(LogLevel::ERROR, "登录的结果发送失败" << strerror(errno));
        return;
    }
    LOG(LogLevel::INFO, "发送数据大小: " << send_size);
}
// 响应客户端查询请求函数
void Server::responseQuery(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering Query ");
    // 执行查询操作
    LOG(LogLevel::INFO, "Query word : " << msg.text);
    string meaning;
    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = S;
    safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);

    int collectFlag = 0;
    int vocabFlag = 0;
    if (db_manager_.querryWord(msg.name, msg.text, meaning, &collectFlag, &vocabFlag))
    {
        // 响应格式与客户端 WordSearch::processSearchResponse 约定：
        // 单词|^|释义|^|音标|^|例句|^|翻译|^|收藏标志|^|生词本标志
        // 当前词库只有单词与释义两列，音标/例句/翻译按空串返回。
        // 拼接后用 safeStrcpy 受控拷贝，避免 snprintf 变参截断告警。
        string payload = string(msg.text) + VOCAB_FIELD_SEP + meaning +
                         VOCAB_FIELD_SEP + VOCAB_FIELD_SEP + VOCAB_FIELD_SEP + VOCAB_FIELD_SEP +
                         std::to_string(collectFlag) + VOCAB_FIELD_SEP + std::to_string(vocabFlag);
        safeStrcpy(response.text, TEXT_BUF_SIZE, payload.c_str());
        LOG(LogLevel::INFO, "单词查询回包内容：" << response.text);
    }
    else
    {
        // 客户端按精确字符串 "NOT_FOUND" 判定单词不存在
        safeStrcpy(response.text, TEXT_BUF_SIZE, "NOT_FOUND");
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
        safeStrcpy(errorMsg.name, NAME_BUF_SIZE, msg.name);
        safeStrcpy(errorMsg.text, TEXT_BUF_SIZE, "**ERROR**");
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
        safeStrcpy(emptyMsg.text, TEXT_BUF_SIZE, "**EMPTY**");
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
            safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);
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
                        safeStrcpy(errorMsg.text, TEXT_BUF_SIZE, "**ERROR**");
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
    safeStrcpy(overMsg.text, TEXT_BUF_SIZE, "**OVER**");
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
    safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);

    // 执行退出操作
    const char *result = db_manager_.logoutUser(msg.name);
    LOG(LogLevel::DEBUG, "退出登录结果: " << result);
    if (strcmp(result, "user not online") == 0)
    {
        LOG(LogLevel::INFO, msg.name << ":" << "已经下线");
        return;
    }

    if (strcmp(result, "user offline success") == 0)
    {
        // 协议格式与客户端 UserSession::processLogoutResponse 约定：SUCCESS 前缀
        snprintf(response.text, sizeof(response.text), "SUCCESS:%s", msg.name);
        LOG(LogLevel::INFO, "用户退出成功: " << msg.name);
    }
    else if (strcmp(result, "user offline fail") == 0)
    {
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:退出失败");
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
    memset(&response, 0, sizeof(response));
    response.type = C;
    // 解析请求文本：6 个字段，缺一视为格式错误
    // 请求格式与客户端 WordCollect::collectWord 约定：
    // 单词|^|释义|^|音标|^|例句|^|翻译|^|当前收藏状态
    std::vector<std::string> fields = splitFields(std::string(msg.text), VOCAB_FIELD_SEP);
    if (fields.size() < 6)
    {
        LOG(LogLevel::ERROR, "收藏请求格式错误，字段数: " << fields.size());
        safeStrcpy(response.text, TEXT_BUF_SIZE, "format is incorrect");
        response.networkByteOrder();
        if (send(cfd, &response, sizeof(response), 0) < 0)
        {
            LOG(LogLevel::ERROR, "发送错误响应失败");
        }
        return;
    }

    const std::string &word = fields[0];
    const std::string &meaning = fields[1];
    const std::string &phonetic = fields[2];
    const std::string &example = fields[3];
    const std::string &translation = fields[4];

    // 收藏采用 toggle 语义：已收藏则取消，未收藏则插入
    const char *result = db_manager_.collectWord(msg.name, word, meaning, phonetic, example, translation);
    LOG(LogLevel::DEBUG, "收藏操作结果: " << result);
    if (strcmp(result, "already collected") == 0)
    {
        // 已经收藏,进行取消收藏
        result = db_manager_.cancelCollectWord(msg.name, word);
        if (strcmp(result, "delete success") == 0)
        {
            LOG(LogLevel::INFO, "取消收藏成功");
            safeStrcpy(response.text, TEXT_BUF_SIZE,
                       ("cancel_success" + std::string(VOCAB_FIELD_SEP) + word).c_str());
        }
        else
        {
            LOG(LogLevel::ERROR, "取消收藏失败");
            safeStrcpy(response.text, TEXT_BUF_SIZE, "cancel_error");
        }
    }
    else if (strcmp(result, "success") == 0)
    {
        LOG(LogLevel::INFO, "收藏成功");
        safeStrcpy(response.text, TEXT_BUF_SIZE,
                   ("collect_success" + std::string(VOCAB_FIELD_SEP) + word).c_str());
    }
    else
    {
        LOG(LogLevel::ERROR, "收藏功能出现未知错误");
        safeStrcpy(response.text, TEXT_BUF_SIZE, "collect_error");
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
        safeStrcpy(errorMsg.name, NAME_BUF_SIZE, msg.name);
        safeStrcpy(errorMsg.text, TEXT_BUF_SIZE, "**ERROR**");
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
        safeStrcpy(emptyMsg.text, TEXT_BUF_SIZE, "**EMPTY**");
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
            safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);
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
    safeStrcpy(overMsg.text, TEXT_BUF_SIZE, "**OVER**");
    overMsg.networkByteOrder();

    if (send(cfd, &overMsg, sizeof(overMsg), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送收藏记录结束标志失败");
    }
}

// ============================================================================
// 账号注销
// ============================================================================

// 响应客户端注销账号请求：校验密码后删除用户及其关联数据
void Server::responseUnregister(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering Unregister");

    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = U;
    safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);

    // 密码在 msg.text 中，与注册/登录保持一致的传参约定。
    // 删除前取出用户标识，注销成功后回传给客户端用于清理本地数据。
    // 协议格式与客户端 UserSession::processUnregisterResponse 约定：
    //   成功：SUCCESS:<用户名>:<用户ID>
    //   失败：ERROR:<失败原因>
    long long userId = 0;
    const char *result = db_manager_.unregisterUser(std::string(msg.name), std::string(msg.text), &userId);

    if (strcmp(result, "unregister success") == 0)
    {
        snprintf(response.text, TEXT_BUF_SIZE, "SUCCESS:%s:%lld", msg.name, userId);
        LOG(LogLevel::INFO, "用户注销成功: " << msg.name << ", userId=" << userId);
    }
    else if (strcmp(result, "user not found") == 0)
    {
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:用户不存在");
        LOG(LogLevel::INFO, "注销失败，用户不存在: " << msg.name);
    }
    else if (strcmp(result, "password error") == 0)
    {
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:密码错误");
        LOG(LogLevel::INFO, "注销失败，密码错误: " << msg.name);
    }
    else
    {
        safeStrcpy(response.text, TEXT_BUF_SIZE, "ERROR:注销失败");
        LOG(LogLevel::ERROR, "用户注销失败: " << msg.name << " 原因: " << result);
    }

    response.networkByteOrder();
    if (send(cfd, &response, sizeof(response), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送注销结果失败" << strerror(errno));
    }
}

// ============================================================================
// 生词本
// ============================================================================

/*
响应客户端生词本增删请求
客户端请求 text 格式：单词|^|释义|^|音标|^|例句|^|翻译|^|当前状态(1加入/0移出)
响应 text 格式：状态码|^|单词，其中状态码取值
  add_success / add_error / remove_success / remove_error / format is incorrect
与客户端 WordVocabulary::processVocabularyResponse 的解析保持一致
*/
void Server::responseVocabulary(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, " 生词本操作: " << msg.name << " " << msg.text);

    Msg response;
    memset(&response, 0, sizeof(response));
    response.type = V;
    safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);

    // 按 |^| 分割请求字段：单词/释义/音标/例句/翻译/状态
    string requestText = std::string(msg.text);
    vector<string> fields;
    size_t start = 0;
    size_t sepLen = strlen(VOCAB_FIELD_SEP);
    while (true)
    {
        size_t pos = requestText.find(VOCAB_FIELD_SEP, start);
        if (pos == string::npos)
        {
            fields.push_back(requestText.substr(start));
            break;
        }
        fields.push_back(requestText.substr(start, pos - start));
        start = pos + sepLen;
    }

    // 字段数不足，无法解析出单词与状态，直接返回格式错误
    if (fields.size() < static_cast<size_t>(VOCAB_FIELD_COUNT))
    {
        safeStrcpy(response.text, TEXT_BUF_SIZE, "format is incorrect");
        response.networkByteOrder();
        if (send(cfd, &response, sizeof(response), 0) < 0)
        {
            LOG(LogLevel::ERROR, "发送生词本格式错误响应失败");
        }
        return;
    }

    const string &word = fields[0];
    const string &meaning = fields[1];
    const string &phonetic = fields[2];
    const string &example = fields[3];
    const string &translation = fields[4];
    // 状态字段为 "1" 表示当前在生词本中，需要移出；"0" 表示需要加入
    bool isAdd = (fields[5] != "1");

    const char *result = db_manager_.updateVocabulary(std::string(msg.name), word,
                                                      meaning, phonetic, example, translation,
                                                      isAdd);

    string status;
    if (strcmp(result, "add success") == 0)
    {
        status = "add_success";
    }
    else if (strcmp(result, "already exists") == 0)
    {
        // 已在生词本中，视为加入成功（幂等）
        status = "add_success";
    }
    else if (strcmp(result, "remove success") == 0)
    {
        status = "remove_success";
    }
    else if (strcmp(result, "not exists") == 0)
    {
        // 本就不在生词本中，视为移出成功（幂等）
        status = "remove_success";
    }
    else
    {
        status = isAdd ? "add_error" : "remove_error";
        LOG(LogLevel::ERROR, "生词本操作失败: " << msg.name << " " << result);
    }

    // 响应格式：状态码|^|单词
    snprintf(response.text, sizeof(response.text), "%s%s%s", status.c_str(), VOCAB_FIELD_SEP, word.c_str());
    response.networkByteOrder();

    if (send(cfd, &response, sizeof(response), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送生词本操作结果失败" << strerror(errno));
    }
}

// 响应客户端生词本列表请求：逐条下发记录，以 **OVER** 标志收尾
void Server::responseVocabularyList(int cfd, Msg &msg)
{
    LOG(LogLevel::INFO, msg.name << " :Entering VocabularyList");

    vector<string> vocabularyRecords;
    if (!db_manager_.getVocabularyWords(std::string(msg.name), vocabularyRecords))
    {
        // 查询失败，发送错误标志
        Msg errorMsg;
        memset(&errorMsg, 0, sizeof(errorMsg));
        errorMsg.type = W;
        safeStrcpy(errorMsg.name, NAME_BUF_SIZE, msg.name);
        safeStrcpy(errorMsg.text, TEXT_BUF_SIZE, "**ERROR**");
        errorMsg.networkByteOrder();
        send(cfd, &errorMsg, sizeof(errorMsg), 0);
        return;
    }

    if (vocabularyRecords.empty())
    {
        // 空记录标志
        Msg emptyMsg;
        memset(&emptyMsg, 0, sizeof(emptyMsg));
        emptyMsg.type = W;
        safeStrcpy(emptyMsg.name, NAME_BUF_SIZE, msg.name);
        safeStrcpy(emptyMsg.text, TEXT_BUF_SIZE, "**EMPTY**");
        emptyMsg.networkByteOrder();
        send(cfd, &emptyMsg, sizeof(emptyMsg), 0);
        return;
    }

    // 逐条发送生词本记录，处理部分写
    for (size_t i = 0; i < vocabularyRecords.size(); i++)
    {
        Msg response;
        memset(&response, 0, sizeof(response));
        response.type = W;
        safeStrcpy(response.name, NAME_BUF_SIZE, msg.name);
        safeStrcpy(response.text, TEXT_BUF_SIZE, vocabularyRecords[i].c_str());
        response.networkByteOrder();

        ssize_t bytesSent = 0;
        ssize_t totalSize = sizeof(response);
        const char *data = reinterpret_cast<const char *>(&response);

        while (bytesSent < totalSize)
        {
            ssize_t result = send(cfd, data + bytesSent, totalSize - bytesSent, MSG_DONTWAIT);
            if (result < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 发送缓冲区满，稍作等待后重试
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                LOG(LogLevel::ERROR, "发送生词本记录失败：" << strerror(errno));
                return;
            }
            bytesSent += result;
        }
        LOG(LogLevel::DEBUG, "已发送生词本记录" << (i + 1) << "/" << vocabularyRecords.size());
    }

    // 确保发送结束标志
    Msg overMsg;
    memset(&overMsg, 0, sizeof(overMsg));
    overMsg.type = W;
    safeStrcpy(overMsg.name, NAME_BUF_SIZE, msg.name);
    safeStrcpy(overMsg.text, TEXT_BUF_SIZE, "**OVER**");
    overMsg.networkByteOrder();

    if (send(cfd, &overMsg, sizeof(overMsg), 0) < 0)
    {
        LOG(LogLevel::ERROR, "发送生词本记录结束标志失败: " << strerror(errno));
    }
    else
    {
        LOG(LogLevel::INFO, "生词本记录发送完成，共发送 " << vocabularyRecords.size() << " 条记录");
    }
}