#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QApplication>
#include<QtTest/QTest>
#include<QtTest/QSignalSpy>
#include<QThread>
#include<QSettings>
#include"ui/search/search_base.h"
#include"ui/word/interactive_wordcard.h"
#include"ui/search/searchhistory_widget.h"
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

TEST(UiTest, SearchHistoryWidgetTest)
{
    // 确保有 QApplication 实例
    ASSERT_NE(QApplication::instance(), nullptr);

    qDebug() << "\n=== 开始 SearchHistoryWidget 交互测试 ===";

    // 创建主窗口
    BaseWidget window;
    window.setWindowTitle("SearchHistoryWidget 交互测试");
    window.setStyleSheet("background-color: #f8fafc;");
    window.resize(600, 600);

    window.mainLayout()->setContentsMargins(20, 20, 20, 20);
    window.mainLayout()->setSpacing(20);

    qDebug() << "=== 测试1: 创建 SearchHistoryWidget ===";

    SearchHistoryWidget* searchWidget = new SearchHistoryWidget(&window);
    window.mainLayout()->addWidget(searchWidget);

    // 显示窗口
    window.show();
    QApplication::processEvents();
    QTest::qWait(1000);

    qDebug() << "SearchHistoryWidget 创建成功";
    qDebug() << "初始历史记录数量:" << searchWidget->historyCount();
    qDebug() << "最大历史记录数:" << searchWidget->maxHistoryCount();

    EXPECT_EQ(searchWidget->historyCount(), 0) << "初始历史记录应为空";
    QTest::qWait(3000);

    qDebug() << "=== 测试2: 添加历史记录 ===";

    // 添加几个搜索历史
    QStringList testHistory = {"Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig"};

    for (int i = 0; i < testHistory.size(); i++) {
        QString word = testHistory[i];
        searchWidget->addHistory(word);
        qDebug() << "添加历史记录" << (i+1) << ":" << word;

        QApplication::processEvents();
        QTest::qWait(100);
    }

    qDebug() << "添加后历史记录数量:" << searchWidget->historyCount();
    qDebug() << "历史记录列表:" << searchWidget->getHistory();

    // 默认最大历史记录数是5，所以应该有5条记录
    EXPECT_EQ(searchWidget->historyCount(), 5) << "应只保留5条历史记录（默认最大数）";
    EXPECT_EQ(searchWidget->getHistory().first(), "Fig") << "最新的记录应在最前面";
    EXPECT_EQ(searchWidget->getHistory().last(), "Banana") << "最旧的记录应在最后面";
    QTest::qWait(3000);

    qDebug() << "=== 测试3: 历史记录去重测试 ===";

    // 添加重复的记录
    searchWidget->addHistory("Apple");
    QApplication::processEvents();
    QTest::qWait(200);

    qDebug() << "重复添加Apple后历史记录:" << searchWidget->getHistory();
    EXPECT_EQ(searchWidget->historyCount(), 5) << "去重后应仍然只有5条记录";
    EXPECT_EQ(searchWidget->getHistory().first(), "Apple") << "重复添加的记录应更新到最前面";
    QTest::qWait(3000);

    qDebug() << "=== 测试4: 设置最大历史记录数 ===";

    // 修改最大历史记录数
    searchWidget->setMaxHistoryCount(3);
    QApplication::processEvents();
    QTest::qWait(200);

    qDebug() << "设置最大历史记录数为3后:" << searchWidget->getHistory();
    EXPECT_EQ(searchWidget->historyCount(), 3) << "应只保留3条历史记录";
    EXPECT_EQ(searchWidget->maxHistoryCount(), 3) << "最大历史记录数应更新为3";

    // 恢复默认
    searchWidget->setMaxHistoryCount(5);
     QTest::qWait(3000);

    qDebug() << "=== 测试5: 焦点显示历史记录测试 ===";

    // 确保输入框为空
    searchWidget->clearInput();
    QApplication::processEvents();
    QTest::qWait(500);

    // 获得焦点
    qDebug() << "模拟输入框获得焦点...";
    searchWidget->setFocusToInput();
    QApplication::processEvents();
    QTest::qWait(1000);

    // 检查历史记录是否显示
    bool historyVisible = searchWidget->isHistoryVisible();
    qDebug() << "历史记录列表是否可见:" << historyVisible;

    // 保持焦点，防止自动隐藏
    if (historyVisible) {
        // 模拟用户移动鼠标到搜索框内
        QTest::mouseMove(searchWidget->inputField());
        QApplication::processEvents();
        QTest::qWait(500);

        // 再次检查
        historyVisible = searchWidget->isHistoryVisible();
        qDebug() << "移动鼠标后历史记录列表是否可见:" << historyVisible;
    }
    QTest::qWait(1000);

    qDebug() << "=== 测试6: 点击历史记录项测试 ===";

    QSignalSpy searchSpy(searchWidget, &SearchBase::searchRequested);
    QSignalSpy historySpy(searchWidget, &SearchHistoryWidget::historyItemClicked);

    if (searchWidget->isHistoryVisible()) {
        // 使用新增的方法获取历史列表
        QListWidget* historyList = searchWidget->getHistoryList();
        QWidget* historyContainer = searchWidget->getHistoryContainer();

        if (historyList && historyContainer && historyContainer->isVisible()) {
            qDebug() << "找到历史列表，项目数:" << historyList->count();

            if (historyList->count() > 0) {
                // 获取第一个历史项
                QListWidgetItem* firstItem = historyList->item(0);
                QString firstHistory = firstItem->text();
                qDebug() << "点击历史记录:" << firstHistory;

                // 确保列表可见并且有内容
                if (historyList->isVisible() && firstItem) {
                    // 计算点击位置
                    QRect itemRect = historyList->visualItemRect(firstItem);
                    QPoint clickPos = itemRect.center();

                    qDebug() << "历史列表可见区域:" << historyList->rect();
                    qDebug() << "历史项矩形:" << itemRect;
                    qDebug() << "点击位置:" << clickPos;

                    // 确保点击位置在可见区域内
                    if (historyList->rect().contains(clickPos)) {
                        // 模拟点击历史项
                        QTest::mouseClick(historyList->viewport(), Qt::LeftButton,
                                          Qt::NoModifier, clickPos);
                        QApplication::processEvents();
                        QTest::qWait(1000);

                        // 验证结果
                        qDebug() << "搜索信号触发次数:" << searchSpy.count();
                        qDebug() << "历史项点击信号触发次数:" << historySpy.count();

                        if (searchSpy.count() > 0) {
                            QList<QVariant> arguments = searchSpy.takeFirst();
                            QString keyword = arguments.at(0).toString();
                            qDebug() << "搜索信号关键词:" << keyword;
                            EXPECT_EQ(keyword, firstHistory) << "搜索信号应传递正确的历史关键词";
                        } else {
                            qDebug() << "搜索信号未触发";
                        }

                        if (historySpy.count() > 0) {
                            QList<QVariant> arguments = historySpy.takeFirst();
                            QString keyword = arguments.at(0).toString();
                            EXPECT_EQ(keyword, firstHistory) << "历史项点击信号应传递正确的关键词";
                        } else {
                            qDebug() << "历史项点击信号未触发";
                        }

                        // 检查输入框是否填充了历史关键词
                        QString inputText = searchWidget->inputField()->text();
                        qDebug() << "输入框内容:" << inputText;
                        EXPECT_EQ(inputText, firstHistory) << "点击历史项后输入框应填充对应的关键词";

                        // 检查历史列表是否隐藏
                        bool historyVisibleAfterClick = searchWidget->isHistoryVisible();
                        qDebug() << "点击后历史列表是否可见:" << historyVisibleAfterClick;

                        if (!historyVisibleAfterClick) {
                            qDebug() << "✅ 点击后历史列表正确隐藏";
                        } else {
                            qDebug() << "⚠️ 点击后历史列表未隐藏";
                        }
                    } else {
                        qDebug() << "点击位置不在可见区域内";
                    }
                } else {
                    qDebug() << "历史列表不可见或没有历史项";
                }
            } else {
                qDebug() << "历史列表为空";
            }
        } else {
            qDebug() << "未找到历史列表或历史容器";
            qDebug() << "历史列表指针:" << historyList;
            qDebug() << "历史容器指针:" << historyContainer;
            if (historyContainer) {
                qDebug() << "历史容器可见性:" << historyContainer->isVisible();
            }
        }
    } else {
        qDebug() << "历史列表不可见，跳过点击测试";
    }
     QTest::qWait(1000);

    qDebug() << "=== 测试7: 清除历史记录测试 ===";

    QSignalSpy clearSpy(searchWidget, &SearchHistoryWidget::historyCleared);

    // 先添加一条历史记录
    searchWidget->addHistory("TestClear");
    QApplication::processEvents();
    QTest::qWait(200);

    int beforeClearCount = searchWidget->historyCount();
    qDebug() << "清除前历史记录数量:" << beforeClearCount;

    // 清除历史记录
    searchWidget->clearHistory();
    QApplication::processEvents();
    QTest::qWait(500);

    int afterClearCount = searchWidget->historyCount();
    qDebug() << "清除后历史记录数量:" << afterClearCount;
    qDebug() << "清除信号触发次数:" << clearSpy.count();

    EXPECT_EQ(afterClearCount, 0) << "清除后历史记录应为空";
    EXPECT_EQ(clearSpy.count(), 1) << "应触发历史清除信号";
     QTest::qWait(1000);

    qDebug() << "=== 测试9: 持久化测试（保存/加载）===";

     // 设置 QSettings 配置
     QCoreApplication::setOrganizationName("DictionaryApp");
     QCoreApplication::setApplicationName("Dictionary");

     // 1. 准备测试数据
     qDebug() << "步骤1: 准备测试数据";
     QStringList testWords = {"历史记录1", "历史记录2", "历史记录3"};

     // 按顺序添加测试数据
     for (const QString& word : testWords) {
         searchWidget->addHistory(word);
         QApplication::processEvents();
         QTest::qWait(10);  // 添加微小延迟确保时间戳不同
     }

     // 验证添加后的顺序（最新的在最前）
     QStringList savedHistory = searchWidget->getHistory();
     qDebug() << "添加后历史记录顺序（最新的在最前）:" << savedHistory;
     qDebug() << "添加后历史记录数量:" << searchWidget->historyCount();

     // 预期顺序：["历史记录3", "历史记录2", "历史记录1"]
     EXPECT_EQ(savedHistory.size(), 3) << "应有3条历史记录";
     if (savedHistory.size() == 3) {
         EXPECT_EQ(savedHistory[0], "历史记录3") << "最新记录应显示在最前面";
         EXPECT_EQ(savedHistory[1], "历史记录2") << "第二条记录位置错误";
         EXPECT_EQ(savedHistory[2], "历史记录1") << "最旧记录应显示在最后面";
     }

     // 2. 保存历史记录
     qDebug() << "\n步骤2: 保存历史记录";
     QString testStorageKey = "test_persistence";
     searchWidget->saveHistory(testStorageKey);
     qDebug() << "历史记录已保存";

     // 验证保存成功
     QSettings settings;
     QString storageKey = QString("SearchHistory/%1").arg(testStorageKey);
     EXPECT_TRUE(settings.contains(storageKey)) << "应在设置中找到保存的键";

     // 3. 清除当前历史记录
     qDebug() << "\n步骤3: 清除当前历史记录";
     searchWidget->clearHistory();
     EXPECT_EQ(searchWidget->historyCount(), 0) << "清除后历史记录应为空";
     qDebug() << "清除后历史记录数量:" << searchWidget->historyCount();

     // 4. 加载历史记录
     qDebug() << "\n步骤4: 加载历史记录";
     searchWidget->loadHistory(testStorageKey);
     QApplication::processEvents();
     QTest::qWait(200);

     // 5. 验证加载结果
     QStringList loadedHistory = searchWidget->getHistory();
     qDebug() << "加载后历史记录顺序:" << loadedHistory;
     qDebug() << "加载后历史记录数量:" << searchWidget->historyCount();

     // 验证数量和顺序
     EXPECT_EQ(loadedHistory.size(), savedHistory.size())
         << "加载后应恢复相同数量的历史记录";

     if (loadedHistory.size() == savedHistory.size()) {
         // 验证每一条记录的顺序和内容
         bool allMatch = true;
         for (int i = 0; i < savedHistory.size(); i++) {
             if (loadedHistory[i] != savedHistory[i]) {
                 qDebug() << "❌ 位置" << i << "不匹配:"
                          << "期望:" << savedHistory[i]
                          << "实际:" << loadedHistory[i];
                 allMatch = false;
             } else {
                 qDebug() << "✅ 位置" << i << "匹配:"
                          << loadedHistory[i];
             }
         }

         if (allMatch) {
             qDebug() << "\n✅ 持久化测试通过：记录数量和顺序完全一致";
         } else {
             qDebug() << "\n❌ 持久化测试失败：记录顺序不一致";
             ADD_FAILURE() << "加载的历史记录顺序与保存的顺序不一致";
         }
     } else {
         qDebug() << "\n❌ 持久化测试失败：记录数量不一致";
         ADD_FAILURE() << "加载的历史记录数量与保存的数量不一致";
     }

     // 6. 清理测试数据
     qDebug() << "\n步骤6: 清理测试数据";
     settings.remove(storageKey);
     EXPECT_FALSE(settings.contains(storageKey)) << "清理后应不再包含测试键";
     qDebug() << "测试数据清理完成";

     // 7. 额外测试：验证逆序特性
     qDebug() << "\n步骤7: 额外测试 - 验证最新记录在最前的特性";

     // 清除并重新添加，验证顺序
     searchWidget->clearHistory();
     QApplication::processEvents();
     QTest::qWait(100);

     // 添加新的测试数据
     QStringList newTestWords = {"A", "B", "C", "D", "E"};
     for (const QString& word : newTestWords) {
         searchWidget->addHistory(word);
         QApplication::processEvents();
         QTest::qWait(10);
     }

     QStringList currentHistory = searchWidget->getHistory();
     qDebug() << "添加顺序: A, B, C, D, E";
     qDebug() << "实际显示顺序:" << currentHistory;

     // 验证最新的在最前
     if (!currentHistory.isEmpty()) {
         EXPECT_EQ(currentHistory.first(), "E") << "最新添加的记录应在最前面";
         EXPECT_EQ(currentHistory.last(), "A") << "最早添加的记录应在最后面";

         qDebug() << "✅ 最新记录验证:";
         qDebug() << "  最新记录:" << currentHistory.first() << "(应为E)";
         qDebug() << "  最旧记录:" << currentHistory.last() << "(应为A)";
     }

     qDebug() << "\n=== 测试9: 持久化测试完成 ===";
     QTest::qWait(1000);

     qDebug() << "=== 测试10: 输入时隐藏历史记录测试 ===";

     // 清除输入框
     searchWidget->clearInput();
     QApplication::processEvents();
     QTest::qWait(200);

     // 获得焦点显示历史记录
     searchWidget->setFocusToInput();
     QApplication::processEvents();
     QTest::qWait(1000);

     bool visibleBeforeInput = searchWidget->isHistoryVisible();
     qDebug() << "输入前历史记录可见:" << visibleBeforeInput;

     // 开始输入
     searchWidget->inputField()->setText("T");
     QApplication::processEvents();
     QTest::qWait(500);

     bool visibleDuringInput = searchWidget->isHistoryVisible();
     qDebug() << "输入时历史记录可见:" << visibleDuringInput;

     // 注意：根据实现，输入时历史记录应该隐藏
     if (visibleBeforeInput && !visibleDuringInput) {
         qDebug() << "✅ 输入时历史记录正确隐藏";
     }

     // 清空输入，历史记录应重新显示
     searchWidget->clearInput();
     QApplication::processEvents();
     QTest::qWait(500);

     bool visibleAfterClear = searchWidget->isHistoryVisible();
     qDebug() << "清空后历史记录可见:" << visibleAfterClear;
      QTest::qWait(1000);

     qDebug() << "=== 测试11: 主题切换测试 ===";

     UITheme initialTheme = searchWidget->uiTheme();
     qDebug() << "初始主题:" << (initialTheme == UITheme::Light ? "亮色" : "暗色");

     // 切换为暗色主题
     searchWidget->setUITheme(UITheme::Dark);
     window.setUITheme(UITheme::Dark);
     QApplication::processEvents();
     QTest::qWait(1000);

     EXPECT_EQ(searchWidget->uiTheme(), UITheme::Dark) << "主题应切换为暗色";
     qDebug() << "切换到暗色主题";

     // 切换回亮色主题
     searchWidget->setUITheme(UITheme::Light);
     window.setUITheme(UITheme::Light);
     QApplication::processEvents();
     QTest::qWait(1000);

     EXPECT_EQ(searchWidget->uiTheme(), UITheme::Light) << "主题应切换回亮色";
     qDebug() << "切换回亮色主题";
    QTest::qWait(1000);

     qDebug() << "=== 测试12: 与InteractiveWordCard联动测试 ===";

     // 创建卡片展示区域
     QWidget* cardArea = new QWidget(&window);
     QVBoxLayout* cardLayout = new QVBoxLayout(cardArea);
     cardLayout->setContentsMargins(0, 0, 0, 0);
     window.mainLayout()->addWidget(cardArea, 1);

     // 清空之前的内容
     QLayoutItem* child;
     while ((child = cardLayout->takeAt(0)) != nullptr) {
         delete child->widget();
         delete child;
     }

     // 通过搜索历史进行搜索
     QString searchWord = "Dictionary";
     searchWidget->setKeyword(searchWord);
     QApplication::processEvents();
     QTest::qWait(500);

     qDebug() << "搜索单词:" << searchWord;

     // 模拟搜索
     searchWidget->doSearch();
     QApplication::processEvents();
     QTest::qWait(1000);

     // 创建对应的单词卡片
     WordData cardData(
         searchWord,
         "/ˈdɪkʃəneri/",
         "n. 词典；字典\n"
         "1. 收录词汇并解释其含义的参考书\n"
         "2. 特定领域术语的汇编",
         "n.",
         "I need to look up this word in the dictionary."
         );

     InteractiveWordCard* resultCard = new InteractiveWordCard(cardData, cardArea);
     cardLayout->addWidget(resultCard);

     QApplication::processEvents();
     QTest::qWait(2000);

     qDebug() << "单词卡片已创建";
     qDebug() << "卡片单词:" << resultCard->getWordData().word;

     // 验证卡片显示
     EXPECT_EQ(resultCard->getWordData().word, searchWord) << "卡片应显示搜索的单词";
     EXPECT_TRUE(resultCard->isVisible()) << "卡片应可见";

     // 检查历史记录是否已添加
     bool wordInHistory = searchWidget->getHistory().contains(searchWord);
     qDebug() << "搜索词是否在历史记录中:" << wordInHistory;

     if (wordInHistory) {
         qDebug() << "✅ 搜索词已添加到历史记录";
     } else {
         qDebug() << "⚠️ 搜索词未添加到历史记录";
     }
     QTest::qWait(2000);

     qDebug() << "=== 测试13: 多单词搜索历史演示 ===";

     // 演示多个单词搜索
     QStringList demoWords = {"Apple", "Banana", "Cherry", "Date", "Elderberry"};

     for (int i = 0; i < demoWords.size(); i++) {
         QString word = demoWords[i];
         qDebug() << "--- 搜索演示" << (i+1) << ":" << word << "---";

         // 搜索单词
         searchWidget->setKeyword(word);
         QApplication::processEvents();
         QTest::qWait(300);

         // 模拟搜索
         searchWidget->doSearch();
         QApplication::processEvents();
         QTest::qWait(500);

         // 更新卡片
         WordData newData(
             word,
             QString("/demo/%1/").arg(word.toLower()),
             QString("n. 这是一个演示单词: %1").arg(word),
             "n.",
             QString("This is an example sentence for %1.").arg(word)
             );

         resultCard->setWordData(newData);
         QApplication::processEvents();
         QTest::qWait(500);
     }

     qDebug() << "最终历史记录:" << searchWidget->getHistory();
     qDebug() << "最终历史记录数量:" << searchWidget->historyCount();
      QTest::qWait(2000);
}
