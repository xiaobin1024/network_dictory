#include <gtest/gtest.h>
#include<QTest>
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <memory>
#include<QElapsedTimer>
#include "page_manager/page_stack.h"
#include "page_manager/page_types.h"

// 在测试文件中
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        char* argv[] = {nullptr};
        app = new QApplication(argc, argv);
    }

    void TearDown() override {
        delete app;
    }

private:
    QApplication* app;
};


// 注册全局环境
::testing::Environment* const qt_env =
    ::testing::AddGlobalTestEnvironment(new QtTestEnvironment);

// 用于测试的简单页面类
class TestPage : public BaseWidget
{
public:
    explicit TestPage(const QString& name, QWidget* parent = nullptr)
        : BaseWidget(parent)
    {
        setObjectName(name);
    }
};

// 测试夹具
class PageStackTestFixtrue : public ::testing::Test {
protected:
    void SetUp() override {
        m_stack = std::make_unique<PageStack>();
        qDebug() << "测试夹具 SetUp - 创建新的 PageStack";
    }

    void TearDown() override {
        m_stack.reset();
        qDebug() << "测试夹具 TearDown - 清理 PageStack";
    }

    std::unique_ptr<PageStack> m_stack;
};

// ============================================
// 测试1: 创建和销毁测试
// ============================================
TEST_F(PageStackTestFixtrue, CreateAndDestroy) {
    qDebug() << "测试1: 创建和销毁测试";

    EXPECT_NE(m_stack.get(), nullptr) << "PageStack 创建失败";
    EXPECT_TRUE(m_stack->isEmpty()) << "新创建的堆栈应该为空";
    EXPECT_EQ(m_stack->size(), 0) << "新创建的堆栈大小应该为0";
    EXPECT_FALSE(m_stack->canGoBack()) << "空堆栈不能返回";

    qDebug() << "✅ 测试1通过";
}

// ============================================
// 测试2: 基本推入和弹出测试
// ============================================
TEST_F(PageStackTestFixtrue, BasicPushPop) {
    qDebug() << "测试2: 基本推入和弹出测试";

    // 创建测试页面
    TestPage* page1 = new TestPage("Page1");
    TestPage* page2 = new TestPage("Page2");

    // 创建堆栈项
    PageStack::StackItem item1;
    item1.pageType = PageType::Login;
    item1.data = QVariant("login_data");
    item1.timestamp = QDateTime::currentDateTime();
    item1.widget =  page1;

    PageStack::StackItem item2;
    item2.pageType = PageType::Main;
    item2.data = QVariant("main_data");
    item2.timestamp = QDateTime::currentDateTime();
    item2.widget = page2;

    // 测试推入
    m_stack->push(item1);
    EXPECT_EQ(m_stack->size(), 1) << "推入后堆栈大小应为1";
    EXPECT_FALSE(m_stack->isEmpty()) << "堆栈不应为空";
    EXPECT_FALSE(m_stack->canGoBack()) << "只有一个页面时不能返回";

    m_stack->push(item2);
    EXPECT_EQ(m_stack->size(), 2) << "再次推入后堆栈大小应为2";
    EXPECT_TRUE(m_stack->canGoBack()) << "有两个页面时可以返回";

    // 测试顶部元素
    PageStack::StackItem topItem = m_stack->top();
    EXPECT_EQ(topItem.pageType, PageType::Main) << "顶部页面类型应为Main";
    EXPECT_EQ(topItem.data.toString(), "main_data") << "顶部页面数据不正确";
    EXPECT_EQ(topItem.widget, page2) << "顶部页面指针不正确";

    // 测试弹出
    PageStack::StackItem popped = m_stack->pop();
    EXPECT_EQ(popped.pageType, PageType::Main) << "弹出的页面类型应为Main";
    EXPECT_EQ(popped.widget, page2) << "弹出的页面指针不正确";
    EXPECT_EQ(m_stack->size(), 1) << "弹出后堆栈大小应为1";

    // 再次弹出
    popped = m_stack->pop();
    EXPECT_EQ(popped.pageType, PageType::Login) << "弹出的页面类型应为Login";
    EXPECT_TRUE(m_stack->isEmpty()) << "全部弹出后堆栈应为空";
    EXPECT_EQ(m_stack->size(), 0) << "全部弹出后堆栈大小应为0";

    // // 清理
    delete page1;
    delete page2;

    qDebug() << "✅ 测试2通过";
}

