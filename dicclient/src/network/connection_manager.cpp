#include "connection_manager.h"
#include<QDebug>
ConnectionManager::ConnectionManager(QObject* parent)
    :QObject(parent),
    m_socket(new QTcpSocket(this)),
    m_lastServerPort(0),
    m_shouldReconnect(false),
    m_lastError(QAbstractSocket::UnknownSocketError)
{
    // 连接信号槽
    connect(m_socket, &QTcpSocket::connected, this, &ConnectionManager::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ConnectionManager::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error){
        onSocketError(error);
    });
    connect(m_socket,&QTcpSocket::readyRead,this,&ConnectionManager::onSocketReadyRead);
}
bool ConnectionManager::isConnected() const
{
    return m_socket && m_socket->state() == QTcpSocket::ConnectedState;
}
void ConnectionManager::connectToServer(const QString& serverIP, int serverPort)
{
    if(isConnected())
    {
        qDebug() << "Already connected to server";
        return;
    }

    if(serverIP.isEmpty() || serverPort <= 0)
    {
        qDebug() << "Invalid IP address or port";
        QString error("Invalid IP address or port");
        emit networkError(error);
        return;
    }

    m_lastServerIp = serverIP;
    m_lastServerPort = serverPort;
    qDebug() << "Connecting to server：" << serverIP << ":" << serverPort;
    m_socket->connectToHost(serverIP, serverPort);
}
void ConnectionManager::disconnectFromServer()
{

    if(m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_socket->disconnectFromHost();
        if(m_socket->state() != QAbstractSocket::UnconnectedState)
        {
            m_socket->waitForDisconnected(3000);
        }
    }
    m_readBffer.clear();
}
bool ConnectionManager::shouldReconnectOnError() const
{
    return m_shouldReconnect;
}

QString ConnectionManager::getLastServerIp() const
{
    return m_lastServerIp;
}

int ConnectionManager::getLastServerPort() const
{
    return m_lastServerPort;
}
void ConnectionManager::onSocketReadyRead()
{
    emit dataReady();   //发出数据可读取信号
}
void ConnectionManager::onSocketConnected()
{
    qDebug() << "Socket connected successfully";
    emit connectionEstablished();
}

void ConnectionManager::onSocketDisconnected()
{
    qDebug() << "Socket disconnected";
    emit connectionLost();
}
void ConnectionManager::onSocketError(QAbstractSocket::SocketError error)
{
    m_lastError = error;
    QString errorString = m_socket->errorString();
    QString errorType = "未知错误";

    switch(error)
    {
    case QAbstractSocket::ConnectionRefusedError:
        errorType = "连接被拒绝";
        m_shouldReconnect = true;
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorType = "远程主机关闭连接";
        m_shouldReconnect = true;
        break;
    case QAbstractSocket::HostNotFoundError:
        errorType = "主机未找到";
        m_shouldReconnect = false;
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorType = "连接超时";
        m_shouldReconnect = true;
        break;
    case QAbstractSocket::NetworkError:
        errorType = "网络错误";
        m_shouldReconnect = true;
        break;
    case QAbstractSocket::TemporaryError:
        errorType = "临时错误";
        m_shouldReconnect = true;
        break;
    case QAbstractSocket::SocketAccessError:
        errorType = "套接字访问错误";
        m_shouldReconnect = false;
        break;
    case QAbstractSocket::AddressInUseError:
        errorType = "地址已被使用";
        m_shouldReconnect = false;
        break;
    case QAbstractSocket::UnsupportedSocketOperationError:
        errorType = "不支持的套接字操作";
        m_shouldReconnect = false;
        break;
    case QAbstractSocket::ProxyAuthenticationRequiredError:
        errorType = "需要代理认证";
        m_shouldReconnect = false;
        break;
    case QAbstractSocket::SslHandshakeFailedError:
        errorType = "SSL握手失败";
        m_shouldReconnect = false;
        break;
    default:
        if(!m_lastServerIp.isEmpty() && m_lastServerPort > 0)
        {
            m_shouldReconnect = true;
        }
        break;
    }

    QString detailedError = QString("%1: %2").arg(errorType).arg(errorString);
    qDebug() << "Socket error occurred:" << detailedError;
    emit networkError(detailedError);
}
bool ConnectionManager::sendMessage(const CoreMessage::Msg &message)
{
    if(!isConnected())
    {
        qDebug()<<"Cannot send raw message: not connected to server";
        return false;
    }
    //创建消息，并转换为网络字节序
    CoreMessage::Msg netMsg = message;
    netMsg.toNetWorkByteOrder();
    qint64 byteWritten;
    //发送消息结构体
    //确保同一时刻发送一条消息，添加锁
    {
        QMutexLocker locker(&m_writeMutex);
        byteWritten=m_socket->write(reinterpret_cast<const char*>(&netMsg),sizeof(CoreMessage::Msg));
    }

    if(byteWritten==-1)
    {
        qDebug()<<"Failed to write data  to socket: "<<m_socket->errorString();
        return false;
    }
    if(byteWritten!=sizeof(CoreMessage::Msg))
    {
        qDebug() << "Incomplete message sent:" << byteWritten << "of" << sizeof(CoreMessage::Msg) << "bytes";
        return false;
    }
    //等待数据写入完成
    if(!m_socket->waitForBytesWritten(3000))
    {
        qDebug() << "Timeout waiting for bytes to be written";
        return false;
    }
    qDebug() << "Message sent successfully, type:" << message.type
             << ", size:" << byteWritten << "bytes";
    return true;
}
 QVector<CoreMessage::Msg>ConnectionManager::readMessages()
{
     qDebug()<<"进入readMessages()";
     QVector<CoreMessage::Msg> messages;
    if(!isConnected())
     {
         qDebug()<<"Cannot send raw message: not connected to server";
         return messages;
    }
    //持续读取直到没有完整消息
    while(m_socket->bytesAvailable()>=static_cast<qint64>(sizeof(CoreMessage::Msg)))
    {
        //直接读取完整消息结构体
        CoreMessage::Msg receivedMsg;
        qint64 bytesRead=m_socket->read(reinterpret_cast<char*>(&receivedMsg),sizeof(CoreMessage::Msg));
        if(bytesRead!=sizeof(CoreMessage::Msg))
        {
            qDebug()<<"Incomplete message received: "<<bytesRead<<"of"<<sizeof(CoreMessage::Msg)<<"bytes";
            break;
        }
        //转换字节序
        receivedMsg.toHostByteOrder();
        messages.append(receivedMsg);
    }
    return messages;
}
