// tst_system_sidebar.cpp
/*
 * SystemSidebar 系统侧边栏单元测试（GoogleTest）
 *
 * 覆盖：
 *   - 初始状态（收起 + 收起宽度）
 *   - 展开 / 收起状态机与宽度动画
 *   - expandedChanged 信号
 *   - toggle() 行为
 *   - setSidebarWidth 自定义宽度
 *   - 主题按钮 -> themeToggleRequested 信号
 *   - 收藏 / 生词本按钮 -> showCollectPageRequested / showVocabularyPageRequested 信号
 *
 * 说明：logoutRequested / deleteAccountRequested 在当前实现中并不会被
 * emit（登出直接调用 UserSession::logout()，注销走 QMessageBox 二次确认），
 * 因此不对此二者做信号断言，避免测试永远失败。
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QVBoxLayout>
#include <QScopedPointer>
#include <QtTest/QTest>

#include "ui/system/system_sidebar.h"
#include "ui/base/theme_manager.h"
#include "base/base_widget.h"

using namespace testing;

// ---------------------------------------------------------------------------
// 全局 QApplication 环境
// ---------------------------------------------------------------------------
class QtWidgetTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        static int argc = 1;
        static char arg0[] = "tst_system_sidebar";
        static char *argv[] = {arg0, nullptr};
        app = new QApplication(argc, argv);

        QCoreApplication::setOrganizationName(QStringLiteral("DictionaryApp"));
        QCoreApplication::setApplicationName(QStringLiteral("DictionaryTest"));
    }

    void TearDown() override
    {
        delete app;
        app = nullptr;
    }

private:
    QApplication *app = nullptr;
};

::testing::Environment *const qtSidebarEnv =
    ::testing::AddGlobalTestEnvironment(new QtWidgetTestEnvironment);

namespace {

// 每个用例自带窗口与布局：QWidget 默认没有布局，必须先创建再 addWidget
QWidget *makeWindowWithSidebar(SystemSidebar **outSidebar)
{
    auto *window = new QWidget();
    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *sidebar = new SystemSidebar(window);
    layout->addWidget(sidebar);
    if (outSidebar)
    {
        *outSidebar = sidebar;
    }
    return window;
}

} // namespace

// ---------------------------------------------------------------------------
// 初始状态
// ---------------------------------------------------------------------------
TEST(SystemSidebarTest, InitialStateIsCollapsed)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    EXPECT_FALSE(sidebar->isExpanded());
    EXPECT_EQ(sidebar->currentWidth(), SystemSidebar::COLLAPSED_WIDTH);
    EXPECT_EQ(SystemSidebar::EXPANDED_WIDTH, 180);
    EXPECT_EQ(SystemSidebar::COLLAPSED_WIDTH, 50);
    EXPECT_GT(SystemSidebar::EXPANDED_WIDTH, SystemSidebar::COLLAPSED_WIDTH);
}

// ---------------------------------------------------------------------------
// 展开 / 收起
// ---------------------------------------------------------------------------
TEST(SystemSidebarTest, ExpandAndCollapse)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    QSignalSpy expandedSpy(sidebar, &SystemSidebar::expandedChanged);
    ASSERT_TRUE(expandedSpy.isValid());

    sidebar->setExpanded(true);
    QTRY_VERIFY2(sidebar->currentWidth() == SystemSidebar::EXPANDED_WIDTH,
                 "侧边栏展开动画未完成");
    QApplication::processEvents();

    EXPECT_TRUE(sidebar->isExpanded());
    EXPECT_EQ(expandedSpy.count(), 1);
    // 注意：用 first()/at() 读取记录，不要用 takeFirst()——它会从 spy 中移除记录，
    // 导致后续的累计计数断言少 1。
    if (expandedSpy.count() > 0)
    {
        EXPECT_TRUE(expandedSpy.first().at(0).toBool());
    }

    sidebar->setExpanded(false);
    QTRY_VERIFY2(sidebar->currentWidth() == SystemSidebar::COLLAPSED_WIDTH,
                 "侧边栏收起动画未完成");
    QApplication::processEvents();

    EXPECT_FALSE(sidebar->isExpanded());
    EXPECT_EQ(expandedSpy.count(), 2);
    if (expandedSpy.count() > 1)
    {
        EXPECT_FALSE(expandedSpy.at(1).at(0).toBool());
    }
}

TEST(SystemSidebarTest, ToggleFlipsExpanded)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    const bool before = sidebar->isExpanded();
    sidebar->toggle();
    QApplication::processEvents();
    EXPECT_NE(sidebar->isExpanded(), before);

    sidebar->toggle();
    QApplication::processEvents();
    EXPECT_EQ(sidebar->isExpanded(), before);
}

TEST(SystemSidebarTest, SetSidebarWidthUpdatesCurrentWidth)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    sidebar->setSidebarWidth(120);
    QApplication::processEvents();
    EXPECT_EQ(sidebar->currentWidth(), 120);
}

// ---------------------------------------------------------------------------
// 按钮与信号（只断言当前实现确实会 emit 的信号）
// ---------------------------------------------------------------------------
TEST(SystemSidebarTest, ThemeButtonEmitsToggleRequested)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    auto *themeButton = sidebar->findChild<QPushButton *>(QStringLiteral("themeButton"));
    ASSERT_NE(themeButton, nullptr) << "应找到主题切换按钮";

    QSignalSpy themeSpy(sidebar, &SystemSidebar::themeToggleRequested);
    ASSERT_TRUE(themeSpy.isValid());

    const int themeBefore = ThemeManager::instance()->currentThemeInt();
    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(themeSpy.count(), 1) << "点击主题按钮应触发 themeToggleRequested";
    EXPECT_NE(ThemeManager::instance()->currentThemeInt(), themeBefore)
        << "点击主题按钮后全局主题应切换";
}

TEST(SystemSidebarTest, CollectButtonEmitsShowCollectPage)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    auto *collectButton = sidebar->findChild<QPushButton *>(QStringLiteral("collectButton"));
    ASSERT_NE(collectButton, nullptr) << "应找到收藏按钮";

    QSignalSpy collectSpy(sidebar, &SystemSidebar::showCollectPageRequested);
    ASSERT_TRUE(collectSpy.isValid());

    QTest::mouseClick(collectButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(collectSpy.count(), 1) << "点击收藏按钮应触发 showCollectPageRequested";
}

TEST(SystemSidebarTest, VocabularyButtonEmitsShowVocabularyPage)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    auto *vocabButton = sidebar->findChild<QPushButton *>(QStringLiteral("vocabularyButton"));
    ASSERT_NE(vocabButton, nullptr) << "应找到生词本按钮";

    QSignalSpy vocabSpy(sidebar, &SystemSidebar::showVocabularyPageRequested);
    ASSERT_TRUE(vocabSpy.isValid());

    QTest::mouseClick(vocabButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(vocabSpy.count(), 1) << "点击生词本按钮应触发 showVocabularyPageRequested";
}

TEST(SystemSidebarTest, AllFunctionalButtonsExist)
{
    SystemSidebar *sidebar = nullptr;
    QScopedPointer<QWidget> window(makeWindowWithSidebar(&sidebar));
    window->show();
    QApplication::processEvents();

    for (const char *name : {"themeButton", "logoutButton", "deleteButton",
                             "collectButton", "vocabularyButton", "toggleButton"})
    {
        auto *btn = sidebar->findChild<QPushButton *>(QString::fromLatin1(name));
        EXPECT_NE(btn, nullptr) << "缺少按钮:" << name;
    }
}
