// word_search.h
#ifndef WORD_SEARCH_H
#define WORD_SEARCH_H

#include <QObject>
#include "word_data.h"
#include"user_session.h"

// 前向声明，不要包含头文件
class MessageDispatcher;

class WordSearch : public QObject
{
    Q_OBJECT

public:
    static WordSearch* instance();
    ~WordSearch();

    // 搜索单词
    void searchWord(const QString& word);

    // 获取当前搜索的单词
    WordData getCurrentWord() const { return m_currentWord; }

    void setMessageDispatcher(MessageDispatcher* dispatcher);

    void setUserSession(UserSession* session);


signals:
    // 搜索请求信号
    void searchRequested(const QString& username, const QString& word);
    // 搜索成功信号
    void searchSuccess(const WordData& word);

    // 搜索失败信号
    void searchFailed(const QString& error);

public slots:
    // 处理搜索响应
    void handleSearchResponse(const QString& responseData);

private:
    explicit WordSearch(QObject* parent = nullptr);

    void processSearchResponse(const QString& response);

    static WordSearch* s_instance;

    WordData m_currentWord;
    MessageDispatcher* m_dispatcher{nullptr};
   UserSession* m_userSession{nullptr};
};

#endif // WORD_SEARCH_H
