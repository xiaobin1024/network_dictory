#include"network_manager.h"
NetworkManager::NetworkManager(QObject* parent,const AppConfig& config):
    QObject(parent),
    m_reconnectInterval(config.reconnectInterval),
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
    m_connectionManager->disconnectFromServer();
    //清空消息队列
     m_messageQueueManager->clear();
    qDebug()<< "Disconnected from server";
}
void NetworkManager::onConnectionEstablished()
{
    qDebug() << "Connected to server successfully";
    m_reconnectionManager->stopReconnect();

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
    // 对每条消息，直接发射信号，不进行任何处理
    for (const auto& message : messages) {
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
