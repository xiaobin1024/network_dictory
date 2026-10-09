#include "user_session.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include<QJsonDocument>
#include "message_dispatcher.h"




UserSession::UserSession(QObject* parent)
    : QObject(parent)
    , m_storage(new SessionStorage(this))
    ,m_messageDispatcher(nullptr)
{
    qDebug() << "UserSession 创建";

    // 连接存储错误信号
    connect(m_storage, &SessionStorage::error, this, &UserSession::error);
}

UserSession::~UserSession()
{
    // 断开所有连接
    if (m_messageDispatcher) {
        disconnect(m_messageDispatcher, nullptr, this, nullptr);
    }

    // 清理数据
    m_currentUser = UserData();
    m_userData = QJsonObject();

    qDebug() << "UserSession 销毁";
}
void UserSession::setMessageDispatcher(MessageDispatcher* dispatcher)
{
    if (m_messageDispatcher == dispatcher) {
        return;
    }

    // 断开旧的连接
    if (m_messageDispatcher) {
        disconnect(m_messageDispatcher, &MessageDispatcher::loginResponseReceived,
                   this, &UserSession::processLoginResponse);
        disconnect(m_messageDispatcher, &MessageDispatcher::registerResponseReceived,
                   this, &UserSession::processRegisterResponse);
        disconnect(m_messageDispatcher, &MessageDispatcher::logoutResponseReceived,
                   this, &UserSession::processLogoutResponse);
        disconnect(m_messageDispatcher, &MessageDispatcher::unregisterResponseReceived,  // 新增
                   this, &UserSession::processUnregisterResponse);
        disconnect(m_messageDispatcher, &MessageDispatcher::errorOccurred,
                   this, &UserSession::error);
    }

    m_messageDispatcher = dispatcher;

    // 连接新的信号
    if (m_messageDispatcher) {
        connect(m_messageDispatcher, &MessageDispatcher::loginResponseReceived,
                this, &UserSession::processLoginResponse);
        connect(m_messageDispatcher, &MessageDispatcher::registerResponseReceived,
                this, &UserSession::processRegisterResponse);
        connect(m_messageDispatcher, &MessageDispatcher::logoutResponseReceived,
                this, &UserSession::processLogoutResponse);
        connect(m_messageDispatcher, &MessageDispatcher::unregisterResponseReceived,  // 新增
                this, &UserSession::processUnregisterResponse);
        connect(m_messageDispatcher, &MessageDispatcher::errorOccurred,
                this, &UserSession::error);

        qDebug() << "MessageDispatcher 已设置到 UserSession";
    }
}

MessageDispatcher* UserSession::messageDispatcher() const
{
    return m_messageDispatcher;
}

// UserSession* UserSession::instance()
// {
//     if (!m_instance) {
//         m_instance = new UserSession();
//     }
//     return m_instance;
// }

void UserSession::initialize()
{
    if (m_initialized) {
        qWarning() << "UserSession 已经初始化";
        return;
    }

    m_initialized = true;
    qDebug() << "UserSession 初始化完成";

    emit initialized();
}

bool UserSession::isInitialized() const
{
    return m_initialized;
}

UserData UserSession::currentUser() const
{
    return m_currentUser;
}

bool UserSession::isLoggedIn() const
{
    return m_currentUser.isLoggedIn() && m_currentUser.isValid();
}

bool UserSession::isGuest() const
{
    if (!m_currentUser.isValid()) {
        return true;
    }
    return m_currentUser.role() == UserRole::Guest;
}

bool UserSession::isFreeUser() const
{
    if (!m_currentUser.isValid()) {
        return false;
    }
    return m_currentUser.role() == UserRole::FreeUser;
}

bool UserSession::isPremiumUser() const
{
    if (!m_currentUser.isValid()) {
        return false;
    }
    return m_currentUser.role() == UserRole::PremiumUser;
}

void UserSession::login(const QString& username, const QString& password, bool rememberMe)
{
    if (!m_initialized) {
        emit loginFailed("UserSession 未初始化");
        return;
    }

    if (username.isEmpty() || password.isEmpty()) {
        emit loginFailed("用户名或密码不能为空");
        return;
    }
    // 生产环境：通过MessageDispatcher发送登录请求
    if (!m_messageDispatcher) {
        emit loginFailed("系统错误: 消息分发器未设置");
        qWarning() << "MessageDispatcher 未设置, 无法发送登录请求";
        return;
    }

    qDebug() << "通过MessageDispatcher发送登录请求:" << username;
    emit loginRequest(username, password);
    m_name=username,m_password=password;
}

