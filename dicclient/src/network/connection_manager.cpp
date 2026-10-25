#include "connection_manager.h"
#include "protocol_frame.hpp"
#include<QDebug>
#include<string>
#include<cstring>
#include<algorithm>
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
void ConnectionManager::abortConnection()
{
    if(m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        // abort() 立即丢弃缓冲并关闭，会触发 disconnected() → connectionLost()，
        // 从而走既有的「显示遮罩 + 启动重连」流程，不再等 TCP RTO（~20s）。
        m_socket->abort();
    }
}

bool ConnectionManager::sendMessage(const CoreMessage::Msg &message)
{
    if(!isConnected())
    {
        qDebug()<<"Cannot send raw message: not connected to server";
        return false;
    }
    // 用 v2 变长帧发送：name/text 按实际长度走，不再补满 1280 字节。
    // 字节序与帧格式由 shared/protocol_frame.hpp 统一处理，两端同一份实现。
    std::string frame = ProtocolFrame::encode(static_cast<int>(message.type),
                                              std::string(message.name),
                                              std::string(message.text));
    qint64 byteWritten;
    //发送消息
    //确保同一时刻发送一条消息，添加锁
    {
        QMutexLocker locker(&m_writeMutex);
        byteWritten=m_socket->write(frame.data(), static_cast<qint64>(frame.size()));
    }

    if(byteWritten==-1)
    {
        qDebug()<<"Failed to write data  to socket: "<<m_socket->errorString();
        return false;
    }
    if(byteWritten!=static_cast<qint64>(frame.size()))
    {
        qDebug() << "Incomplete message sent:" << byteWritten << "of" << frame.size() << "bytes";
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
    // 持续读取直到缓冲里凑不出一整帧。
    // v2 是变长帧：先用 peek 看帧头拿到长度，不够一帧就留给下次 readyRead，
    // 不做半包解析（bytesAvailable 本身就是 QTcpSocket 的累积缓冲）。
    while(m_socket->bytesAvailable()>=static_cast<qint64>(ProtocolFrame::kHeaderSize))
    {
        char head[ProtocolFrame::kHeaderSize];
        qint64 headRead=m_socket->peek(head,static_cast<qint64>(ProtocolFrame::kHeaderSize));
        if(headRead!=static_cast<qint64>(ProtocolFrame::kHeaderSize))
        {
            break;
        }

        if(ProtocolFrame::readU32(head)!=ProtocolFrame::kMagic)
        {
            // 服务端若仍回 v1 定长帧（例如回滚到旧版本），这里按旧格式兼容解析，
            // 保证客户端升级后连旧服务端不会直接卡死。
            if(m_socket->bytesAvailable()<static_cast<qint64>(sizeof(CoreMessage::Msg)))
            {
                break;
            }
            CoreMessage::Msg legacyMsg;
            qint64 bytesRead=m_socket->read(reinterpret_cast<char*>(&legacyMsg),sizeof(CoreMessage::Msg));
            if(bytesRead!=sizeof(CoreMessage::Msg))
            {
                qDebug()<<"Incomplete legacy message received: "<<bytesRead;
                break;
            }
            legacyMsg.toHostByteOrder();
            messages.append(legacyMsg);
            continue;
        }

        uint32_t bodyLen=ProtocolFrame::readU32(head+4);
        if(bodyLen<8 || bodyLen>ProtocolFrame::kMaxBodyLen)
        {
            qDebug()<<"Invalid frame bodyLen: "<<bodyLen<<"，丢弃该连接的剩余数据";
            m_socket->readAll();
            break;
        }

        qint64 total=static_cast<qint64>(bodyLen)+8;
        if(m_socket->bytesAvailable()<total)
        {
            break; // 半包，等下次 readyRead
        }

        QByteArray raw=m_socket->read(total);
        ProtocolFrame::Frame frame;
        size_t frameLen=0;
        ProtocolFrame::ParseResult r=ProtocolFrame::parse(raw.constData(),
                                                          static_cast<size_t>(raw.size()),
                                                          frame,frameLen);
        if(r!=ProtocolFrame::ParseResult::Ok)
        {
            qDebug()<<"Failed to parse frame, result="<<static_cast<int>(r);
            break;
        }

        // 转回定长 Msg 交给既有分发链路；name/text 超长时按字段宽度截断。
        // 用 memcpy 而非 strncpy：MSVC /W4 下 strncpy 会报 C4996，
        // 且这里已经 memset 清零，截断处天然有 '\0' 终止。
        CoreMessage::Msg receivedMsg;
        std::memset(&receivedMsg,0,sizeof(receivedMsg));
        receivedMsg.type=static_cast<CoreMessage::MsgType>(frame.type);
        size_t nameLen=std::min(frame.name.size(),sizeof(receivedMsg.name)-1);
        size_t textLen=std::min(frame.text.size(),sizeof(receivedMsg.text)-1);
        if(nameLen>0) std::memcpy(receivedMsg.name,frame.name.data(),nameLen);
        if(textLen>0) std::memcpy(receivedMsg.text,frame.text.data(),textLen);
        messages.append(receivedMsg);
    }
    return messages;
}
