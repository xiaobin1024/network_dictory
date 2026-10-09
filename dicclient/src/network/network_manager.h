#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H
#include"app_config.h"
#include"utils.h"
#include<QTcpSocket>
#include<QTimer>
#include<QQueue>
#include <QMutex>
#include<QVector>
#include"connection_manager.h"
#include"message_queue_manager.h"
#include"reconnection_manager.h"

class NetworkManager: public QObject
{
    Q_OBJECT
public:

    explicit NetworkManager(QObject* parent=nullptr,const AppConfig& config=AppConfig());
    bool isConnected() const;
    void connectToServer() ;
    void disconnectFromServer() ;

    //发送消息接口
    bool sendRawMessage(const CoreMessage::Msg &message);
    //处理接收到的数据接口
    void handleIncomingMessage(const CoreMessage::Msg &message);
    //异步发送消息
    void sendMessageAsync(const CoreMessage::Msg &message);
    //获取待处理的消息数量
    int getPendingMessageCount();

signals:
    //void messageReceived(const  CoreMessage::Msg &message);                             //向上层传递收到数据信号
    void heartbeatSent();                                                               //向上层传递收到心跳包
    void reconnectAttempt(int m_currentReconnectAttempts,int m_maxReconnectAttempts);   //尝试重连的信号

    void connectionEstablished();
    void connectionLost();
    void networkError(QString error);

    // 原始消息信号（不进行任何处理，只转发）
    void messageReceived(const CoreMessage::Msg& message);

private slots:


    void processMessageQueue(); //处理消息队列


    void onNetworkError(QString &error);
    void onConnectionEstablished();
    void onConnectionLost();
    void onSocketReadyRead();    //接收消息接口
     void onReconnectAttempt(int currentAttempt,int maxAttempts);  //尝试重连
private:


    QString m_serverIP;         //当前连接的服务器IP
    int m_serverPort;           //当前连接的服务器端口

    //定时器相关
    QTimer *m_reconnectTimer;   //重连定时器

    //配置参数
    int m_reconnectInterval;        //重连间隔
    int m_maxReconnectAttempts;     //最大重连次数
    int m_currentReconnectAttempts; //当前重连次数尝试

    //状态标志
    bool m_autoReconnect{true};     //是否启用自动重连
    bool m_isReconnecting{false};   //是否正在重连
private:
    // 子管理器
    ConnectionManager* m_connectionManager;
    MessageQueueManager* m_messageQueueManager;
    ReconnectionManager* m_reconnectionManager;
private:
    //连接管理
    void setupConnections();
public:

    //测试用的变量
     QVector<CoreMessage::MsgType> receivedMessageTypes;     // 记录收到的消息类型

};

#endif // NETWORK_MANAGER_H