// user_session.cpp
void UserSession::logout()
{
    if (!isLoggedIn()) {
        qDebug() << "UserSession::logout()：用户未登录，无需登出";
        return;
    }

    UserData oldUser = m_currentUser;

    if (m_messageDispatcher) {
        qDebug() << "通过MessageDispatcher发送登出请求:" << oldUser.username();
        emit logoutRequest();
        // 异步路径：processLogoutResponse 会负责最终清理
    } else {
        // 直接本地登出
        if (oldUser.isValid()) {
            m_storage->setUserId(oldUser.userId());
            m_storage->saveUserData(m_userData);
        }

        // 【修复】立即清理内存与存储上下文
        m_currentUser = UserData();
        m_userData = QJsonObject();
        m_storage->setUserId("");   // <-- 清空

        emit logoutSuccess();
        emit userChanged(m_currentUser);

        qDebug() << "用户已本地登出:" << oldUser.username();
    }
}

void UserSession::registerUser(const QString& username, const QString& password)
{
    if (!m_initialized) {
        emit registerFailed("UserSession 未初始化");
        return;
    }

    if (username.isEmpty() || password.isEmpty()) {
        emit registerFailed("用户名或密码不能为空");
        return;
    }

    if (username.length() < 3 || username.length() > 20) {
        emit registerFailed("用户名长度必须在3-20个字符之间");
        return;
    }

    if (password.length() < 6) {
        emit registerFailed("密码长度至少6个字符");
        return;
    }

    qDebug() << "通过MessageDispatcher发送注册请求:" << username;
    emit registerRequest(username, password);
}

void UserSession::processLoginResponse(const QString& responseData)
{
    if (!m_initialized) {
        emit loginFailed("UserSession 未初始化");
        return;
    }

    qDebug() << "处理登录响应:" << responseData;

    // 解析响应数据
    if (responseData.startsWith("SUCCESS:")) {
        QStringList parts = responseData.mid(8).split(":");
        if (parts.size() >= 3) {
            QString username = parts[0];
            QString password=parts[1];
            QString userId = parts[2];
            int roleInt = parts[3].toInt();
            UserRole role = static_cast<UserRole>(roleInt);

            m_storage->setUserId(userId);
            QJsonObject userDataJson=m_storage->loadUserData();

            // 创建用户数据对象
            UserData userData(userId, username, role);
            userData.setLoggedIn(true);
            userData.updateLastActiveTime();

            //从加载的JSON数据中提取历史记录
            if (!userDataJson.isEmpty()) {
                qDebug() << "成功加载用户数据，开始提取历史记录";
                if (userDataJson.contains("searchHistory")) {
                    QJsonArray historyArray = userDataJson["searchHistory"].toArray();
                    userData.setSearchHistory(historyArray);
                    qDebug() << "已从用户数据中提取历史记录，数量:" << historyArray.size();
                }
            }

            // 设置当前用户
            m_currentUser = userData;
            m_userData = userDataJson; // 保留原始JSON数据用于后续操作

            // **关键修复**：如果用户数据不存在，创建新文件
            if (m_userData.isEmpty()) {
                qDebug() << "初始化新用户数据对象";
                m_userData["userId"] = userId;
                m_userData["username"] = username;
                m_userData["password"] = password;
                m_userData["role"] = roleInt;
                m_userData["loggedIn"] = true;
                m_userData["loginTime"] = userData.loginTime().toString(Qt::ISODate);
                m_userData["lastActiveTime"] = userData.lastActiveTime().toString(Qt::ISODate);
                m_userData["searchHistory"] = QJsonArray(); // 初始化空的历史记录

                m_autoLoginEnabled = true;
                m_userData["autoLogin"] = true;

                // 保存新用户数据
                m_storage->saveUserData(m_userData);
            }else {
                // **关键修复**：更新现有用户数据
                m_userData["userId"] = userId;
                m_userData["username"] = username;
                m_userData["password"] = password;
                m_userData["role"] = roleInt;
                m_userData["loggedIn"] = true;
                m_userData["lastActiveTime"] = userData.lastActiveTime().toString(Qt::ISODate);
                m_userData["loginTime"] = userData.loginTime().toString(Qt::ISODate);

                m_autoLoginEnabled = true;
                m_userData["autoLogin"] = true;
                // 保存用户数据
                m_storage->saveUserData(m_userData);
            }

            // 保存更新后的用户数据
            bool rememberMe = (parts.size() > 3) ? (parts[3] == "1") : true;
            if (rememberMe) {
                m_autoLoginEnabled = true;
                m_userData["autoLogin"] = true;
                m_storage->saveUserData(m_userData);
            }

            emit loginSuccess(m_currentUser);
            qDebug() << "登录成功:" << username << "用户ID:" << userId;
            qDebug() << "UserSession 已发射 loginSuccess 信号";
            return;
        }
    } else if (responseData.startsWith("ERROR:")) {
        QString errorMsg = responseData.mid(6);
        emit loginFailed(errorMsg);
        m_name = nullptr;
        m_password = nullptr;
        return;
    }

    // 默认错误
    emit loginFailed("登录失败: 未知响应格式");
}
// user_session.cpp
void UserSession::processLogoutResponse(const QString& responseData)
{
    qDebug() << "处理登出响应:" << responseData;

    // 只要本地有有效用户，先保存登出状态（loggedIn=false）
    if (m_currentUser.isValid()) {
        m_storage->setUserId(m_currentUser.userId());

        m_currentUser.setLoggedIn(false);
        m_currentUser.updateLastActiveTime();

        m_userData["loggedIn"] = false;
        m_userData["lastActiveTime"] = m_currentUser.lastActiveTime().toString(Qt::ISODate);
         m_userData["autoLogin"] = false;  // 【关键】持久化标记：下次启动也不自动登录
        m_storage->saveUserData(m_userData);
        qDebug() << "本地登出状态已保存:" << m_currentUser.username();
    }

    //彻底清理会话状态，防止后续注册/登录新用户时关联到旧路径
    m_currentUser = UserData();
    m_userData = QJsonObject();
    m_storage->setUserId("");   // <-- 清空存储上下文，切断与旧用户目录的关联

     m_autoLoginEnabled = false;
    emit logoutSuccess();
    emit userChanged(m_currentUser);

    if (responseData.startsWith("SUCCESS:")) {
        qDebug() << "服务器登出成功";
    } else {
        qWarning() << "服务器登出返回异常，已强制本地清理:" << responseData;
    }
}

