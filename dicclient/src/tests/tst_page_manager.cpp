#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QSignalSpy>
#include <QDebug>
#include <memory>
#include<QTest>
#include "page_manager/page_manager.h"
#include "page_manager/page_factory.h"
#include "page_manager/page_stack.h"
#include "page_manager/page_types.h"
#include "base/base_widget.h"

// 全局测试环境
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        char* argv[] = {nullptr};
        app = new QApplication(argc, argv);
        qDebug() << "全局测试环境初始化完成 (PageManager)";
    }

    void TearDown() override {
        qDebug() << "全局测试环境清理开始";

        // 处理事件
        QApplication::processEvents();
        QTest::qWait(50);


        // 再次处理事件
        QApplication::processEvents();
        QTest::qWait(50);

        // 销毁 QApplication
        delete app;
        app = nullptr;

        qDebug() << "全局测试环境清理完成";
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
    explicit TestPage(PageType type, const QString& name, QWidget* parent = nullptr)
        : BaseWidget(parent)
        , m_type(type)
    {
        setObjectName(name);
    }

    PageType pageType() const { return m_type; }
    QString pageName() const { return objectName(); }

    void setPageData(const QVariant& data) override {
        m_receivedData = data;
        m_dataSetCount++;
    }

    QVariant getReceivedData() const { return m_receivedData; }
    int getDataSetCount() const { return m_dataSetCount; }

    void onPageShow() override {
        m_showCount++;
    }

    void onPageHide() override {
        m_hideCount++;
    }

    int getShowCount() const { return m_showCount; }
    int getHideCount() const { return m_hideCount; }

private:
    PageType m_type;
    QVariant m_receivedData;
    int m_dataSetCount = 0;
    int m_showCount = 0;
    int m_hideCount = 0;
};

// 测试夹具
class PageManagerTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保我们在 GUI 线程
        Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());
        qDebug() << "SetUp 开始，当前线程:" << QThread::currentThread();

        // 创建容器
        m_container = std::make_unique<QWidget>();
        m_container->setObjectName("TestContainer");
        m_container->resize(800, 600);

        qDebug() << "容器创建完成，地址:" << m_container.get();
        qDebug() << "容器对象名称:" << m_container->objectName();
        qDebug() << "容器类名:" << m_container->metaObject()->className();

        // 验证容器有效性
        if (!m_container) {
            qCritical() << "容器创建失败!";
            return;
        }

        // 创建布局
        m_layout = new QVBoxLayout(m_container.get());
        m_layout->setContentsMargins(0, 0, 0, 0);

        qDebug() << "布局创建完成";

        // 获取 PageManager 单例
        m_manager = PageManager::instance();
        qDebug() << "获取 PageManager 单例完成，地址:" << m_manager;

        if (!m_manager) {
            qCritical() << "获取 PageManager 单例失败!";
            return;
        }

        // 验证 PageManager 状态
        qDebug() << "调用 initialize 前...";

        try {
            m_manager->initialize(m_container.get());
            qDebug() << "initialize 调用成功";
        } catch (const std::exception& e) {
            qCritical() << "initialize 抛出异常:" << e.what();
            throw;
        } catch (...) {
            qCritical() << "initialize 抛出未知异常";
            throw;
        }

        qDebug() << "SetUp 完成";

        // 注册测试页面
        m_manager->registerPage(PageType::Login, []() {
            return new TestPage(PageType::Login, "LoginPage");
        });

        m_manager->registerPage(PageType::Main, []() {
            return new TestPage(PageType::Main, "MainPage");
        });

        m_manager->registerPage(PageType::Settings, []() {
            return new TestPage(PageType::Settings, "SettingsPage");
        });

        m_manager->registerPage(PageType::Search, []() {
            return new TestPage(PageType::Search, "SearchPage");
        });

        m_manager->registerPage(PageType::About, []() {
            return new TestPage(PageType::About, "AboutPage");
        });

        qDebug() << "测试夹具 SetUp - 创建新的 PageManager";
    }

    void TearDown() override {
        if (m_manager) {
            m_manager->cleanup();  // 清理缓存和堆栈
        }
        // 但我们需要清理容器
        m_container.reset();
        qDebug() << "测试夹具 TearDown - 清理 PageManager";
    }

    TestPage* getCurrentTestPage() {
        BaseWidget* current = m_manager->currentPage();
        return dynamic_cast<TestPage*>(current);  // 使用 dynamic_cast
    }

    std::unique_ptr<QWidget> m_container;
    QVBoxLayout* m_layout = nullptr;
     PageManager* m_manager = nullptr;  // 改为原始指针，指向单例
};

