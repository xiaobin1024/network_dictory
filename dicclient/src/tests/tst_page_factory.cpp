#include <gtest/gtest.h>
#include<QTest>
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <memory>
#include<QElapsedTimer>
#include "page_manager/page_stack.h"
#include "page_manager/page_types.h"
#include "page_manager/page_factory.h"

// 用于测试的简单页面类
class TestPage : public BaseWidget
{
public:
    explicit TestPage(const QString& name, QWidget* parent = nullptr)
        : BaseWidget(parent)
    {
        setObjectName(name);
    }

    QString getName() const { return objectName(); }
};

// 全局测试环境
class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        char* argv[] = {nullptr};
        app = new QApplication(argc, argv);
        qDebug() << "全局测试环境初始化完成 (PageFactory)";
    }

    void TearDown() override {
        delete app;
        qDebug() << "全局测试环境清理完成 (PageFactory)";
    }

private:
    QApplication* app;
};

// 注册全局环境
::testing::Environment* const qt_env =
    ::testing::AddGlobalTestEnvironment(new QtTestEnvironment);

// 测试夹具
class PageFactoryTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        m_factory = std::make_unique<PageFactory>();
        qDebug() << "测试夹具 SetUp - 创建新的 PageFactory";
    }

    void TearDown() override {
        m_factory.reset();
        qDebug() << "测试夹具 TearDown - 清理 PageFactory";
    }

    std::unique_ptr<PageFactory> m_factory;
};


// ============================================
// 测试1: 创建和销毁测试
// ============================================
TEST_F(PageFactoryTestFixture, CreateAndDestroy) {
    qDebug() << "测试1: 创建和销毁测试";

    EXPECT_NE(m_factory.get(), nullptr) << "PageFactory 创建失败";

    qDebug() << "✅ 测试1通过";
}

// ============================================
// 测试2: 页面注册测试
// ============================================
TEST_F(PageFactoryTestFixture, RegisterPage) {
    qDebug() << "测试2: 页面注册测试";

    int registerCount = 0;

    // 注册多个页面
    m_factory->registerPage(PageType::Login, [&registerCount]() {
        registerCount++;
        return new TestPage("LoginPage");
    });

    m_factory->registerPage(PageType::Main, [&registerCount]() {
        registerCount++;
        return new TestPage("MainPage");
    });

    m_factory->registerPage(PageType::Settings, [&registerCount]() {
        registerCount++;
        return new TestPage("SettingsPage");
    });

    // 创建页面，验证注册成功
    BaseWidget* loginPage = m_factory->createPage(PageType::Login);
    EXPECT_NE(loginPage, nullptr) << "登录页面创建失败";
    EXPECT_EQ(loginPage->objectName(), "LoginPage") << "登录页面名称不正确";
    EXPECT_EQ(registerCount, 1) << "登录页面创建函数应被调用一次";

    BaseWidget* mainPage = m_factory->createPage(PageType::Main);
    EXPECT_NE(mainPage, nullptr) << "主页面创建失败";
    EXPECT_EQ(mainPage->objectName(), "MainPage") << "主页面名称不正确";
    EXPECT_EQ(registerCount, 2) << "主页面创建函数应被调用两次";

    BaseWidget* settingsPage = m_factory->createPage(PageType::Settings);
    EXPECT_NE(settingsPage, nullptr) << "设置页面创建失败";
    EXPECT_EQ(settingsPage->objectName(), "SettingsPage") << "设置页面名称不正确";
    EXPECT_EQ(registerCount, 3) << "设置页面创建函数应被调用三次";

    // 清理
    delete loginPage;
    delete mainPage;
    delete settingsPage;

    qDebug() << "✅ 测试2通过";
}

