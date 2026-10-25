#ifndef CONNECTION_OVERLAY_H
#define CONNECTION_OVERLAY_H

#include <QWidget>
#include <QTimer>
#include <QString>
#include <QLabel>
#include <QPushButton>

// 旋转加载动画：纯 Qt 绘制，不依赖任何图片资源，随主题切换颜色。
class SpinnerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpinnerWidget(QWidget* parent = nullptr);
    QSize sizeHint() const override;

public slots:
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onTick();

private:
    QTimer* m_timer{nullptr};
    int m_angle{0};      // 当前旋转角度（度）
    int m_step{30};      // 每帧旋转角度
};

// 网络连接遮罩：覆盖整个窗口，在未连接 / 断线重连时：
//   1. 阻塞底层页面（按钮 / 输入框）的输入；
//   2. 显示动态转圈加载画面；
//   3. 显示“正在尝试连接，已尝试 N 次”；
//   4. 达到阈值后提示用户“请检查网络后重启应用，或稍后重新打开”，并提供“立即重试”。
class ConnectionOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit ConnectionOverlay(QWidget* parent = nullptr);

    // 设置被遮挡（需要被禁用输入）的目标控件，通常为 PageController。
    void setBlockTarget(QWidget* target);

    // 设置“达到阈值后提示重启”的重连次数阈值（默认 10 次）。
    void setGiveUpAttempts(int attempts);

    // 初始连接阶段（尚未建立过连接）。
    void showConnecting();
    // 已建立连接后断开，进入重连阶段。
    void showReconnecting();
    // 每次重连尝试时更新计数；达到阈值切换为“提示重启”文案。
    void updateAttempt(int current, int max);
    // 连接成功，隐藏遮罩并恢复底层输入。
    void hideOverlay();

    // 启动“延迟显示”：若 delayMs 内仍未能连接（连接成功会先调用 hideOverlay），
    // 才弹出遮罩，避免服务器在线时一闪而过的加载画面。
    void armPendingShow(int delayMs = 600);

signals:
    // 用户点击“立即重试”时发出，上层可调用 networkManager->connectToServer()。
    void retryRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void ensureVisible(bool wasConnected);
    void relayout();
    void applyTheme();

    QWidget*     m_blockTarget{nullptr};
    int          m_giveUpAttempts{10};
    bool         m_shown{false};

    QTimer*      m_pendingTimer{nullptr};

    SpinnerWidget* m_spinner{nullptr};
    QWidget*     m_panel{nullptr};
    QLabel*      m_primaryLabel{nullptr};
    QLabel*      m_secondaryLabel{nullptr};
    QLabel*      m_hintLabel{nullptr};   // 阈值后的重启提示
    QPushButton* m_retryButton{nullptr};
};

#endif // CONNECTION_OVERLAY_H
