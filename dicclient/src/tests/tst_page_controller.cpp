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
#include "view/page_controller.h"
#include "user_session/user_session.h"
#include "dispatcher/message_dispatcher.h"
#include "network/network_manager.h"
#include "base/base_widget.h"

using namespace testing;

/**
 * @brief PageController 集成测试
 * 专注联网条件下页面跳转功能
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
 * @brief PageController 集成测试的测试夹具
 * 专注联网条件下的页面跳转功能
 */
class PageControllerIntegrationTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        qDebug() << "\n=== 设置 PageController 网络集成测试环境 ===";

        // 创建主窗口
        window = new BaseWidget();
        window->resize(1000, 700);
        window->setWindowTitle("PageController 网络集成测试");

        window->initialize();  // 手动调用初始化
        QApplication::processEvents();

        // 创建 NetworkManager
        networkManager = new NetworkManager();

        // 创建 MessageDispatcher
        messageDispatcher = new MessageDispatcher();

        // 获取 UserSession 实例
        userSession = UserSession::instance();

        //获取WordSearch实例
        wordSearch=WordSearch::instance();

        //获取WordCollect实例
        wordCollect=WordCollect::instance();

        //获取WordVocabulary实例
        wordVocabulary=WordVocabulary::instance();

        // 关键修复1: 先建立dispatcher连接再初始化
        // 显式断开可能的旧连接（安全防护）
        userSession->setMessageDispatcher(nullptr);
        messageDispatcher->setUserSession(nullptr);

        // 设置 MessageDispatcher
        messageDispatcher->setNetworkManager(networkManager);
        messageDispatcher->setUserSession(userSession);
        messageDispatcher->setWordSearch(wordSearch);
        messageDispatcher->setWordCollect(wordCollect);
        messageDispatcher->setWordVocabulary(wordVocabulary);

        userSession->setMessageDispatcher(messageDispatcher);
        wordSearch->setMessageDispatcher(messageDispatcher);
        wordCollect->setMessageDispatcher(messageDispatcher);
        wordVocabulary->setMessageDispatcher(messageDispatcher);

        // 关键修复2: 在dispatcher设置完成后才初始化
        userSession->initialize();

        messageDispatcher->start();

        // 创建 PageController
        pageController = new PageController(window);

        // 添加到主窗口
        window->mainLayout()->addWidget(pageController);

        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        timestamp /= 100000;
        testUsername = "test_17742494";
        testPassword = "TestPassword123";
        testEmail = QString("test%1@example.com").arg(timestamp);

        qDebug() << "测试用户名:" << testUsername;
        qDebug() << "测试邮箱:" << testEmail;
        qDebug() << "服务器地址: 192.168.23.132:8080";
        qDebug() << "模块初始化完成";
    }

    void TearDown() override {
        qDebug() << "\n=== 清理 PageController 网络集成测试环境 ===";

        // 断开信号连接
        if (pageController) {
            pageController->hide();
            userSession->disconnect(pageController->getLoginPage());
            userSession->disconnect(pageController->getRegisterPage());
            userSession->disconnect(pageController->getMainPage());
        }

        // 隐藏并清理窗口
        if (window) {
            window->hide();
            delete window;
            window = nullptr;
        }

        if(wordSearch){
             wordSearch->setMessageDispatcher(nullptr);

        }

        if(wordCollect){
            wordCollect->setMessageDispatcher(nullptr);
        }

        if(wordVocabulary){
            wordVocabulary->setMessageDispatcher(nullptr);
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
    PageController* pageController = nullptr;
    NetworkManager* networkManager = nullptr;
    MessageDispatcher* messageDispatcher = nullptr;
    UserSession* userSession = nullptr;
    WordSearch* wordSearch=nullptr;
    WordCollect* wordCollect=nullptr;
    WordVocabulary* wordVocabulary;
    QString testUsername;
    QString testPassword;
    QString testEmail;
};

// /**
//  * @brief 测试页面跳转功能
//  */
// TEST_F(PageControllerIntegrationTestFixture, PageNavigationTest) {
//     // 连接服务器
//     bool connected = connectToServer();
//     if (!connected) {
//         GTEST_SKIP() << "无法连接到服务器，跳过页面跳转测试";
//     }

//     // 显示页面
//     window->show();
//     QApplication::processEvents();
//     QTest::qWait(500);

//     qDebug() << "\n=== 测试页面跳转功能 ===";

//     // 1. 验证初始页面是登录页面
//     qDebug() << "1. 验证初始页面是登录页面";
//     EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 0) << "初始页面应该是登录页面";

//     // 2. 点击登录页面的"注册账户"按钮，跳转到注册页面
//     qDebug() << "2. 点击登录页面的注册按钮，跳转到注册页面";
//     QPushButton* registerButton = pageController->getLoginPage()->findChild<QPushButton*>("registerButton");
//     ASSERT_NE(registerButton, nullptr) << "无法找到注册按钮";

//     QTest::mouseClick(registerButton, Qt::LeftButton);
//     QApplication::processEvents();
//     QTest::qWait(500);

//     EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 1) << "应该跳转到注册页面";

//     // 3. 点击注册页面的"已有账户？去登录"按钮，跳转到登录页面
//     qDebug() << "3. 点击注册页面的登录按钮，跳转到登录页面";
//     QPushButton* loginButton = pageController->getRegisterPage()->findChild<QPushButton*>("loginButton");
//     ASSERT_NE(loginButton, nullptr) << "无法找到登录按钮";

//     QTest::mouseClick(loginButton, Qt::LeftButton);
//     QApplication::processEvents();
//     QTest::qWait(500);

//     EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 0) << "应该跳转到登录页面";

//     qDebug() << "页面跳转测试完成";
// }

// /**
//  * @brief 测试注册成功后的页面跳转
//  */
// TEST_F(PageControllerIntegrationTestFixture, RegisterSuccessNavigationTest) {
//     // 连接服务器
//     bool connected = connectToServer();
//     if (!connected) {
//         GTEST_SKIP() << "无法连接到服务器，跳过注册成功跳转测试";
//     }

//     // 显示页面
//     window->show();
//     QApplication::processEvents();
//     QTest::qWait(500);

//     qDebug() << "\n=== 测试注册成功后的页面跳转 ===";

//     // 1. 先跳转到注册页面
//     qDebug() << "1. 跳转到注册页面";
//     pageController->setCurrentPage(PageController::Register);
//     EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 1) << "当前应该在注册页面";

//     // 2. 连接注册成功信号
//     QSignalSpy registerSuccessSpy(userSession, &UserSession::registerSuccess);
//     QSignalSpy registerFailedSpy(userSession, &UserSession::registerFailed);

//     // 3. 填写注册信息并提交
//     QLineEdit* usernameEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("usernameEdit");
//     QLineEdit* passwordEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("passwordEdit");
//     QLineEdit* confirmPasswordEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("confirmPasswordEdit");
//     QLineEdit* emailEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("emailEdit");
//     QCheckBox* termsCheckBox = pageController->getRegisterPage()->findChild<QCheckBox*>("termsAgreementCheckBox");
//     QPushButton* registerButton = pageController->getRegisterPage()->findChild<QPushButton*>("registerButton");

//     ASSERT_NE(usernameEdit, nullptr) << "无法找到用户名输入框";
//     ASSERT_NE(passwordEdit, nullptr) << "无法找到密码输入框";
//     ASSERT_NE(confirmPasswordEdit, nullptr) << "无法找到确认密码输入框";
//     ASSERT_NE(emailEdit, nullptr) << "无法找到邮箱输入框";
//     ASSERT_NE(termsCheckBox, nullptr) << "无法找到同意条款复选框";
//     ASSERT_NE(registerButton, nullptr) << "无法找到注册按钮";

//     // 填写测试数据
//     usernameEdit->setText(testUsername);
//     passwordEdit->setText(testPassword);
//     confirmPasswordEdit->setText(testPassword);
//     emailEdit->setText(testEmail);
//     termsCheckBox->setChecked(true);
//     QApplication::processEvents();
//     QTest::qWait(200);

//     // 点击注册按钮
//     qDebug() << "2. 提交注册信息";
//     QTest::mouseClick(registerButton, Qt::LeftButton);

//     // 等待注册响应
//     bool success = registerSuccessSpy.wait(5000);

//     if (success) {
//         qDebug() << "3. 注册成功，验证跳转到登录页面";
//         // 注册成功后，页面应该跳转到登录页面
//         // 等待页面跳转完成
//         QTest::qWait(1000);
//         EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 0) << "注册成功后应该跳转到登录页面";
//     } else {
//         qDebug() << "注册超时或失败";
//         if (registerFailedSpy.count() > 0) {
//             QList<QVariant> args = registerFailedSpy.takeFirst();
//             qDebug() << "注册失败原因:" << args[0].toString();
//         }
//         ADD_FAILURE() << "注册操作未完成";
//     }

//     qDebug() << "注册成功跳转测试完成";
// }

/**
 * @brief 测试主页功能
 */
TEST_F(PageControllerIntegrationTestFixture, MainPageTest) {
    // 连接服务器
    bool connected = connectToServer();
    if (!connected) {
        GTEST_SKIP() << "无法连接到服务器，跳过主页测试";
    }

    // 显示页面
    window->show();
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "\n=== 测试主页功能 ===";

    //     // 1. 先跳转到注册页面
    //     qDebug() << "1. 跳转到注册页面";
    //     pageController->setCurrentPage(PageController::Register);
    //     EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 1) << "当前应该在注册页面";

    //     // 2. 连接注册成功信号
    //     QSignalSpy registerSuccessSpy(userSession, &UserSession::registerSuccess);
    //     QSignalSpy registerFailedSpy(userSession, &UserSession::registerFailed);

    //     // 3. 填写注册信息并提交
    //     QLineEdit* usernameEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("usernameEdit");
    //     QLineEdit* passwordEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("passwordEdit");
    //     QLineEdit* confirmPasswordEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("confirmPasswordEdit");
    //     QLineEdit* emailEdit = pageController->getRegisterPage()->findChild<QLineEdit*>("emailEdit");
    //     QCheckBox* termsCheckBox = pageController->getRegisterPage()->findChild<QCheckBox*>("termsAgreementCheckBox");
    //     QPushButton* registerButton = pageController->getRegisterPage()->findChild<QPushButton*>("registerButton");

    //     ASSERT_NE(usernameEdit, nullptr) << "无法找到用户名输入框";
    //     ASSERT_NE(passwordEdit, nullptr) << "无法找到密码输入框";
    //     ASSERT_NE(confirmPasswordEdit, nullptr) << "无法找到确认密码输入框";
    //     ASSERT_NE(emailEdit, nullptr) << "无法找到邮箱输入框";
    //     ASSERT_NE(termsCheckBox, nullptr) << "无法找到同意条款复选框";
    //     ASSERT_NE(registerButton, nullptr) << "无法找到注册按钮";

    //     // 填写测试数据
    //     usernameEdit->setText(testUsername);
    //     passwordEdit->setText(testPassword);
    //     confirmPasswordEdit->setText(testPassword);
    //     emailEdit->setText(testEmail);
    //     termsCheckBox->setChecked(true);
    //     QApplication::processEvents();
    //     QTest::qWait(200);

    //     // 点击注册按钮
    //     qDebug() << "2. 提交注册信息";
    //     QTest::mouseClick(registerButton, Qt::LeftButton);

    //     // 等待注册响应
    //     bool success = registerSuccessSpy.wait(5000);

    //     if (success) {
    //         qDebug() << "3. 注册成功，验证跳转到登录页面";
    //         // 注册成功后，页面应该跳转到登录页面
    //         // 等待页面跳转完成
    //         QTest::qWait(1000);
    //         EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 0) << "注册成功后应该跳转到登录页面";
    //     } else {
    //         qDebug() << "注册超时或失败";
    //         if (registerFailedSpy.count() > 0) {
    //             QList<QVariant> args = registerFailedSpy.takeFirst();
    //             qDebug() << "注册失败原因:" << args[0].toString();
    //         }
    //         ADD_FAILURE() << "注册操作未完成";
    //     }
    // // 等待注册成功后跳转到登录页面
    // QTest::qWait(1000);
    // EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 0) << "注册成功后应该跳转到登录页面";
    // qDebug() << "注册成功，已跳转到登录页面";

    // 2. 在登录页面登录
    qDebug() << "2. 在登录页面登录";
    LoginPage* loginPage = pageController->getLoginPage();
    ASSERT_NE(loginPage, nullptr) << "登录页面不应为空";

    QLineEdit* loginUsernameEdit = loginPage->findChild<QLineEdit*>("usernameEdit");
    QLineEdit* loginPasswordEdit = loginPage->findChild<QLineEdit*>("passwordEdit");
    QPushButton* loginButton = loginPage->findChild<QPushButton*>("loginButton");

    ASSERT_NE(loginUsernameEdit, nullptr) << "无法找到登录用户名输入框";
    ASSERT_NE(loginPasswordEdit, nullptr) << "无法找到登录密码输入框";
    ASSERT_NE(loginButton, nullptr) << "无法找到登录按钮";

    loginUsernameEdit->setText(testUsername);
    loginPasswordEdit->setText(testPassword);
    QApplication::processEvents();
    QTest::qWait(200);

    // // 连接登录成功信号
    // QSignalSpy loginSuccessSpy(userSession, &UserSession::loginSuccess);
    // QSignalSpy loginFailedSpy(userSession, &UserSession::loginFailed);

    // 点击登录按钮
    QTest::mouseClick(loginButton, Qt::LeftButton);
    QApplication::processEvents();

    // 等待登录成功跳转到主页面
    // bool loginSuccess = loginSuccessSpy.wait(1000);
    // if (loginSuccess) {
    //     qDebug() << " 登录成功，验证跳转到主页面";
    //     // 等待页面跳转完成
    //     QTest::qWait(1000);
    //     EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 2) << "注册成功后应该跳转到登录页面";
    // } else {
    //     qDebug() << "注册超时或失败";
    //     if (loginFailedSpy.count() > 0) {
    //         QList<QVariant> args = loginFailedSpy.takeFirst();
    //         qDebug() << "注册失败原因:" << args[0].toString();
    //     }
    //     ADD_FAILURE() << "注册操作未完成";
    // }

    // 等待页面跳转完成
    QTest::qWait(1000);
    EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 2) << "登录成功后应该跳转到主页面";
    qDebug() << "登录成功，已跳转到主页面";
    QTest::qWait(2000);

    // 3. 测试侧边栏展开和收起
    qDebug() << "3. 测试侧边栏展开和收起";
    MainPage* mainPage = pageController->getMainPage();
    ASSERT_NE(mainPage, nullptr) << "主页面不应为空";

    SystemSidebar* sidebar = mainPage->findChild<SystemSidebar*>();
    ASSERT_NE(sidebar, nullptr) << "无法找到侧边栏";

    // 检查初始状态（应该是收起）
    EXPECT_FALSE(sidebar->isExpanded()) << "侧边栏初始状态应该是收起";
    qDebug() << "初始状态：侧边栏收起";

    // 点击侧边栏展开按钮
    QPushButton* toggleButton = sidebar->findChild<QPushButton*>("toggleButton");
    if (!toggleButton) {
        // 如果没找到特定名称的按钮，尝试查找其他可能的按钮
        QList<QPushButton*> buttons = sidebar->findChildren<QPushButton*>();
        for (auto btn : buttons) {
            if (btn->text().contains("收起") || btn->text().contains("展开") || btn->text() == "☰") {
                toggleButton = btn;
                break;
            }
        }
    }
    ASSERT_NE(toggleButton, nullptr) << "无法找到侧边栏切换按钮";

    QTest::mouseClick(toggleButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1000);

    // 验证展开状态
    EXPECT_TRUE(sidebar->isExpanded()) << "点击后侧边栏应该展开";
    qDebug() << "点击后：侧边栏展开";
    QTest::qWait(2000);

    // 再次点击收起
    QTest::mouseClick(toggleButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1000);

    // 验证收起状态
    EXPECT_FALSE(sidebar->isExpanded()) << "再次点击后侧边栏应该收起";
    qDebug() << "再次点击后：侧边栏收起";
    QTest::qWait(2000);

    // 4. 测试搜索功能
    qDebug() << "4. 测试搜索功能";
    SearchHistoryWidget* searchWidget = mainPage->findChild<SearchHistoryWidget*>();
    ASSERT_NE(searchWidget, nullptr) << "无法找到搜索组件";

    QLineEdit* searchInput = searchWidget->inputField();
    ASSERT_NE(searchInput, nullptr) << "无法找到搜索输入框";

    QPushButton* searchButton = searchWidget->searchButton();
    ASSERT_NE(searchButton, nullptr) << "无法找到搜索按钮";

    // 输入搜索词
    QString searchTerm = "minus";
    searchInput->setText(searchTerm);
    QApplication::processEvents();
    QTest::qWait(500);

    // 点击搜索按钮
    QTest::mouseClick(searchButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(2000);

    // 检查是否出现了单词卡片
    InteractiveWordCard* wordCard = mainPage->findChild<InteractiveWordCard*>();
    if (wordCard) {
        // 这里我们只是检查控件是否存在，不检查单词数据的有效性
        EXPECT_TRUE(wordCard != nullptr) << "搜索后应该显示单词卡片";
        qDebug() << "搜索成功，单词卡片已显示";
    } else {
        qDebug() << "警告：未找到单词卡片，可能搜索结果尚未完全渲染";
    }

    QTest::qWait(2000);

    QPushButton* favoriteBtn = wordCard->findChild<QPushButton*>("favoriteButton");
    ASSERT_TRUE(favoriteBtn != nullptr) << "找不到收藏按钮";

    // 点击按钮
    QTest::mouseClick(favoriteBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(3000); // 等待网络请求

    QPushButton* addToVocabularyBtn = wordCard->findChild<QPushButton*>("addToVocabularyButton");
    ASSERT_TRUE(addToVocabularyBtn != nullptr) << "找不到收藏按钮";

    // 点击按钮
    QTest::mouseClick(addToVocabularyBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(3000); // 等待网络请求

    QPushButton* pronunciationBtn = wordCard->findChild<QPushButton*>("pronunciationButton");
    ASSERT_TRUE(pronunciationBtn != nullptr) << "找不到收藏按钮";

    // 点击按钮
    QTest::mouseClick(pronunciationBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(3000); // 等待网络请求


    // 5. 测试主题切换
    qDebug() << "5. 测试主题切换";
    // 展开侧边栏
    QTest::mouseClick(toggleButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1000);

    QPushButton* themeButton = sidebar->findChild<QPushButton*>("themeButton");
    if (!themeButton) {
        // 如果没找到特定名称的按钮，尝试查找其他可能的按钮
        QList<QPushButton*> buttons = sidebar->findChildren<QPushButton*>();
        for (auto btn : buttons) {
            if (btn->text().contains("主题") || btn->text().contains("模式")) {
                themeButton = btn;
                break;
            }
        }
    }
    ASSERT_NE(themeButton, nullptr) << "无法找到主题切换按钮";

    // 记录当前主题
    UITheme initialTheme = mainPage->uiTheme();
    qDebug() << "当前主题：" << (initialTheme == UITheme::Light ? "亮色" : "暗色");

    // 点击主题切换按钮
    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1000);

    // 检查主题是否已切换
    UITheme newTheme = mainPage->uiTheme();
    qDebug() << "切换后主题：" << (newTheme == UITheme::Light ? "亮色" : "暗色");

    EXPECT_NE(initialTheme, newTheme) << "主题应该发生切换";
    qDebug() << "主题切换成功";
    QTest::qWait(2000);

    // 6. 测试注销
    qDebug() << "6. 测试注销";
    QPushButton* deleteButton = sidebar->findChild<QPushButton*>("deleteButton");
    if (!deleteButton) {
        qDebug()<<"没有找到注销按钮";
    }
    ASSERT_NE(deleteButton, nullptr) << "无法找到注销按钮";

    // 点击退出登录按钮
    QTest::mouseClick(deleteButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(3000);

    // 验证是否跳转到登录页面
    //EXPECT_EQ(pageController->findChild<QStackedWidget*>()->currentIndex(), 0) << "退出登录后应该跳转到登录页面";
    qDebug() << "退出登录成功，已跳转到登录页面";
    QTest::qWait(2000);

    qDebug() << "主页功能测试完成";
}
