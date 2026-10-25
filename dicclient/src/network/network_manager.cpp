#include"network_manager.h"
#include<QDateTime>
NetworkManager::NetworkManager(QObject* parent,const AppConfig& config):
    QObject(parent),
    m_reconnectInterval(config.reconnectInterval),
    m_heartbeatInterval(config.heartbeatInterval),
    m_maxReconnectAttempts(config.maxReconnectAttempts),
    m_serverIP(config.serverIp),
    m_serverPort(config.serverPort),
    m_autoReconnect(true)

{
    qDebug()<<"NetworkManager constructor";

    // 初始化子管理器
    m_connectionManager = new ConnectionManager(this);
    m_messageQueueManager=new MessageQueueManager(this);
    m_reconnectionManager=new ReconnectionManager(this);
    //初始化重连参数
    m_reconnectionManager->setReconnectInterval(m_reconnectInterval);
    m_reconnectionManager->setMaxReconnectAttempts(m_maxReconnectAttempts);
    // 心跳定时器：连接建立后周期性发送心跳，服务端据此判定连接存活
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(m_heartbeatInterval);
    connect(m_heartbeatTimer, &QTimer::timeout,
            this, &NetworkManager::onHeartbeatTimeout);
    // 存活看门狗：周期性检查心跳往返是否超时，超时则主动 abort 连接，
    // 避免静默断网时被动等待 TCP 重传超时（Windows 上约 20s）才被发现。
    m_livenessTimer = new QTimer(this);
    m_livenessTimer->setInterval(kLivenessCheckMs);
    connect(m_livenessTimer, &QTimer::timeout,
            this, &NetworkManager::onLivenessCheck);
    // 设置连接
    setupConnections();

}
 void NetworkManager::setupConnections()
{
     // 连接信号槽
     connect(m_connectionManager, &ConnectionManager::connectionEstablished,
             this, &NetworkManager::onConnectionEstablished);
     connect(m_connectionManager, &ConnectionManager::connectionLost,
             this, &NetworkManager::onConnectionLost);
     connect(m_connectionManager, &ConnectionManager::networkError,
             this,[this](QString error){onNetworkError(error);});
     connect(m_connectionManager,&ConnectionManager::dataReady,
             this,&NetworkManager::onSocketReadyRead);
    connect(m_reconnectionManager,&ReconnectionManager::reconnectAttempt,
             this,[this](int currentAttempt,int maxAttempts){onReconnectAttempt(currentAttempt,maxAttempts);});
}

bool NetworkManager::isConnected() const
{
     return m_connectionManager->isConnected();
}

void NetworkManager::connectToServer()
{
    m_connectionManager->connectToServer(m_serverIP, m_serverPort);
}

void NetworkManager::disconnectFromServer()
{
     m_autoReconnect = false;
     m_reconnectionManager->stopReconnect();
     m_heartbeatTimer->stop();
    m_connectionManager->disconnectFromServer();
    //清空消息队列
     m_messageQueueManager->clear();
    qDebug()<< "Disconnected from server";
}
void NetworkManager::onConnectionEstablished()
{
    qDebug() << "Connected to server successfully";
    m_reconnectionManager->stopReconnect();

    // 连接建立后启动心跳定时器
    if(m_heartbeatInterval > 0)
    {
        m_heartbeatTimer->start();
    }

    // 重置存活看门狗状态并启动：以当前时间为基线，避免历史残留导致误判
    m_pendingPing = false;
    m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
    m_livenessTimer->start();

    // 连接建立时立即处理积压队列
    if(!m_messageQueueManager->isEmpty())
    {
        processMessageQueue();
    }

    emit connectionEstablished();
}
void NetworkManager::onConnectionLost()
{
    qDebug() << "Disconnected from server";
    // 连接断开后停止心跳与存活看门狗，重连成功后再恢复
    m_heartbeatTimer->stop();
    m_livenessTimer->stop();
    m_pendingPing = false;
    emit connectionLost();

    // 自动重连
    if(m_autoReconnect)
    {
        m_reconnectionManager->startReconnect();
    }
}
void NetworkManager::onNetworkError(QString &error)
{
    qDebug() << "Network error occurred:" << error;
    emit networkError(error);

    // 根据错误类型判断是否需要重连
    bool shouldReconnect = m_connectionManager->shouldReconnectOnError();
    if(shouldReconnect && m_autoReconnect && !m_reconnectionManager->isReconnecting())
    {
        m_reconnectionManager->startReconnect();
    }
}
bool NetworkManager::sendRawMessage(const CoreMessage::Msg &message)
{
    return m_connectionManager->sendMessage(message);
}
void NetworkManager::onSocketReadyRead()
{
    qDebug()<<"进入NetworkManager::onSocketReadyRead()";
    QVector<CoreMessage::Msg> messages=m_connectionManager->readMessages();
    // 收到服务端任何数据都说明链路存活，刷新活动时间；
    // 其中 type==HEARTBEAT 的回包即服务端对心跳的 ALIVE 应答，清除待确认标记。
    if(!messages.isEmpty())
    {
        m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
    }
    for (const auto& message : messages) {
        if(message.type == CoreMessage::MsgType::HEARTBEAT)
        {
            m_pendingPing = false;
        }
        qDebug() << "NetworkManager 转发原始消息:" << message;
        emit messageReceived(message);
    }
}
// void NetworkManager::handleIncomingMessage(const CoreMessage::Msg &message)
// {
//     qDebug()<<"Received message: "<<message;
//     //根据消息类型处理
//     switch(message.type)
//     {
//     case CoreMessage::MsgType::HEARTBEAT:
//         qDebug()<<"Heartbeat received from: "<<message.name;
//         break;
//     case CoreMessage::MsgType::LOGIN:
//         qDebug()<<"Login request from: "<<message.name;
//         break;
//     case CoreMessage::MsgType::REGISTER:
//         qDebug()<<"Register request from: "<<message.name;
//         break;
//     case CoreMessage::MsgType::SEARCH:
//         qDebug()<<"Search request: "<<message.text;
//         break;
//     case CoreMessage::MsgType::HISTORY:
//         qDebug() << "History query from:" << message.name;
//         break;

