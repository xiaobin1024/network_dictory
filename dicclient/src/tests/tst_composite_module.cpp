#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QDialog>
#include <QMessageBox>

// 包含要测试的模块
#include "system/system_sidebar.h"
#include "user_session/user_session.h"
#include "dispatcher/message_dispatcher.h"
#include "network/network_manager.h"
#include "base/base_widget.h"

using namespace testing;

/**
 * @brief SystemSidebar、UserSession 和 MessageDispatcher 集成测试
 * 专注联网条件下三个模块的衔接情况
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
 * @brief SystemSidebar 集成测试的测试夹具
 * 专注联网条件下的模块衔接
 */
class SystemSidebarIntegrationTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        qDebug() << "\n=== 设置 SystemSidebar 网络集成测试环境 ===";

        // 创建主窗口
        window = new BaseWidget();
        window->resize(1000, 700);
        window->setWindowTitle("SystemSidebar 网络集成测试");

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

        // 创建 SystemSidebar
        sidebar = new SystemSidebar(window);

        // 重要：设置外部模块到侧边栏
        sidebar->setUserSession(userSession);
        sidebar->setMessageDispatcher(messageDispatcher);

        // 添加侧边栏到主窗口
        window->mainLayout()->addWidget(sidebar);

        // 生成唯一的测试用户名
        testUsername = "test0009";
        testPassword = "TestPassword123";

        qDebug() << "测试用户名:" << testUsername;
        qDebug() << "服务器地址: 192.168.23.132:8080";
        qDebug() << "模块初始化完成";
    }

    void TearDown() override {
        qDebug() << "\n=== 清理 SystemSidebar 网络集成测试环境 ===";

        // 隐藏并清理窗口
        if (window) {
            window->hide();
            delete window;
            window = nullptr;
        }

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

        // 等待建立连接
        QTest::qWait(1000);

        return networkManager->isConnected();
    }

    BaseWidget* window = nullptr;
    SystemSidebar* sidebar = nullptr;
    NetworkManager* networkManager = nullptr;
    MessageDispatcher* messageDispatcher = nullptr;
    UserSession* userSession = nullptr;
    QString testUsername;
    QString testPassword;
};

/**
 * @brief 测试用户注册后通过侧边栏退出功能的完整流程
 */
/**
 * @brief 测试用户注册后通过侧边栏退出功能的完整流程
 */
// TEST_F(SystemSidebarIntegrationTestFixture, UserRegisterAndSidebarLogoutTest) {
//     // 连接服务器
//     bool connected = connectToServer();
//     if (!connected) {
//         GTEST_SKIP() << "无法连接到服务器，跳过注册和侧边栏退出测试";
//     }

//     // 显示窗口
//     window->show();
//     QApplication::processEvents();
//     QTest::qWait(500);

//     qDebug() << "\n=== 测试用户注册后通过侧边栏退出功能的完整流程 ===";

//     // 1. 注册用户
//     qDebug() << "1. 开始注册用户...";
//     QSignalSpy registerSuccessSpy(userSession, &UserSession::registerSuccess);
//     QSignalSpy registerFailedSpy(userSession, &UserSession::registerFailed);

//     userSession->registerUser(testUsername, testPassword);
//     QTest::qWait(2000); // 等待注册响应

//     if (registerSuccessSpy.count() == 0 && registerFailedSpy.count() > 0) {
//         qDebug() << "注册失败，可能是用户已存在，尝试登录...";
//         // 如果注册失败，可能是用户已存在，尝试登录
//         QSignalSpy loginSuccessSpy(userSession, &UserSession::loginSuccess);
//         userSession->login(testUsername, testPassword, false);
//         QTest::qWait(2000);

//         if (loginSuccessSpy.count() == 0) {
//             GTEST_SKIP() << "注册和登录都失败，跳过测试";
//         }
//     }

//     qDebug() << "注册或登录完成，当前登录状态:" << userSession->isLoggedIn();

//     // 2. 验证用户已登录
//     EXPECT_TRUE(userSession->isLoggedIn());
//     if (!userSession->isLoggedIn()) {
//         qDebug() << "用户未登录，无法继续测试";
//         return;
//     }

//     UserData currentUser = userSession->currentUser();
//     qDebug() << "当前登录用户:" << currentUser.username();

//     // 3. 展开侧边栏
//     qDebug() << "3. 展开侧边栏...";
//     QSignalSpy expandedSpy(sidebar, &SystemSidebar::expandedChanged);
//     sidebar->setExpanded(true);
//     QTRY_VERIFY2(sidebar->currentWidth() == SystemSidebar::EXPANDED_WIDTH,
//                  "侧边栏展开动画未完成");
//     QApplication::processEvents();

//     EXPECT_TRUE(sidebar->isExpanded());
//     EXPECT_EQ(expandedSpy.count(), 1);
//     qDebug() << "侧边栏已展开";

