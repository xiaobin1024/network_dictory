#ifndef WORD_COLLECT_H
#define WORD_COLLECT_H

#include <QObject>
#include "word_data.h"
#include "user_session.h"

// 前向声明
class MessageDispatcher;

class WordCollect : public QObject
{
    Q_OBJECT

public:
    static WordCollect* instance();
    ~WordCollect();

    // 收藏/取消收藏单词
    void collectWord(const WordData& wordData);

    // 查询收藏列表
    void queryCollectList();

    // 获取当前收藏列表
    QList<WordData> getCollectList() const { return m_collectList; }

    // 设置消息分发器
    void setMessageDispatcher(MessageDispatcher* dispatcher);

    // 设置用户会话
    void setUserSession(UserSession* session);

    bool isCollected(const QString& word);

signals:
    // 收藏请求信号
    void collectRequested(const QString& username, const QString& wordData);

    // 查询收藏列表请求信号
    void collectListRequested(const QString& username);

    // 收藏成功信号
    void collectSuccess(const QString& message);

    // 收藏失败信号
    void collectFailed(const QString& error);

    // 查询收藏列表成功信号
    void collectListSuccess(const QList<WordData>& collectList);

    // 查询收藏列表失败信号
    void collectListFailed(const QString& error);

    void collectStatusChanged(QString& word,bool isCollected); // 当收藏状态改变时发射

public slots:
    // 处理收藏响应
    void handleCollectResponse(const QString& responseData);

    // 处理查询收藏列表响应
    void handleCollectListResponse(const QString& responseData);

private:
    explicit WordCollect(QObject* parent = nullptr);

    // 处理收藏响应
    void processCollectResponse(const QString& response);

    // 处理查询收藏列表响应
    void processCollectListResponse(const QString& response);

    // 静态实例
    static WordCollect* s_instance;

    // 收藏列表
    QList<WordData> m_collectList;

    // 消息分发器
    MessageDispatcher* m_dispatcher{nullptr};

    // 用户会话
    UserSession* m_userSession{nullptr};
};

#endif // WORD_COLLECT_H
