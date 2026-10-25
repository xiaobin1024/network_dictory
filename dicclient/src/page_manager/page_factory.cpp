// page_factory.cpp
#include "page_factory.h"
#include "base/base_widget.h"
#include <QDebug>
#include<QCoreApplication>


PageFactory::PageFactory() {}

PageFactory::~PageFactory()
{
    // 只在 QApplication 存在时清理
    if (QCoreApplication::instance()) {
       clearCache();
    } else {
        qWarning() << "PageFactory 析构时 QApplication 已不存在，跳过清理";
    }

}

void PageFactory::registerPage(PageType type, PageCreator creator)
{
    if (!creator) {
        qWarning() << "尝试注册无效的页面创建函数，页面类型:" << static_cast<int>(type);
        return;
    }

    m_creators[type] = creator;
    qDebug() << "注册页面类型:" << static_cast<int>(type);
}

BaseWidget* PageFactory::createPage(PageType type)
{
    auto it = m_creators.find(type);
    if (it == m_creators.end()) {
        qWarning() << "未注册的页面类型:" << static_cast<int>(type);
        return nullptr;
    }

    try {
        BaseWidget* page = it.value()();
        if (!page) {
            qWarning() << "页面创建函数返回nullptr，页面类型:" << static_cast<int>(type);
            return nullptr;
        }

        qDebug() << "创建页面:" << static_cast<int>(type) << "地址:" << page;
        return page;
    } catch (const std::exception& e) {
        qCritical() << "创建页面时发生异常:" << e.what()
                    << "页面类型:" << static_cast<int>(type);
        return nullptr;
    } catch (...) {
        qCritical() << "创建页面时发生未知异常，页面类型:" << static_cast<int>(type);
        return nullptr;
    }
}

BaseWidget* PageFactory::getOrCreatePage(PageType type)
{
    // 如果缓存已启用，尝试从缓存获取
    if (m_cacheEnabled) {
        BaseWidget* cached = getCachedPage(type);
        if (cached) {
            qDebug() << "从缓存获取页面:" << static_cast<int>(type);
            return cached;
        }
    }

    // 创建新页面
    BaseWidget* page = createPage(type);
    if (page && m_cacheEnabled) {
        cachePage(type, page);
    }

    return page;
}

void PageFactory::cachePage(PageType type, BaseWidget* page)
{
    if (!page) {
        qWarning() << "尝试缓存空页面，类型:" << static_cast<int>(type);
        return;
    }

    // 如果已缓存同类型页面，先清理旧的
    if (m_cache.contains(type)) {
        BaseWidget* oldPage = m_cache.take(type);
        if (oldPage && oldPage != page) {
            oldPage->deleteLater();
        }
    }

    m_cache[type] = page;
    qDebug() << "缓存页面:" << static_cast<int>(type) << "地址:" << page;
}

BaseWidget* PageFactory::getCachedPage(PageType type) const
{
    return m_cache.value(type, nullptr);
}

void PageFactory::clearCache()
{
    qDebug() << "清理页面缓存，数量:" << m_cache.size();

    for (BaseWidget* page : m_cache) {
        if (page) {
             page->deleteLater();
        }
    }

    m_cache.clear();
}

void PageFactory::removeFromCache(PageType type)
{
    if (m_cache.contains(type)) {
        BaseWidget* page = m_cache.take(type);
        if (page) {
          page->deleteLater();
        }
        qDebug() << "从缓存移除页面:" << static_cast<int>(type);
    }
}

void PageFactory::setCacheEnabled(bool enabled)
{
    if (m_cacheEnabled != enabled) {
        m_cacheEnabled = enabled;
        if (!enabled) {
            clearCache();
        }
        qDebug() << "页面缓存" << (enabled ? "启用" : "禁用");
    }
}

bool PageFactory::isCacheEnabled() const
{
    return m_cacheEnabled;
}
