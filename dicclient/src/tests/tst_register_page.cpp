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
#include "view/register_page.h"
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
class RegisterPageIntegrationTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        qDebug() << "\n=== 设置 RegisterPage 网络集成测试环境 ===";

        // 创建主窗口
        window = new BaseWidget();
        window->resize(1000, 700);
        window->setWindowTitle("RegisterPage 网络集成测试");


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

        // 创建 RegisterPage
        //loginPage = new LoginPage();
        registerPage = new RegisterPage();

        // 添加登录页面到主窗口
        QVBoxLayout* window_mainLayout = window->mainLayout();  // 使用指针
        if (window_mainLayout) {
            window_mainLayout->addWidget(registerPage);
        } else {
            qCritical() << "window 的主布局为空！";
        }


        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        timestamp/=10000;
        testUsername = QString("test_%1").arg(timestamp);
        testPassword = "TestPassword123";
        testEmail = QString("test%1@example.com").arg(timestamp);

        qDebug() << "测试用户名:" << testUsername;
        qDebug() << "测试邮箱:" << testEmail;
        qDebug() << "服务器地址: 192.168.23.132:8080";
        qDebug() << "模块初始化完成";

    }

    void TearDown() override {
        qDebug() << "\n=== 清理 RegisterPage 网络集成测试环境 ===";

        // 【关键修复】先断开信号连接，再删除对象
        if (registerPage) {
            registerPage->hide();
            // 断开 UserSession 连接
            userSession->disconnect(registerPage);
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
    RegisterPage* registerPage = nullptr;
    NetworkManager* networkManager = nullptr;
    MessageDispatcher* messageDispatcher = nullptr;
    UserSession* userSession = nullptr;
    QString testUsername;
    QString testPassword;
    QString testEmail;

};

/**
 * @brief 测试正常注册功能
 */
TEST_F(RegisterPageIntegrationTestFixture, NormalRegisterTest) {
    // 连接服务器
    bool connected = connectToServer();
    if (!connected) {
        GTEST_SKIP() << "无法连接到服务器，跳过正常注册测试";
    }

    // 显示注册页面
    window->show();
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "\n=== 测试正常注册功能 ===";

    // 【关键修复】在点击注册按钮之前连接 QSignalSpy
    // 使用构造函数直接连接，确保不会错过信号
    QSignalSpy userSessionRegisterSpy(userSession, &UserSession::registerSuccess);
    QSignalSpy userSessionRegisterFailedSpy(userSession, &UserSession::registerFailed);
    //QSignalSpy registerPageRegisterSpy(registerPage, &RegisterPage::registerSuccess);

    // 查找控件
    QLineEdit* usernameEdit = registerPage->findChild<QLineEdit*>("usernameEdit");
    QLineEdit* passwordEdit = registerPage->findChild<QLineEdit*>("passwordEdit");
    QLineEdit* confirmPasswordEdit = registerPage->findChild<QLineEdit*>("confirmPasswordEdit");
    QLineEdit* emailEdit = registerPage->findChild<QLineEdit*>("emailEdit");
    QCheckBox* termsCheckBox = registerPage->findChild<QCheckBox*>("termsAgreementCheckBox");
    QPushButton* registerButton = registerPage->findChild<QPushButton*>("registerButton");

    if (!usernameEdit || !passwordEdit || !confirmPasswordEdit ||
        !emailEdit || !termsCheckBox || !registerButton) {
        ADD_FAILURE() << "UI 控件未正确初始化";
        return;
    }

    // 设置测试数据
    usernameEdit->setText(testUsername);
    passwordEdit->setText(testPassword);
    confirmPasswordEdit->setText(testPassword);
    emailEdit->setText(testEmail);
    termsCheckBox->setChecked(true);
    QApplication::processEvents();
    QTest::qWait(200);

    // 验证注册按钮状态
    EXPECT_TRUE(registerButton->isEnabled()) << "注册按钮应该被启用";

    // 【关键修复】点击前记录当前信号数量
    int beforeCount = userSessionRegisterSpy.count();
    qDebug() << "点击前信号数量:" << beforeCount;

    // 点击注册按钮
    QTest::mouseClick(registerButton, Qt::LeftButton);

    // 【关键修复】等待信号（使用 wait() 阻塞等待）
    // 注意：wait() 会进入事件循环，等待新信号
    bool success = userSessionRegisterSpy.wait(5000);

    // 【关键修复】检查信号数量变化
    int afterCount = userSessionRegisterSpy.count();
    qDebug() << "点击后信号数量:" << afterCount;
    qDebug() << "wait() 返回值:" << success;
    QTest::qWait(2000);

    if (success || afterCount > beforeCount) {
        qDebug() << "注册成功";
    } else {
        qDebug() << "注册超时或失败";
        qDebug() << "userSessionRegisterSpy 数量:" << userSessionRegisterSpy.count();
        //qDebug() << "registerPageRegisterSpy 数量:" << registerPageRegisterSpy.count();

        if (userSessionRegisterFailedSpy.count() > 0) {
            QList<QVariant> args = userSessionRegisterFailedSpy.takeFirst();
            qDebug() << "注册失败原因:" << args[0].toString();
        }
        ADD_FAILURE() << "注册操作未完成";
    }

    QTest::qWait(2000);
    qDebug() << "正常注册测试完成";
}
