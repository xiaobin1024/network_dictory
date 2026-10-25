#include "word_vocabulary.h"
#include "message_dispatcher.h"
#include <QStringList>
#include <QDebug>

WordVocabulary* WordVocabulary::s_instance = nullptr;

WordVocabulary* WordVocabulary::instance()
{
    if (!s_instance) {
        s_instance = new WordVocabulary();
    }
    return s_instance;
}

WordVocabulary::WordVocabulary(QObject* parent)
    : QObject(parent)
{
}

WordVocabulary::~WordVocabulary()
{
    s_instance = nullptr;
}

void WordVocabulary::setMessageDispatcher(MessageDispatcher* dispatcher)
{
    if (m_dispatcher == dispatcher) {
        return;
    }

    // 断开旧的连接
    if (m_dispatcher) {
        disconnect(m_dispatcher, &MessageDispatcher::vocabularyResponseReceived,
                   this, &WordVocabulary::handleVocabularyResponse);
        disconnect(m_dispatcher,&MessageDispatcher::vocabularyListResponseReceived,
                this,&WordVocabulary::handleVocabularyListResponse);
    }

    m_dispatcher = dispatcher;

    // 连接新的信号
    if (m_dispatcher) {
        connect(m_dispatcher, &MessageDispatcher::vocabularyResponseReceived,
                this, &WordVocabulary::handleVocabularyResponse);
        connect(m_dispatcher,&MessageDispatcher::vocabularyListResponseReceived,
                this,&WordVocabulary::handleVocabularyListResponse);

        qDebug() << "MessageDispatcher 已设置到 WordVocabulary";
    }
}

void WordVocabulary::setUserSession(UserSession* session)
{
    m_userSession = session;
}

void WordVocabulary::updateVocabularyStatus(const WordData& wordData)
{
    if (!m_userSession) {
        emit vocabularyOperationFailed(wordData.word, "用户会话未设置");
        return;
    }

    if (!wordData.isValid()) {
        emit vocabularyOperationFailed(wordData.word,"单词数据无效");
        return;
    }

    // 获取当前用户名
    QString username = m_userSession->currentUser().username();
    if (username.isEmpty()) {
        emit vocabularyOperationFailed(wordData.word,"用户未登录");
        return;
    }

    // 构建生词本数据字符串
    // 格式: 单词|^|释义|^|音标|^|例句|^|翻译|^|当前生词本状态
    QString vocabularyData = QString("%1|^|%2|^|%3|^|%4|^|%5|^|%6")
                                 .arg(wordData.word)
                                 .arg(wordData.meaning)
                                 .arg(wordData.phonetic)
                                 .arg(wordData.example)
                                 .arg(wordData.translation)
                                 .arg(wordData.isVocabulary ? "1" : "0");

    qDebug() << "WordVocabulary::updateVocabularyStatus - 发送生词本请求:" << vocabularyData;

    // 发射生词本操作请求信号
    emit vocabularyStatusRequested(username, vocabularyData);
}

void WordVocabulary::handleVocabularyResponse(const QString& responseData)
{
    processVocabularyResponse(responseData);
}

void WordVocabulary::processVocabularyResponse(const QString& response)
{
    qDebug() << "WordVocabulary::processVocabularyResponse - 响应:" << response;

    // 按 |^| 分割，提取状态和单词
    QStringList parts = response.split("|^|");
    QString status = parts.value(0);
    QString word = parts.value(1);  // 第二个字段是单词

    if (status.startsWith("ERROR:")) {
        emit vocabularyOperationFailed(word, response);
        return;
    }

    if (status == "add_success") {
        emit vocabularyStatusChanged(word, true);
        emit vocabularyOperationSuccess(word, "添加生词成功");
        return;
    }

    if (status == "add_error") {
        emit vocabularyOperationFailed(word, "添加生词失败");
        return;
    }

    if (status == "remove_success") {
        emit vocabularyStatusChanged(word, false);
        emit vocabularyOperationSuccess(word, "移出生词本成功");
        return;
    }

    if (status == "remove_error") {
        emit vocabularyOperationFailed(word, "移出生词本失败");
        return;
    }

    if (status == "format is incorrect") {
        emit vocabularyOperationFailed(word, "数据格式错误");
        return;
    }

    emit vocabularyOperationFailed(word, "操作失败，未知响应: " + response);
}

 void WordVocabulary::queryVocabularyList()
{
     if (!m_userSession) {
         emit vocabularyListFailed("用户会话未设置");
         return;
     }

     // 获取当前用户名
     QString username = m_userSession->currentUser().username();
     if (username.isEmpty()) {
         emit vocabularyListFailed("用户未登录");
         return;
     }

     m_vocabularyList.clear();
     qDebug() << "WordVocabulary::queryVocabularyList - 查询生词本列表，用户:" << username;

     // 发射查询收藏列表请求信号
     emit vocabularyListRequested(username);
}

void WordVocabulary::handleVocabularyListResponse(const QString& responseData)
{
    processVocabularyListResponse(responseData);
}

void WordVocabulary::processVocabularyListResponse(const QString& response)
{
    qDebug() << "WordVocabulary::processVocabularyListResponse - 响应:" << response;

    // 1. 错误处理
    if (response.startsWith("ERROR:")) {
        emit vocabularyListFailed(response);
        return;
    }

    // 2. 空数据处理
    if (response == "**EMPTY**") {
        m_vocabularyList.clear();
        emit vocabularyListSuccess(m_vocabularyList);
        return;
    }

    // 3. 结束标志处理
    if (response == "**OVER**") {
        return;
    }

    // 4. 正常数据解析
    // 后端格式：单词|^|释义|^|音标|^|例句|^|翻译|^|收藏标识(1/0)
    QStringList lines = response.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QStringList parts = line.split("|^|");

        // 检查是否有 6 个字段 (前5个是单词信息，第6个是状态)
        if (parts.size() >= 6) {
            WordData wordData;

            // 按顺序赋值基础信息
            wordData.word = parts[0];
            wordData.meaning = parts[1];
            wordData.phonetic = parts[2];
            wordData.example = parts[3];
            wordData.translation = parts[4];

            // 因为在生词本列表中，所以 isVocabulary 必定为 true
            wordData.isVocabulary = true;

            // 解析第6个字段作为收藏状态 (isCollect)
            QString collectFlag = parts[5];
            wordData.isCollected = (collectFlag == "1");

            if (wordData.isValid()) {
                m_vocabularyList.append(wordData);
            }
        } else {
            qDebug() << "生词本数据格式错误，字段不足:" << line;
        }
    }

    // 5. 发送最终信号
    if (m_vocabularyList.isEmpty()) {
        emit vocabularyListFailed("没有生词记录");
    } else {
        emit vocabularyListSuccess(m_vocabularyList);
    }
}
