// page_manager.h
#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "page_types.h"
#include "page_stack.h"
#include "page_factory.h"
#include <QObject>
#include <memory>

class PageManager : public QObject
{
    Q_OBJECT

public:
    // 单例访问
    static PageManager* instance();

    // 初始化
    void initialize(QWidget* container = nullptr);
    void cleanup();

    // 页面注册
    void registerPage(PageType type, PageCreator creator);

    // 导航接口
    bool navigateTo(PageType page, const QVariant& data = {},
                    const NavigateOptions& options = {});
    bool navigateBack(const QVariant& result = {});
    bool navigateReplace(PageType page, const QVariant& data = {});
    bool navigateClear(PageType page, const QVariant& data = {});

    // 状态查询
    bool canGoBack() const;
    BaseWidget* currentPage() const;
    PageType currentPageType() const;

    // 堆栈管理
    QList<PageType> pageHistory() const;
    void clearHistory();

    // 获取内部组件
    PageStack* stack() { return m_stack.get(); }
    PageFactory* factory() { return m_factory.get(); }

signals:
    // 导航信号
    void navigationStarted(PageType from, PageType to, const QVariant& data);
    void navigationCompleted(PageType from, PageType to, const QVariant& data);
    void navigationFailed(PageType from, PageType to, const QString& error);

    // 状态变化
    void currentPageChanged(PageType newPage, PageType oldPage);
    void navigationStateChanged(bool canGoBack);
    void pageCreated(PageType pageType, BaseWidget* page);

private:
    // 私有构造函数
    explicit PageManager(QObject* parent = nullptr);
    ~PageManager();

    // 禁止复制
    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;

    // 内部导航实现
    bool doNavigate(PageType to, const QVariant& data, const NavigateOptions& options);
    void switchPage(BaseWidget* from, BaseWidget* to);

    // 内部数据
    std::unique_ptr<PageStack> m_stack;
    std::unique_ptr<PageFactory> m_factory;
    QWidget* m_container{nullptr};
    BaseWidget* m_currentPage{nullptr};
    bool m_initialized{false};

    // 防止重入保护
    bool m_navigating{false};

};
#endif // PAGE_MANAGER_H
