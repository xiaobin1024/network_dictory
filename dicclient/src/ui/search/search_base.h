#ifndef SEARCH_BASE_H
#define SEARCH_BASE_H

#include "base/base_widget.h"
#include <QLineEdit>
#include <QPushButton>

class QKeyEvent;
class QEvent;

/**
 * @brief 基础搜索组件
 *
 * 提供基本的搜索功能，包含：
 * 1. 搜索输入框
 * 2. 搜索按钮（Unicode 字符 🔍）
 * 3. 清除按钮（Unicode 字符 ✕）
 * 4. 亮/暗主题切换（由 QSS 控制）
 *
 * 继承自 BaseWidget，使用信号机制与父组件通信。
 */
class SearchBase : public BaseWidget {
    Q_OBJECT
public:
    explicit SearchBase(QWidget* parent = nullptr);
    ~SearchBase() override;

    // 获取和设置搜索关键词
    QString keyword() const;
    void setKeyword(const QString& keyword);

    // 获取/设置输入框占位符文本
    QString placeholder() const;
    void setPlaceholder(const QString& placeholder);

    // 控制按钮显示
    void setShowClearButton(bool show);
    void setShowSearchButton(bool show);

    // 获取输入框和按钮控件（用于外部特殊定制）
    QLineEdit* inputField() const { return m_input; }
    QPushButton* searchButton() const { return m_searchButton; }
    QPushButton* clearButton() const { return m_clearButton; }

    // 设置焦点到输入框并全选文本
    void setFocusToInput();

signals:
    /**
     * @brief 搜索请求信号
     * @param keyword 搜索关键词
     * 触发条件：点击搜索按钮、按下回车键、外部调用 doSearch()
     */
    void searchRequested(const QString& keyword);

    /** @brief 关键词变化信号 */
    void keywordChanged(const QString& keyword);

    /** @brief 输入框获得焦点信号 */
    void inputFocused();

    /** @brief 输入框失去焦点信号 */
    void inputLostFocus();

     void clearClicked();

public slots:
    // 执行搜索（触发 searchRequested 信号）
    void doSearch();

    // 清空输入框
    void clearInput();

protected:
    // 主题变化处理（样式由 QSS 控制，此处仅做必要的非 QSS 更新）
    void onThemeChanged(UITheme theme) override;

    // 事件过滤器（监听输入框焦点变化）
    bool eventFilter(QObject* watched, QEvent* event) override;

    //void keyPressEvent(QKeyEvent* event) override;


private slots:
    void onTextChanged(const QString& text);
    void onSearchClicked();
    void onClearClicked();
    void onReturnPressed();

private:
    void setupLayout();
    void setupConnections();
    void updateClearButtonVisibility();

    // UI 组件
    QLineEdit* m_input = nullptr;          // 搜索输入框
    QPushButton* m_searchButton = nullptr; // 搜索按钮
    QPushButton* m_clearButton = nullptr;  // 清除按钮

    // 配置
    bool m_showClearButton = true;   // 是否显示清除按钮
    bool m_showSearchButton = true;  // 是否显示搜索按钮

    // 私有数据
    class SearchBasePrivate;
    SearchBasePrivate* d = nullptr;
};

#endif // SEARCH_BASE_H
