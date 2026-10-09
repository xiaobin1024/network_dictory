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
#include "view/login_page.h"
#include "user_session/user_session.h"
#include "dispatcher/message_dispatcher.h"
#include "network/network_manager.h"
#include "base/base_widget.h"

using namespace testing;

/**
 * @brief LoginPage 集成测试
 * 专注联网条件下登录页面与用户会话的衔接情况
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
 * @brief LoginPage 集成测试的测试夹具
 * 专注联网条件下的模块衔接
 */
class LoginPageIntegrationTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        qDebug() << "\n=== 设置 LoginPage 网络集成测试环境 ===";

        // 创建主窗口
        window = new BaseWidget();
        window->resize(1000, 700);
        window->setWindowTitle("LoginPage 网络集成测试");

        window->initialize();  // 手动调用初始化
        QApplication::processEvents();

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

        // 创建 LoginPage
        loginPage = new LoginPage();

        // 添加登录页面到主窗口
        QVBoxLayout* window_mainLayout = window->mainLayout();  // 使用指针
        if (window_mainLayout) {
            window_mainLayout->addWidget(loginPage);
        } else {
            qCritical() << "window 的主布局为空！";
        }


        // 生成测试用户名
        testUsername = "test00010";
        testPassword = "TestPassword123";

        qDebug() << "测试用户名:" << testUsername;
        qDebug() << "服务器地址: 192.168.23.132:8080";
        qDebug() << "模块初始化完成";
    }

    void TearDown() override {
        qDebug() << "\n=== 清理 LoginPage 网络集成测试环境 ===";

        // 【关键修复】先断开信号连接，再删除对象
        if (loginPage) {
            loginPage->hide();
            // 断开 UserSession 连接
            userSession->disconnect(loginPage);
        }

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
    LoginPage* loginPage = nullptr;
    NetworkManager* networkManager = nullptr;
    MessageDispatcher* messageDispatcher = nullptr;
    UserSession* userSession = nullptr;
    QString testUsername;
    QString testPassword;
};

/**
 * @brief 测试正常登录功能
 */
TEST_F(LoginPageIntegrationTestFixture, NormalLoginTest) {
    // 连接服务器
    bool connected = connectToServer();
    if (!connected) {
        GTEST_SKIP() << "无法连接到服务器，跳过正常登录测试";
    }

    // 显示登录页面
    window->show();
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "\n=== 测试正常登录功能 ===";

    // 【关键修复 1】在点击登录按钮之前连接 QSignalSpy
    // 使用构造函数直接连接，确保不会错过信号
    QSignalSpy userSessionLoginSpy(userSession, &UserSession::loginSuccess);
    QSignalSpy userSessionLogoutSpy(userSession, &UserSession::loginFailed);
    QSignalSpy loginPageLoginSpy(loginPage, &LoginPage::loginSuccess);

    // 查找控件
    QLineEdit* usernameEdit = loginPage->findChild<QLineEdit*>("usernameEdit");
    QLineEdit* passwordEdit = loginPage->findChild<QLineEdit*>("passwordEdit");
    QPushButton* loginButton = loginPage->findChild<QPushButton*>("loginButton");

    if (!usernameEdit || !passwordEdit || !loginButton) {
        ADD_FAILURE() << "UI 控件未正确初始化";
        return;
    }

    // 设置测试数据
    usernameEdit->setText(testUsername);
    passwordEdit->setText(testPassword);
    QApplication::processEvents();
    QTest::qWait(200);

    // 验证登录按钮状态
    EXPECT_TRUE(loginButton->isEnabled()) << "登录按钮应该被启用";

    // 【关键修复 2】点击前记录当前信号数量
    int beforeCount = userSessionLoginSpy.count();
    qDebug() << "点击前信号数量:" << beforeCount;

    // 点击登录按钮
    QTest::mouseClick(loginButton, Qt::LeftButton);

    // 【关键修复 3】等待信号（使用 wait() 阻塞等待）
    // 注意：wait() 会进入事件循环，等待新信号
    bool success = userSessionLoginSpy.wait(5000);

    // 【关键修复 4】检查信号数量变化
    int afterCount = userSessionLoginSpy.count();
    qDebug() << "点击后信号数量:" << afterCount;
    qDebug() << "wait() 返回值:" << success;

    if (success || afterCount > beforeCount) {
        qDebug() << "登录成功";
        EXPECT_TRUE(userSession->isLoggedIn());

        // 获取信号参数
        if (afterCount > 0) {
            QList<QVariant> arguments = userSessionLoginSpy.takeFirst();
            if (arguments.size() > 0) {
                UserData user = arguments[0].value<UserData>();
                EXPECT_EQ(user.username(), testUsername);
                qDebug() << "登录成功的用户名:" << user.username();
                qDebug() << "用户 ID:" << user.userId();
                qDebug() << "用户角色:" << static_cast<int>(user.role());
            }
        }
    } else {
        qDebug() << "登录超时或失败";
        qDebug() << "userSessionLoginSpy 数量:" << userSessionLoginSpy.count();
        qDebug() << "loginPageLoginSpy 数量:" << loginPageLoginSpy.count();

        if (userSessionLogoutSpy.count() > 0) {
            QList<QVariant> args = userSessionLogoutSpy.takeFirst();
            qDebug() << "登录失败原因:" << args[0].toString();
        }
        ADD_FAILURE() << "登录操作未完成";
    }

    QTest::qWait(2000);

    // 清理 - 登出用户
    if (userSession->isLoggedIn()) {
        userSession->logout();
        QTest::qWait(1000);
    }

    EXPECT_FALSE(userSession->isLoggedIn());
    qDebug() << "正常登录测试完成";
}


