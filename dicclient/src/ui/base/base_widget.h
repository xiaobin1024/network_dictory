// base_widget.h - 第2版（添加样式管理）
#ifndef BASE_WIDGET_H
#define BASE_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QProgressBar>
#include <QShowEvent>
#include <QHideEvent>
#include <QDebug>
#include <QPointer>

enum class UIState { Normal, Loading, Error };
class ThemeManager; // 前向声明
enum class UITheme : int;
class BaseWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int uiState READ uiStateInt WRITE setUIStateInt)

public:
    explicit BaseWidget(QWidget* parent = nullptr);
    virtual ~BaseWidget();
    UIState uiState() const { return m_uiState; }
    int uiStateInt() const { return static_cast<int>(m_uiState); }
    void setUIStateInt(int state) { setUIState(static_cast<UIState>(state)); }
    void setUIState(UIState state);

    QVBoxLayout* mainLayout() const { return m_mainLayout; }

    // ============ 消息管理 ============
     void showMessage(const QString& message, bool isError = false,int duration=0);
    void clearMessage();

     // 初始化悬浮层
     void ensureMessageOverlay();
     // 重写 resizeEvent 更新消息位置
     void resizeEvent(QResizeEvent* event) override;
signals:
    void stateChanged(UIState newState);
protected:
    // 子类重写此方法处理 QSS 无法控制的动态元素（如图标、自定义绘制）
    virtual void onThemeChanged(UITheme theme);

private slots:
    void onThemeManagerChanged(UITheme theme);
private:
    UIState m_uiState = UIState::Normal;
    QVBoxLayout* m_mainLayout = nullptr;

    //使用悬浮层方式，不依赖布局
    QPointer<QLabel> m_messageOverlay;  // 悬浮消息标签
    QTimer* m_messageTimer{nullptr};
    QString m_currentMessage;




};

#endif // BASE_WIDGET_H