// ============================================
// 测试3: 重复注册测试
// ============================================
TEST_F(PageFactoryTestFixture, DuplicateRegister) {
    qDebug() << "测试3: 重复注册测试";

    int callCount = 0;

    // 第一次注册
    m_factory->registerPage(PageType::Login, [&callCount]() {
        callCount++;
        return new TestPage("FirstLoginPage");
    });

    // 第二次注册（应该覆盖第一次）
    m_factory->registerPage(PageType::Login, [&callCount]() {
        callCount++;
        return new TestPage("SecondLoginPage");
    });

    // 创建页面，应该使用第二次注册的工厂函数
    BaseWidget* page = m_factory->createPage(PageType::Login);
    EXPECT_NE(page, nullptr) << "页面创建失败";
    EXPECT_EQ(page->objectName(), "SecondLoginPage") << "应使用第二次注册的页面";
    EXPECT_EQ(callCount, 1) << "只有第二次注册的函数应被调用";

    // 再次创建，如果是新创建，调用计数会增加
    delete page;
    page = m_factory->createPage(PageType::Login);
    EXPECT_NE(page, nullptr) << "再次创建页面失败";
    EXPECT_EQ(callCount, 2) << "再次创建时工厂函数应被再次调用";

    // 清理
    delete page;

    qDebug() << "✅ 测试3通过";
}

// ============================================
// 测试4: 未注册页面测试
// ============================================
TEST_F(PageFactoryTestFixture, UnregisteredPage) {
    qDebug() << "测试4: 未注册页面测试";

    // 尝试创建未注册的页面
    BaseWidget* page = m_factory->createPage(PageType::Login);
    EXPECT_EQ(page, nullptr) << "未注册的页面应返回nullptr";

    // 注册后再尝试
    m_factory->registerPage(PageType::Login, []() {
        return new TestPage("LoginPage");
    });

    page = m_factory->createPage(PageType::Login);
    EXPECT_NE(page, nullptr) << "注册后应能创建页面";

    // 清理
    delete page;

    qDebug() << "✅ 测试4通过";
}

// ============================================
// 测试5: 缓存功能测试
// ============================================
TEST_F(PageFactoryTestFixture, CacheFunction) {
    qDebug() << "测试5: 缓存功能测试";

    int createCount = 0;

    // 注册页面
    m_factory->registerPage(PageType::Main, [&createCount]() {
        createCount++;
        return new TestPage("MainPage");
    });

    // 启用缓存（默认是启用的）
    m_factory->setCacheEnabled(true);
    EXPECT_TRUE(m_factory->isCacheEnabled()) << "缓存应被启用";

    // 第一次创建页面
    BaseWidget* page1 = m_factory->getOrCreatePage(PageType::Main);
    EXPECT_NE(page1, nullptr) << "第一次创建页面失败";
    EXPECT_EQ(createCount, 1) << "工厂函数应被调用一次";

    // 第二次获取，应该从缓存获取
    BaseWidget* page2 = m_factory->getOrCreatePage(PageType::Main);
    EXPECT_NE(page2, nullptr) << "第二次获取页面失败";
    EXPECT_EQ(page1, page2) << "应返回相同的页面实例（缓存）";
    EXPECT_EQ(createCount, 1) << "工厂函数不应被再次调用";

    // 禁用缓存
    m_factory->setCacheEnabled(false);
    EXPECT_FALSE(m_factory->isCacheEnabled()) << "缓存应被禁用";

    // 再次获取，应该创建新实例
    BaseWidget* page3 = m_factory->getOrCreatePage(PageType::Main);
    EXPECT_NE(page3, nullptr) << "禁用缓存后创建页面失败";
    EXPECT_NE(page1, page3) << "应返回不同的页面实例（无缓存）";
    EXPECT_EQ(createCount, 2) << "工厂函数应被再次调用";

    // 重新启用缓存
    m_factory->setCacheEnabled(true);

    // 此时缓存应该是空的，因为之前禁用了
    BaseWidget* page4 = m_factory->getOrCreatePage(PageType::Main);
    EXPECT_NE(page4, nullptr) << "重新启用缓存后获取页面失败";
    EXPECT_EQ(createCount, 3) << "工厂函数应被第三次调用";

    // 清理
    //delete page1;
    delete page3;
    //delete page4;

    qDebug() << "✅ 测试5通过";
}

