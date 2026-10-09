#ifndef WORD_VOCABULARY_H
#define WORD_VOCABULARY_H

#include <QObject>
#include "word_data.h" // 确保包含WordData定义
#include "user_session.h" // 确保包含UserSession定义

// 前向声明
class MessageDispatcher;

class WordVocabulary : public QObject
{
    Q_OBJECT

public:
    static WordVocabulary* instance();
    ~WordVocabulary();

    // 添加/移出生词本
    void updateVocabularyStatus(const WordData& wordData);

    //获取生词记录列表
     void queryVocabularyList();

    // 获取当前收藏列表
    QList<WordData> getVocabularyList() const { return m_vocabularyList; }

    // 设置消息分发器
    void setMessageDispatcher(MessageDispatcher* dispatcher);

    // 设置用户会话
    void setUserSession(UserSession* session);

signals:
    // 生词本操作请求信号
    void vocabularyStatusRequested(const QString& username, const QString& wordData);

    // 生词本操作成功信号
    void vocabularyOperationSuccess(const QString& word, const QString& message);

    // 生词本操作失败信号
    void vocabularyOperationFailed(const QString& word, const QString& error);
    // 当生词本状态改变时发射
    void vocabularyStatusChanged(const QString& word, bool isVocabulary);

    // 查询生词记录列表请求信号
    void vocabularyListRequested(const QString& username);

    // 查询生词记录列表成功信号
    void vocabularyListSuccess(const QList<WordData>& collectList);

    // 查询生词记录列表失败信号
    void vocabularyListFailed(const QString& error);

private slots:
    // 处理生词本操作响应
    void handleVocabularyResponse(const QString& responseData);

    //处理生词本记录操作响应
    void handleVocabularyListResponse(const QString& responseData);

private:
    explicit WordVocabulary(QObject* parent = nullptr);

    // 处理生词本响应
    void processVocabularyResponse(const QString& response);

    //处理生词本记录响应
     void processVocabularyListResponse(const QString& response);

    // 静态实例
    static WordVocabulary* s_instance;

    // 消息分发器
    MessageDispatcher* m_dispatcher{nullptr};

    // 用户会话
    UserSession* m_userSession{nullptr};

    // 收藏列表
    QList<WordData> m_vocabularyList;


};

#endif // WORD_VOCABULARY_H
