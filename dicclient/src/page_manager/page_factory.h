// page_factory.h
#ifndef PAGE_FACTORY_H
#define PAGE_FACTORY_H

#include "page_types.h"
#include <QMap>
#include <functional>

// 前向声明
class BaseWidget;

class PageFactory
{
public:
    PageFactory();
    ~PageFactory();

    // 页面注册
    void registerPage(PageType type, PageCreator creator);

    // 页面创建
    BaseWidget* createPage(PageType type);
    BaseWidget* getOrCreatePage(PageType type);

    // 页面缓存
    void cachePage(PageType type, BaseWidget* page);
    BaseWidget* getCachedPage(PageType type) const;
    void clearCache();
    void removeFromCache(PageType type);

    // 内存管理
    void setCacheEnabled(bool enabled);
    bool isCacheEnabled() const;

private:
    QMap<PageType, PageCreator> m_creators;
    QMap<PageType, BaseWidget*> m_cache;
    bool m_cacheEnabled{true};
};
#endif // PAGE_FACTORY_H
