#ifndef USER_SESSION_H
#define USER_SESSION_H

#include "user_data.h"
#include "session_storage.h"
#include <QObject>
#include <QJsonObject>
#include <QVariant>
#include <QStandardPaths>

// 前向声明，不要包含头文件
class MessageDispatcher;

class UserSession : public QObject
{
    Q_OBJECT

public:
    static UserSession* instance() {
        static UserSession* s_instance = new UserSession();  // 只 new 一次
        return s_instance;
    }

    // 初始化
    void initialize();
    bool isInitialized() const;

    // 设置消息分发器
    void setMessageDispatcher(MessageDispatcher* dispatcher);
    MessageDispatcher* messageDispatcher() const;  // 新增：获取消息分发器

    // 用户状态
    UserData currentUser() const;
    bool isLoggedIn() const;
    bool isGuest() const;
    bool isFreeUser() const;
    bool isPremiumUser() const;

    // 登录/登出/注册/注销
    void login(const QString& username, const QString& password, bool rememberMe = true);
    void logout();
    void registerUser(const QString& username, const QString& password);
    void unregisterUser(const QString& username, const QString& password);

    // 响应处理（被调用，不主动请求）
    void processLoginResponse(const QString& responseData);
    void processLogoutResponse(const QString& responseData);
    void processRegisterResponse(const QString& responseData);
    void processUnregisterResponse(const QString& responseData);

    // 自动登录
    bool tryAutoLogin();
    // 自动登录上次保存的用户
    bool autoLoginFromSavedSession();

    bool tryAutoLoginOnce();
    bool isAutoLoginEnabled() const { return m_autoLoginEnabled; }

    // 权限检查
    bool canSaveWords() const;
    bool canCreateNotes() const;
    bool canExportData() const;

    // 用户数据
    void updateCurrentUser(const UserData& user);
    void saveUserData(const QString& key, const QVariant& value);
    QVariant getUserData(const QString& key, const QVariant& defaultValue = QVariant()) const;

    // 序列化
    static QString serializeUserData(const UserData& userData);
    static UserData deserializeUserData(const QString& data);

    // 重置
    void reset();

    // 添加更新用户数据的方法
    void updateUserData(const UserData& userData);

    void addSearchHistory(const QString& keyword);
    QJsonArray currentSearchHistory() const;
    void clearSearchHistory();

    bool saveUserAvatar(const QString& imagePath); // 供 RegisterPage 调用

    // 获取当前用户的头像路径
    QString getUserAvatarPath() const;

signals:
    // 状态变化
    void initialized();
    void loginSuccess(const UserData& user);
    void loginFailed(const QString& error);
    void logoutSuccess();
    void logoutChange(const bool flage);
    void userChanged(const UserData& user);
    void registerSuccess(const QString& username);
    void registerFailed(const QString& error);

    void unregisterRequest(const QString& username, const QString& password);
    void unregisterSuccess(const QString& username, int userId);
    void unregisterFailed(const QString& error);


    // 请求信号（不处理网络，只发出请求）
    void loginRequest(const QString& username, const QString& password);
    void logoutRequest();
    void registerRequest(const QString& username, const QString& password);

    // 错误
    void error(const QString& message);

private:
    UserSession(QObject* parent = nullptr);
    ~UserSession();


private:
    //static UserSession* m_instance;

    SessionStorage* m_storage = nullptr;
    MessageDispatcher* m_messageDispatcher = nullptr;
    UserData m_currentUser;
    QJsonObject m_userData;
    bool m_initialized = false;

    QString m_name,m_password;
    bool m_autoLoginEnabled = true;    // 用户主动退出后设为 false
    bool m_autoLoginAttempted = false; // 本次程序运行是否已尝试过
};
#endif
