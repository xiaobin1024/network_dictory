#include "connection_overlay.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>

#include "theme_manager.h"

/* ============================== SpinnerWidget ============================== */
SpinnerWidget::SpinnerWidget(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    connect(m_timer, &QTimer::timeout, this, &SpinnerWidget::onTick);
}

QSize SpinnerWidget::sizeHint() const
{
    return QSize(56, 56);
}

void SpinnerWidget::start()
{
    if (!m_timer->isActive()) {
        m_timer->start(40);   // ~25fps，足够顺滑且省 CPU
    }
}

void SpinnerWidget::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    update();
}

void SpinnerWidget::onTick()
{
    m_angle = (m_angle + m_step) % 360;
    update();
}

void SpinnerWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int pad = 6;
    QRectF rect = QRectF(pad, pad, width() - 2 * pad, height() - 2 * pad);
    const int penWidth = qMax(3, qRound(rect.width() / 12.0));

    // 轨道（淡色整圆）
    QColor track = ThemeManager::instance()->currentTheme() == UITheme::Light
                       ? QColor(229, 231, 235)   // #E5E7EB
                       : QColor(42, 42, 53);     // #2A2A35
    painter.setPen(QPen(track, penWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawEllipse(rect);

    // 旋转弧线（主题强调色）
    QColor accent = ThemeManager::instance()->currentTheme() == UITheme::Light
                        ? QColor(59, 130, 246)    // #3B82F6
                        : QColor(96, 165, 250);   // #60A5FA
    painter.setPen(QPen(accent, penWidth, Qt::SolidLine, Qt::RoundCap));
    // 画 300° 的弧，留一个缺口，旋转时形成“转动”的视觉
    painter.drawArc(rect, m_angle * 16, 300 * 16);
}

/* ============================== ConnectionOverlay ============================== */
ConnectionOverlay::ConnectionOverlay(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("connectionOverlay");
    // 遮罩本身不参与父布局，由 relayout() 按父窗口尺寸铺满；
    // 也不拦截鼠标穿透——底层目标控件已被 setEnabled(false)，点击落在本遮罩上即无效。
    setAttribute(Qt::WA_StyledBackground, true);
    hide();

    // ---- 居中面板 ----
    m_panel = new QWidget(this);
    m_panel->setObjectName("connPanel");

    auto* panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(28, 28, 28, 28);
    panelLayout->setSpacing(16);
    panelLayout->setAlignment(Qt::AlignCenter);

    m_spinner = new SpinnerWidget(m_panel);
    m_spinner->setFixedSize(56, 56);

    m_primaryLabel = new QLabel("正在连接服务器…", m_panel);
    m_primaryLabel->setObjectName("connPrimaryLabel");
    m_primaryLabel->setAlignment(Qt::AlignCenter);
    m_primaryLabel->setWordWrap(true);

    m_secondaryLabel = new QLabel("已尝试 0 次", m_panel);
    m_secondaryLabel->setObjectName("connSecondaryLabel");
    m_secondaryLabel->setAlignment(Qt::AlignCenter);

    m_hintLabel = new QLabel(m_panel);
    m_hintLabel->setObjectName("connHintLabel");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->hide();

    m_retryButton = new QPushButton("立即重试", m_panel);
    m_retryButton->setObjectName("connRetryBtn");
    m_retryButton->setCursor(Qt::PointingHandCursor);
    connect(m_retryButton, &QPushButton::clicked, this, &ConnectionOverlay::retryRequested);
    m_retryButton->hide();

    panelLayout->addWidget(m_spinner, 0, Qt::AlignCenter);
    panelLayout->addWidget(m_primaryLabel, 0, Qt::AlignCenter);
    panelLayout->addWidget(m_secondaryLabel, 0, Qt::AlignCenter);
    panelLayout->addWidget(m_hintLabel, 0, Qt::AlignCenter);
    panelLayout->addWidget(m_retryButton, 0, Qt::AlignCenter);

    m_panel->setFixedWidth(360);
    m_panel->adjustSize();

    m_pendingTimer = new QTimer(this);
    m_pendingTimer->setSingleShot(true);
    connect(m_pendingTimer, &QTimer::timeout, this, [this]() {
        // 若这段时间仍未连上（hideOverlay 未被调用），则弹出遮罩
        if (!m_shown) {
            showConnecting();
        }
    });

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ConnectionOverlay::applyTheme, Qt::UniqueConnection);
}

