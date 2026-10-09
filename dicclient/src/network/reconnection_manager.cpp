#include "reconnection_manager.h"

ReconnectionManager::ReconnectionManager (QObject* parent):QObject(parent),
    m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer,&QTimer::timeout,this,&ReconnectionManager::onReconnectTimeout);
}
void ReconnectionManager::startReconnect()
{
    qDebug() << "进入startReconnect()";
    if(m_isReconnecting) return;

    m_isReconnecting = true;
    m_currentReconnectAttempts = 0;

    // 第一次尝试重连
    m_reconnectTimer->start(0);
    qDebug() << "Reconnect process started";
}

void ReconnectionManager::stopReconnect()
{
    if(m_reconnectTimer->isActive() || m_isReconnecting)
    {
        m_reconnectTimer->stop();
        m_isReconnecting = false;
        m_currentReconnectAttempts = 0;
        qDebug() << "Reconnect process stopped";
    }
}

bool ReconnectionManager::isReconnecting() const
{
    return m_isReconnecting;
}

void ReconnectionManager::setReconnectInterval(int interval)
{
    m_reconnectInterval = interval;
}
void ReconnectionManager::setMaxReconnectAttempts(int maxAttempts)
{
    m_maxReconnectAttempts = maxAttempts;
}
void ReconnectionManager::onReconnectTimeout()
{
    if(m_currentReconnectAttempts >= m_maxReconnectAttempts)
    {
        qDebug() << "Max reconnect attempts reached: " << m_maxReconnectAttempts;
        stopReconnect();
        return;
    }

    m_currentReconnectAttempts++;
    emit reconnectAttempt(m_currentReconnectAttempts, m_maxReconnectAttempts);

    qDebug() << "Reconnect attempt" << m_currentReconnectAttempts
             << "/" << m_maxReconnectAttempts;

    if(m_currentReconnectAttempts < m_maxReconnectAttempts)
    {
        int interval = qMax(100, m_reconnectInterval);
        m_reconnectTimer->start(interval);
        qDebug() << "Next reconnect in" << interval << "ms";
    }
}
