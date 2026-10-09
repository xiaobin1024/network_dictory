#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include<QtTest/QTest>
#include<QtTest/QSignalSpy>
#include<QThread>
#include"ui/search/search_base.h"
#include"ui/word/interactive_wordcard.h"
using namespace testing;

/**
 * @brief SearchBase GoogleTest 测试
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

TEST(UiTest,SearchBaseTest)
{
    // 确保有 QApplication 实例
    ASSERT_NE(QApplication::instance(), nullptr);

    qDebug() << "\n=== 开始 SearchBase 交互测试 ===";

    // 创建主窗口
    BaseWidget window;
    window.setWindowTitle("SearchBase 交互测试");
    window.resize(600, 500);

    // 创建测试布局
    // QVBoxLayout* windowLayout = new QVBoxLayout(&window);
    // windowLayout->setContentsMargins(20, 20, 20, 20);
    // windowLayout->setSpacing(20);
    window.m_mainLayout->setContentsMargins(20,20,20,20);
    window.m_mainLayout->setSpacing(20);

    qDebug() << "=== 测试1: 创建 SearchBase ===";

    //先创建 SearchBase 但不添加到布局
    SearchBase* searchBar = new SearchBase();
    qDebug() << "SearchBase 创建完成";

    // 检查 SearchBase 的布局
    qDebug() << "SearchBase 布局:" << searchBar->layout();
    qDebug() << "SearchBase 子控件数量:" << searchBar->children().size();

    // 添加到窗口布局
    window.m_mainLayout->addWidget(searchBar);

    // 显示窗口
    window.show();
    QApplication::processEvents();
    QTest::qWait(1000);
    // 验证 SearchBase 是否可见
    qDebug() << "SearchBase 可见:" << searchBar->isVisible();
    qDebug() << "SearchBase 尺寸:" << searchBar->size();

    // 验证子控件
    if (searchBar->inputField()) {
        qDebug() << "输入框可见:" << searchBar->inputField()->isVisible();
        qDebug() << "输入框尺寸:" << searchBar->inputField()->size();
    } else {
        qDebug() << "输入框不存在!";
    }
    QTest::qWait(1000);

    qDebug() << "=== 测试2: 输入框基本功能 ===";

    // 测试输入框
    QString testWord = "Hello";
    searchBar->inputField()->setText(testWord);
    QApplication::processEvents();
    QTest::qWait(500);

    qDebug() << "输入框内容:" << searchBar->inputField()->text();
    EXPECT_EQ(searchBar->inputField()->text(), testWord) << "输入框应显示输入内容";
    EXPECT_TRUE(searchBar->clearButton()->isVisible()) << "有内容时清除按钮应显示";
    QTest::qWait(1000);

    qDebug() << "=== 测试3: 清除按钮功能 ===";

    // 清除按钮应可见
    auto clearButton = window.findChild<QPushButton*>("clearButton");
    if (clearButton) {
        qDebug() << "点击清除按钮";
        QTest::mouseClick(clearButton, Qt::LeftButton);
        QApplication::processEvents();
        QTest::qWait(500);

        EXPECT_TRUE(searchBar->inputField()->text().isEmpty()) << "清除后输入框应为空";
        EXPECT_FALSE(searchBar->clearButton()->isVisible()) << "清除后清除按钮应隐藏";
    }
    QTest::qWait(1000);

    qDebug() << "=== 测试4: 搜索信号测试 ===";

    QSignalSpy searchSpy(searchBar, &SearchBase::searchRequested);

    // 输入新单词
    QString searchWord = "Test";
    searchBar->inputField()->setText(searchWord);
    QApplication::processEvents();
    QTest::qWait(500);

    // 点击搜索按钮
    auto searchButton = window.findChild<QPushButton*>("searchButton");
    if (searchButton) {
        qDebug() << "点击搜索按钮";
        QTest::mouseClick(searchButton, Qt::LeftButton);
        QApplication::processEvents();
        QTest::qWait(500);

        qDebug() << "搜索信号次数:" << searchSpy.count();
        EXPECT_EQ(searchSpy.count(), 1) << "应触发搜索信号";

        if (searchSpy.count() > 0) {
            QList<QVariant> arguments = searchSpy.takeFirst();
            QString keyword = arguments.at(0).toString();
            EXPECT_EQ(keyword, searchWord) << "搜索信号应传递正确的关键词";
        }
    }
     QTest::qWait(1000);

    qDebug() << "=== 测试5: 回车键搜索测试 ===";

    // 清空输入框
    searchBar->inputField()->clear();
    QApplication::processEvents();
    QTest::qWait(200);

    // 输入新单词
    QString enterWord = "EnterTest";
    searchBar->inputField()->setText(enterWord);
    QApplication::processEvents();
    QTest::qWait(200);

    // 模拟回车键
    qDebug() << "模拟回车键";
    QTest::keyPress(searchBar->inputField(), Qt::Key_Return);
    QApplication::processEvents();
    QTest::qWait(500);

    EXPECT_GE(searchSpy.count(), 1) << "回车键应触发搜索信号";
    if (searchSpy.count() > 0) {
        QList<QVariant> arguments = searchSpy.takeFirst();
        QString keyword = arguments.at(0).toString();
        EXPECT_EQ(keyword, enterWord) << "回车键搜索应传递正确的关键词";
    }
     QTest::qWait(1000);

    qDebug() << "=== 测试6: 快捷键测试 ===";

    // 测试 Ctrl+F 聚焦
    searchBar->inputField()->clear();
    searchBar->clearInput();

    // 模拟 Ctrl+F
    qDebug() << "测试 Ctrl+F 聚焦快捷键";
    QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
    QApplication::processEvents();
    QTest::qWait(500);

    EXPECT_TRUE(searchBar->inputField()->hasFocus()) << "Ctrl+F 应聚焦到输入框";

    // 测试 Esc 清除
    searchBar->inputField()->setText("EscapeTest");
    QApplication::processEvents();
    QTest::qWait(200);

    qDebug() << "测试 Esc 清除快捷键";
    QTest::keyClick(searchBar->inputField(), Qt::Key_Escape);
    QApplication::processEvents();
    QTest::qWait(500);

    EXPECT_TRUE(searchBar->inputField()->text().isEmpty()) << "Esc 应清除输入框";
     QTest::qWait(1000);

     qDebug() << "=== 测试7: 焦点信号测试 ===";

     // 1. 测试编程式焦点
     qDebug() << "测试编程式设置焦点...";
     searchBar->inputField()->setFocus();
     QApplication::processEvents();
     QTest::qWait(500);

     bool hasFocus1 = searchBar->inputField()->hasFocus();
     qDebug() << "编程式设置后，输入框是否有焦点:" << hasFocus1;
     EXPECT_TRUE(hasFocus1) << "编程式设置焦点应使输入框获得焦点";

     // 2. 测试编程式失去焦点
     qDebug() << "测试编程式清除焦点...";
     searchBar->clearInput();
     QApplication::processEvents();
     QTest::qWait(500);

     // 创建一个按钮来接收焦点
     QPushButton* focusTarget = new QPushButton("测试焦点", &window);
     focusTarget->setFocusPolicy(Qt::StrongFocus);
     focusTarget->move(10, 10);
     focusTarget->show();
     QApplication::processEvents();
     QTest::qWait(200);

     // 点击按钮，而不是窗口空白区域
     QTest::mouseClick(focusTarget, Qt::LeftButton);
     QApplication::processEvents();
     QTest::qWait(1000);

     bool hasFocus2 = searchBar->inputField()->hasFocus();
     bool targetHasFocus = focusTarget->hasFocus();

     qDebug() << "点击按钮后:";
     qDebug() << "  输入框是否有焦点:" << hasFocus2;
     qDebug() << "  按钮是否有焦点:" << targetHasFocus;

     delete focusTarget;
     QTest::qWait(1000);

     qDebug() << "=== 测试8: 主题切换测试 ===";

     UITheme initialTheme = searchBar->uiTheme();
     qDebug() << "初始主题:" << (initialTheme == UITheme::Light ? "亮色" : "暗色");

     // 切换为暗色主题
     window.setUITheme(UITheme::Dark);
     searchBar->setUITheme(UITheme::Dark);
     QApplication::processEvents();
     QTest::qWait(2000);

     EXPECT_EQ(searchBar->uiTheme(), UITheme::Dark) << "主题应切换为暗色";
     qDebug() << "切换到暗色主题";

     // 切换回亮色主题
     window.setUITheme(UITheme::Light);
     searchBar->setUITheme(UITheme::Light);
     QApplication::processEvents();
     QTest::qWait(2000);

     EXPECT_EQ(searchBar->uiTheme(), UITheme::Light) << "主题应切换回亮色";
     qDebug() << "切换回亮色主题";
     QTest::qWait(2000);

     qDebug() << "=== 测试9: 编程式控制测试 ===";

     // 测试 setKeyword
     QString programmaticWord = "Programmatic";
     searchBar->setKeyword(programmaticWord);
     QApplication::processEvents();
     QTest::qWait(500);

     EXPECT_EQ(searchBar->inputField()->text(), programmaticWord) << "setKeyword 应设置输入框内容";

     // 测试 setPlaceholder
     QString placeholderText = "请输入要搜索的单词...";
     searchBar->setPlaceholder(placeholderText);
     QApplication::processEvents();
     QTest::qWait(500);

     EXPECT_EQ(searchBar->inputField()->placeholderText(), placeholderText) << "setPlaceholder 应设置占位符";

     // 测试 doSearch
     int initialSearchCount = searchSpy.count();
     searchBar->doSearch();
     QApplication::processEvents();
     QTest::qWait(500);

     EXPECT_GT(searchSpy.count(), initialSearchCount) << "doSearch 应触发搜索";
      QTest::qWait(2000);

     qDebug() << "=== 测试10: 搜索与卡片联动测试 ===";

     // 创建卡片展示区域
     QWidget* cardArea = new QWidget(&window);
     QVBoxLayout* cardLayout = new QVBoxLayout(cardArea);
     cardLayout->setContentsMargins(0, 0, 0, 0);
     window.m_mainLayout->addWidget(cardArea, 1);

     // 清空之前的内容
     QLayoutItem* child;
     while ((child = cardLayout->takeAt(0)) != nullptr) {
         delete child->widget();
         delete child;
     }

     // 搜索一个单词
     QString searchForCard = "Beautiful";
     searchBar->setKeyword(searchForCard);
     QApplication::processEvents();
     QTest::qWait(500);

     qDebug() << "搜索单词:" << searchForCard;

     // 模拟搜索
     searchBar->doSearch();
     QApplication::processEvents();
     QTest::qWait(1000);

     // 创建对应的单词卡片
     WordData cardData(
         searchForCard,
         "/ˈbjuːtɪfl/",
         "adj. 美丽的；美好的\n"
         "1. 形容外观吸引人的\n"
         "2. 形容品质优秀的",
         "adj.",
         "She has a beautiful voice."
         );

     InteractiveWordCard* resultCard = new InteractiveWordCard(cardData, cardArea);
     cardLayout->addWidget(resultCard);

     QApplication::processEvents();
     QTest::qWait(2000);

     qDebug() << "单词卡片已创建";
     qDebug() << "卡片单词:" << resultCard->getWordData().word;
     qDebug() << "卡片释义:" << resultCard->getWordData().definition;

     // 验证卡片显示
     EXPECT_EQ(resultCard->getWordData().word, searchForCard) << "卡片应显示搜索的单词";
     EXPECT_TRUE(resultCard->isVisible()) << "卡片应可见";
      QTest::qWait(3000);

     // 切换为暗色主题
      window.setUITheme(UITheme::Dark);
     searchBar->setUITheme(UITheme::Dark);
      resultCard->setUITheme(UITheme::Dark);
     QApplication::processEvents();
     QTest::qWait(2000);

}
