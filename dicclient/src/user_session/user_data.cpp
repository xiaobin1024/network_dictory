#include "user_data.h"
#include <QJsonDocument>
#include <QDebug>

UserData::UserData()
    : m_role(UserRole::Guest)
    , m_loggedIn(false)
{
}

UserData::UserData(const QString& userId, const QString& username, UserRole role)
    : m_userId(userId)
    , m_username(username)
    , m_role(role)
    , m_loggedIn(false)
{
}

QString UserData::userId() const
{
    return m_userId;
}

QString UserData::username() const
{
    return m_username;
}

UserRole UserData::role() const
{
    return m_role;
}

bool UserData::isLoggedIn() const
{
    return m_loggedIn;
}

bool UserData::isPremium() const
{
    return m_role == UserRole::PremiumUser;
}

QDateTime UserData::loginTime() const
{
    return m_loginTime;
}

QDateTime UserData::lastActiveTime() const
{
    return m_lastActiveTime;
}

void UserData::setUserId(const QString& id)
{
    m_userId = id;
}

void UserData::setUsername(const QString& name)
{
    m_username = name;
}

void UserData::setRole(UserRole role)
{
    m_role = role;
}

void UserData::setLoggedIn(bool loggedIn)
{
    m_loggedIn = loggedIn;
    if (loggedIn && !m_loginTime.isValid()) {
        m_loginTime = QDateTime::currentDateTime();
    }
    updateLastActiveTime();
}

void UserData::setLoginTime(const QDateTime& time)
{
    m_loginTime = time;
}

void UserData::setLastActiveTime(const QDateTime& time)
{
    m_lastActiveTime = time;
}

void UserData::updateLastActiveTime()
{
    m_lastActiveTime = QDateTime::currentDateTime();
}

QJsonObject UserData::toJson() const
{
    QJsonObject json;
    // 1. 先添加基础属性
    json["userId"] = m_userId;
    json["username"] = m_username;
    json["password"]=m_password;
    json["role"] = static_cast<int>(m_role);
    json["loggedIn"] = m_loggedIn;

    if (m_loginTime.isValid()) {
        json["loginTime"] = m_loginTime.toString(Qt::ISODate);
    }
    if (m_lastActiveTime.isValid()) {
        json["lastActiveTime"] = m_lastActiveTime.toString(Qt::ISODate);
    }

    // 2. 关键修复：将 m_userData 中的内容（如 searchHistory）合并到主 JSON 中
    // 遍历 m_userData，把里面的 key-value 放到主 json 里
    for (auto it = m_userData.begin(); it != m_userData.end(); ++it) {
        json[it.key()] = it.value();
    }

    return json;
}

UserData UserData::fromJson(const QJsonObject& json)
{
    UserData userData;

    // 1. 读取基础属性
    if (json.contains("userId")) {
        userData.m_userId = json["userId"].toString();
    }
    if (json.contains("username")) {
        userData.m_username = json["username"].toString();
    }
    if(json.contains("password")){
        userData.m_password=json["password"].toString();
    }
    if (json.contains("role")) {
        userData.m_role = static_cast<UserRole>(json["role"].toInt());
    }
    if (json.contains("loggedIn")) {
        userData.m_loggedIn = json["loggedIn"].toBool();
    }
    if (json.contains("loginTime")) {
        userData.m_loginTime = QDateTime::fromString(json["loginTime"].toString(), Qt::ISODate);
    }
    if (json.contains("lastActiveTime")) {
        userData.m_lastActiveTime = QDateTime::fromString(json["lastActiveTime"].toString(), Qt::ISODate);
    }

    // 2. 关键修复：将 JSON 中其他字段（非基础属性）存入 m_userData
    // 遍历 JSON，把不属于 UserSession 基础属性的 key-value 存入 m_userData
    for (auto it = json.begin(); it != json.end(); ++it) {
        QString key = it.key();
        // 如果这个 key 不是我们上面处理过的基础属性，则存入 m_userData
        if (key != "userId" &&
            key != "username" &&
            key!="password"&&
            key != "role" &&
            key != "loggedIn" &&
            key != "loginTime" &&
            key != "lastActiveTime") {
            userData.m_userData[key] = it.value();
        }
    }

    return userData;
}

bool UserData::isValid() const
{
    return !m_userId.isEmpty() && !m_username.isEmpty();
}

QJsonArray UserData::searchHistory() const {
    if (m_userData.contains("searchHistory")) {
        QJsonArray history = m_userData["searchHistory"].toArray();
        qDebug() << "UserData::searchHistory() - 返回历史记录数量:" << history.size();
        return history;
    }
    qDebug() << "UserData::searchHistory() - 未找到历史记录";
    return QJsonArray();
}

void UserData::setSearchHistory(const QJsonArray& history) {
    qDebug() << "UserData::setSearchHistory() - 设置历史记录数量:" << history.size();
    m_userData["searchHistory"] = history;
}

void UserData::addSearchHistoryItem(const QString& keyword) {
    qDebug() << "UserData::addSearchHistoryItem - 开始执行";
    qDebug() << "keyword = " << keyword;

    QJsonArray history = searchHistory();
    qDebug() << "当前历史记录数量: " << history.size();

    // 检查是否已存在该关键词
    bool exists = false;
    for (int i = 0; i < history.size(); ++i) {
        QJsonObject item = history[i].toObject();
        if (item["keyword"].toString() == keyword) {
            qDebug() << "找到匹配的关键词，更新现有记录";
            qDebug() << "更新前 - keyword: " << item["keyword"].toString()
                     << ", timestamp: " << item["timestamp"].toString()
                     << ", searchCount: " << item["searchCount"].toInt();

            // 更新时间戳和计数
            item["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            item["searchCount"] = item["searchCount"].toInt() + 1;
            history[i] = item;
            exists = true;
            break;
        }
    }

    // 如果不存在，添加新记录
    if (!exists) {
        qDebug() << "未找到匹配关键词，创建新记录";
        QJsonObject newItem;
        newItem["keyword"] = keyword;
        newItem["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        newItem["searchCount"] = 1;
        history.append(newItem);

        qDebug() << "新记录已添加 - keyword: " << newItem["keyword"].toString()
                 << ", timestamp: " << newItem["timestamp"].toString()
                 << ", searchCount: " << newItem["searchCount"].toInt();
    }

    // 保持最近10条记录
    if (history.size() > 10) {
        qDebug() << "历史记录数量超过10条，进行截断";
        QJsonArray tempArray;
        // 从倒数第10个元素开始，复制到数组末尾
        for (int i = history.size() - 10; i < history.size(); ++i) {
            tempArray.append(history[i]);
        }
        history = tempArray;
    }

    qDebug() << "设置更新后的历史记录，数量: " << history.size();
    setSearchHistory(history);

    qDebug() << "UserData::addSearchHistoryItem - 执行完成";
}

void UserData::clearSearchHistory() {
    m_userData.remove("searchHistory");
}