// ============================================
// 测试6: 清理缓存测试
// ============================================
TEST_F(PageFactoryTestFixture, ClearCache) {
    qDebug() << "测试6: 清理缓存测试";

    int createCount = 0;

    // 注册多个页面
    m_factory->registerPage(PageType::Login, [&createCount]() {
        createCount++;
        return new TestPage("LoginPage");
    });

    m_factory->registerPage(PageType::Main, [&createCount]() {
        createCount++;
        return new TestPage("MainPage");
    });

    // 启用缓存
    m_factory->setCacheEnabled(true);

    // 创建页面并缓存
    BaseWidget* loginPage1 = m_factory->getOrCreatePage(PageType::Login);
    BaseWidget* mainPage1 = m_factory->getOrCreatePage(PageType::Main);

    EXPECT_NE(loginPage1, nullptr) << "登录页面创建失败";
    EXPECT_NE(mainPage1, nullptr) << "主页面创建失败";
    EXPECT_EQ(createCount, 2) << "两个页面都应被创建";

    // 清理缓存
    m_factory->clearCache();

    // 再次获取，应该创建新实例
    BaseWidget* loginPage2 = m_factory->getOrCreatePage(PageType::Login);
    BaseWidget* mainPage2 = m_factory->getOrCreatePage(PageType::Main);

    EXPECT_NE(loginPage2, nullptr) << "清理缓存后登录页面创建失败";
    EXPECT_NE(mainPage2, nullptr) << "清理缓存后主页面创建失败";
    EXPECT_NE(loginPage1, loginPage2) << "登录页面应是新实例";
    EXPECT_NE(mainPage1, mainPage2) << "主页面应是新实例";
    EXPECT_EQ(createCount, 4) << "清理缓存后应重新创建";

    // 从缓存移除特定页面
    BaseWidget* loginPage3 = m_factory->getOrCreatePage(PageType::Login);
    BaseWidget* mainPage3 = m_factory->getOrCreatePage(PageType::Main);
    EXPECT_EQ(loginPage2, loginPage3) << "登录页面应从缓存获取";
    EXPECT_EQ(mainPage2, mainPage3) << "主页面应从缓存获取";

    // 从缓存移除登录页面
    m_factory->removeFromCache(PageType::Login);

    // 再次获取登录页面，应该创建新实例
    BaseWidget* loginPage4 = m_factory->getOrCreatePage(PageType::Login);
    EXPECT_NE(loginPage3, loginPage4) << "移除缓存后登录页面应是新实例";
    EXPECT_EQ(mainPage2, m_factory->getOrCreatePage(PageType::Main)) << "主页面仍应从缓存获取";

    // 清理
    // delete loginPage1;
    // delete mainPage1;
    // delete loginPage2;
    // delete mainPage2;
    // delete loginPage4;

    qDebug() << "✅ 测试6通过";
}

// ============================================
// 测试7: 异常处理测试
// ============================================
TEST_F(PageFactoryTestFixture, ExceptionHandling) {
    qDebug() << "测试7: 异常处理测试";

    // 测试1: 工厂函数返回nullptr
    m_factory->registerPage(PageType::Login, []() -> BaseWidget* {
        qDebug() << "工厂函数返回nullptr";
        return nullptr;
    });

    BaseWidget* page = m_factory->createPage(PageType::Login);
    EXPECT_EQ(page, nullptr) << "工厂函数返回nullptr时，createPage应返回nullptr";

    // 测试2: 工厂函数抛出异常
    m_factory->registerPage(PageType::Main, []() -> BaseWidget* {
        qDebug() << "工厂函数抛出异常";
        throw std::runtime_error("测试异常");
        return new TestPage("MainPage");
    });

    // 注意：createPage内部应有try-catch，返回nullptr
    page = m_factory->createPage(PageType::Main);
    EXPECT_EQ(page, nullptr) << "工厂函数抛出异常时，createPage应返回nullptr";

    // 测试3: 注册空函数
    m_factory->registerPage(PageType::Settings, nullptr);
    page = m_factory->createPage(PageType::Settings);
    EXPECT_EQ(page, nullptr) << "注册空函数时，createPage应返回nullptr";

    qDebug() << "✅ 测试7通过";
}

