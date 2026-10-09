#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include<QtTest/QTest>
#include<QtTest/QSignalSpy>
#include<QThread>
#include"ui/word/word_card.h"
using namespace testing;

/**
 * @brief WordCard GoogleTest 测试
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

/**
 * @brief 测试 WordCard 的基础功能
 */
TEST(UiTest, UiWordCardTest) {
    // 测试默认构造函数
    WordCard card1;
    EXPECT_TRUE(card1.getWordData().word.isEmpty());
    EXPECT_FALSE(card1.isExpanded());

    // 测试带数据的构造函数
    WordData data("Hello", "/həˈləʊ/", "int. 你好", "int.", "Hello!");
    WordCard card2(data);

    EXPECT_EQ(card2.getWordData().word, "Hello");
    EXPECT_EQ(card2.getWordData().phonetic, "/həˈləʊ/");
    EXPECT_EQ(card2.getWordData().definition, "int. 你好");
    EXPECT_EQ(card2.getWordData().partOfSpeech, "int.");
    EXPECT_EQ(card2.getWordData().example, "Hello!");
    EXPECT_FALSE(card2.isExpanded());
}

TEST(UiTest, SetWordDataTest) {
    WordCard card;
    WordData data("Test", "/test/", "测试", "n.", "Test example");

    card.setWordData(data);

    EXPECT_EQ(card.getWordData().word, "Test");
    EXPECT_EQ(card.getWordData().definition, "测试");
}

TEST(UiTest, ExpandCollapseTest) {
    WordCard card;

    // 初始状态应该是折叠的
    EXPECT_FALSE(card.isExpanded());

    // 设置为展开
    card.setExpanded(true);
    EXPECT_TRUE(card.isExpanded());

    // 设置为折叠
    card.setExpanded(false);
    EXPECT_FALSE(card.isExpanded());
}

TEST(UiTest, SizeHintTest) {
    WordCard card;
    QSize size = card.sizeHint();
    QSize minSize = card.minimumSizeHint();

    // 检查返回的尺寸是否有效
    EXPECT_TRUE(size.isValid());
    EXPECT_TRUE(minSize.isValid());

    // 最小尺寸应该小于等于建议尺寸
    EXPECT_LE(minSize.width(), size.width());
    EXPECT_LE(minSize.height(), size.height());
}

TEST(UiTest, ThemeTest) {
    WordCard card;

    // 测试默认主题
    EXPECT_EQ(card.uiTheme(), UITheme::Light);

    // 测试主题切换
    card.setUITheme(UITheme::Dark);
    EXPECT_EQ(card.uiTheme(), UITheme::Dark);

    card.setUITheme(UITheme::Light);
    EXPECT_EQ(card.uiTheme(), UITheme::Light);
}