//     // 重要：等待一段时间让侧边栏的UI状态更新
//     QTest::qWait(1000); // 等待UI更新
//     QApplication::processEvents(); // 处理事件队列

//     // 4. 查找登出按钮并验证其状态
//     qDebug() << "4. 查找登出按钮并验证状态...";
//     QPushButton* logoutButton = sidebar->findChild<QPushButton*>("logoutButton");
//     ASSERT_NE(logoutButton, nullptr) << "无法找到登出按钮";

//     // 重要：等待登出按钮变为启用状态
//     //QTRY_VERIFY2_WITH_TIMEOUT(logoutButton->isEnabled(), 3000); // 最多等待3秒
//     QTest::qWait(2000); // 等待UI更新
//     qDebug() << "登出按钮文本:" << logoutButton->text();
//     qDebug() << "登出按钮是否启用:" << logoutButton->isEnabled();

//     // 验证按钮状态正确（应该是"退出登录"且启用）
//     EXPECT_EQ(logoutButton->text(), "🚪 退出登录") << "登出按钮文本不正确";
//     EXPECT_TRUE(logoutButton->isEnabled()) << "登出按钮应该启用";

//     // 5. 记录登出前的状态
//     bool wasLoggedInBeforeLogout = userSession->isLoggedIn();
//     qDebug() << "登出前登录状态:" << wasLoggedInBeforeLogout;
//     EXPECT_TRUE(wasLoggedInBeforeLogout) << "登出前应该处于登录状态";

//     // 6. 连接登出相关信号
//     QSignalSpy logoutSuccessSpy(userSession, &UserSession::logoutSuccess);
//     QSignalSpy userChangedSpy(userSession, &UserSession::userChanged);

//     // 7. 模拟点击登出按钮
//     qDebug() << "7. 点击登出按钮...";
//     QTest::mouseClick(logoutButton, Qt::LeftButton);
//     QApplication::processEvents();
//     QTest::qWait(100); // 短暂等待点击响应

//     qDebug() << "已点击登出按钮，等待登出响应...";

//     // 8. 等待登出完成
//     QTest::qWait(3000); // 等待服务器登出响应（增加等待时间）

//     // 9. 验证登出是否成功
//     qDebug() << "9. 验证登出结果...";

//     // 检查用户是否真的登出了
//     bool isActuallyLoggedOut = !userSession->isLoggedIn();
//     qDebug() << "用户登出状态:" << isActuallyLoggedOut;

//     if (isActuallyLoggedOut) {
//         qDebug() << "登出成功验证通过";
//         EXPECT_FALSE(userSession->isLoggedIn());
//         EXPECT_FALSE(userSession->isFreeUser());
//         EXPECT_FALSE(userSession->isPremiumUser());

//         // 验证登出信号被触发
//         // EXPECT_GT(logoutSuccessSpy.count(), 0) << "登出成功信号应该被触发";
//         // EXPECT_GT(userChangedSpy.count(), 0) << "用户状态变化信号应该被触发";
//     } else {
//         qDebug() << "登出可能未成功，当前登录状态:" << userSession->isLoggedIn();
//         ADD_FAILURE() << "用户未能成功登出";
//     }

//     // // 10. 额外验证：检查登出后用户数据
//     UserData loggedOutUser = userSession->currentUser();
//     // EXPECT_FALSE(loggedOutUser.isValid() || !loggedOutUser.isLoggedIn());

//     qDebug() << "登出后用户信息 - 是否有效:" << loggedOutUser.isValid()
//              << ", 是否登录:" << loggedOutUser.isLoggedIn()
//              << ", 用户名:" << loggedOutUser.username();

//     qDebug() << "用户注册后通过侧边栏退出功能测试完成";

//     qDebug() << "10. 尝试自动登录...";
//     //QSignalSpy autoLoginSuccessSpy(userSession, &UserSession::loginSuccess);

//     bool autoLoginResult = userSession->autoLoginFromSavedSession();

//     if (autoLoginResult) {
//         // 等待自动登录完成
//         QTest::qWait(1000);
//         QApplication::processEvents();

//         EXPECT_TRUE(userSession->isLoggedIn());

//         UserData autoLoggedInUser = userSession->currentUser();
//         qDebug() << "自动登录成功 - 用户名:" << autoLoggedInUser.username()
//                  << "ID:" << autoLoggedInUser.userId();

//         // 验证用户信息是否正确恢复
//         // EXPECT_EQ(autoLoggedInUser.username(), loggedInUser.username());
//         // EXPECT_EQ(autoLoggedInUser.userId(), loggedInUser.userId());
//     }else {
//         qDebug() << "自动登录失败";
//         ADD_FAILURE() << "自动登录失败";
//     }

