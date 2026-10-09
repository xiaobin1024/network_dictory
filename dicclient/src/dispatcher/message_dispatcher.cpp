#include "message_dispatcher.h"
#include "user_session.h"
#include "word_search.h"
#include"word_collect.h"
#include"word_vocabulary.h"
#include <QDebug>

MessageDispatcher::MessageDispatcher(QObject* parent)
    : QObject(parent)
    , m_started(false)
    , m_networkConnected(false)
{
    qDebug() << "MessageDispatcher 创建";
}

void MessageDispatcher::setNetworkManager(NetworkManager* manager)
{
    if (m_networkManager == manager) {
        return;
    }

    // 断开旧的连接
    disconnectNetworkSignals();

    m_networkManager = manager;

    // 连接新的信号
    connectNetworkSignals();

    qDebug() << "NetworkManager 已设置到 MessageDispatcher";
}
void MessageDispatcher::setUserSession(UserSession* session)
{
    if (m_userSession == session) {
        return;
    }

    // 断开旧的连接
    if (m_userSession) {
        disconnect(m_userSession, &UserSession::loginRequest,
                   this, &MessageDispatcher::onLoginRequested);
        disconnect(m_userSession, &UserSession::logoutRequest,
                   this, &MessageDispatcher::onLogoutRequested);
        disconnect(m_userSession, &UserSession::registerRequest,
                   this, &MessageDispatcher::onRegisterRequested);
        disconnect(m_userSession, &UserSession::unregisterRequest,  // 新增
                   this, &MessageDispatcher::onUnregisterRequested);
    }

    m_userSession = session;

    // 连接新的信号
    if (m_userSession) {
        connect(m_userSession, &UserSession::loginRequest,
                this, &MessageDispatcher::onLoginRequested);
        connect(m_userSession, &UserSession::logoutRequest,
                this, &MessageDispatcher::onLogoutRequested);
        connect(m_userSession, &UserSession::registerRequest,
                this, &MessageDispatcher::onRegisterRequested);
        connect(m_userSession, &UserSession::unregisterRequest,  // 新增
                this, &MessageDispatcher::onUnregisterRequested);

        qDebug() << "UserSession 已设置到 MessageDispatcher";
    }
}

void MessageDispatcher::setWordSearch(WordSearch* wordSearch)
{
    if (m_wordSearch == wordSearch) {
        return;
    }

    // 断开旧的连接
    if (m_wordSearch) {
        disconnect(m_wordSearch, &WordSearch::searchRequested,  // 新增
                   this, &MessageDispatcher::onSearchRequested);
    }

    m_wordSearch = wordSearch;

    // 连接新的信号
    if (m_wordSearch) {
        connect(m_wordSearch, &WordSearch::searchRequested,  // 新增
                this, &MessageDispatcher::onSearchRequested);

        qDebug() << "UserSession 已设置到 MessageDispatcher";
    }
}

void MessageDispatcher::setWordCollect(WordCollect* wordCollect)
{
    if (m_wordCollect == wordCollect) {
        return;
    }

    // 断开旧的连接
    if (m_wordCollect) {
        disconnect(m_wordCollect, &WordCollect::collectRequested,
                   this, &MessageDispatcher::onCollectRequested);
        disconnect(m_wordCollect, &WordCollect::collectListRequested,
                   this, &MessageDispatcher::onCollectListRequested);
    }

    m_wordCollect = wordCollect;

    // 连接新的信号
    if (m_wordCollect) {
        connect(m_wordCollect, &WordCollect::collectRequested,
                this, &MessageDispatcher::onCollectRequested);
        connect(m_wordCollect, &WordCollect::collectListRequested,
                   this, &MessageDispatcher::onCollectListRequested);

        qDebug() << "WordCollect 已设置到 MessageDispatcher";
    }
}

void MessageDispatcher::setWordVocabulary(WordVocabulary* wordVocabulary)
{
    if (m_wordVocabulary == wordVocabulary) {
        return;
    }

    // 断开旧的连接
    if (m_wordVocabulary) {
        disconnect(m_wordVocabulary, &WordVocabulary::vocabularyStatusRequested,
                   this, &MessageDispatcher::onVocabularyRequested);
        disconnect(m_wordVocabulary,&WordVocabulary::vocabularyListRequested,
                   this,&MessageDispatcher::onVocabularyListRequested);
    }

    m_wordVocabulary = wordVocabulary;

    // 连接新的信号
    if (m_wordVocabulary) {
        connect(m_wordVocabulary, &WordVocabulary::vocabularyStatusRequested,
                this, &MessageDispatcher::onVocabularyRequested);
        connect(m_wordVocabulary,&WordVocabulary::vocabularyListRequested,
                this,&MessageDispatcher::onVocabularyListRequested);
        qDebug() << "WordVocabulary 已设置到 MessageDispatcher";
    }
}

