// page_stack.cpp
#include "page_stack.h"
#include "base/base_widget.h"
#include <QDebug>
#include<QCoreApplication>

PageStack::PageStack() {}

PageStack::~PageStack()
{
    // 只在 QApplication 存在时清理
    if (QCoreApplication::instance()) {
         clear();
    } else {
        qWarning() << "PageStack 析构时 QApplication 已不存在，跳过清理";
    }

}

void PageStack::push(const StackItem& item)
{
    if (!item.isValid()) {
        qWarning() << "尝试推入无效的堆栈项";
        return;
    }

    // 如果超过最大深度，移除最旧的项
    if (m_maxDepth > 0 && m_stack.size() >= m_maxDepth) {
        StackItem oldItem = m_stack.takeFirst();
        if (oldItem.widget) {
            oldItem.widget->deleteLater();
        }
    }

    m_stack.append(item);
    qDebug() << "页面堆栈推入:" << static_cast<int>(item.pageType)
             << "当前堆栈大小:" << m_stack.size();
}

PageStack::StackItem PageStack::pop()
{
    if (m_stack.isEmpty()) {
        qWarning() << "尝试从空堆栈弹出";
        return StackItem{};
    }

    StackItem item = m_stack.takeLast();
    qDebug() << "页面堆栈弹出:" << static_cast<int>(item.pageType)
             << "剩余堆栈大小:" << m_stack.size();
    return item;
}

PageStack::StackItem PageStack::replaceTop(const StackItem& newItem)
{
    if (m_stack.isEmpty()) {
        push(newItem);
        return StackItem{};
    }

    StackItem oldItem = m_stack.last();
    m_stack.last() = newItem;

    // 清理旧页面的widget
    if (oldItem.widget && oldItem.widget != newItem.widget) {
        oldItem.widget->deleteLater();
    }

    qDebug() << "页面堆栈替换顶部: 旧页" << static_cast<int>(oldItem.pageType)
             << " -> 新页" << static_cast<int>(newItem.pageType);
    return oldItem;
}

void PageStack::clear()
{
    qDebug() << "清空页面堆栈，大小:" << m_stack.size();

    // 清理所有页面的widget
    for (const auto& item : m_stack) {
        if (item.widget) {
            item.widget->deleteLater();
        }
    }

    m_stack.clear();
}

PageStack::StackItem PageStack::top() const
{
    if (m_stack.isEmpty()) {
        return StackItem{};
    }
    return m_stack.last();
}

PageStack::StackItem PageStack::at(int index) const
{
    if (index < 0 || index >= m_stack.size()) {
        return StackItem{};
    }
    return m_stack.at(index);
}

bool PageStack::canGoBack() const
{
    return m_stack.size() > 1;  // 至少有2个页面才能返回
}

int PageStack::size() const
{
    return m_stack.size();
}

bool PageStack::isEmpty() const
{
    return m_stack.isEmpty();
}

QList<PageType> PageStack::history() const
{
    QList<PageType> result;
    for (const auto& item : m_stack) {
        result.append(item.pageType);
    }
    return result;
}

void PageStack::setMaxDepth(int maxDepth)
{
    if (maxDepth < 1) {
        qWarning() << "堆栈最大深度必须大于0，当前设置:" << maxDepth;
        return;
    }
    m_maxDepth = maxDepth;

    // 如果当前堆栈超过新深度，移除最旧的项
    while (m_stack.size() > m_maxDepth) {
        StackItem oldItem = m_stack.takeFirst();
        if (oldItem.widget) {
            oldItem.widget->deleteLater();
        }
    }
}

int PageStack::maxDepth() const
{
    return m_maxDepth;
}