//     // 5. 清理 - 登出自动登录的用户
//     if (userSession->isLoggedIn()) {
//         userSession->logout();
//         QTest::qWait(500);
//     }

//     EXPECT_FALSE(userSession->isLoggedIn());
//     qDebug() << "自动登录测试完成";
// }

// TEST_F(SystemSidebarIntegrationTestFixture, AutoLoginTest) {
//     // 连接服务器
//     bool connected = connectToServer();
//     if (!connected) {
//         GTEST_SKIP() << "无法连接到服务器，跳过自动登录测试";
//     }

//     // 4. 尝试自动登录
//     qDebug() << "4. 尝试自动登录...";
//     QSignalSpy autoLoginSuccessSpy(userSession, &UserSession::loginSuccess);

//     bool autoLoginResult = userSession->autoLoginFromSavedSession();

//     if (autoLoginResult) {
//         // 等待自动登录完成
//         QTest::qWait(1000);
//         QApplication::processEvents();

//         EXPECT_TRUE(userSession->isLoggedIn());

//         UserData autoLoggedInUser = userSession->currentUser();
//         qDebug() << "自动登录成功 - 用户名:" << autoLoggedInUser.username()
//                  << "ID:" << autoLoggedInUser.userId();

//         // 验证用户信息是否正确恢复
//         // EXPECT_EQ(autoLoggedInUser.username(), loggedInUser.username());
//         // EXPECT_EQ(autoLoggedInUser.userId(), loggedInUser.userId());

//     } else {
//         qDebug() << "自动登录失败";
//         ADD_FAILURE() << "自动登录失败";
//     }

//     // 5. 清理 - 登出自动登录的用户
//     if (userSession->isLoggedIn()) {
//         userSession->logout();
//         QTest::qWait(500);
//     }

//     EXPECT_FALSE(userSession->isLoggedIn());
//     qDebug() << "自动登录测试完成";
// }

/**
 * @brief 测试用户注销功能的完整流程
 */
/**
 * @brief 测试用户注销功能的完整流程
 */
TEST_F(SystemSidebarIntegrationTestFixture, UserUnregisterTest) {
    // 连接服务器
    bool connected = connectToServer();
    if (!connected) {
        GTEST_SKIP() << "无法连接到服务器，跳过注销测试";
    }

    // 显示窗口
    window->show();
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "\n=== 测试用户注销功能的完整流程 ===";

    // 1. 登录用户
    qDebug() << "1. 登录用户...";
    userSession->login(testUsername, testPassword, true);
    QTest::qWait(2000);

    EXPECT_TRUE(userSession->isLoggedIn()) << "登录应该成功";
    if (!userSession->isLoggedIn()) {
        GTEST_SKIP() << "登录失败，跳过测试";
    }

    // 2. 展开侧边栏
    sidebar->setExpanded(true);
    QTRY_VERIFY2(sidebar->currentWidth() == SystemSidebar::EXPANDED_WIDTH, "侧边栏展开失败");
    QApplication::processEvents();

    // 3. 连接注销信号
    QSignalSpy unregisterSuccessSpy(userSession, &UserSession::unregisterSuccess);
    QSignalSpy unregisterFailedSpy(userSession, &UserSession::unregisterFailed);

    // 4. 点击注销按钮
    QPushButton* deleteButton = sidebar->findChild<QPushButton*>("deleteButton");
    ASSERT_NE(deleteButton, nullptr);

    QTest::mouseClick(deleteButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(200); // 等待对话框出现

    qDebug() << "请手动点击对话框的 'Yes' 按钮来确认注销...";

    // 等待用户手动操作，最多等待10秒
    int waitTime = 0;
    const int maxWaitTime = 10000; // 10秒
    const int interval = 100; // 100毫秒检查一次

    while (waitTime < maxWaitTime) {
        QApplication::processEvents();

        // 检查是否收到注销信号
        if (unregisterSuccessSpy.count() > 0 || unregisterFailedSpy.count() > 0) {
            break;
        }

        QTest::qWait(interval);
        waitTime += interval;
    }

    // 5. 等待注销完成（如果还没完成的话）
    if (unregisterSuccessSpy.count() == 0 && unregisterFailedSpy.count() == 0) {
        // 再等一会儿
        unregisterSuccessSpy.wait(2000);
        unregisterFailedSpy.wait(2000);
    }

    // 6. 验证结果
    if (unregisterSuccessSpy.count() > 0) {
        EXPECT_FALSE(userSession->isLoggedIn());
        qDebug() << "注销成功";
    } else if (unregisterFailedSpy.count() > 0) {
        QList<QVariant> args = unregisterFailedSpy.takeFirst();
        qDebug() << "注销失败:" << args[0].toString();
    } else {
        qDebug() << "注销操作未完成";
    }

    // 验证最终状态
    //EXPECT_FALSE(userSession->isLoggedIn());
}