// ============================================
// 测试3: 替换顶部元素测试
// ============================================
TEST_F(PageStackTestFixtrue, ReplaceTop) {
    qDebug() << "测试3: 替换顶部元素测试";

    // 创建测试页面
    TestPage* page1 = new TestPage("Page1");
    TestPage* page2 = new TestPage("Page2");
    TestPage* page3 = new TestPage("Page3");

    // 创建堆栈项
    PageStack::StackItem item1;
    item1.pageType = PageType::Login;
    item1.widget = page1;

    PageStack::StackItem item2;
    item2.pageType = PageType::Main;
    item2.widget = page2;

    PageStack::StackItem item3;
    item3.pageType = PageType::Settings;
    item3.widget = page3;

    // 先推入两个页面
    m_stack->push(item1);
    m_stack->push(item2);

    EXPECT_EQ(m_stack->size(), 2) << "推入两个页面后大小应为2";
    EXPECT_EQ(m_stack->top().pageType, PageType::Main) << "顶部页面应为Main";

    // 替换顶部页面
    PageStack::StackItem replaced = m_stack->replaceTop(item3);
    EXPECT_EQ(m_stack->size(), 2) << "替换后堆栈大小应不变";
    EXPECT_EQ(m_stack->top().pageType, PageType::Settings) << "替换后顶部页面应为Settings";
    EXPECT_EQ(replaced.pageType, PageType::Main) << "返回的被替换页面应为Main";
    EXPECT_EQ(replaced.widget, page2) << "返回的页面指针应为page2";

    // 验证页面已被清理
    EXPECT_TRUE(page2->isHidden()) << "被替换的页面应被隐藏";

    qDebug() << "✅ 测试3通过";
}

// ============================================
// 测试4: 清空堆栈测试
// ============================================
TEST_F(PageStackTestFixtrue, ClearStack) {
    qDebug() << "测试4: 清空堆栈测试";

    // 创建多个测试页面
    QList<TestPage*> pages;
    for (int i = 0; i < 5; ++i) {
        TestPage* page = new TestPage(QString("Page%1").arg(i));
        pages.append(page);

        PageStack::StackItem item;
        item.pageType = static_cast<PageType>(static_cast<int>(PageType::Login) + i);
        item.widget = page;

        m_stack->push(item);
    }

    EXPECT_EQ(m_stack->size(), 5) << "推入5个页面后大小应为5";

    // 清空堆栈
    m_stack->clear();

    EXPECT_TRUE(m_stack->isEmpty()) << "清空后堆栈应为空";
    EXPECT_EQ(m_stack->size(), 0) << "清空后堆栈大小应为0";
    EXPECT_FALSE(m_stack->canGoBack()) << "清空后不能返回";

    qDebug() << "✅ 测试4通过";

}

// ============================================
// 测试5: 获取历史记录测试
// ============================================
TEST_F(PageStackTestFixtrue, GetHistory) {
    qDebug() << "测试5: 获取历史记录测试";

    // 创建测试页面
    TestPage* page1 = new TestPage("Page1");
    TestPage* page2 = new TestPage("Page2");
    TestPage* page3 = new TestPage("Page3");

    // 创建堆栈项
    PageStack::StackItem items[3];
    items[0].pageType = PageType::Login;
    items[0].widget = page1;

    items[1].pageType = PageType::Main;
    items[1].widget = page2;

    items[2].pageType = PageType::Settings;
    items[2].widget = page3;

    // 推入页面
    for (int i = 0; i < 3; ++i) {
        m_stack->push(items[i]);
    }

    // 获取历史记录
    QList<PageType> history = m_stack->history();
    EXPECT_EQ(history.size(), 3) << "历史记录应有3个页面";

    // 验证顺序
    EXPECT_EQ(history[0], PageType::Login) << "历史记录[0]应为Login";
    EXPECT_EQ(history[1], PageType::Main) << "历史记录[1]应为Main";
    EXPECT_EQ(history[2], PageType::Settings) << "历史记录[2]应为Settings";

    // 测试 at 方法
    PageStack::StackItem item0 = m_stack->at(0);
    EXPECT_EQ(item0.pageType, PageType::Login) << "at(0)应为Login";

    PageStack::StackItem item2 = m_stack->at(2);
    EXPECT_EQ(item2.pageType, PageType::Settings) << "at(2)应为Settings";

    // 测试边界情况
    PageStack::StackItem invalid = m_stack->at(-1);
    EXPECT_FALSE(invalid.isValid()) << "at(-1)应返回无效项";

    invalid = m_stack->at(3);
    EXPECT_FALSE(invalid.isValid()) << "at(3)应返回无效项";

    // 清空堆栈
    m_stack->clear();

    // 清空后的历史记录
    history = m_stack->history();
    EXPECT_TRUE(history.isEmpty()) << "清空后历史记录应为空";

    // 清空后的顶部
    PageStack::StackItem top = m_stack->top();
    EXPECT_FALSE(top.isValid()) << "清空后顶部应返回无效项";

    qDebug() << "✅ 测试5通过";

}

