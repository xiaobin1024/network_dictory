#ifndef SESSION_STORAGE_H
#define SESSION_STORAGE_H

#include "user_data.h"
#include <QObject>
#include <QJsonObject>

class SessionStorage : public QObject
{
    Q_OBJECT

public:
    explicit SessionStorage(QObject* parent = nullptr);

    // 配置
    void setStoragePath(const QString& path);
     void setUserId(const QString& userId);
    QString storagePath() const;
     QString userId() const;

     QJsonObject loadUserData() const;
    bool deleteUserData();

    // 保存完整的用户数据
    bool saveUserData(const QJsonObject& userData);

    bool saveUserAvatar(const QString& imagePath); // 新增：保存头像

    // 扫描所有用户目录，返回修改时间最新的那个目录路径
    QString findLatestUserDirectory() const;

    // 查找指定用户目录下的头像路径
    // 如果找到返回绝对路径，否则返回空字符串
    QString findUserAvatarPath(const QString& userId) const;

    QString getUserId(){return m_userId;}

signals:
    void userDataSaved();
    void userDataLoaded();
    void userDataCleared();
    void error(const QString& message);

private:
    QString getUserDataFilePath() const;
    bool ensureDirectoryExists(const QString& path) const;
    bool writeJsonFile(const QString& filePath, const QJsonObject& json) const;
    QJsonObject readJsonFile(const QString& filePath) const;


    QString m_storagePath;
    QString m_userId;
};

#endif // SESSION_STORAGE_H