// user_session.cpp
void UserSession::processRegisterResponse(const QString& responseData)
{
    qDebug() << "处理注册响应:" << responseData;

    if (responseData.startsWith("SUCCESS:")) {
        QStringList parts = responseData.mid(8).split(":");

        if (parts.size() >= 3) {
            QString username = parts[0];
            QString password = parts[1];
            QString userId   = parts[2];

            qDebug() << "解析出的用户名:" << username;
            qDebug() << "解析出的密码:" << password;
            qDebug() << "解析出的用户ID:" << userId;

            if (userId.isEmpty()) {
                qWarning() << "注册响应中 userId 为空，拒绝设置存储上下文";
                emit registerFailed("注册响应格式错误: 缺少用户ID");
                return;
            }

            // 【修复】清理旧状态
            m_currentUser = UserData();
            m_userData    = QJsonObject();

            // 【关键修复】构建新用户的 UserData，让 currentUser() 有效
            m_currentUser = UserData(userId, username, UserRole::FreeUser);
            // 注册后未登录，所以 loggedIn = false
            m_currentUser.setLoggedIn(false);

            m_storage->setUserId(userId);
            qDebug() << "注册成功，已构建 UserData 并切换存储上下文到新用户:" << userId;

            emit registerSuccess(username);
        } else {
            qWarning() << "注册响应字段不足，期望至少 3 段，实际:" << parts.size();
            emit registerFailed("注册响应格式错误: 字段不足");
        }
    } else if (responseData.startsWith("ERROR:")) {
        emit registerFailed(responseData.mid(6));
    } else {
        emit registerFailed("注册失败: 未知响应格式");
    }
}


bool UserSession::canSaveWords() const
{
    return isLoggedIn();
}

bool UserSession::canCreateNotes() const
{
    return isLoggedIn();
}

bool UserSession::canExportData() const
{
    return isPremiumUser();
}

void UserSession::updateCurrentUser(const UserData& user)
{
    UserData oldUser = m_currentUser;
    m_currentUser = user;

    if (m_currentUser.isLoggedIn()) {
        m_currentUser.updateLastActiveTime();

        // 如果用户ID变化，加载对应的用户数据
        if (oldUser.userId() != user.userId()) {
            m_userData = m_storage->loadUserData();
        }
    }

    emit userChanged(m_currentUser);

    qDebug() << "用户数据已更新:" << user.username();
}

