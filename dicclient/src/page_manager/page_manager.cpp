// page_manager.cpp
#include "page_manager.h"
#include "base/base_widget.h"
#include <QWidget>
#include <QDebug>
#include <QApplication>

PageManager* PageManager::instance()
{
    static PageManager instance;  // 静态局部变量，线程安全（C++11及以上）
    return &instance;
}
PageManager::PageManager(QObject* parent)
    : QObject(parent)
    , m_stack(std::make_unique<PageStack>())
    , m_factory(std::make_unique<PageFactory>())
{
    qDebug() << "PageManager 创建";
}

PageManager::~PageManager()
{
    qDebug() << "PageManager 析构开始";

    // 只在 QApplication 存在时清理
    if (QCoreApplication::instance()) {
        cleanup();
    } else {
        qWarning() << "PageManager 析构时 QApplication 已不存在，跳过清理";
    }

    qDebug() << "PageManager 销毁";
}

void PageManager::initialize(QWidget* container)
{
    qDebug() << "=== PageManager::initialize 开始 ===";
    qDebug() << "传入容器地址:" << static_cast<void*>(container);
    qDebug() << "传入容器对象名称:" << (container ? container->objectName() : "nullptr");
    qDebug() << "传入容器类名:" << (container ? container->metaObject()->className() : "nullptr");
    qDebug() << "当前 m_initialized:" << m_initialized;
    qDebug() << "当前 m_container地址:" << static_cast<void*>(m_container);  // 只打印地址，不解引用

    // 如果已初始化且容器相同，跳过
    if (m_initialized && m_container == container) {
        qDebug() << "PageManager 已用相同容器初始化，跳过";
        qDebug() << "=== PageManager::initialize 结束 ===";
        return;
    }

    // 如果已初始化但容器不同，警告
    if (m_initialized && m_container != container) {
        qWarning() << "PageManager 已初始化，正在重新设置容器"
                   << "旧容器地址:" << static_cast<void*>(m_container)  // 只打印地址
                   << "新容器地址:" << static_cast<void*>(container);
    }

    m_container = container;
    m_initialized = (container != nullptr);

    qDebug() << "PageManager 初始化完成，容器:" << container
             << "，已初始化:" << m_initialized;
    qDebug() << "=== PageManager::initialize 结束 ===";
}

void PageManager::cleanup()
{
    if (!m_initialized) {
        return;
    }

    qDebug() << "清理 PageManager";

    // 清理当前页面
    if (m_currentPage) {
        // 只在页面有效时调用
        m_currentPage->hidePage();
        m_currentPage = nullptr;
    }

    // 清理堆栈和工厂
    if (m_stack) {
        m_stack->clear();
    }

    if (m_factory) {
        m_factory->clearCache();
    }

    m_container = nullptr;
    m_initialized = false;
    m_navigating = false;
}

void PageManager::registerPage(PageType type, PageCreator creator)
{
    m_factory->registerPage(type, creator);
}

bool PageManager::navigateTo(PageType page, const QVariant& data, const NavigateOptions& options)
{
    if (!m_initialized) {
        qWarning() << "PageManager 未初始化，无法导航";
        return false;
    }

    return doNavigate(page, data, options);
}

bool PageManager::navigateBack(const QVariant& result)
{
    if (!m_initialized) {
        qWarning() << "PageManager 未初始化，无法返回";
        return false;
    }

    if (!canGoBack()) {
        qDebug() << "无法返回，历史记录为空";
        return false;
    }

    // 弹出当前页
    PageStack::StackItem currentItem = m_stack->pop();
    if (!currentItem.isValid()) {
        qWarning() << "当前页面无效";
        return false;
    }

    // 获取上一页
    PageStack::StackItem prevItem = m_stack->top();
    if (!prevItem.isValid()) {
        qWarning() << "上一页无效";
        return false;
    }

    // 准备导航数据
    NavigateData navData;
    navData.fromPage = currentItem.pageType;
    navData.toPage = prevItem.pageType;
    navData.result = result;
    navData.data = prevItem.data;

    qDebug() << "返回导航: 从" << static_cast<int>(navData.fromPage)
             << "到" << static_cast<int>(navData.toPage);

    // 切换到上一页
    switchPage(currentItem.widget, prevItem.widget);

    // 通知页面变化
    m_currentPage = prevItem.widget;
    emit currentPageChanged(prevItem.pageType, currentItem.pageType);
    emit navigationStateChanged(canGoBack());

    return true;
}