// ============================================
// 测试1: 创建和初始化测试
// ============================================
TEST_F(PageManagerTestFixture, CreateAndInitialize) {
    qDebug() << "测试1: 创建和初始化测试";

    EXPECT_NE(m_manager, nullptr) << "PageManager 单例获取失败";
    EXPECT_NE(m_container.get(), nullptr) << "容器创建失败";

    // 验证单例模式
    PageManager* sameInstance = PageManager::instance();
    EXPECT_EQ(m_manager, sameInstance) << "PageManager 应是单例";

    // 验证初始状态
    EXPECT_EQ(m_manager->currentPageType(), PageType::None)
        << "初始当前页面类型应为None";
    EXPECT_FALSE(m_manager->canGoBack())
        << "初始状态不应能返回";

    qDebug() << "✅ 测试1通过";
}

// ============================================
// 测试2: 基本导航测试
// ============================================
TEST_F(PageManagerTestFixture, BasicNavigation) {
    qDebug() << "测试2: 基本导航测试";

    // 验证容器和管理器
    EXPECT_NE(m_container.get(), nullptr) << "容器不应为空";
    EXPECT_NE(m_manager, nullptr) << "PageManager 不应为空";

    // 验证容器内存地址
    qDebug() << "容器地址:" << m_container.get();
    qDebug() << "容器对象名称:" << m_container->objectName();

    // 验证容器有效性
    EXPECT_FALSE(m_container->objectName().isEmpty()) << "容器应有对象名称";
    EXPECT_GT(m_container->width(), 0) << "容器应有宽度";
    EXPECT_GT(m_container->height(), 0) << "容器应有高度";

    // 显示容器
    m_container->show();
    QApplication::processEvents();

    // 验证 PageManager 内部容器
    // 如果 PageManager 有获取容器的方法，可以验证
    // EXPECT_EQ(m_manager->getContainer(), m_container.get());

    // 尝试导航
    NavigateOptions options;
    bool result;
    try {
        qDebug() << "开始导航...";
        result = m_manager->navigateTo(PageType::Login, QVariant("login_data"), options);
        qDebug() << "导航结果:" << result;

        if (!result) {
            qDebug() << "导航失败";
        }

        QApplication::processEvents();
        QTest::qWait(50);

        EXPECT_TRUE(result) << "导航到登录页失败";
    } catch (const std::exception& e) {
        FAIL() << "导航抛出异常: " << e.what();
    } catch (...) {
        FAIL() << "导航抛出未知异常";
    }

    // 验证当前页面
    TestPage* loginPage = getCurrentTestPage();
    EXPECT_NE(loginPage, nullptr) << "当前页面不应为nullptr";
    EXPECT_EQ(loginPage->pageType(), PageType::Login) << "当前页面应为登录页";
    EXPECT_EQ(loginPage->pageName(), "LoginPage") << "页面名称不正确";
    EXPECT_EQ(m_manager->currentPageType(), PageType::Login)
        << "当前页面类型应为Login";

    // 验证页面数据
    EXPECT_EQ(loginPage->getReceivedData().toString(), "login_data")
        << "页面数据未正确传递";
    EXPECT_EQ(loginPage->getDataSetCount(), 1)
        << "页面应收到一次数据设置";

    // 验证页面显示
    EXPECT_EQ(loginPage->getShowCount(), 1) << "页面应显示一次";
    EXPECT_EQ(loginPage->getHideCount(), 0) << "页面应未隐藏";

    // 导航到主页面
    result = m_manager->navigateTo(PageType::Main, QVariant("main_data"), options);
    EXPECT_TRUE(result) << "导航到主页失败";

    // 验证页面切换
    TestPage* mainPage = getCurrentTestPage();
    EXPECT_NE(mainPage, nullptr) << "当前页面不应为nullptr";
    EXPECT_EQ(mainPage->pageType(), PageType::Main) << "当前页面应为主页";

    // 验证登录页被隐藏
    EXPECT_EQ(loginPage->getHideCount(), 1) << "登录页应被隐藏一次";

    // 验证返回功能
    EXPECT_TRUE(m_manager->canGoBack()) << "有历史记录时应能返回";

    qDebug() << "✅ 测试2通过";
}