void ConnectionOverlay::setBlockTarget(QWidget* target)
{
    m_blockTarget = target;
}

void ConnectionOverlay::setGiveUpAttempts(int attempts)
{
    if (attempts > 0) {
        m_giveUpAttempts = attempts;
    }
}

void ConnectionOverlay::showConnecting()
{
    ensureVisible(false);
}

void ConnectionOverlay::showReconnecting()
{
    ensureVisible(true);
}

void ConnectionOverlay::updateAttempt(int current, int /*max*/)
{
    // 重连已经开始，确保遮罩可见（即便延迟显示计时器还没触发）
    if (!m_shown) {
        ensureVisible(true);
    }
    m_secondaryLabel->setText(QString("已尝试 %1 次").arg(current));

    if (current >= m_giveUpAttempts) {
        // 仅在“阻塞的初始连接态”提示重启并提供手动重试；
        // 重连态由自动重连（ReconnectionManager）接管内，遮罩非阻塞，不显示死按钮。
        if (!m_wasConnected) {
            m_hintLabel->setText(
                QString("网络连接异常，已尝试 %1 次仍未成功。\n请检查网络后重启应用，或稍后重新打开。")
                    .arg(current));
            m_hintLabel->show();
            m_retryButton->show();
        }
    }
}

void ConnectionOverlay::hideOverlay()
{
    if (m_pendingTimer->isActive()) {
        m_pendingTimer->stop();
    }
    m_shown = false;
    hide();
    m_spinner->stop();
    // 复位鼠标穿透属性，避免影响后续阻塞态（初始连接）
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    if (m_blockTarget) {
        m_blockTarget->setEnabled(true);
    }
}

void ConnectionOverlay::armPendingShow(int delayMs)
{
    if (m_shown) {
        return;
    }
    m_pendingTimer->start(delayMs);
}

void ConnectionOverlay::ensureVisible(bool wasConnected)
{
    m_wasConnected = wasConnected;
    m_shown = true;
    if (m_pendingTimer->isActive()) {
        m_pendingTimer->stop();
    }

    m_primaryLabel->setText(wasConnected
                                ? QString("连接已断开，正在自动重连…")
                                : QString("正在连接服务器…"));
    m_secondaryLabel->setText("已尝试 0 次");
    m_hintLabel->hide();
    m_retryButton->hide();

    relayout();
    raise();
    show();
    m_spinner->start();

    if (wasConnected) {
        // 重连态：非阻塞。保留底层页面（含搜索框）可交互，遮罩仅作提示并鼠标穿透，
        // 因此离线缓存查询（LocalWordCache）在断网时仍可用。自动重连持续进行，无需手动重试。
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        if (m_blockTarget) {
            m_blockTarget->setEnabled(true);
        }
    } else {
        // 初始连接态：保持原阻塞行为，避免未连上时空操作。
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        if (m_blockTarget) {
            m_blockTarget->setEnabled(false);
        }
    }
}

void ConnectionOverlay::relayout()
{
    if (!parentWidget()) {
        return;
    }
    // 铺满父窗口（不进布局，直接按父尺寸定位）
    setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
    // 面板居中
    m_panel->move((width() - m_panel->width()) / 2,
                  (height() - m_panel->height()) / 2);
}

void ConnectionOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void ConnectionOverlay::applyTheme()
{
    // 颜色由 QSS + SpinnerWidget 内部按主题绘制，这里仅触发重绘
    update();
}
