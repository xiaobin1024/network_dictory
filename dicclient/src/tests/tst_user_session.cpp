#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>

// 包含要测试的模块
#include "user_session/user_session.h"
#include "dispatcher/message_dispatcher.h"
#include "network/network_manager.h"

using namespace testing;

/**
 * @brief UserSession 和 MessageDispatcher 集成测试
 * 测试服务器注册、登录、退出功能
 */

// 创建全局的 QApplication 实例
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        char* argv[] = {nullptr};
        app = new QApplication(argc, argv);
    }

    void TearDown() override {
        if (app) {
            qDebug() << "清理 QApplication";
            delete app;
            app = nullptr;
        }
    }

private:
    QApplication* app;
};

// 在 main 函数之前注册环境
::testing::Environment* const qt_env = ::testing::AddGlobalTestEnvironment(new QtTestEnvironment);

/**
 * @brief UserSession 基础测试
 */
class UserSessionTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // 获取 UserSession 实例
        userSession = UserSession::instance();

        // 初始化 UserSession
        userSession->initialize();

        // // 等待初始化完成
        // QSignalSpy initSpy(userSession, &UserSession::initialized);
        // if (initSpy.wait(1000)) {
        //     qDebug() << "UserSession 初始化成功";
        // } else {
        //     qWarning() << "UserSession 初始化超时";
        // }
    }

    void TearDown() override {
        // 重置 UserSession
        if (userSession) {
            userSession->reset();
        }
    }

    UserSession* userSession = nullptr;
};

/**
 * @brief 测试 UserSession 的基本功能
 */
TEST_F(UserSessionTestFixture, BasicFunctionTest) {
    EXPECT_TRUE(userSession->isInitialized());
    EXPECT_FALSE(userSession->isLoggedIn());
    EXPECT_TRUE(userSession->isGuest());
    EXPECT_FALSE(userSession->isFreeUser());
    EXPECT_FALSE(userSession->isPremiumUser());

    // 测试未登录时的权限检查
    EXPECT_FALSE(userSession->canSaveWords());
    EXPECT_FALSE(userSession->canCreateNotes());
    EXPECT_FALSE(userSession->canExportData());

    // 测试当前用户
    UserData currentUser = userSession->currentUser();
    EXPECT_FALSE(currentUser.isValid());
    EXPECT_FALSE(currentUser.isLoggedIn());
}

/**
 * @brief 测试 UserSession 的序列化功能
 */
TEST_F(UserSessionTestFixture, SerializationTest) {
    // 创建测试用户数据
    UserData testUser("123", "testuser", UserRole::FreeUser);
    testUser.setLoggedIn(true);
    testUser.updateLastActiveTime();

    // 序列化
    QString serialized = UserSession::serializeUserData(testUser);
    EXPECT_FALSE(serialized.isEmpty());

    // 反序列化
    UserData deserialized = UserSession::deserializeUserData(serialized);
    EXPECT_TRUE(deserialized.isValid());
    EXPECT_EQ(deserialized.userId(), "123");
    EXPECT_EQ(deserialized.username(), "testuser");
    EXPECT_EQ(deserialized.role(), UserRole::FreeUser);
    EXPECT_TRUE(deserialized.isLoggedIn());
}

/**
 * @brief 测试 UserSession 的用户数据存储
 */
TEST_F(UserSessionTestFixture, UserDataStorageTest) {
    // 由于未登录，保存用户数据应该失败
    userSession->saveUserData("test_key", "test_value");

    // 尝试获取用户数据
    QVariant data = userSession->getUserData("test_key");
    EXPECT_FALSE(data.isValid()) << "未登录时不应能保存或获取用户数据";
}

/**
 * @brief UserSession 和 MessageDispatcher 集成测试
 */
class DispatcherIntegrationTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        qDebug() << "\n=== 设置测试环境 ===";

        // 创建 NetworkManager
        networkManager = new NetworkManager();

        // 创建 MessageDispatcher
        messageDispatcher = new MessageDispatcher();

        // 获取 UserSession 实例
        userSession = UserSession::instance();
        userSession->initialize();

        // 设置 MessageDispatcher
        messageDispatcher->setNetworkManager(networkManager);
        messageDispatcher->setUserSession(userSession);
        messageDispatcher->start();

        // 生成唯一的测试用户名
        testUsername = QString("testuser_%1").arg(QDateTime::currentMSecsSinceEpoch());
        testPassword = "TestPassword123";

        qDebug() << "测试用户名:" << testUsername;
        qDebug() << "服务器地址: 192.168.23.132:8080";
    }

    void TearDown() override {
        qDebug() << "\n=== 清理测试环境 ===";

        // 停止消息分发器
        if (messageDispatcher) {
            messageDispatcher->stop();
        }

        // 断开网络连接
        if (networkManager && networkManager->isConnected()) {
            networkManager->disconnectFromServer();
        }

        // 重置 UserSession
        if (userSession) {
            userSession->reset();
        }

        // 删除 MessageDispatcher
        if (messageDispatcher) {
            messageDispatcher->deleteLater();
            messageDispatcher = nullptr;
        }

        // 处理剩余事件
        QApplication::processEvents();
        QTest::qWait(100);
    }

    // 等待信号，带有超时
    bool waitForSignal(QSignalSpy& spy, int timeout = 5000) {
        return spy.wait(timeout);
    }

    // 连接到服务器
    bool connectToServer() {
        if (!networkManager) {
            qWarning() << "NetworkManager 未初始化";
            return false;
        }

        if (networkManager->isConnected()) {
            qDebug() << "已经连接到服务器";
            return true;
        }

        qDebug() << "正在连接到服务器...";

        // 连接服务器
        networkManager->connectToServer();

        //等待建立连接
        QTest::qWait(500);

        // // 等待连接建立
        // QSignalSpy connectedSpy(networkManager, &NetworkManager::connectionEstablished);
        // QSignalSpy errorSpy(networkManager, &NetworkManager::networkError);

        // if (connectedSpy.wait(5000)) {
        //     qDebug() << "成功连接到服务器";
        //     return true;
        // } else if (errorSpy.count() > 0) {
        //     QList<QVariant> arguments = errorSpy.takeFirst();
        //     QString error = arguments.at(0).toString();
        //     qWarning() << "连接服务器失败:" << error;
        //     return false;
        // } else {
        //     qWarning() << "连接服务器超时";
        //     return false;
        // }
    }

    NetworkManager* networkManager = nullptr;
    MessageDispatcher* messageDispatcher = nullptr;
    UserSession* userSession = nullptr;
    QString testUsername;
    QString testPassword;
};

/**
 * @brief 测试网络连接
 */
// TEST_F(DispatcherIntegrationTestFixture, NetworkConnectionTest) {
//     // 测试网络连接
//     bool connected = connectToServer();

//     if (connected) {
//         EXPECT_TRUE(networkManager->isConnected());
//         qDebug() << "网络连接测试通过";
//     } else {
//         // 如果无法连接，跳过后续测试
//         qDebug() << "无法连接到服务器，跳过网络相关测试";
//     }
// }
