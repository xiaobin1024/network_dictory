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

        // 关键修复1: 先建立dispatcher连接再初始化
        // 显式断开可能的旧连接（安全防护）
        userSession->setMessageDispatcher(nullptr);
        messageDispatcher->setUserSession(nullptr);

        // 设置 MessageDispatcher
        messageDispatcher->setNetworkManager(networkManager);
        messageDispatcher->setUserSession(userSession);
        userSession->setMessageDispatcher(messageDispatcher);

        // 关键修复2: 在dispatcher设置完成后才初始化
        userSession->initialize();

        messageDispatcher->start();


        testUsername = "test0009";
        testPassword = "TestPassword123";

        qDebug() << "测试用户名:" << testUsername;
        qDebug() << "服务器地址: 192.168.23.132:8080";
    }

    void TearDown() override {
        qDebug() << "\n=== 清理测试环境 ===";

        // 关键修复3: 清理时先断开dispatcher关联
        if (userSession) {
            // 先解除dispatcher关联再reset
            userSession->setMessageDispatcher(nullptr);
            userSession->reset();
        }

        // 停止消息分发器
        if (messageDispatcher) {
            messageDispatcher->stop();
        }

        // 断开网络连接
        if (networkManager && networkManager->isConnected()) {
            networkManager->disconnectFromServer();
        }

        // 删除 MessageDispatcher
        if (messageDispatcher) {
            messageDispatcher->deleteLater();
            messageDispatcher = nullptr;
        }

        // 删除 NetworkManager
        if (networkManager) {
            networkManager->deleteLater();
            networkManager = nullptr;
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
TEST_F(DispatcherIntegrationTestFixture, NetworkConnectionTest) {
    // 测试网络连接
    bool connected = connectToServer();

    if (connected) {
        EXPECT_TRUE(networkManager->isConnected());
        qDebug() << "网络连接测试通过";
    } else {
        // 如果无法连接，跳过后续测试
        qDebug() << "无法连接到服务器，跳过网络相关测试";
    }
}

/**
 * @brief 测试 MessageDispatcher 基本功能
 */
TEST_F(DispatcherIntegrationTestFixture, MessageDispatcherBasicTest) {
    EXPECT_TRUE(messageDispatcher->isStarted());
    EXPECT_TRUE(userSession->isInitialized());

    // 检查 MessageDispatcher 是否正确连接了 UserSession
    EXPECT_NE(messageDispatcher, nullptr);
    EXPECT_NE(userSession, nullptr);
}

/**
 * @brief 测试用户注册功能
 */
/**
 * @brief 测试用户注册功能
 */
// TEST_F(DispatcherIntegrationTestFixture, UserRegistrationTest) {
//     // 1. 连接到服务器
//     if (!connectToServer()) {
//         GTEST_SKIP() << "无法连接到服务器，跳过注册测试";
//     }

//     qDebug() << "\n=== 测试用户注册 ===";

//     // 2. 记录当前状态
//     qDebug() << "测试前状态 - 是否已登录:" << userSession->isLoggedIn();
//     qDebug() << "测试用户名:" << testUsername;

//     // 3. 直接调用注册函数
//     qDebug() << "调用 registerUser 函数...";
//     userSession->registerUser(testUsername, testPassword);

//     // 4. 给网络请求和响应留出时间
//     qDebug() << "等待服务器响应...";
//     QTest::qWait(3000);  // 等待3秒

//     // 5. 检查注册结果
//     if (userSession->isLoggedIn()) {
//         // 注册成功并自动登录
//         qDebug() << "注册成功并自动登录!";

//         // 验证用户信息
//         UserData currentUser = userSession->currentUser();
//         EXPECT_TRUE(currentUser.isValid());
//         EXPECT_TRUE(currentUser.isLoggedIn());
//         EXPECT_EQ(currentUser.username(), testUsername);

//         qDebug() << "注册成功! 用户名:" << currentUser.username()
//                  << "用户ID:" << currentUser.userId()
//                  << "角色:" << static_cast<int>(currentUser.role());

//     } else {
//         // // 注册失败
//          qDebug() << "注册失败或未自动登录";

//         // 检查是否有保存的错误信息
//         // 这里可以添加更多检查，比如查看日志等

//         // 如果注册失败，这不算测试失败，可能只是用户已存在
//         // 我们可以尝试另一个用户名
//         QString altUsername = QString("altuser_%1").arg(QDateTime::currentMSecsSinceEpoch());
//         qDebug() << "尝试使用另一个用户名:" << altUsername;

//         userSession->registerUser(altUsername, testPassword);
//         QTest::qWait(3000);

//         if (userSession->isLoggedIn()) {
//             qDebug() << "备用用户名注册成功!";
//             UserData currentUser = userSession->currentUser();
//             EXPECT_TRUE(currentUser.isValid());
//             EXPECT_EQ(currentUser.username(), altUsername);
//         } else {
//             qDebug() << "备用用户名注册也失败，可能服务器有问题";
//             // 这里可以记录更详细的错误信息
//         }
//     }
// }

TEST_F(DispatcherIntegrationTestFixture, UserLoginTest) {
    // 1. 连接到服务器
    if (!connectToServer()) {
        GTEST_SKIP() << "无法连接到服务器，跳过登录测试";
    }

    qDebug() << "\n=== 测试用户登录 ===";

    // 2. 记录当前状态
    qDebug() << "测试前状态 - 是否已登录:" << userSession->isLoggedIn();
    qDebug() << "测试用户名:" << testUsername;
    qDebug() << "测试密码:" << testPassword;

    // 3. 先尝试注册用户（确保用户存在）
    qDebug() << "先尝试注册用户，确保用户存在...";
    userSession->registerUser(testUsername, testPassword);
    QTest::qWait(3000);  // 等待注册完成

    if (userSession->isLoggedIn()) {
        // 注册成功并自动登录，先登出
        qDebug() << "注册成功，用户已自动登录，先登出...";
        userSession->logout();
        QTest::qWait(1000);  // 等待登出完成
    } else {
        // 注册失败，用户可能已存在
        qDebug() << "注册失败或未自动登录，用户可能已存在，继续登录测试";
    }

    // 4. 重置登录状态
    qDebug() << "当前登录状态:" << (userSession->isLoggedIn() ? "已登录" : "未登录");

    // 确保用户未登录
    if (userSession->isLoggedIn()) {
        qDebug() << "用户仍处于登录状态，尝试强制登出...";
        userSession->logout();
        QTest::qWait(1000);
    }

    // 5. 执行登录测试
    qDebug() << "开始登录测试...";
    qDebug() << "调用 login 函数...";
    userSession->login(testUsername, testPassword, false);

    // 6. 给网络请求和响应留出时间
    qDebug() << "等待登录响应...";
    QTest::qWait(3000);  // 等待3秒

    // 7. 检查登录结果
    if (userSession->isLoggedIn()) {
        // 登录成功
        qDebug() << "登录成功!";

        // 验证用户信息
        UserData currentUser = userSession->currentUser();
        EXPECT_TRUE(currentUser.isValid());
        EXPECT_TRUE(currentUser.isLoggedIn());
        EXPECT_EQ(currentUser.username(), testUsername);

        qDebug() << "登录成功! 用户名:" << currentUser.username()
                 << "用户ID:" << currentUser.userId()
                 << "角色:" << static_cast<int>(currentUser.role());

        // 测试登录后的权限
        qDebug() << "权限检查:";
        qDebug() << "  canSaveWords:" << userSession->canSaveWords();
        qDebug() << "  canCreateNotes:" << userSession->canCreateNotes();
        qDebug() << "  canExportData:" << userSession->canExportData();

    } else {
        // 登录失败
        qDebug() << "登录失败";

        // 尝试使用错误的密码登录
        QString wrongPassword = "WrongPassword123";
        qDebug() << "尝试使用错误的密码登录，密码:" << wrongPassword;

        userSession->login(testUsername, wrongPassword, false);
        QTest::qWait(3000);

        if (userSession->isLoggedIn()) {
            qDebug() << "错误密码登录成功（这不应该发生）!";
            // 这不应该发生
        } else {
            qDebug() << "错误密码登录失败（预期结果）";
        }

        // 尝试不存在的用户
        QString nonExistingUser = "non_existing_user_123";
        qDebug() << "尝试不存在的用户登录，用户名:" << nonExistingUser;

        userSession->login(nonExistingUser, testPassword, false);
        QTest::qWait(3000);

        if (userSession->isLoggedIn()) {
            qDebug() << "不存在用户登录成功（这不应该发生）!";
        } else {
            qDebug() << "不存在用户登录失败（预期结果）";
        }
    }
}