TEST(UiTest, InteractiveTest)
{
    // 确保有 QApplication 实例
    ASSERT_NE(QApplication::instance(), nullptr);

    qDebug() << "\n开始 WordCard 交互式测试...";

    // 创建主窗口
    QWidget window;
    window.setWindowTitle("WordCard 交互式测试");
    window.resize(500, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(&window);

    qDebug() << "=== 测试场景 1: 创建 WordCard 并显示基本数据 ===";

    // 创建 WordCard
    WordData data("Hello", "/həˈləʊ/",
                  "int. 你好；喂\n"
                  "1. 问候语，用于打招呼\n"
                  "2. 电话用语，表示接听",
                  "int.",
                  "Hello, how are you today?");

    WordCard card(data, &window);
    mainLayout->addWidget(&card);

    // 显示窗口
    window.show();
    window.raise();
    window.activateWindow();

    // 处理窗口显示事件
    QApplication::processEvents();

    // 等待窗口完全显示
    QTest::qWait(3000);

    qDebug() << "单词: " << card.getWordData().word;
    qDebug() << "音标: " << card.getWordData().phonetic;
    qDebug() << "词性: " << card.getWordData().partOfSpeech;
    qDebug() << "释义: " << card.getWordData().definition;
    qDebug() << "例句: " << card.getWordData().example;

    qDebug() << "=== 测试场景 2: 测试点击和双击事件 ===";

    // 连接点击信号
    QSignalSpy clickSpy(&card, &WordCard::clicked);
    QSignalSpy doubleClickSpy(&card, &WordCard::doubleClicked);

    // 处理事件，确保信号连接生效
    QApplication::processEvents();
    QTest::qWait(100);

    qDebug() << "信号连接完成，准备模拟鼠标事件...";

    // 获取卡片的可见矩形
    QRect cardRect = card.rect();
    QPoint clickPoint(cardRect.width() / 2, cardRect.height() / 2);

    // 模拟点击
    qDebug() << "模拟点击...";
    QTest::mouseClick(&card, Qt::LeftButton, Qt::NoModifier, clickPoint);

    // 处理点击事件
    QApplication::processEvents();
    QTest::qWait(200);

    // 验证信号
    qDebug() << "点击信号次数: " << clickSpy.count();
    qDebug() << "双击信号次数: " << doubleClickSpy.count();

    // 点击信号应该是1
    if (clickSpy.count() > 1) {
        qDebug() << "警告: 检测到" << clickSpy.count() << "次点击信号，但只模拟了1次点击";
    }

    // 模拟双击
    qDebug() << "模拟双击...";
    QTest::mouseDClick(&card, Qt::LeftButton, Qt::NoModifier, clickPoint);

    // 处理双击事件
    QApplication::processEvents();
    QTest::qWait(200);

    // 重新读取信号计数
    qDebug() << "点击信号次数: " << clickSpy.count();
    qDebug() << "双击信号次数: " << doubleClickSpy.count();
    EXPECT_GE(clickSpy.count(), 1) << "点击信号应该至少触发1次";
    EXPECT_EQ(doubleClickSpy.count(), 1) << "双击信号应该触发1次";

    // 测试双击展开/折叠
    qDebug() << "展开状态: " << card.isExpanded();
    QTest::mouseDClick(&card, Qt::LeftButton);
    QTest::qWait(200);
    qDebug() << "双击后展开状态: " << card.isExpanded();

    qDebug() << "=== 测试场景 3: 测试数据更新 ===";

    // 测试更新数据
    WordData newData("Beautiful", "/ˈbjuːtɪfl/",
                     "adj. 美丽的；美好的\n"
                     "1. 形容外观吸引人的\n"
                     "2. 形容品质优秀的",
                     "adj.",
                     "She has a beautiful voice.");

    card.setWordData(newData);
    QTest::qWait(1000);

    qDebug() << "更新后单词: " << card.getWordData().word;
    qDebug() << "更新后释义: " << card.getWordData().definition;

    qDebug() << "=== 测试场景 4: 测试主题切换 ===";

    // 记录当前主题
    UITheme initialTheme = card.uiTheme();
    qDebug() << "当前主题: " << (initialTheme == UITheme::Light ? "亮色" : "暗色");

    // 切换主题
    card.setUITheme(UITheme::Dark);
    QTest::qWait(1000);
    qDebug() << "切换到暗色主题";

    // 再次切换
    card.setUITheme(UITheme::Light);
    QTest::qWait(1000);
    qDebug() << "切换回亮色主题";

    qDebug() << "=== 测试场景 5: 测试消息功能 ===";

    // 测试普通消息
    card.showMessage("这是一条普通消息", false, 2000);
    QTest::qWait(2500);

    // 测试错误消息
    card.showMessage("这是一条错误消息！", true, 2000);
    QTest::qWait(2500);

    // 清除消息
    card.clearMessage();
    QTest::qWait(1000);

    qDebug() << "=== 测试场景 6: 测试加载指示器 ===";

    // 显示加载
    card.showLoading("正在加载单词数据...");
    QTest::qWait(2000);

    // 显示完成消息
    card.hideLoading();
    card.showMessage("数据加载完成！", false, 2000);
    QTest::qWait(2500);

    qDebug() << "=== 测试场景 7: 测试边界情况 ===";

    // 测试空数据
    WordData emptyData;
    WordCard emptyCard(emptyData, &window);
    mainLayout->addWidget(&emptyCard);
    QTest::qWait(2000);

    qDebug() << "空数据卡片是否有效: " << emptyCard.getWordData().isValid();

    // 测试只有部分数据
    WordData partialData("Test", "", "n. 测试", "", "");
    WordCard partialCard(partialData, &window);
    mainLayout->addWidget(&partialCard);
    QTest::qWait(1000);

    qDebug() << "部分数据卡片单词: " << partialCard.getWordData().word;
    qDebug() << "部分数据卡片音标: " << partialCard.getWordData().phonetic;

    qDebug() << "=== 测试场景 8: 测试多卡片 ===";

    // 创建多个卡片
    // QWidget cardsWidget;
    // QVBoxLayout* cardsLayout = new QVBoxLayout(&cardsWidget);

    // 使用堆分配，避免栈变量的自动销毁
    QWidget* cardsWidget = new QWidget();
    QVBoxLayout* cardsLayout = new QVBoxLayout(cardsWidget);

    QVector<WordData> testWords = {
        WordData("Apple", "/ˈæpl/", "n. 苹果", "n.", "I eat an apple every day."),
        WordData("Run", "/rʌn/", "v. 跑；运行", "v.", "I run every morning."),
        WordData("Book", "/bʊk/", "n. 书", "n.", "This is a good book.")
    };

    for (int i = 0; i < testWords.size(); i++) {
        WordCard* multiCard = new WordCard(testWords[i],cardsWidget);
        cardsLayout->addWidget(multiCard);

        // 为每个卡片设置不同的展开状态
        if (i % 2 == 0) {
            multiCard->setExpanded(true);
        }
    }

    // 创建一个新的窗口显示多个卡片
    QWidget* multiWindow = new QWidget();
    multiWindow->setWindowTitle("多卡片测试");
    multiWindow->resize(400, 500);

    QVBoxLayout* multiLayout = new QVBoxLayout(multiWindow);
    multiLayout->addWidget(cardsWidget);
    multiWindow->show();

    QTest::qWait(2000);

    // 切换多窗口的主题
    for (auto card : multiWindow->findChildren<WordCard*>()) {
        card->setUITheme(UITheme::Dark);
    }
    QTest::qWait(1000);

    for (auto card : multiWindow->findChildren<WordCard*>()) {
        card->setUITheme(UITheme::Light);
    }
    QTest::qWait(1000);

    qDebug() << "=== 测试场景 9: 测试尺寸自适应 ===";

    // 测试不同尺寸
    qDebug() << "卡片的建议尺寸: " << card.sizeHint();
    qDebug() << "卡片的最小建议尺寸: " << card.minimumSizeHint();

    // 测试长单词
    WordData longWordData("Pneumonoultramicroscopicsilicovolcanoconiosis",
                          "/ˌnjuːmənoʊˌʌltrəˌmaɪkrəˈskɒpɪkˌsɪlɪkoʊvɒlˌkeɪnoʊˌkoʊniˈoʊsɪs/",
                          "n. 肺尘病，硅酸盐沉着病",
                          "n.",
                          "A lung disease caused by inhaling very fine silicate or quartz dust.");

    WordCard longWordCard(longWordData, &window);
    mainLayout->addWidget(&longWordCard);
    QTest::qWait(1000);

    qDebug() << "长单词卡片的建议尺寸: " << longWordCard.sizeHint();

    // 测试长释义
    QString longDefinition = "这是一个非常长的释义，";
    for (int i = 0; i < 10; i++) {
        longDefinition += "这一行是为了测试多行文本显示功能。";
    }

    WordData longDefData("Test", "/test/", longDefinition, "n.", "");
    WordCard longDefCard(longDefData, &window);
    mainLayout->addWidget(&longDefCard);
    QTest::qWait(1000);

    qDebug() << "长释义卡片的建议尺寸: " << longDefCard.sizeHint();

    qDebug() << "=== 测试场景 10: 状态验证 ===";

    // 验证最终状态
    qDebug() << "最终主题: " << (card.uiTheme() == UITheme::Light ? "亮色" : "暗色");
    qDebug() << "最终状态: " << (card.uiState() == UIState::Normal ? "正常" :
                                     card.uiState() == UIState::Loading ? "加载" : "错误");
    qDebug() << "最终展开状态: " << card.isExpanded();

    EXPECT_TRUE(card.getWordData().isValid()) << "WordCard 应包含有效数据";
    EXPECT_EQ(card.uiTheme(), UITheme::Light) << "WordCard 最终应为亮色主题";
    EXPECT_EQ(card.uiState(), UIState::Normal) << "WordCard 最终应为正常状态";

    qDebug() << "交互式测试完成，窗口即将关闭...";
    QTest::qWait(2000);

    qDebug() << "=== 测试完成 ===";

    // 清理
    window.close();
    // 清理：手动删除窗口
    multiWindow->close();
    multiWindow->deleteLater();  // 安全删除
    cardsWidget->deleteLater();  // 由于是子对象，可以不单独删除
    QTest::qWait(200);
}