// ============================================
// 测试6: 最大深度限制测试
// ============================================
TEST_F(PageStackTestFixtrue, MaxDepth) {
    qDebug() << "测试6: 最大深度限制测试";

    // 设置最大深度为3
    m_stack->setMaxDepth(3);
    EXPECT_EQ(m_stack->maxDepth(), 3) << "最大深度应设置为3";


    // 创建测试页面
    for (int i = 0; i < 5; ++i) {
        TestPage* page = new TestPage(QString("Page%1").arg(i));

        PageStack::StackItem item;
        item.pageType = static_cast<PageType>(static_cast<int>(PageType::Login) + i);
        item.widget = page;

        qDebug() << "准备推入页面" << i << "，指针:" << page;
        m_stack->push(item);
        qDebug() << "推入后堆栈大小:" << m_stack->size();
    }

    // 由于最大深度为3，应该只保留最后3个
    EXPECT_EQ(m_stack->size(), 3) << "最大深度限制后应只保留3个页面";

    // 验证被移除的是最早的两个页面
    QList<PageType> history = m_stack->history();
    EXPECT_EQ(history.size(), 3) << "历史记录也应为3个";

    // 页面顺序实际是：Login, Register, Main, Search, Settings
    // 最大深度3，应该保留：Main, Search, Settings
    EXPECT_EQ(history[0], PageType::Main) << "历史记录[0]应为Main";
    EXPECT_EQ(history[1], PageType::Search) << "历史记录[1]应为Search";
    EXPECT_EQ(history[2], PageType::Settings) << "历史记录[2]应为Settings";

    // 修改最大深度为2
    m_stack->setMaxDepth(2);
    EXPECT_EQ(m_stack->size(), 2) << "修改最大深度为2后应只保留2个页面";

    // 验证被移除的是最早的页面
    history = m_stack->history();
    EXPECT_EQ(history.size(), 2) << "历史记录应为2个";
    EXPECT_EQ(history[0], PageType::Search) << "历史记录[1]应为Search";
    EXPECT_EQ(history[1], PageType::Settings) << "历史记录[2]应为Settings";

    // 测试无效的最大深度
    m_stack->setMaxDepth(0);
    EXPECT_GT(m_stack->maxDepth(), 0) << "设置最大深度为0应该被拒绝，保持原值";

    m_stack->setMaxDepth(-5);
    EXPECT_GT(m_stack->maxDepth(), 0) << "设置最大深度为负数应该被拒绝，保持原值";

    qDebug() << "✅ 测试6通过";
    QApplication::processEvents();
}


// ============================================
// 测试7: 边界条件和异常情况测试
// ============================================
TEST_F(PageStackTestFixtrue, EdgeCases) {
    qDebug() << "测试7: 边界条件和异常情况测试";

    // 1. 从空堆栈弹出
    PageStack::StackItem popped = m_stack->pop();
    EXPECT_FALSE(popped.isValid()) << "从空堆栈弹出应返回无效项";

    // 2. 替换空堆栈的顶部
    TestPage* page = new TestPage("TestPage");
    PageStack::StackItem item;
    item.pageType = PageType::Login;
    item.widget = page;

    PageStack::StackItem replaced = m_stack->replaceTop(item);
    EXPECT_FALSE(replaced.isValid()) << "替换空堆栈顶部应返回无效项";
    EXPECT_EQ(m_stack->size(), 1) << "替换空堆栈后应推入新项";
    EXPECT_EQ(m_stack->top().pageType, PageType::Login) << "顶部应为Login";

    // 3. 推入无效项
    PageStack::StackItem invalidItem;
    m_stack->push(invalidItem);
    EXPECT_EQ(m_stack->size(), 1) << "推入无效项应该被忽略";

    // 4. 清空空堆栈
    m_stack->clear();
    m_stack->clear();  // 再次清空应该没问题
    EXPECT_TRUE(m_stack->isEmpty()) << "多次清空应该没问题";

    // 5. 测试 canGoBack
    EXPECT_FALSE(m_stack->canGoBack()) << "空堆栈不能返回";

    m_stack->push(item);
    EXPECT_FALSE(m_stack->canGoBack()) << "只有一个页面时不能返回";

    PageStack::StackItem item2;
    item2.pageType = PageType::Main;
    TestPage* page2 = new TestPage("Page2");
    item2.widget = page2;

    m_stack->push(item2);
    EXPECT_TRUE(m_stack->canGoBack()) << "有两个页面时可以返回";

    qDebug() << "✅ 测试7通过";
}

