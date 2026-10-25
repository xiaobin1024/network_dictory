// page_stack.h
#ifndef PAGE_STACK_H
#define PAGE_STACK_H

#include "page_types.h"
#include <QList>

// 前向声明
class BaseWidget;

class PageStack
{
public:
    // 堆栈项
    struct StackItem {
        PageType pageType{PageType::None};
        QVariant data;
        QDateTime timestamp;
        BaseWidget* widget{nullptr};

        bool isValid() const { return pageType != PageType::None; }
    };

    PageStack();
    ~PageStack();

    // 堆栈操作
    void push(const StackItem& item);
    StackItem pop();
    StackItem replaceTop(const StackItem& item);
    void clear();

    // 查询
    StackItem top() const;
    StackItem at(int index) const;
    bool canGoBack() const;
    int size() const;
    bool isEmpty() const;

    // 历史记录
    QList<PageType> history() const;

    // 最大深度限制
    void setMaxDepth(int maxDepth);
    int maxDepth() const;

private:
    QList<StackItem> m_stack;
    int m_maxDepth{20};  // 合理的最大堆栈深度
};
#endif // PAGE_STACK_H
