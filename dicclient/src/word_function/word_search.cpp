#include "word_search.h"
#include"message_dispatcher.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

WordSearch* WordSearch::s_instance = nullptr;

WordSearch* WordSearch::instance()
{
    if (!s_instance) {
        s_instance = new WordSearch();
    }
    return s_instance;
}

WordSearch::WordSearch(QObject* parent)
    : QObject(parent)
{
}

WordSearch::~WordSearch()
{
    s_instance = nullptr;
}

void WordSearch::setUserSession(UserSession* session)
{
    m_userSession = session;
}

// 修改 searchWord 方法
void WordSearch::searchWord(const QString& word)
{
    qDebug()<<" WordSearch::searchWord";
    if (word.isEmpty()) {
        emit searchFailed("单词不能为空");
        return;
    }

    // 获取当前用户名
    QString username;
    if (m_userSession) {
        UserData currentUser = m_userSession->currentUser();
        if (currentUser.isLoggedIn()) {
            username = currentUser.username();
        }
    }

    // 发射搜索请求信号，包含单词和用户名
    emit searchRequested(username, word);
}



// 添加方法实现
void WordSearch::setMessageDispatcher(MessageDispatcher* dispatcher)
{
    if (m_dispatcher == dispatcher) {
        return;
    }

    // 断开旧的连接
    if (m_dispatcher) {
        disconnect(m_dispatcher, &MessageDispatcher::searchResponseReceived,
                   this, &WordSearch::handleSearchResponse);
    }

    m_dispatcher = dispatcher;

    // 连接新的信号
    if (m_dispatcher) {
        connect(m_dispatcher, &MessageDispatcher::searchResponseReceived,
                this, &WordSearch::handleSearchResponse);

        qDebug() << "MessageDispatcher 已设置到 WordSearch";
    }
}


void WordSearch::handleSearchResponse(const QString& responseData)
{
    processSearchResponse(responseData);
}

void WordSearch::processSearchResponse(const QString& response) {
    qDebug()<<"WordSearch::processSearchResponse";
    // 检查是否为错误响应
    if (response.startsWith("ERROR:")) {
        emit searchFailed(response);
        return;
    }

    if (response == "NOT_FOUND") {
        emit searchFailed("单词未找到");
        return;
    }

    // 使用分隔符"|^|"分割字符串
    QStringList parts = response.split("|^|");

    // 现在需要至少7个字段
    if (parts.size() < 7) {
        emit searchFailed("响应格式错误，字段数量不足");
        return;
    }

    // 解析各个字段
    WordData wordData;
    wordData.word = parts[0];
    wordData.meaning = parts[1];
    wordData.phonetic = parts[2];
    wordData.example = parts[3];
    wordData.translation = parts[4];

    // 解析第6个字段：收藏状态
    bool ok1;
    int collectFlag = parts[5].toInt(&ok1);
    wordData.isCollected = ok1 && (collectFlag == 1);

    // 解析第7个字段：生词本状态
    bool ok2;
    int vocabFlag = parts[6].toInt(&ok2);
    wordData.isVocabulary = ok2 && (vocabFlag == 1);

    // 验证数据
    if (!wordData.isValid()) {
        emit searchFailed("单词数据无效");
        return;
    }

    m_currentWord = wordData;
    emit searchSuccess(wordData);
}
