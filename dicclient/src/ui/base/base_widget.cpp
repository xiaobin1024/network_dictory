#include "base_widget.h"
#include "theme_manager.h"
#include <QApplication>
#include <QStyle>


BaseWidget::BaseWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageTimer(new QTimer(this))
{
    setAttribute(Qt::WA_StyledBackground, true);

    // 默认垂直布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);
    m_mainLayout->setSpacing(12);

    // 设置消息定时器
    m_messageTimer->setSingleShot(true);
    connect(m_messageTimer, &QTimer::timeout, this, &BaseWidget::clearMessage);

    // 监听全局主题切换（仅用于非样式逻辑，如图标更新）
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &BaseWidget::onThemeManagerChanged, Qt::UniqueConnection);
}

BaseWidget::~BaseWidget()
{

}
void BaseWidget::ensureMessageOverlay() {
    if (m_messageOverlay) {
        return;  // 已创建
    }

    // 创建悬浮标签
    m_messageOverlay = new QLabel(this);
    m_messageOverlay->setAlignment(Qt::AlignCenter);
    m_messageOverlay->setWordWrap(true);
    m_messageOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);  // 鼠标事件穿透
    m_messageOverlay->setVisible(false);
    m_messageOverlay->setObjectName("messageOverlay");  // 用于 QSS 选择器
    m_messageOverlay->raise();

    qDebug() << "消息悬浮层已创建";
}

// ============ 消息管理 ============
void BaseWidget::showMessage(const QString& message, bool isError, int duration) {
    qDebug() << "显示消息：" << message << "，错误：" << isError << "，持续时间：" << duration << "ms";

    ensureMessageOverlay();

    if (!m_messageOverlay) {
        qCritical() << "无法创建消息悬浮层";
        return;
    }

    // 停止之前的定时器
    if (m_messageTimer && m_messageTimer->isActive()) {
        m_messageTimer->stop();
    }

    // 【适配主题系统】通过动态属性让 QSS 控制样式，不再硬编码颜色
    m_messageOverlay->setProperty("messageType", isError ? "error" : "success");

    // 触发样式重计算
    if (QStyle *s = m_messageOverlay->style()) {
        s->unpolish(m_messageOverlay);
        s->polish(m_messageOverlay);
    }

    m_messageOverlay->setText(message);

    // 计算并设置位置（顶部居中）
    int overlayWidth = qMin(400, this->width() - 40);  // 适配窄窗口
    int overlayHeight = 50;
    int x = (this->width() - overlayWidth) / 2;
    int y = 20;

    m_messageOverlay->setGeometry(x, y, overlayWidth, overlayHeight);
    m_messageOverlay->setVisible(true);
    m_messageOverlay->raise();

    m_currentMessage = message;

    if (duration > 0) {
        m_messageTimer->start(duration);
        qDebug() << "已设置消息自动清除定时器：" << duration << "ms";
    }
}

void BaseWidget::clearMessage() {
    qDebug() << "开始清除消息";

    if (m_messageTimer && m_messageTimer->isActive()) {
        m_messageTimer->stop();
    }

    if (m_messageOverlay) {
        m_messageOverlay->clear();
        m_messageOverlay->setVisible(false);
    }

    m_currentMessage.clear();
    qDebug() << "消息清除完成";
}

void BaseWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    // 更新悬浮消息位置
    if (m_messageOverlay && m_messageOverlay->isVisible()) {
        int overlayWidth = qMin(400, this->width() - 40);
        int overlayHeight = 50;
        int x = (this->width() - overlayWidth) / 2;
        int y = 20;
        m_messageOverlay->setGeometry(x, y, overlayWidth, overlayHeight);
    }
}

void BaseWidget::setUIState(UIState state) {
    if (m_uiState == state) return;
    m_uiState = state;

    // 通过 Qt 动态属性驱动 QSS 状态样式（见 light.qss 底部）
    setProperty("uiState", (state == UIState::Loading) ? "Loading" :
                               (state == UIState::Error) ? "Error" : "Normal");
    // 安全起见，加个空指针检查
    if (QStyle *s = style()) {
        s->unpolish(this);
        s->polish(this);
    }
    update();

    emit stateChanged(state);
}

void BaseWidget::onThemeManagerChanged(UITheme theme) {
    // 默认空实现。子类可重写以更新图标/自定义绘制
    // 注意：背景、边框、字体颜色等已由 qApp->setStyleSheet() 自动处理
    onThemeChanged(theme);
}

void BaseWidget::onThemeChanged(UITheme theme) {
    Q_UNUSED(theme);
    // 子类按需实现，例如：
    // m_iconLabel->setPixmap(QIcon::fromTheme("my-icon").pixmap(24));
}
