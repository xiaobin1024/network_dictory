#include "message_queue_manager.h"

MessageQueueManager::MessageQueueManager(QObject* parent) : QObject(parent) {}


void MessageQueueManager::enqueueMessage(const CoreMessage::Msg &message)
{
    QMutexLocker locker(&m_queueMutex);
    m_messageQueue.enqueue(message);
}
CoreMessage::Msg MessageQueueManager::dequeueMessage()
{
    QMutexLocker locker(&m_queueMutex);
    if(!m_messageQueue.isEmpty())
    {
        return m_messageQueue.dequeue();
    }
    return CoreMessage::Msg();
}
void MessageQueueManager::clear()
{
    QMutexLocker locker(&m_queueMutex);
    m_messageQueue.clear();
}
bool MessageQueueManager::isEmpty() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.isEmpty();
}
int MessageQueueManager::size() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.size();
}
void MessageQueueManager::processQueue(std::function<bool(const CoreMessage::Msg&)> sendFunction)
{
    QMutexLocker locker(&m_queueMutex);

    int processedCount = 0;
    int failedCount = 0;

    while(!m_messageQueue.isEmpty())
    {
        CoreMessage::Msg message = m_messageQueue.dequeue();
        if(sendFunction(message))
        {
            processedCount++;
        }
        else
        {
            m_messageQueue.prepend(message);
            failedCount++;
            break;
        }
    }

    if(processedCount > 0 || failedCount > 0)
    {
        qDebug() << "Message queue processed:" << processedCount << "sent,"
                 << failedCount << "failed, remaining:" << m_messageQueue.size();
    }
}
