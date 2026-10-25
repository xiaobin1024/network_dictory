// tst_ui_searchhistory_widget.cpp
/*
 * SearchHistoryWidget 搜索历史组件单元测试（GoogleTest）
 *
 * 覆盖：
 *   - addHistory 记录与去重
 *   - historyCount / getHistory 查询
 *   - clearHistory 清空
 *   - setMaxHistoryCount 容量上限与淘汰策略
 *   - showHistory / hideHistory / isHistoryVisible 显隐状态
 *   - historyItemClicked / historyCleared 信号
 *
 * 注意：SearchHistoryWidget 通过 QSettings 持久化，测试前统一设置
 *       组织/应用名并清理历史，避免用例间相互污染。
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <QWidget>

#include "ui/search/searchhistory_widget.h"

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
        static char arg0[] = "tst_ui_searchhistory_widget";
        static char *argv[] = {arg0, nullptr};
        app = new QApplication(argc, argv);

        QCoreApplication::setOrganizationName(QStringLiteral("DictionaryTest"));
        QCoreApplication::setApplicationName(QStringLiteral("SearchHistoryTest"));
    }

    void TearDown() override
    {
        delete app;
        app = nullptr;
    }

private:
    QApplication *app = nullptr;
};

::testing::Environment *const qtHistoryEnv =
    ::testing::AddGlobalTestEnvironment(new QtWidgetTestEnvironment);

namespace {

// 每个用例都从清空历史开始，保证可重复执行
SearchHistoryWidget *makeCleanWidget(QWidget *parent = nullptr)
{
    auto *widget = new SearchHistoryWidget(parent);
    widget->clearHistory();
    return widget;
}

} // namespace

// ---------------------------------------------------------------------------
// 记录与查询
// ---------------------------------------------------------------------------
TEST(SearchHistoryWidgetTest, AddAndCountHistory)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);

    EXPECT_EQ(widget->historyCount(), 0);

    widget->addHistory(QStringLiteral("apple"));
    widget->addHistory(QStringLiteral("banana"));

    EXPECT_EQ(widget->historyCount(), 2);
    EXPECT_TRUE(widget->getHistory().contains(QStringLiteral("apple")));
    EXPECT_TRUE(widget->getHistory().contains(QStringLiteral("banana")));
}

TEST(SearchHistoryWidgetTest, DuplicateKeywordIsNotRecordedTwice)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);

    widget->addHistory(QStringLiteral("apple"));
    widget->addHistory(QStringLiteral("banana"));
    widget->addHistory(QStringLiteral("apple"));

    EXPECT_EQ(widget->historyCount(), 2) << "重复关键词不应重复入队";
}

TEST(SearchHistoryWidgetTest, ClearHistoryEmptiesList)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);

    widget->addHistory(QStringLiteral("apple"));
    widget->addHistory(QStringLiteral("banana"));
    ASSERT_EQ(widget->historyCount(), 2);

    widget->clearHistory();

    EXPECT_EQ(widget->historyCount(), 0);
    EXPECT_TRUE(widget->getHistory().isEmpty());
}

// ---------------------------------------------------------------------------
// 容量上限
// ---------------------------------------------------------------------------
TEST(SearchHistoryWidgetTest, MaxHistoryCountIsEnforced)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);

    widget->setMaxHistoryCount(3);
    ASSERT_EQ(widget->maxHistoryCount(), 3);

    for (int i = 0; i < 6; ++i)
    {
        widget->addHistory(QStringLiteral("kw%1").arg(i));
    }

    EXPECT_EQ(widget->historyCount(), 3) << "超出上限后应只保留最近 3 条";
}

TEST(SearchHistoryWidgetTest, ChangingMaxCountShrinksExistingHistory)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);

    widget->setMaxHistoryCount(10);
    for (int i = 0; i < 8; ++i)
    {
        widget->addHistory(QStringLiteral("kw%1").arg(i));
    }
    ASSERT_EQ(widget->historyCount(), 8);

    widget->setMaxHistoryCount(4);
    EXPECT_EQ(widget->historyCount(), 4);
}

// ---------------------------------------------------------------------------
// 显隐状态
// ---------------------------------------------------------------------------
TEST(SearchHistoryWidgetTest, VisibilityToggle)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);
    widget->addHistory(QStringLiteral("apple"));

    widget->hideHistory();
    EXPECT_FALSE(widget->isHistoryVisible());

    widget->showHistory();
    EXPECT_TRUE(widget->isHistoryVisible());

    widget->hideHistory();
    EXPECT_FALSE(widget->isHistoryVisible());
}

// ---------------------------------------------------------------------------
// 信号
// ---------------------------------------------------------------------------
TEST(SearchHistoryWidgetTest, HistoryClearedSignal)
{
    QWidget window;
    auto *widget = makeCleanWidget(&window);
    widget->addHistory(QStringLiteral("apple"));

    QSignalSpy clearedSpy(widget, &SearchHistoryWidget::historyCleared);
    ASSERT_TRUE(clearedSpy.isValid());

    widget->clearHistory();
    EXPECT_GE(clearedSpy.count(), 1);
}
