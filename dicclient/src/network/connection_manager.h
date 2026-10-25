#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H
#include"app_config.h"
#include"utils.h"
#include"message.h"
#include<QTcpSocket>
#include<QMutex>
#include<QVector>
#include <QObject>


class ConnectionManager :public QObject
{
    Q_OBJECT
public:
    explicit ConnectionManager(QObject* parent=nullptr );
    bool isConnected() const;
    void connectToServer(const QString& serverIP, int serverPort);
    void disconnectFromServer() ;
    bool sendMessage(const CoreMessage::Msg &message);
    QVector<CoreMessage::Msg>readMessages();

    // 错误处理
    bool shouldReconnectOnError() const;
    QString getLastServerIp() const;
    int getLastServerPort() const;
signals:
    void networkError(QString &error);
    void connectionEstablished();
    void connectionLost();
    void dataReady();
private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
private:
    QTcpSocket* m_socket;
    QString m_lastServerIp;
    int m_lastServerPort;
    QMutex m_writeMutex;
    QAbstractSocket::SocketError m_lastError;
    bool m_shouldReconnect;
    //内部读取缓冲区
    QByteArray m_readBffer;
};

#endif // CONNECTION_MANAGER_H