// ============================================
// 测试3: 导航返回测试
// ============================================
TEST_F(PageManagerTestFixture, NavigationBack) {
    qDebug() << "测试3: 导航返回测试";

    // 导航多个页面
    NavigateOptions options;
    m_manager->navigateTo(PageType::Login, QVariant("page1"), options);
    m_manager->navigateTo(PageType::Main, QVariant("page2"), options);
    m_manager->navigateTo(PageType::Settings, QVariant("page3"), options);

    EXPECT_EQ(m_manager->currentPageType(), PageType::Settings)
        << "当前页面应为设置页";
    EXPECT_TRUE(m_manager->canGoBack()) << "应有返回历史";

    // 返回到主页面
    bool result = m_manager->navigateBack(QVariant("back_from_settings"));
    EXPECT_TRUE(result) << "返回失败";
    EXPECT_EQ(m_manager->currentPageType(), PageType::Main)
        << "返回到主页面";

    // 返回到登录页
    result = m_manager->navigateBack(QVariant("back_from_main"));
    EXPECT_TRUE(result) << "再次返回失败";
    EXPECT_EQ(m_manager->currentPageType(), PageType::Login)
        << "返回到登录页";

    // 尝试再次返回（应该失败）
    result = m_manager->navigateBack();
    EXPECT_FALSE(result) << "无历史记录时返回应失败";

    qDebug() << "✅ 测试3通过";
}

// ============================================
// 测试4: 替换当前页测试
// ============================================
TEST_F(PageManagerTestFixture, ReplaceCurrentPage) {
    qDebug() << "测试4: 替换当前页测试";

    // 导航到登录页
    NavigateOptions options;
    m_manager->navigateTo(PageType::Login, QVariant("data1"), options);

    TestPage* page1 = getCurrentTestPage();
    QString page1Name = page1->pageName();

    // 替换当前页为主页
    options.replaceCurrent = true;
    m_manager->navigateTo(PageType::Main, QVariant("data2"), options);

    TestPage* page2 = getCurrentTestPage();
    EXPECT_EQ(page2->pageType(), PageType::Main) << "当前页面应为主页";

    // 验证页面堆栈大小
    // 替换后堆栈大小应为1（登录页被替换）
    EXPECT_FALSE(m_manager->canGoBack()) << "替换后不应有返回历史";

    qDebug() << "✅ 测试4通过";
}

// ============================================
// 测试5: 清空堆栈测试
// ============================================
TEST_F(PageManagerTestFixture, ClearStackNavigation) {
    qDebug() << "测试5: 清空堆栈测试";

    // 导航多个页面
    NavigateOptions options;
    m_manager->navigateTo(PageType::Login, QVariant("page1"), options);
    m_manager->navigateTo(PageType::Main, QVariant("page2"), options);
    m_manager->navigateTo(PageType::Settings, QVariant("page3"), options);

    EXPECT_TRUE(m_manager->canGoBack()) << "应有返回历史";

    // 清空堆栈并导航到新页面
    options.clearStack = true;
    m_manager->navigateTo(PageType::About, QVariant("clear_and_go"), options);

    // 验证
    EXPECT_EQ(m_manager->currentPageType(), PageType::About)
        << "当前页面应为关于页";
    EXPECT_FALSE(m_manager->canGoBack())
        << "清空堆栈后不应有返回历史";

    qDebug() << "✅ 测试5通过";
}

// ============================================
// 测试6: 信号测试
// ============================================
TEST_F(PageManagerTestFixture, SignalTest) {
    qDebug() << "测试6: 信号测试";

    // 创建信号间谍
    QSignalSpy navigationStartedSpy(m_manager, &PageManager::navigationStarted);
    QSignalSpy currentPageChangedSpy(m_manager, &PageManager::currentPageChanged);
    QSignalSpy navigationCompletedSpy(m_manager, &PageManager::navigationCompleted);
    QSignalSpy navigationStateChangedSpy(m_manager, &PageManager::navigationStateChanged);

    NavigateOptions options;

    // 第一次导航
    m_manager->navigateTo(PageType::Login, QVariant("data1"), options);

    // 验证信号
    EXPECT_EQ(navigationStartedSpy.count(), 1) << "应发射navigationStarted信号";
    EXPECT_EQ(currentPageChangedSpy.count(), 1) << "应发射currentPageChanged信号";
    EXPECT_EQ(navigationCompletedSpy.count(), 1) << "应发射navigationCompleted信号";
    EXPECT_EQ(navigationStateChangedSpy.count(), 1) << "应发射navigationStateChanged信号";

    // 验证信号参数
    QList<QVariant> args = navigationStartedSpy.takeFirst();
    EXPECT_EQ(args.at(0).value<PageType>(), PageType::None) << "起始页面应为None";
    EXPECT_EQ(args.at(1).value<PageType>(), PageType::Login) << "目标页面应为Login";

    args = currentPageChangedSpy.takeFirst();
    EXPECT_EQ(args.at(0).value<PageType>(), PageType::Login) << "新页面应为Login";
    EXPECT_EQ(args.at(1).value<PageType>(), PageType::None) << "旧页面应为None";

    // 清空信号间谍，重新开始计数
    navigationStartedSpy.clear();
    currentPageChangedSpy.clear();
    navigationCompletedSpy.clear();
    navigationStateChangedSpy.clear();


    QApplication::processEvents();
    QTest::qWait(50);

    // 第二次导航
    m_manager->navigateTo(PageType::Main, QVariant("data2"), options);

    // 验证信号计数增加
    EXPECT_EQ(navigationStartedSpy.count(), 1) << "应再次发射navigationStarted";
    EXPECT_EQ(currentPageChangedSpy.count(), 1) << "应再次发射currentPageChanged";
    EXPECT_EQ(navigationCompletedSpy.count(), 1) << "应再次发射navigationCompleted";
    EXPECT_EQ(navigationStateChangedSpy.count(), 1) << "应再次发射navigationStateChanged";

    qDebug() << "✅ 测试6通过";
}