// ============================================
// 测试8: 获取或创建页面测试
// ============================================
TEST_F(PageFactoryTestFixture, GetOrCreatePage) {
    qDebug() << "测试8: 获取或创建页面测试";

    int createCount = 0;

    // 注册页面
    m_factory->registerPage(PageType::Login, [&createCount]() {
        createCount++;
        return new TestPage("LoginPage");
    });

    // 测试 getOrCreatePage
    BaseWidget* page1 = m_factory->getOrCreatePage(PageType::Login);
    EXPECT_NE(page1, nullptr) << "getOrCreatePage 应创建页面";
    EXPECT_EQ(createCount, 1) << "工厂函数应被调用一次";

    BaseWidget* page2 = m_factory->getOrCreatePage(PageType::Login);
    EXPECT_EQ(page1, page2) << "getOrCreatePage 应返回缓存页面";
    EXPECT_EQ(createCount, 1) << "工厂函数不应被再次调用";

    // 测试未注册的页面
    BaseWidget* page3 = m_factory->getOrCreatePage(PageType::Register);
    EXPECT_EQ(page3, nullptr) << "未注册的页面应返回nullptr";

    // 清理
    //delete page1;

    qDebug() << "✅ 测试8通过";
}

// ============================================
// 测试9: 并发缓存访问测试
// ============================================
TEST_F(PageFactoryTestFixture, ConcurrentCacheAccess) {
    qDebug() << "测试9: 并发缓存访问测试";

    // 注意：PageFactory 不是线程安全的，这个测试只是为了验证基本功能
    // 实际使用中应在外部加锁

    int createCount = 0;

    // 注册页面
    m_factory->registerPage(PageType::Main, [&createCount]() {
        QThread::msleep(10);  // 模拟创建延迟
        createCount++;
        return new TestPage(QString("MainPage_%1").arg(createCount));
    });

    // 多次获取同一页面
    const int numRequests = 10;
    QList<BaseWidget*> pages;

    for (int i = 0; i < numRequests; ++i) {
        BaseWidget* page = m_factory->getOrCreatePage(PageType::Main);
        pages.append(page);
    }

    // 验证所有指针都相同（来自缓存）
    for (int i = 1; i < pages.size(); ++i) {
        EXPECT_EQ(pages[0], pages[i]) << "所有请求应返回相同的页面实例";
    }

    // 创建计数应该为1（只创建了一次）
    EXPECT_EQ(createCount, 1) << "工厂函数应只被调用一次";

    qDebug() << "✅ 测试9通过";
}

// ============================================
// 测试10: 内存管理测试
// ============================================
TEST_F(PageFactoryTestFixture, MemoryManagement) {
    qDebug() << "测试10: 内存管理测试";

    int deleteCount = 0;
    int createCount = 0;

    // 使用 QPointer 追踪页面是否被删除
    QPointer<BaseWidget> page1;

    // 创建自定义页面类
    class TrackedPage : public BaseWidget {
    public:
        TrackedPage(int& counter, const QString& name, QWidget* parent = nullptr)
            : BaseWidget(parent)
            , m_counter(counter)
        {
            setObjectName(name);
        }

        ~TrackedPage() {
            m_counter++;
        }

    private:
        int& m_counter;
    };

    // 注册页面
    m_factory->registerPage(PageType::Login, [&createCount, &deleteCount]() {
        createCount++;
        return new TrackedPage(deleteCount, "LoginPage");
    });

    // 启用缓存
    m_factory->setCacheEnabled(true);

    // 创建页面
    page1 = m_factory->getOrCreatePage(PageType::Login);
    EXPECT_NE(page1.data(), nullptr) << "页面创建失败";
    EXPECT_EQ(createCount, 1) << "页面应被创建一次";

    // 清理缓存
    m_factory->clearCache();

    // 多次处理事件，确保 deleteLater 被执行
    for (int i = 0; i < 10 && page1; ++i) {
        QApplication::processEvents();
        QTest::qWait(10);
    }

    // 验证页面已被删除
    EXPECT_TRUE(page1.isNull()) << "清理缓存后页面应被删除";
    EXPECT_EQ(deleteCount, 1) << "页面应被删除一次";

    qDebug() << "✅ 测试10通过";
}
