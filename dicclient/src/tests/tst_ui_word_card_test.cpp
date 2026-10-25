// tst_ui_word_card_test.cpp
/*
 * WordCard 组件单元测试（GoogleTest）
 *
 * 覆盖：
 *   - 构造与数据读写（WordCard / WordData）
 *   - 展开/折叠状态机
 *   - sizeHint / minimumSizeHint 约束
 *   - 鼠标点击 / 双击信号
 *   - 消息悬浮层（showMessage / clearMessage）
 *   - UIState 状态切换
 *
 * 注意：QWidget 测试必须先创建 QApplication，这里用
 *       ::testing::AddGlobalTestEnvironment 在 main 前完成。
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/word/word_card.h"
#include "word_data.h"

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
        static char arg0[] = "tst_ui_word_card_test";
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

::testing::Environment *const qtWidgetEnv =
    ::testing::AddGlobalTestEnvironment(new QtWidgetTestEnvironment);

// ---------------------------------------------------------------------------
// 构造与数据
// ---------------------------------------------------------------------------
TEST(UiWordCardTest, ConstructWithData)
{
    const WordData data(QStringLiteral("Hello"),
                        QStringLiteral("/həˈləʊ/"),
                        QStringLiteral("int. 你好"),
                        QStringLiteral("Hello, how are you?"),
                        QStringLiteral("嗨"));

    const WordCard card(data);

    EXPECT_EQ(card.getWordData().word, QStringLiteral("Hello"));
    EXPECT_EQ(card.getWordData().phonetic, QStringLiteral("/həˈləʊ/"));
    EXPECT_EQ(card.getWordData().meaning, QStringLiteral("int. 你好"));
    EXPECT_EQ(card.getWordData().example, QStringLiteral("Hello, how are you?"));
    EXPECT_TRUE(card.getWordData().isValid());
}

TEST(UiWordCardTest, ConstructWithEmptyData)
{
    const WordCard card(WordData{});
    EXPECT_TRUE(card.getWordData().word.isEmpty());
    EXPECT_FALSE(card.getWordData().isValid());
}

TEST(UiWordCardTest, SetWordDataUpdatesContent)
{
    WordCard card(WordData{});
    const WordData data(QStringLiteral("Test"),
                        QStringLiteral("/test/"),
                        QStringLiteral("n. 测试"));

    card.setWordData(data);

    EXPECT_EQ(card.getWordData().word, QStringLiteral("Test"));
    EXPECT_EQ(card.getWordData().meaning, QStringLiteral("n. 测试"));
    EXPECT_EQ(card.getWordData().phonetic, QStringLiteral("/test/"));
}

TEST(UiWordCardTest, CollectedAndVocabularyFlagsArePreserved)
{
    const WordData data(QStringLiteral("book"),
                        QStringLiteral("/bʊk/"),
                        QStringLiteral("n. 书"),
                        QString(),
                        QString(),
                        /*isCollected=*/true,
                        /*isVocabulary=*/true);

    const WordCard card(data);

    EXPECT_TRUE(card.getWordData().isCollected);
    EXPECT_TRUE(card.getWordData().isVocabulary);
}

// ---------------------------------------------------------------------------
// 展开 / 折叠
// ---------------------------------------------------------------------------
TEST(UiWordCardTest, ExpandCollapse)
{
    WordCard card(WordData{QStringLiteral("book"), QStringLiteral("/bʊk/"),
                           QStringLiteral("n. 书")});

    EXPECT_FALSE(card.isExpanded());

    card.setExpanded(true);
    EXPECT_TRUE(card.isExpanded());

    card.setExpanded(false);
    EXPECT_FALSE(card.isExpanded());
}

// ---------------------------------------------------------------------------
// 尺寸约束
// ---------------------------------------------------------------------------
TEST(UiWordCardTest, SizeHintsAreValid)
{
    const WordCard card(WordData{QStringLiteral("Pneumonoultramicroscopicsilicovolcanoconiosis"),
                                 QStringLiteral("/ˌnjuːmənoʊ/"),
                                 QStringLiteral("n. 肺尘病")});

    const QSize hint = card.sizeHint();
    const QSize minHint = card.minimumSizeHint();

    EXPECT_TRUE(hint.isValid());
    EXPECT_TRUE(minHint.isValid());
    EXPECT_GT(hint.width(), 0);
    EXPECT_GT(hint.height(), 0);
    EXPECT_LE(minHint.width(), hint.width());
    EXPECT_LE(minHint.height(), hint.height());
}