//     case CoreMessage::MsgType::COLLECT:
//         qDebug() << "Collect request:" << message.text;
//         break;

//     case CoreMessage::MsgType::QUERYCOLLECT:
//         qDebug() << "Query collect from:" << message.name;
//         break;

//     case CoreMessage::MsgType::Quit:
//         qDebug() << "Quit request from:" << message.name;
//         break;

//     default:
//         qDebug() << "Unknown message type received:" << static_cast<int>(message.type);
//         break;
//     }
//     emit messageReceived(message);
// }

void NetworkManager::sendMessageAsync(const CoreMessage::Msg &message)
{
    //验证消息
    if(strlen(message.name)>=sizeof(message.name) ||
        strlen(message.text)>=sizeof(message.text))
    {
        qDebug()<<"Cannot send message: name or text too long";
        return;
    }
    m_messageQueueManager->enqueueMessage(message);

    if(isConnected())
    {
        processMessageQueue();
    }
    else
    {
        qDebug()<<"Message queued, pending count: "<<m_messageQueueManager->size();
    }
}
void NetworkManager::processMessageQueue()
{
    if(!isConnected())
    {
        qDebug() << "Cannot process message queue: not connected to server";
        return;
    }
    m_messageQueueManager->processQueue([this](const CoreMessage::Msg& message) {
        return sendRawMessage(message);
    });
}
int NetworkManager::getPendingMessageCount()
{
    return m_messageQueueManager->size();
}

void NetworkManager::onReconnectAttempt(int currentAttempt,int maxAttempts)
{
    qDebug()<<"NetworkManager Reconnect attempt"<<currentAttempt<<"/"<<maxAttempts;
    if(!m_connectionManager->getLastServerIp().isEmpty() &&
        m_connectionManager->getLastServerPort()>0)
    {
        m_connectionManager->connectToServer(m_connectionManager->getLastServerIp(),
                                             m_connectionManager->getLastServerPort());
    }

    emit reconnectAttempt(currentAttempt, maxAttempts);
}

void NetworkManager::sendHeartbeat()
{
    if(!isConnected())
    {
        return;
    }
    CoreMessage::Msg heartbeat;
    memset(&heartbeat, 0, sizeof(heartbeat));
    heartbeat.type = CoreMessage::MsgType::HEARTBEAT;
    strncpy(heartbeat.text, "PING", sizeof(heartbeat.text) - 1);
    // 记录已发出心跳，等待服务端 ALIVE 回包；存活看门狗据此判定往返是否超时
    m_pendingPing = true;
    m_pingSentMs = QDateTime::currentMSecsSinceEpoch();
    // 心跳为实时保活包，直接发送不入队，避免断线重连后补发过期心跳
    sendRawMessage(heartbeat);
    emit heartbeatSent();
}

void NetworkManager::onHeartbeatTimeout()
{
    sendHeartbeat();
}

void NetworkManager::onLivenessCheck()
{
    if(!isConnected())
    {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 主判定：已发出心跳但超过 kPingTimeoutMs 仍未收到服务端 ALIVE
    // → 链路已死，主动 abort 连接（绕过 TCP 重传超时 ~20s），
    // abort 会触发 disconnected() → connectionLost()，进入既有的遮罩+重连流程。
    if(m_pendingPing && (now - m_pingSentMs > kPingTimeoutMs))
    {
        qDebug() << "心跳往返超时（>" << kPingTimeoutMs
                 << "ms 未收到 ALIVE），主动断开连接";
        m_connectionManager->abortConnection();
        return;
    }

    // 兜底：长时间无任何服务端数据（超过约两个心跳周期+超时）也判定死亡，
    // 覆盖「心跳尚未发出但连接已悄悄断掉」的窗口。
    const qint64 backstopMs =
        static_cast<qint64>(m_heartbeatInterval) * 2 + kPingTimeoutMs;
    if(now - m_lastActivityMs > backstopMs)
    {
        qDebug() << "长时间无服务端数据（>" << backstopMs
                 << "ms），主动断开连接";
        m_connectionManager->abortConnection();
    }
}