void UserSession::saveUserData(const QString& key, const QVariant& value)
{
    if (!isLoggedIn()) {
        qWarning() << "无法保存用户数据: 用户未登录";
        return;
    }

    m_userData[key] = QJsonValue::fromVariant(value);

    // 自动保存到文件
    m_storage->saveUserData(m_userData);

    qDebug() << "用户数据已保存:" << key << "=" << value;
}

QVariant UserSession::getUserData(const QString& key, const QVariant& defaultValue) const
{
    if (!isLoggedIn()) {
        qWarning() << "无法获取用户数据: 用户未登录";
        return defaultValue;
    }

    if (!m_userData.contains(key)) {
        return defaultValue;
    }

    QJsonValue value = m_userData[key];
    if (value.isUndefined() || value.isNull()) {
        return defaultValue;
    }

    return value.toVariant();
}

QString UserSession::serializeUserData(const UserData& userData)
{
    QJsonObject json = userData.toJson();
    QJsonDocument doc(json);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

UserData UserSession::deserializeUserData(const QString& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        return UserData();
    }

    return UserData::fromJson(doc.object());
}

void UserSession::reset()
{
    // 保存当前用户数据
    if (m_currentUser.isValid() && m_currentUser.isLoggedIn()) {
        if (!m_userData.isEmpty() && m_userData.contains("userId")) {
            qDebug() << "重置前保存用户数据";
            m_storage->saveUserData(m_userData);
        }
    }

    // 清除内存数据
    m_currentUser = UserData();
    m_userData = QJsonObject();

    m_initialized = false;

    qDebug() << "UserSession 已重置";
}

bool UserSession::tryAutoLoginOnce()
{
    // 本次运行已尝试过，不再重复（解决反复跳转登录页的问题）
    if (m_autoLoginAttempted) {
        qDebug() << "tryAutoLoginOnce: 本次运行已尝试过自动登录，跳过";
        return false;
    }
    m_autoLoginAttempted = true;

    // 用户主动退出过，禁用自动登录
    if (!m_autoLoginEnabled) {
        qDebug() << "tryAutoLoginOnce: 自动登录已被禁用（用户主动退出过）";
        return false;
    }

    return autoLoginFromSavedSession();
}

bool UserSession::autoLoginFromSavedSession()
{
    if (!m_initialized) {
        qWarning() << "UserSession 未初始化，无法自动登录";
        return false;
    }

    if (isLoggedIn()) {
        qDebug() << "用户已登录，无需自动登录";
        return true;
    }

    QString userDirPath = m_storage->findLatestUserDirectory();
    if (userDirPath.isEmpty()) {
        m_storage->setUserId(""); // 【修复】确保不残留旧ID
        return false;
    }

    QFileInfo dirInfo(userDirPath);
    QString userId = dirInfo.fileName();

    // 临时设置以加载数据
    m_storage->setUserId(userId);
    QJsonObject userData = m_storage->loadUserData();

    if (!userData.isEmpty() && userData.contains("loggedIn") && userData["loggedIn"].toBool()) {
        QString username = userData["username"].toString();
        QString password = userData["password"].toString();

        if (!username.isEmpty() && !password.isEmpty()) {
            emit loginRequest(username, password);
            m_name = username;
            m_password = password;
            qDebug() << "已发送自动登录请求:" << username << "ID:" << userId;
            return true;
        }
    }

    // 【修复】不满足自动登录条件，必须把 m_userId 恢复为空，避免污染后续流程
    m_storage->setUserId("");
    qDebug() << "自动登录失败: 用户未标记为已登录或数据无效";
    return false;
}


// 为了兼容，保留原有的 tryAutoLogin 方法
bool UserSession::tryAutoLogin()
{
    return autoLoginFromSavedSession();
}

void UserSession::unregisterUser(const QString& username, const QString& password)
{
    if (!m_initialized) {
        emit unregisterFailed("UserSession 未初始化");
        return;
    }

    if (username.isEmpty() || password.isEmpty()) {
        emit unregisterFailed("用户名或密码不能为空");
        return;
    }

    if (!m_messageDispatcher) {
        emit unregisterFailed("系统错误: 消息分发器未设置");
        qWarning() << "MessageDispatcher 未设置, 无法发送注销请求";
        return;
    }

    qDebug() << "通过MessageDispatcher发送注销请求:" << username;
    emit unregisterRequest(username, password);
}