void MessageDispatcher::start()
{
    if (m_started) {
        qWarning() << "MessageDispatcher 已经启动";
        return;
    }

    if (!m_networkManager) {
        qWarning() << "MessageDispatcher 启动失败: NetworkManager 未设置";
        emit errorOccurred("消息分发器启动失败: 网络管理器未设置");
        return;
    }

    m_started = true;
    m_networkConnected = m_networkManager->isConnected();

    qDebug() << "MessageDispatcher 已启动";
    emit started();
}

void MessageDispatcher::stop()
{
    if (!m_started) {
        return;
    }

    m_started = false;
    m_networkConnected = false;

    qDebug() << "MessageDispatcher 已停止";
    emit stopped();
}

bool MessageDispatcher::isStarted() const
{
    return m_started;
}

void MessageDispatcher::connectNetworkSignals()
{
    if (!m_networkManager) {
        return;
    }

    // 连接消息接收信号
    connect(m_networkManager, &NetworkManager::messageReceived,
            this, &MessageDispatcher::onMessageReceived);

    // 连接网络状态信号
    connect(m_networkManager, &NetworkManager::connectionEstablished,
            this, &MessageDispatcher::onNetworkConnected);
    connect(m_networkManager, &NetworkManager::connectionLost,
            this, &MessageDispatcher::onNetworkDisconnected);
    connect(m_networkManager, &NetworkManager::networkError,
            this, &MessageDispatcher::onNetworkError);

    // 检查当前网络状态
    m_networkConnected = m_networkManager->isConnected();
    qDebug() << "网络状态信号已连接，当前状态:" << (m_networkConnected ? "已连接" : "未连接");
}

void MessageDispatcher::disconnectNetworkSignals()
{
    if (!m_networkManager) {
        return;
    }

    disconnect(m_networkManager, &NetworkManager::messageReceived,
               this, &MessageDispatcher::onMessageReceived);
    disconnect(m_networkManager, &NetworkManager::connectionEstablished,
               this, &MessageDispatcher::onNetworkConnected);
    disconnect(m_networkManager, &NetworkManager::connectionLost,
               this, &MessageDispatcher::onNetworkDisconnected);
    disconnect(m_networkManager, &NetworkManager::networkError,
               this, &MessageDispatcher::onNetworkError);
}


void MessageDispatcher::onNetworkConnected()
{
    qDebug() << "MessageDispatcher: 网络已连接";
    m_networkConnected = true;
    emit networkStateChanged(true);

    // 如果MessageDispatcher已启动，但网络刚连接，可以处理积压的消息
    if (m_started) {
        qDebug() << "网络恢复连接，MessageDispatcher准备处理消息";
    }
}

void MessageDispatcher::onNetworkDisconnected()
{
    qDebug() << "MessageDispatcher: 网络断开连接";
    m_networkConnected = false;
    emit networkStateChanged(false);

    // 网络断开时，可以停止分发或标记状态
    if (m_started) {
        qWarning() << "网络断开，MessageDispatcher仍在运行但无法发送消息";
    }
}

void MessageDispatcher::onNetworkError(const QString& error)
{
    qWarning() << "MessageDispatcher: 网络错误:" << error;
    emit errorOccurred(QString("网络错误: %1").arg(error));
}

void MessageDispatcher::onMessageReceived(const CoreMessage::Msg& message)
{
    if (!m_started) {
        qWarning() << "MessageDispatcher 未启动，忽略收到的消息";
        return;
    }

    qDebug() << "MessageDispatcher 收到消息, 类型:" << message.type;

    // 分发消息
    dispatchMessage(message);

    // 发送通用消息信号
    emit messageDispatched(message.type, QString::fromUtf8(message.text));
}

void MessageDispatcher::dispatchMessage(const CoreMessage::Msg& message)
{
    QString data = QString::fromUtf8(message.text);

    switch (message.type) {
    case CoreMessage::MsgType::LOGIN:
        emit loginResponseReceived(data);
        break;

    case CoreMessage::MsgType::REGISTER:
        emit registerResponseReceived(data);
        break;

    case CoreMessage::MsgType::Quit:
        emit logoutResponseReceived(data);
        break;

    case CoreMessage::MsgType::UNREGISTER:  // 新增
        emit unregisterResponseReceived(data);
        break;

    case CoreMessage::MsgType::SEARCH:
        emit searchResponseReceived(data);
        break;

    case CoreMessage::MsgType::HISTORY:
        emit historyResponseReceived(data);
        break;

    case CoreMessage::MsgType::COLLECT:
        emit collectResponseReceived(data);
        break;

    case CoreMessage::MsgType::COLLECTList:
        emit CollectResponseListReceived(data);
        break;

    case CoreMessage::MsgType::VOCABULARY:
        emit vocabularyResponseReceived(data);
        break;

    case CoreMessage::MsgType::VOCABULARYLIST:
        emit vocabularyListResponseReceived(data);
        break;

    case CoreMessage::MsgType::HEARTBEAT:
        qDebug() << "收到心跳包，忽略处理";
        break;
    default:
        qWarning() << "未知消息类型:" << static_cast<int>(message.type);
        emit errorOccurred(QString("未知消息类型: %1").arg(static_cast<int>(message.type)));
        break;
    }
}

