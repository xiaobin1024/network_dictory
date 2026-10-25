// tst_ui_search_base.cpp
/*
 * SearchBase 基础搜索组件单元测试（GoogleTest）
 *
 * 覆盖：
 *   - keyword 读写与同步
 *   - placeholder 读写
 *   - 清除 / 搜索按钮显隐控制
 *   - searchRequested 信号（回车触发）
 *   - keywordChanged 信号
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <QWidget>

#include "ui/search/search_base.h"

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
        static char arg0[] = "tst_ui_search_base";
        static char *argv[] = {arg0, nullptr};
        app = new QApplication(argc, argv);
    }

    void TearDown() override
    {
        delete app;
        app = nullptr;
    }

private:
    QApplication *app = nullptr;
};

::testing::Environment *const qtSearchBaseEnv =
    ::testing::AddGlobalTestEnvironment(new QtWidgetTestEnvironment);

// ---------------------------------------------------------------------------
// keyword
// ---------------------------------------------------------------------------
TEST(SearchBaseTest, KeywordRoundTrip)
{
    SearchBase search;
    EXPECT_TRUE(search.keyword().isEmpty());

    search.setKeyword(QStringLiteral("hello"));
    EXPECT_EQ(search.keyword(), QStringLiteral("hello"));
}

TEST(SearchBaseTest, SetKeywordUpdatesInputField)
{
    SearchBase search;
    search.setKeyword(QStringLiteral("world"));

    ASSERT_NE(search.inputField(), nullptr);
    EXPECT_EQ(search.inputField()->text(), QStringLiteral("world"));
}

// ---------------------------------------------------------------------------
// placeholder
// ---------------------------------------------------------------------------
TEST(SearchBaseTest, PlaceholderRoundTrip)
{
    SearchBase search;
    const QString original = search.placeholder();
    EXPECT_FALSE(original.isEmpty()) << "默认应有占位提示文本";

    search.setPlaceholder(QStringLiteral("请输入单词"));
    EXPECT_EQ(search.placeholder(), QStringLiteral("请输入单词"));
}

// ---------------------------------------------------------------------------
// 按钮显隐
// ---------------------------------------------------------------------------
TEST(SearchBaseTest, ClearButtonVisibility)
{
    SearchBase search;
    ASSERT_NE(search.clearButton(), nullptr);
    search.show();  // 子控件 isVisible 受父窗口可见性影响
    QApplication::processEvents();

    // 无关键词时清除按钮应隐藏
    search.setKeyword("");
    search.setShowClearButton(true);
    QApplication::processEvents();
    EXPECT_FALSE(search.clearButton()->isVisible());

    // 有关键词时才显示
    search.setKeyword("hello");
    QApplication::processEvents();
    EXPECT_TRUE(search.clearButton()->isVisible());

    // 关闭开关后重新隐藏
    search.setShowClearButton(false);
    QApplication::processEvents();
    EXPECT_FALSE(search.clearButton()->isVisible());
}

TEST(SearchBaseTest, SearchButtonVisibility)
{
    SearchBase search;
    ASSERT_NE(search.searchButton(), nullptr);
    search.show();  // 子控件 isVisible 受父窗口可见性影响
    QApplication::processEvents();

    search.setShowSearchButton(false);
    QApplication::processEvents();
    EXPECT_FALSE(search.searchButton()->isVisible());

    search.setShowSearchButton(true);
    QApplication::processEvents();
    EXPECT_TRUE(search.searchButton()->isVisible());
}

// ---------------------------------------------------------------------------
// 信号
// ---------------------------------------------------------------------------
TEST(SearchBaseTest, EnterKeyEmitsSearchRequested)
{
    SearchBase search;
    search.setKeyword(QStringLiteral("abandon"));
    search.show();
    QApplication::processEvents();

    QSignalSpy searchSpy(&search, &SearchBase::searchRequested);
    ASSERT_TRUE(searchSpy.isValid());

    QTest::keyClick(search.inputField(), Qt::Key_Return);
    QApplication::processEvents();

    EXPECT_EQ(searchSpy.count(), 1) << "按下回车应触发一次搜索请求";
    ASSERT_EQ(searchSpy.size(), 1);
    EXPECT_EQ(searchSpy.takeFirst().at(0).toString(), QStringLiteral("abandon"));
}

TEST(SearchBaseTest, TypingEmitsKeywordChanged)
{
    SearchBase search;
    search.show();
    QApplication::processEvents();

    QSignalSpy keywordSpy(&search, &SearchBase::keywordChanged);
    ASSERT_TRUE(keywordSpy.isValid());

    search.setKeyword(QStringLiteral("ab"));
    QApplication::processEvents();

    EXPECT_GE(keywordSpy.count(), 1);
}
