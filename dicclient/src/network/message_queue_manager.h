#ifndef MESSAGE_QUEUE_MANAGER_H
#define MESSAGE_QUEUE_MANAGER_H

#include <QObject>
#include"utils.h"
#include"message.h"
#include<QQueue>
#include<QMutex>
#include<functional>

class MessageQueueManager : public QObject
{
    Q_OBJECT
public:
    explicit MessageQueueManager(QObject* parent=nullptr);

    //消息队列操作
    void enqueueMessage(const CoreMessage::Msg &message);
    CoreMessage::Msg dequeueMessage();
    void clear();
    bool isEmpty() const;
    int size() const;
    //处理队列
    void processQueue(std::function<bool(const CoreMessage::Msg&)> sendFunction);
private:
    QQueue<CoreMessage::Msg>m_messageQueue;
    mutable QRecursiveMutex m_queueMutex;    //递归锁保护消息队列
};

#endif // MESSAGE_QUEUE_MANAGER_H