bool PageManager::navigateReplace(PageType page, const QVariant& data)
{
    NavigateOptions options;
    options.replaceCurrent = true;
    return navigateTo(page, data, options);
}

bool PageManager::navigateClear(PageType page, const QVariant& data)
{
    NavigateOptions options;
    options.clearStack = true;
    return navigateTo(page, data, options);
}

bool PageManager::doNavigate(PageType to, const QVariant& data, const NavigateOptions& options)
{
    // 防止重入
    if (m_navigating) {
        qWarning() << "导航正在进行中，忽略新的导航请求";
        return false;
    }

    m_navigating = true;

    // 获取当前页面信息
    PageStack::StackItem currentItem = m_stack->top();
    PageType fromPage = currentItem.pageType;

    qDebug() << "开始导航: 从" << static_cast<int>(fromPage)
             << "到" << static_cast<int>(to)
             << "数据:" << data
             << "替换当前页:" << options.replaceCurrent
             << "清空堆栈:" << options.clearStack;

    // 发射导航开始信号
    emit navigationStarted(fromPage, to, data);

    qDebug() << "1. 发射信号完成";

    // 获取或创建目标页面
    BaseWidget* toPage = m_factory->getOrCreatePage(to);
    qDebug() << "2. 获取页面完成，页面:" << toPage;

    if (!toPage) {
        qCritical() << "无法创建页面:" << static_cast<int>(to);
        emit navigationFailed(fromPage, to, "页面创建失败");
        m_navigating = false;
        return false;
    }

    // 如果清空堆栈，先清理
    if (options.clearStack) {
        m_stack->clear();
    }

    qDebug() << "3. 准备堆栈项";

    // 准备新的堆栈项
    PageStack::StackItem newItem;
    newItem.pageType = to;
    newItem.data = data;
    newItem.timestamp = QDateTime::currentDateTime();
    newItem.widget = toPage;

    // 处理堆栈
    if (options.replaceCurrent && !m_stack->isEmpty()) {
        // 替换当前页
        m_stack->replaceTop(newItem);
    } else {
        // 推入新页面
        m_stack->push(newItem);
    }

    qDebug() << "4. 堆栈处理完成，准备切换页面";

    // 切换到新页面
    switchPage(currentItem.widget, toPage);

    qDebug() << "5. 页面切换完成";

    // 设置页面数据
    toPage->setPageData(data);

    qDebug() << "6. 设置页面数据完成";

    // 通知页面变化
    m_currentPage = toPage;

    emit currentPageChanged(to, fromPage);
    emit navigationStateChanged(canGoBack());
    emit navigationCompleted(fromPage, to, data);

    qDebug() << "7. 发射完成信号";

    m_navigating = false;
    return true;
}

void PageManager::switchPage(BaseWidget* from, BaseWidget* to)
{
    // 先获取局部变量，避免多次访问成员变量
    QWidget* container = m_container;
    if (!container) {
        qWarning() << "没有容器，无法切换页面";
        return;
    }

    qDebug() << "switchPage: 从" << from << "到" << to;
    qDebug() << "容器:" << container;
    qDebug() << "容器大小:" << (container ? container->size() : QSize(0, 0));

    // 隐藏旧页面
    if (from) {
        qDebug() << "隐藏页面:" << from;
        from->deactivatePage();
        from->hidePage();
        from->hide();
    }

    // 显示新页面
    if (to) {
        qDebug() << "设置父对象，当前父对象:" << to->parent() << "新父对象:" << container;
        // 确保页面是容器的子控件
        if (to->parent() != container) {
            to->setParent(container);
        }

        // 设置页面大小
        to->resize(container->size());
        to->show();
        to->showPage();
        to->activatePage();
    }

    qDebug() << "页面切换完成: 从" << (from ? from->metaObject()->className() : "nullptr")
             << "到" << (to ? to->metaObject()->className() : "nullptr");
}

bool PageManager::canGoBack() const
{
    return m_stack->canGoBack();
}

BaseWidget* PageManager::currentPage() const
{
    return m_currentPage;
}

PageType PageManager::currentPageType() const
{
    PageStack::StackItem item = m_stack->top();
    return item.pageType;
}

QList<PageType> PageManager::pageHistory() const
{
    return m_stack->history();
}

void PageManager::clearHistory()
{
    m_stack->clear();
    emit navigationStateChanged(canGoBack());
}