/**
 * @brief 测试自动登录功能
 */
// TEST_F(LoginPageIntegrationTestFixture, AutoLoginTest) {
//     // 连接服务器
//     bool connected = connectToServer();
//     if (!connected) {
//         GTEST_SKIP() << "无法连接到服务器，跳过自动登录测试";
//     }

//     qDebug() << "\n=== 测试自动登录功能 ===";

//     // ===== 第一阶段：正常登录并保存会话 =====
//     qDebug() << "1. 首次登录以创建会话...";

//     // 设置登录数据（勾选记住我）
//     QLineEdit* usernameEdit = loginPage->findChild<QLineEdit*>("usernameEdit");
//     QLineEdit* passwordEdit = loginPage->findChild<QLineEdit*>("passwordEdit");
//     QCheckBox* rememberMeBox = loginPage->findChild<QCheckBox*>("rememberMeCheckBox");
//     QPushButton* loginButton = loginPage->findChild<QPushButton*>("loginButton");

//     usernameEdit->setText(testUsername);
//     passwordEdit->setText(testPassword);
//     rememberMeBox->setChecked(true);  // ← 关键：勾选记住我
//     QApplication::processEvents();

//     // 连接信号
//     QSignalSpy loginSuccessSpy(userSession, &UserSession::loginSuccess);

//     // 点击登录
//     QTest::mouseClick(loginButton, Qt::LeftButton);
//     loginSuccessSpy.wait(5000);

//     EXPECT_TRUE(userSession->isLoggedIn()) << "首次登录应该成功";
//     if (!userSession->isLoggedIn()) {
//         GTEST_SKIP() << "首次登录失败，跳过自动登录测试";
//     }

//     UserData originalUser = userSession->currentUser();
//     qDebug() << "原始登录用户:" << originalUser.username();

//     // ===== 第二阶段：模拟程序重启（不登出！）=====
//     qDebug() << "2. 模拟程序重启...";

//     // 不要调用 logout()！直接重置 UserSession 模拟程序重启
//     //传递 false 保留会话文件
//     userSession->reset(false);  // ← 不清除会话文件
//     QTest::qWait(100);

//     // 验证会话文件仍然存在
//     //qDebug() << "重置后会话文件是否存在:" << QFile::exists(sessionPath);

//     // 重新初始化
//     userSession->initialize();
//     userSession->setMessageDispatcher(messageDispatcher);
//     QTest::qWait(100);

//     // ===== 第三阶段：触发自动登录 =====
//     qDebug() << "3. 触发自动登录...";

//     // 连接自动登录信号
//     QSignalSpy autoLoginSpy(userSession, &UserSession::loginSuccess);

//     // 显示登录页面（会触发 onPageShow → autoLoginFromSavedSession）
//      loginPage->showPage();  // ← 使用 showPage() 而不是 show()
//     QApplication::processEvents();
//     QTest::qWait(1000);

//     // ===== 第四阶段：验证自动登录 =====
//     if (userSession->isLoggedIn()) {
//         UserData autoLoggedInUser = userSession->currentUser();
//         qDebug() << "自动登录成功 - 用户名:" << autoLoggedInUser.username();

//         EXPECT_EQ(autoLoggedInUser.username(), originalUser.username());
//         EXPECT_EQ(autoLoggedInUser.userId(), originalUser.userId());
//         EXPECT_TRUE(autoLoggedInUser.isLoggedIn());
//     } else if (autoLoginSpy.count() > 0) {
//         // 通过信号验证
//         QList<QVariant> arguments = autoLoginSpy.takeFirst();
//         if (arguments.size() > 0) {
//             UserData user = arguments[0].value<UserData>();
//             qDebug() << "通过信号接收到自动登录 - 用户名:" << user.username();
//             EXPECT_EQ(user.username(), originalUser.username());
//         }
//     } else {
//         ADD_FAILURE() << "自动登录未成功";
//     }

//     // // ===== 第五阶段：清理（最后清除会话文件）=====
//     // userSession->reset(true);  // ← 最后清除会话文件
//     // EXPECT_FALSE(userSession->isLoggedIn());
//     qDebug() << "自动登录测试完成";

// }