bool MessageDispatcher::sendMessage(CoreMessage::MsgType type, const QString& name,
                                    const QString& text)
{
    // if (!m_started) {
    //     emit errorOccurred("消息分发器未启动");
    //     return false;
    // }

    // if (!m_networkManager) {
    //     emit errorOccurred("网络管理器未设置");
    //     return false;
    // }

    // if (!m_networkConnected) {
    //     emit errorOccurred("网络未连接");
    //     return false;
    // }

    CoreMessage::Msg msg;
    msg.type = type;

    // 确保字符串不会溢出
    QByteArray nameBytes = name.toUtf8().left(19);
    QByteArray textBytes = text.toUtf8().left(1279);

    strncpy(msg.name, nameBytes.constData(), sizeof(msg.name) - 1);
    msg.name[sizeof(msg.name) - 1] = '\0';

    strncpy(msg.text, textBytes.constData(), sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    qDebug() << "MessageDispatcher: 发送消息, 类型:" << type << "名称:" << name;

    // 使用异步发送
    m_networkManager->sendMessageAsync(msg);

    // 注意：sendMessageAsync 是 void 函数，无法知道是否成功
    // 可以根据需要修改 NetworkManager 使其返回发送结果
    return true;
}

void MessageDispatcher::onLoginRequested(const QString& username, const QString& password)
{
    qDebug() << "MessageDispatcher: 处理登录请求, 用户名:" << username;

    if (!sendMessage(CoreMessage::MsgType::LOGIN, username, password)) {
        // 发送失败，通知登录失败
        emit loginResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}

void MessageDispatcher::onLogoutRequested()
{
    if (!m_userSession) {
        emit logoutResponseReceived("ERROR:用户会话未设置");
        return;
    }

    // 使用UserSession的当前用户信息来获取用户名
    UserData currentUser = m_userSession->currentUser();
    if (currentUser.isValid() && currentUser.isLoggedIn()) {
        QString username = currentUser.username();
        qDebug() << "MessageDispatcher: 处理登出请求, 用户名:" << username;

        if (!sendMessage(CoreMessage::MsgType::Quit, username)) {
            emit logoutResponseReceived("ERROR:网络未连接或消息发送失败");
        }
    } else {
        qDebug() << "MessageDispatcher::onLogoutRequested()：用户未登录，无需发送登出请求";
        // 即使用户未登录，也要发出成功信号，因为这可能意味着本地登出
        emit logoutResponseReceived("SUCCESS:本地登出");
    }
}

void MessageDispatcher::onRegisterRequested(const QString& username, const QString& password)
{
    qDebug() << "MessageDispatcher: 处理注册请求, 用户名:" << username;

    if (!sendMessage(CoreMessage::MsgType::REGISTER, username, password)) {
        emit registerResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}

void MessageDispatcher::onUnregisterRequested(const QString& username, const QString& password)
{
    qDebug() << "MessageDispatcher: 处理注销请求, 用户名:" << username;

    if (!sendMessage(CoreMessage::MsgType::UNREGISTER, username, password)) {
        emit unregisterResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}
void MessageDispatcher::onSearchRequested(const QString &username, const QString& word)
{
    qDebug() << "MessageDispatcher: 处理搜索请求,用户名 单词:" <<username<<" "<< word;

    if (!sendMessage(CoreMessage::MsgType::SEARCH, username,word)) {
        emit searchResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}

void MessageDispatcher::onCollectRequested(const QString& username, const QString& wordData)
{
    qDebug() << "MessageDispatcher: 处理收藏请求, 用户:" << username
             << " 数据:" << wordData;

    if (!sendMessage(CoreMessage::MsgType::COLLECT, username, wordData)) {
        emit collectResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}

void MessageDispatcher::onCollectListRequested(const QString& username)
{
    qDebug() << "MessageDispatcher: 处理查询收藏列表请求, 用户:" << username;

    if (!sendMessage(CoreMessage::MsgType::COLLECTList, username, "")) {
        emit CollectResponseListReceived("ERROR:网络未连接或消息发送失败");
    }
}

void MessageDispatcher::onVocabularyRequested(const QString& username, const QString& wordData)
{
    qDebug() << "MessageDispatcher: 处理生词请求, 用户:" << username
             << " 数据:" << wordData;

    if (!sendMessage(CoreMessage::MsgType::VOCABULARY, username, wordData)) {
        emit vocabularyResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}

void MessageDispatcher::onVocabularyListRequested(const QString& username)
{
    qDebug() << "MessageDispatcher: 处理生词列表请求, 用户:" << username;

    if (!sendMessage(CoreMessage::MsgType::VOCABULARYLIST, username,"")) {
        emit vocabularyListResponseReceived("ERROR:网络未连接或消息发送失败");
    }
}