// ============================================
// 测试8: 数据持久性测试
// ============================================
TEST_F(PageStackTestFixtrue, DataPersistence) {
    qDebug() << "测试8: 数据持久性测试";

    // 创建测试数据
    QVariantMap data1;
    data1["username"] = "user1";
    data1["timestamp"] = QDateTime::currentDateTime();

    QVariantMap data2;
    data2["username"] = "user2";
    data2["score"] = 100;

    // 创建测试页面
    TestPage* page1 = new TestPage("Page1");
    TestPage* page2 = new TestPage("Page2");

    // 创建带数据的堆栈项
    PageStack::StackItem item1;
    item1.pageType = PageType::Login;
    item1.data = data1;
    item1.widget = page1;

    PageStack::StackItem item2;
    item2.pageType = PageType::Main;
    item2.data = data2;
    item2.widget = page2;

    // 推入堆栈
    m_stack->push(item1);
    m_stack->push(item2);

    // 验证数据保存
    PageStack::StackItem top = m_stack->top();
    EXPECT_TRUE(top.data.canConvert<QVariantMap>()) << "顶部数据应为QVariantMap";

    QVariantMap topData = top.data.toMap();
    EXPECT_EQ(topData["username"].toString(), "user2") << "顶部数据username应为user2";
    EXPECT_EQ(topData["score"].toInt(), 100) << "顶部数据score应为100";

    // 弹出后验证数据
    m_stack->pop();
    top = m_stack->top();
    EXPECT_TRUE(top.data.canConvert<QVariantMap>()) << "弹出后顶部数据应为QVariantMap";

    topData = top.data.toMap();
    EXPECT_EQ(topData["username"].toString(), "user1") << "弹出后顶部数据username应为user1";

    qDebug() << "✅ 测试8通过";
    delete page2;
}

// ============================================
// 测试9: 时间戳功能测试
// ============================================
TEST_F(PageStackTestFixtrue, Timestamp) {
    qDebug() << "测试9: 时间戳功能测试";

    QDateTime time1 = QDateTime::currentDateTime();
    QTest::qWait(10);  // 等待10ms，确保时间不同

    QDateTime time2 = QDateTime::currentDateTime();

    // 创建测试页面
    TestPage* page1 = new TestPage("Page1");
    TestPage* page2 = new TestPage("Page2");

    // 创建带时间戳的堆栈项
    PageStack::StackItem item1;
    item1.pageType = PageType::Login;
    item1.timestamp = time1;
    item1.widget = page1;

    PageStack::StackItem item2;
    item2.pageType = PageType::Main;
    item2.timestamp = time2;
    item2.widget = page2;

    // 推入堆栈
    m_stack->push(item1);
    m_stack->push(item2);

    // 验证时间戳
    PageStack::StackItem top = m_stack->top();
    EXPECT_EQ(top.timestamp, time2) << "顶部时间戳应为time2";

    // 弹出后验证时间戳
    m_stack->pop();
    top = m_stack->top();
    EXPECT_EQ(top.timestamp, time1) << "弹出后顶部时间戳应为time1";

    qDebug() << "✅ 测试9通过";

    // 清理
    delete page2;
}


// ============================================
// 测试10: 性能测试
// ============================================
TEST_F(PageStackTestFixtrue, Performance) {
    qDebug() << "测试10: 性能测试";

    const int NUM_ITERATIONS = 1000;

    // 设置足够大的最大深度，避免在推入时删除页面
    m_stack->setMaxDepth(NUM_ITERATIONS + 10);  // 设为1010，确保不删除

    // 测试大量推入
    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        TestPage* page = new TestPage(QString("Page%1").arg(i));

        PageStack::StackItem item;
        // 使用有效的页面类型
        int pageTypeValue = (i % 6) + 1;  // 1-6循环
        item.pageType = static_cast<PageType>(pageTypeValue);
        item.widget = page;

        m_stack->push(item);
    }

    qint64 pushTime = timer.elapsed();
    qDebug() << "推入" << NUM_ITERATIONS << "个页面耗时:" << pushTime << "ms";

    // 由于设置了足够大的最大深度，这里应该是1000
    EXPECT_EQ(m_stack->size(), NUM_ITERATIONS)
        << "推入" << NUM_ITERATIONS << "个页面后大小应为" << NUM_ITERATIONS;

    // 测试大量弹出
    timer.restart();

    // 只弹出实际存在的页面
    int actualPopCount = 0;
    while (!m_stack->isEmpty() && actualPopCount < NUM_ITERATIONS) {
        PageStack::StackItem item = m_stack->pop();
        if (item.widget) {
            delete item.widget;  // 删除弹出的页面
        }
        actualPopCount++;
    }

    qint64 popTime = timer.elapsed();
    qDebug() << "弹出" << actualPopCount << "个页面耗时:" << popTime << "ms";

    EXPECT_TRUE(m_stack->isEmpty()) << "全部弹出后堆栈应为空";

    // 调整性能要求，1000次操作应在合理时间内完成
    const qint64 maxTotalTime = 2000;  // 2秒
    EXPECT_LT(pushTime + popTime, maxTotalTime)
        << "性能不达标: 1000次推入+弹出应在" << maxTotalTime << "ms内完成";

    qDebug() << "✅ 测试10通过";
}

