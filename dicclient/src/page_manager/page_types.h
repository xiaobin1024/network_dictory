// page_types.h
#ifndef PAGE_TYPES_H
#define PAGE_TYPES_H

#include <QVariant>
#include <QDateTime>
#include "base/base_widget.h"

// 页面类型枚举
enum class PageType {
    None,       // 无页面
    Login,      // 登录页面
    Register,   // 注册页面
    Main,       // 主页面
    Search,     // 搜索页面
    Settings,   // 设置页面
    About,      // 关于页面
    Error       // 错误页面
};

// 导航选项
struct NavigateOptions {
    bool replaceCurrent{false};    // 替换当前页
    bool clearStack{false};        // 清空堆栈
};

// 页面创建函数类型
using PageCreator = std::function<BaseWidget*()>;

// 导航信号传递的数据
struct NavigateData {
    PageType fromPage{PageType::None};
    PageType toPage{PageType::None};
    QVariant data;  // 传递给目标页面的数据
    QVariant result; // 返回给源页面的结果
    QDateTime timestamp{QDateTime::currentDateTime()};
};

#endif // PAGE_TYPES_H