TEST(UiWordCardTest, LongMeaningDoesNotBreakSizeHint)
{
    QString longMeaning = QStringLiteral("这是一个非常长的释义，");
    for (int i = 0; i < 10; ++i)
    {
        longMeaning += QStringLiteral("这一行是为了测试多行文本显示功能。");
    }

    const WordCard card(WordData{QStringLiteral("Test"),
                                 QStringLiteral("/test/"),
                                 longMeaning});

    EXPECT_GT(card.sizeHint().height(), 0);
}

// ---------------------------------------------------------------------------
// 鼠标信号
// ---------------------------------------------------------------------------
TEST(UiWordCardTest, ClickSignalsAreEmitted)
{
    QWidget window;
    QVBoxLayout *layout = new QVBoxLayout(&window);
    const WordData data(QStringLiteral("Hello"),
                        QStringLiteral("/həˈləʊ/"),
                        QStringLiteral("int. 你好"));
    auto *card = new WordCard(data, &window);
    layout->addWidget(card);
    window.show();
    QApplication::processEvents();

    QSignalSpy clickSpy(card, &WordCard::clicked);
    QSignalSpy doubleClickSpy(card, &WordCard::doubleClicked);
    ASSERT_TRUE(clickSpy.isValid());
    ASSERT_TRUE(doubleClickSpy.isValid());

    const QPoint center = card->rect().center();

    QTest::mouseClick(card, Qt::LeftButton, Qt::NoModifier, center);
    QApplication::processEvents();
    EXPECT_EQ(clickSpy.count(), 1) << "单击应触发 1 次 clicked 信号";

    QTest::mouseDClick(card, Qt::LeftButton, Qt::NoModifier, center);
    QApplication::processEvents();
    EXPECT_EQ(doubleClickSpy.count(), 1) << "双击应触发 1 次 doubleClicked 信号";
}

TEST(UiWordCardTest, DoubleClickTogglesExpanded)
{
    QWidget window;
    const WordData data(QStringLiteral("Hello"),
                        QStringLiteral("/həˈləʊ/"),
                        QStringLiteral("int. 你好"));
    WordCard card(data, &window);
    window.show();
    QApplication::processEvents();

    const bool before = card.isExpanded();
    QTest::mouseDClick(&card, Qt::LeftButton, Qt::NoModifier, card.rect().center());
    QApplication::processEvents();
    EXPECT_NE(card.isExpanded(), before) << "双击应切换展开状态";
}

// ---------------------------------------------------------------------------
// BaseWidget 消息与状态
// ---------------------------------------------------------------------------
TEST(UiWordCardTest, ShowAndClearMessage)
{
    WordCard card(WordData{QStringLiteral("book"), QStringLiteral("/bʊk/"),
                           QStringLiteral("n. 书")});

    card.showMessage(QStringLiteral("普通消息"), false);
    QApplication::processEvents();
    card.clearMessage();
    QApplication::processEvents();
    SUCCEED();
}

TEST(UiWordCardTest, UIStateSwitching)
{
    WordCard card(WordData{QStringLiteral("book"), QStringLiteral("/bʊk/"),
                           QStringLiteral("n. 书")});

    EXPECT_EQ(card.uiState(), UIState::Normal);

    QSignalSpy stateSpy(&card, &BaseWidget::stateChanged);
    card.setUIState(UIState::Loading);
    EXPECT_EQ(card.uiState(), UIState::Loading);
    EXPECT_EQ(stateSpy.count(), 1);

    card.setUIState(UIState::Normal);
    EXPECT_EQ(card.uiState(), UIState::Normal);
    EXPECT_EQ(stateSpy.count(), 2);
}

TEST(UiWordCardTest, UIStateIntProperty)
{
    WordCard card(WordData{});
    card.setUIStateInt(static_cast<int>(UIState::Error));
    EXPECT_EQ(card.uiStateInt(), static_cast<int>(UIState::Error));
    EXPECT_EQ(card.uiState(), UIState::Error);
}