// ============================================
// 测试7: 页面缓存测试
// ============================================
TEST_F(PageManagerTestFixture, PageCacheTest) {
    qDebug() << "测试7: 页面缓存测试";

    NavigateOptions options;

    // 导航到登录页
    m_manager->navigateTo(PageType::Login, QVariant("login1"), options);
    TestPage* loginPage1 = getCurrentTestPage();

    // 导航到主页
    m_manager->navigateTo(PageType::Main, QVariant("main1"), options);

    // 返回登录页
    m_manager->navigateBack();
    TestPage* loginPage2 = getCurrentTestPage();

    // 验证返回的是同一页面实例（缓存）
    EXPECT_EQ(loginPage1, loginPage2) << "应返回缓存的页面实例";

    // 验证页面数据是否重新设置
    EXPECT_EQ(loginPage2->getDataSetCount(), 1)
        << "返回时不应重新设置数据";

    qDebug() << "✅ 测试7通过";
}

// ============================================
// 测试8: 复杂导航场景测试
// ============================================
TEST_F(PageManagerTestFixture, ComplexNavigationScenarios) {
    qDebug() << "测试8: 复杂导航场景测试";

    NavigateOptions options;

    // 场景1: 登录 -> 主页 -> 设置 -> 返回主页 -> 关于页
    m_manager->navigateTo(PageType::Login, QVariant("login"), options);
    m_manager->navigateTo(PageType::Main, QVariant("main"), options);
    m_manager->navigateTo(PageType::Settings, QVariant("settings"), options);

    EXPECT_EQ(m_manager->currentPageType(), PageType::Settings);

    m_manager->navigateBack();
    EXPECT_EQ(m_manager->currentPageType(), PageType::Main);

    m_manager->navigateTo(PageType::About, QVariant("about"), options);
    EXPECT_EQ(m_manager->currentPageType(), PageType::About);
    EXPECT_TRUE(m_manager->canGoBack()) << "应有返回历史";

    // 场景2: 清空历史，重新开始
    options.clearStack = true;
    m_manager->navigateTo(PageType::Login, QVariant("fresh_start"), options);
    EXPECT_EQ(m_manager->currentPageType(), PageType::Login);
    EXPECT_FALSE(m_manager->canGoBack()) << "清空后应无历史";

    // 场景3: 替换当前页
    options.clearStack = false;
    options.replaceCurrent = true;
    m_manager->navigateTo(PageType::Main, QVariant("replace"), options);
    EXPECT_EQ(m_manager->currentPageType(), PageType::Main);
    EXPECT_FALSE(m_manager->canGoBack()) << "替换后应无历史";

    qDebug() << "✅ 测试8通过";
}

// ============================================
// 测试9: 错误处理测试
// ============================================
TEST_F(PageManagerTestFixture, ErrorHandling) {
    qDebug() << "测试9: 错误处理测试";

    NavigateOptions options;

    // 测试1: 导航到未注册的页面
    bool result = m_manager->navigateTo(PageType::Error, QVariant(), options);
    EXPECT_FALSE(result) << "导航到未注册页面应失败";

    // 测试2: 导航到已注册页面，但工厂函数返回nullptr
    m_manager->registerPage(PageType::Search, []() -> BaseWidget* {
        qWarning() << "工厂函数返回nullptr";
        return nullptr;
    });

    result = m_manager->navigateTo(PageType::Search, QVariant(), options);
    EXPECT_FALSE(result) << "工厂函数返回nullptr时导航应失败";

    // 重新注册正常的搜索页
    m_manager->registerPage(PageType::Search, []() -> BaseWidget* {
        return new TestPage(PageType::Search, "SearchPage");
    });

    qDebug() << "✅ 测试9通过";
}
