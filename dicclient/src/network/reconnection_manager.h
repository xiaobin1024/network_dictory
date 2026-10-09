#ifndef RECONNECTION_MANAGER_H
#define RECONNECTION_MANAGER_H
#include<QTimer>
#include <QObject>
#include<QDebug>

class ReconnectionManager: public QObject
{
    Q_OBJECT
public:
    explicit ReconnectionManager(QObject* parent=nullptr);

    void startReconnect();      //开始重连
    void stopReconnect();       //停止重连
    bool isReconnecting()const;//正在重连

    void setReconnectInterval(int interval);
    void setMaxReconnectAttempts(int maxAttempts);
signals:
    void reconnectAttempt(int currentAttempt,int maxAttempts);
private slots:
    void onReconnectTimeout();  //重连超时
private:
    QTimer* m_reconnectTimer;
    int m_reconnectInterval{0};
    int m_maxReconnectAttempts{0};
    int m_currentReconnectAttempts{0};
    bool m_isReconnecting{false};
};

#endif // RECONNECTION_MANAGER_H
