#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include<QtTest/QTest>
#include<QtTest/QSignalSpy>
#include<QThread>
#include"ui/word/interactive_wordcard.h"
using namespace testing;

/**
 * @brief InteractiveWordCard GoogleTest 测试
 * 注意：在测试 QWidget 之前必须先创建 QApplication
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

TEST(UiTest,ButtonVisibilityTest)
{
    ASSERT_NE(QApplication::instance(), nullptr);

    QWidget window;
    window.resize(400, 300);

    WordData data("Test", "/test/", "测试", "n.", "Test example");
    InteractiveWordCard card(data, &window);

    card.show();
    QApplication::processEvents();
    QTest::qWait(1000);

    // 查找按钮
    auto buttons = card.findChildren<QPushButton*>();
    qDebug() << "找到按钮数量:" << buttons.size();

    for (auto btn : buttons) {
        qDebug() << "按钮:" << btn->objectName()
            << "可见:" << btn->isVisible()
            << "尺寸:" << btn->size()
            << "位置:" << btn->pos();
    }

    EXPECT_GE(buttons.size(), 4) << "应至少找到4个按钮";
}
TEST(UiTest,InterativeTest)
{
    // 确保有 QApplication 实例
    ASSERT_NE(QApplication::instance(), nullptr);

    qDebug() << "\n=== 开始 InteractiveWordCard 交互式测试 ===";

    // 创建主窗口
    QWidget window;
    window.setWindowTitle("InteractiveWordCard 交互式测试");
    window.setStyleSheet("background-color: #f8fafc;");
    window.resize(500, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(&window);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    qDebug() << "=== 测试1: 创建 InteractiveWordCard ===";

    WordData data("Hello", "/həˈləʊ/",
                  "int. 你好；喂\n"
                  "1. 用于打招呼\n"
                  "2. 电话接听用语",
                  "int.",
                  "Hello, how are you today?");

    InteractiveWordCard* card = new InteractiveWordCard(data, &window);
    mainLayout->addWidget(card);


    // 显示窗口
    window.show();
    QApplication::processEvents();
    QTest::qWait(3000);

    qDebug() << "卡片创建成功";
    qDebug() << "单词: " << card->getWordData().word;
    qDebug() << "初始收藏状态: " << card->isFavorite();
    qDebug() << "初始生词本状态: " << card->isAddedToVocabulary();

    EXPECT_FALSE(card->isFavorite()) << "初始应为未收藏状态";
    EXPECT_FALSE(card->isAddedToVocabulary()) << "初始应为未添加到生词本状态";

    qDebug() << "\n=== 测试2: 收藏按钮测试 ===";

    // 创建信号间谍
    QSignalSpy favoriteSpy(card, &InteractiveWordCard::favoriteToggled);

    qDebug() << "模拟点击收藏按钮...";

    // 查找收藏按钮
    auto favoriteButtons = window.findChildren<QPushButton*>();
    QPushButton* favoriteBtn = nullptr;
    for (auto btn : favoriteButtons) {
        if (btn->toolTip().contains("收藏")) {
            favoriteBtn = btn;
            break;
        }
    }

    ASSERT_NE(favoriteBtn, nullptr) << "应能找到收藏按钮";

    // 模拟点击收藏按钮
    QTest::mouseClick(favoriteBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1500);

    qDebug() << "收藏信号触发次数: " << favoriteSpy.count();

    if (favoriteSpy.count() > 0) {
        QList<QVariant> arguments = favoriteSpy.takeFirst();
        QString word = arguments.at(0).toString();
        bool favorite = arguments.at(1).toBool();

        qDebug() << "收藏信号参数: 单词=" << word << ", 收藏状态=" << favorite;

        EXPECT_EQ(word, "Hello") << "收藏信号应传递正确的单词";
        EXPECT_TRUE(favorite) << "收藏信号应为true";
    }

    EXPECT_TRUE(card->isFavorite()) << "点击后应为收藏状态";
    QTest::qWait(2000);

    qDebug() << "\n=== 测试3: 取消收藏测试 ===";

    qDebug() << "模拟再次点击收藏按钮取消收藏...";

    QTest::mouseClick(favoriteBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1500);

    EXPECT_EQ(favoriteSpy.count(), 1) << "应有第二个收藏信号";

    if (favoriteSpy.count() > 0) {
        QList<QVariant> arguments = favoriteSpy.takeFirst();
        bool favorite = arguments.at(1).toBool();

        EXPECT_FALSE(favorite) << "第二个收藏信号应为false";
    }

    EXPECT_FALSE(card->isFavorite()) << "第二次点击后应为取消收藏状态";
    QTest::qWait(2000);

    qDebug() << "\n=== 测试4: 发音按钮测试 ===";

    QSignalSpy pronunciationSpy(card, &InteractiveWordCard::pronunciationRequested);

    qDebug() << "模拟点击发音按钮...";

    // 查找发音按钮
    QPushButton* pronunciationBtn = nullptr;
    for (auto btn : favoriteButtons) {
        if (btn->toolTip().contains("发音")) {
            pronunciationBtn = btn;
            break;
        }
    }

    ASSERT_NE(pronunciationBtn, nullptr) << "应能找到发音按钮";
    QTest::mouseClick(pronunciationBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1500);

    EXPECT_EQ(pronunciationSpy.count(), 1) << "发音信号应触发一次";

    if (pronunciationSpy.count() > 0) {
        QList<QVariant> arguments = pronunciationSpy.takeFirst();
        QString word = arguments.at(0).toString();

        EXPECT_EQ(word, "Hello") << "发音信号应传递正确的单词";
    }
    QTest::qWait(2000);

    qDebug() << "\n=== 测试5: 添加到生词本按钮测试 ===";

    QSignalSpy addVocabularySpy(card, &InteractiveWordCard::addToVocabularyRequested);

    qDebug() << "模拟点击添加到生词本按钮...";

    // 查找添加到生词本按钮
    QPushButton* vocabularyBtn = nullptr;
    for (auto btn : favoriteButtons) {
        if (btn->toolTip().contains("添加到生词本")) {
            vocabularyBtn = btn;
            break;
        }
    }

    ASSERT_NE(vocabularyBtn, nullptr) << "应能找到添加到生词本按钮";

    QTest::mouseClick(vocabularyBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1500);

    EXPECT_EQ(addVocabularySpy.count(), 1) << "添加到生词本信号应触发一次";
    EXPECT_TRUE(card->isAddedToVocabulary()) << "点击后应标记为已添加到生词本";

    if (addVocabularySpy.count() > 0) {
        QList<QVariant> arguments = addVocabularySpy.takeFirst();
        WordData data = arguments.at(0).value<WordData>();

        EXPECT_EQ(data.word, "Hello") << "信号应传递正确的单词数据";
    }

    qDebug() << "工具提示更新为: " << vocabularyBtn->toolTip();
    EXPECT_TRUE(vocabularyBtn->toolTip().contains("从生词本移除"))
        << "添加到生词本后工具提示应更新";
    QTest::qWait(2000);

    qDebug() << "\n=== 测试6: 从生词本移除测试 ===";

    QSignalSpy removeVocabularySpy(card, &InteractiveWordCard::removeFromVocabularyRequested);

    qDebug() << "模拟再次点击从生词本移除...";

    QTest::mouseClick(vocabularyBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(1500);

    EXPECT_EQ(removeVocabularySpy.count(), 1) << "从生词本移除信号应触发一次";
    EXPECT_FALSE(card->isAddedToVocabulary()) << "第二次点击后应标记为未添加到生词本";

    if (removeVocabularySpy.count() > 0) {
        QList<QVariant> arguments = removeVocabularySpy.takeFirst();
        QString word = arguments.at(0).toString();

        EXPECT_EQ(word, "Hello") << "移除信号应传递正确的单词";
    }

    qDebug() << "工具提示更新为: " << vocabularyBtn->toolTip();
    EXPECT_TRUE(vocabularyBtn->toolTip().contains("添加到生词本"))
        << "从生词本移除后工具提示应更新";
    QTest::qWait(2000);

    qDebug() << "\n=== 测试7: 更多操作按钮测试 ===";

    QSignalSpy editSpy(card, &InteractiveWordCard::editRequested);
    QSignalSpy shareSpy(card, &InteractiveWordCard::shareRequested);

    qDebug() << "模拟点击更多操作按钮...";

    // 查找更多操作按钮
    QPushButton* moreBtn = nullptr;
    for (auto btn : favoriteButtons) {
        if (btn->toolTip().contains("更多操作")) {
            moreBtn = btn;
            break;
        }
    }

    ASSERT_NE(moreBtn, nullptr) << "应能找到更多操作按钮";

    // 点击更多操作按钮弹出菜单
    QTest::mouseClick(moreBtn, Qt::LeftButton);
    QApplication::processEvents();
    QTest::qWait(500);

    // 查找菜单
    QMenu* menu = window.findChild<QMenu*>();
    if (menu) {
        qDebug() << "找到更多操作菜单";

        // 测试菜单项数量
        QList<QAction*> actions = menu->actions();
        qDebug() << "菜单项数量: " << actions.size();
        EXPECT_GE(actions.size(), 3) << "菜单应至少有3个选项";

        // 查找编辑操作
        QAction* editAction = nullptr;
        for (QAction* action : actions) {
            if (action->text().contains("编辑")) {
                editAction = action;
                break;
            }
        }

        ASSERT_NE(editAction, nullptr) << "应能找到编辑菜单项";

        // 触发编辑操作
        qDebug() << "模拟点击编辑菜单项...";
        editAction->trigger();
        QApplication::processEvents();
        QTest::qWait(1000);

        EXPECT_EQ(editSpy.count(), 1) << "编辑信号应触发一次";

        if (editSpy.count() > 0) {
            QList<QVariant> arguments = editSpy.takeFirst();
            WordData data = arguments.at(0).value<WordData>();

            EXPECT_EQ(data.word, "Hello") << "编辑信号应传递正确的单词数据";
        }

        // 关闭菜单
        menu->close();
    } else {
        qWarning() << "未找到更多操作菜单，跳过菜单测试";
    }
    QTest::qWait(2000);

    qDebug() << "\n=== 测试8: 主题切换测试 ===";

    qDebug() << "初始主题: " << (card->uiTheme() == UITheme::Light ? "亮色" : "暗色");

    // 切换为暗色主题
    card->setUITheme(UITheme::Dark);
    QApplication::processEvents();
    QTest::qWait(1000);

    EXPECT_EQ(card->uiTheme(), UITheme::Dark) << "主题应切换为暗色";
    qDebug() << "切换到暗色主题";
    QTest::qWait(2000);

    // 切换回亮色主题
    card->setUITheme(UITheme::Light);
    QApplication::processEvents();
    QTest::qWait(1000);

    EXPECT_EQ(card->uiTheme(), UITheme::Light) << "主题应切换回亮色";
    qDebug() << "切换回亮色主题";
     QTest::qWait(2000);

    qDebug() << "\n=== 测试10: 数据更新测试 ===";

    WordData newData("Beautiful", "/ˈbjuːtɪfl/",
                     "adj. 美丽的；美好的",
                     "adj.",
                     "She has a beautiful voice.");

    card->setWordData(newData);
    QApplication::processEvents();
    QTest::qWait(1000);

    EXPECT_EQ(card->getWordData().word, "Beautiful") << "单词数据应更新";

    // 验证状态重置
    EXPECT_FALSE(card->isFavorite()) << "更新数据后收藏状态应重置";
    EXPECT_FALSE(card->isAddedToVocabulary()) << "更新数据后生词本状态应重置";
     QTest::qWait(2000);
}
