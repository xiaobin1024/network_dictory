// tst_ui_interactive_wordcard.cpp
/*
 * InteractiveWordCard 组件单元测试（GoogleTest）
 *
 * 覆盖：
 *   - 收藏状态读写
 *   - 生词本状态读写
 *   - 功能性按钮的存在性（收藏 / 发音 / 生词本 / 更多操作）
 *
 * InteractiveWordCard 需要 WordCollect / WordVocabulary 两个协作者，
 * 二者均为单例，测试中直接取 instance()。
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <QWidget>

#include "ui/word/interactive_wordcard.h"
#include "word_function/word_collect.h"
#include "word_function/word_vocabulary.h"
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
        static char arg0[] = "tst_ui_interactive_wordcard";
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

::testing::Environment *const qtInteractiveEnv =
    ::testing::AddGlobalTestEnvironment(new QtWidgetTestEnvironment);

namespace {

WordData makeWord()
{
    return WordData(QStringLiteral("Test"),
                    QStringLiteral("/test/"),
                    QStringLiteral("n. 测试"),
                    QStringLiteral("This is a test."),
                    QStringLiteral("这是一个测试。"));
}

InteractiveWordCard *makeCard(QWidget *parent = nullptr)
{
    return new InteractiveWordCard(makeWord(),
                                   WordCollect::instance(),
                                   WordVocabulary::instance(),
                                   parent);
}

} // namespace

// ---------------------------------------------------------------------------
// 收藏状态
// ---------------------------------------------------------------------------
TEST(InteractiveWordCardTest, FavoriteStateRoundTrip)
{
    QWidget window;
    auto *card = makeCard(&window);

    card->setFavorite(true);
    EXPECT_TRUE(card->isFavorite());

    card->setFavorite(false);
    EXPECT_FALSE(card->isFavorite());
}

// ---------------------------------------------------------------------------
// 生词本状态
// ---------------------------------------------------------------------------
TEST(InteractiveWordCardTest, VocabularyStateRoundTrip)
{
    QWidget window;
    auto *card = makeCard(&window);

    card->setAddedToVocabulary(true);
    EXPECT_TRUE(card->isAddedToVocabulary());

    card->setAddedToVocabulary(false);
    EXPECT_FALSE(card->isAddedToVocabulary());
}

// ---------------------------------------------------------------------------
// 按钮布局
// ---------------------------------------------------------------------------
TEST(InteractiveWordCardTest, FunctionalButtonsExist)
{
    QWidget window;
    auto *card = makeCard(&window);
    window.show();
    QApplication::processEvents();

    const QList<QPushButton *> buttons = card->findChildren<QPushButton *>();
    EXPECT_GE(buttons.size(), 4)
        << "卡片应至少包含收藏、发音、生词本、更多操作四个按钮，实际: "
        << buttons.size();
}

// ---------------------------------------------------------------------------
// 信号连接有效性
// ---------------------------------------------------------------------------
TEST(InteractiveWordCardTest, SignalDeclarationsAreConnectable)
{
    QWidget window;
    auto *card = makeCard(&window);

    QSignalSpy favoriteSpy(card, &InteractiveWordCard::favoriteToggled);
    QSignalSpy vocabularySpy(card, &InteractiveWordCard::addToVocabularyRequested);
    QSignalSpy pronounceSpy(card, &InteractiveWordCard::pronunciationRequested);

    EXPECT_TRUE(favoriteSpy.isValid());
    EXPECT_TRUE(vocabularySpy.isValid());
    EXPECT_TRUE(pronounceSpy.isValid());
}