void UserSession::processUnregisterResponse(const QString& responseData)
{
    qDebug() << "处理注销响应:" << responseData;

    if (responseData.startsWith("SUCCESS:")) {
        QStringList parts = responseData.mid(8).split(":");
        if (parts.size() >= 2) {
            QString username = parts[0];
            int userId = parts[1].toInt();

            // 注销成功后，清理当前用户信息
            if (m_currentUser.isValid() && m_currentUser.isLoggedIn()) {
                //m_storage->saveUserData(m_currentUser.userId(), m_userData);
                m_storage->setUserId(m_currentUser.userId());
                m_storage->deleteUserData();
            }

            m_currentUser = UserData();
            m_userData = QJsonObject();

            emit unregisterSuccess(username, userId);
            emit userChanged(m_currentUser);

            qDebug() << "用户注销成功:" << username << "用户ID:" << userId;
        } else {
            emit unregisterFailed("注销响应格式错误");
        }
    } else if (responseData.startsWith("ERROR:")) {
        QString errorMsg = responseData.mid(6);
        emit unregisterFailed(errorMsg);
    } else {
        emit unregisterFailed("注销失败: 未知响应格式");
    }
}

void UserSession::updateUserData(const UserData& userData) {
    qDebug() << "UserSession::updateUserData() - 开始执行";
    qDebug() << "用户ID:" << userData.userId();
    qDebug() << "当前用户数据状态:" << userData.toJson();

    if (m_initialized && m_storage) {
        qDebug() << "会话已初始化，存储对象有效";
        qDebug() << "准备保存用户数据，JSON大小:" << userData.toJson().size() << "字节";
        //qDebug() << "JSON数据预览:" << userData.toJson().left(100); // 显示前100个字符

        m_storage->saveUserData( userData.toJson());
        m_userData = userData.toJson();

        qDebug() << "用户数据已成功保存";
        qDebug() << "更新后的JSON大小:" << m_userData.size() << "字节";
        //qDebug() << "JSON数据预览:" << m_userData.left(100);
    } else {
        qDebug() << "警告: 会话未初始化或存储对象无效，无法保存用户数据";
    }

    qDebug() << "UserSession::updateUserData() - 执行完成";
}

void UserSession::addSearchHistory(const QString& keyword) {
    if (m_initialized) {
        // 1. 直接操作内部的 m_currentUser
        m_currentUser.addSearchHistoryItem(keyword);

        // 2. 立即持久化
        updateUserData(m_currentUser);
    }
}

QJsonArray UserSession::currentSearchHistory() const {
    // 直接从内存中的 m_currentUser 提取
    // 这样能保证获取到的是最新的、未过期的数据
    qDebug()<<"m_currentUser.userId() = "<<m_currentUser.userId();
    return m_currentUser.searchHistory();
}

void UserSession::clearSearchHistory() {
    if (m_initialized && m_currentUser.isValid()) {
        // 1. 先让 UserData 自己内部清空
        // (注意：UserData::clearSearchHistory() 只是清空内部的 m_userData["searchHistory"] 字段)
        m_currentUser.clearSearchHistory();

        // 2. 关键：必须调用 updateUserData 来同步回 UserSession 的存储层
        // 因为 clearSearchHistory() 只是改了内存里的 UserData 对象，
        // 必须通过 updateUserData 才会触发 SessionStorage 的 saveUserData
        updateUserData(m_currentUser);
    }
}


bool UserSession::saveUserAvatar(const QString& imagePath)
{
    if (!m_initialized || !m_storage) {
        qWarning() << "UserSession 未初始化，无法保存头像";
        return false;
    }
    // 直接委托给 Storage
    return m_storage->saveUserAvatar(imagePath);
}

QString UserSession::getUserAvatarPath() const
{
    // 1. 基础检查：未初始化或存储对象为空
    if (!m_initialized || !m_storage) {
        return ":/images/default_avatar.png";
    }

    // 2. 获取当前用户ID
    QString currentUserId = m_storage->getUserId();
    if (currentUserId.isEmpty()) {
        return ":/images/default_avatar.png";
    }

    // 3. 委托给 SessionStorage 进行实际的文件查找
    QString foundPath = m_storage->findUserAvatarPath(currentUserId);

    // 4. 如果找到了文件路径则返回，否则返回默认资源路径
    if (!foundPath.isEmpty()) {
        return foundPath;
    }

    return ":/images/default_avatar.png";
}
