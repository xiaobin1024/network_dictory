#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QThread>
#include <QSettings>
#include "system/system_sidebar.h"
#include "system/theme_manager.h"
#include "base/base_widget.h"
using namespace testing;

/**
 * @brief SystemSidebar GoogleTest 测试
 * 注意：在测试 QWidget 之前必须先创建 QApplication
 */

// 创建全局的 QApplication 实例
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        char* argv[] = {nullptr};
        app = new QApplication(argc, argv);

        // 设置应用程序信息，供QSettings使用
        QCoreApplication::setOrganizationName("DictionaryApp");
        QCoreApplication::setApplicationName("DictionaryTest");
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

TEST(SystemBaseTestxxxx, SystemSidebarBasicTest)
{
    // 确保有 QApplication 实例
    ASSERT_NE(QApplication::instance(), nullptr);

    qDebug() << "\n=== 开始 SystemSidebar 基础测试 ===";

    // 创建主窗口
    BaseWidget window;
    window.setWindowTitle("SystemSidebar 基础测试");
    window.setStyleSheet("background-color: #f8fafc;");
    window.resize(800, 600);

    window.mainLayout()->setContentsMargins(0, 0, 0, 0);
    window.mainLayout()->setSpacing(0);

    qDebug() << "=== 测试1: 创建 SystemSidebar ===";

    SystemSidebar* sidebar = new SystemSidebar(&window);
    window.mainLayout()->addWidget(sidebar);

    // 显示窗口
    window.show();
    QApplication::processEvents();
    QTest::qWait(1000);

    qDebug() << "SystemSidebar 创建成功";
    qDebug() << "初始宽度：" << sidebar->currentWidth();
    qDebug() << "展开状态：" << sidebar->isExpanded();

    // 验证初始状态 - 现在应为收起状态
    EXPECT_FALSE(sidebar->isExpanded()) << "初始应为收起状态";
    EXPECT_EQ(sidebar->currentWidth(), SystemSidebar::COLLAPSED_WIDTH)
        << "初始宽度应为收起宽度";
    QTest::qWait(2000);

    qDebug() << "=== 测试2: 切换到展开状态 ===";

    QSignalSpy expandedSpy(sidebar, &SystemSidebar::expandedChanged);

    // 切换为展开状态
    qDebug() << "切换到展开状态...";
    sidebar->setExpanded(true);

    // 正确等待动画完成
    QTRY_VERIFY2(sidebar->currentWidth() == SystemSidebar::EXPANDED_WIDTH,
                 "侧边栏动画未完成");

    // 确保事件循环处理完成
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    qDebug() << "展开状态：" << sidebar->isExpanded();
    qDebug() << "展开宽度：" << sidebar->currentWidth();
    qDebug() << "展开状态变化信号次数：" << expandedSpy.count();

    EXPECT_TRUE(sidebar->isExpanded()) << "切换后应为展开状态";
    EXPECT_EQ(sidebar->currentWidth(), SystemSidebar::EXPANDED_WIDTH)
        << "展开状态宽度应为展开宽度";
    EXPECT_EQ(expandedSpy.count(), 1) << "应触发展开状态变化信号";

    if (expandedSpy.count() > 0) {
        QList<QVariant> arguments = expandedSpy.takeFirst();
        bool isExpanded = arguments.at(0).toBool();
        EXPECT_TRUE(isExpanded) << "信号参数应为true（展开状态）";
    }

    QTest::qWait(2000);

    qDebug() << "=== 测试3: 再次收起 ===";

    // 切换回收起状态
    qDebug() << "切换回收起状态...";
    sidebar->setExpanded(false);
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "收起状态：" << sidebar->isExpanded();
    qDebug() << "收起宽度：" << sidebar->currentWidth();

    EXPECT_FALSE(sidebar->isExpanded()) << "应切换回收起状态";
    EXPECT_EQ(sidebar->currentWidth(), SystemSidebar::COLLAPSED_WIDTH)
        << "应恢复收起宽度";

    QTest::qWait(2000);

    qDebug() << "=== 测试4: 主题切换测试 ===";

    // 切换为展开状态
    qDebug() << "切换到展开状态...";
    sidebar->setExpanded(true);

    // 正确等待动画完成
    QTRY_VERIFY2(sidebar->currentWidth() == SystemSidebar::EXPANDED_WIDTH,
                 "侧边栏动画未完成");

    // 确保事件循环处理完成
    QApplication::processEvents(QEventLoop::AllEvents, 100);


    // 获取初始主题
    ThemeManager* themeManager = ThemeManager::instance();
    ASSERT_NE(themeManager, nullptr) << "ThemeManager 实例不应为空";

    int initialTheme = themeManager->currentThemeInt();
    qDebug() << "初始主题：" << (initialTheme == 0 ? "亮色" : "暗色");

    // 监听主题切换信号
    QSignalSpy themeSpy(themeManager, &ThemeManager::themeChanged);

    // 通过侧边栏的按钮切换主题
    qDebug() << "通过侧边栏按钮切换主题...";

    // 找到主题切换按钮
    QPushButton* themeButton = sidebar->findChild<QPushButton*>("themeButton");
    ASSERT_NE(themeButton, nullptr) << "应找到主题切换按钮";

    // 模拟点击主题按钮
    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1000);

    qDebug() << "主题切换信号次数：" << themeSpy.count();
    qDebug() << "切换后主题：" << themeManager->currentThemeInt();

    // 验证主题已切换
    int newTheme = themeManager->currentThemeInt();
    EXPECT_NE(initialTheme, newTheme) << "点击主题按钮后主题应切换";

    if (themeSpy.count() > 0) {
        QList<QVariant> arguments = themeSpy.takeFirst();
        int theme = arguments.at(0).toInt();
        EXPECT_EQ(theme, newTheme) << "信号参数应与当前主题一致";
    }


      QTest::qWait(1000);

    qDebug() << "=== 测试5: 按钮点击信号测试 ===";

    // 监听退出登录信号
    QSignalSpy logoutSpy(sidebar, &SystemSidebar::logoutRequested);

    // 找到退出登录按钮
    QPushButton* logoutButton = sidebar->findChild<QPushButton*>("logoutButton");
    ASSERT_NE(logoutButton, nullptr) << "应找到退出登录按钮";

    // 模拟点击退出登录按钮
    QTest::mouseClick(logoutButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "退出登录信号次数：" << logoutSpy.count();
    EXPECT_EQ(logoutSpy.count(), 1) << "应触发退出登录信号";

    QTest::qWait(2000);

    qDebug() << "=== 测试6: 注销账号按钮测试 ===";

    // 监听注销账号信号
    QSignalSpy deleteSpy(sidebar, &SystemSidebar::deleteAccountRequested);

    // 找到注销账号按钮
    QPushButton* deleteButton = sidebar->findChild<QPushButton*>("deleteButton");
    ASSERT_NE(deleteButton, nullptr) << "应找到注销账号按钮";

    // 模拟点击注销账号按钮
    QTest::mouseClick(deleteButton, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "注销账号信号次数：" << deleteSpy.count();
    EXPECT_EQ(deleteSpy.count(), 1) << "应触发注销账号信号";

    QTest::qWait(2000);
}
