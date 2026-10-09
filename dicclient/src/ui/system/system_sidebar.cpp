#include "system_sidebar.h"
#include "base/theme_manager.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QStyle>
#include <QTimer>

// ============ 构造与析构 ============

SystemSidebar::SystemSidebar(QWidget* parent, UserSession* userSession)
    : BaseWidget(parent)
    , m_widthAnimation(new QPropertyAnimation(this, "sidebarWidth"))
    , m_userSession(userSession)
{
    setObjectName("SystemSidebar");
    setFixedWidth(COLLAPSED_WIDTH);
    m_expanded = false;
    m_currentWidth = COLLAPSED_WIDTH;

    setupUI();
    setupAnimations();
    setupConnections();

    qDebug() << "SystemSidebar 初始化完成，初始状态：收起";
}

SystemSidebar::~SystemSidebar()
{
    if (m_widthAnimation) {
        m_widthAnimation->stop();
        delete m_widthAnimation;
    }

    if (m_userSession) {
        disconnect(m_userSession, nullptr, this, nullptr);
    }

}

// ============ UI 初始化 ============

void SystemSidebar::setupUI()
{
    // 主布局已在 BaseWidget 构造函数中创建，直接使用

    mainLayout()->setContentsMargins(0, 0, 0, 0);
    mainLayout()->setSpacing(0);

    // 主按钮容器
    m_buttonContainer = new QWidget(this);
    m_buttonContainer->setObjectName("buttonContainer");
    //m_buttonContainer->setAttribute(Qt::WA_StyledBackground, true);

    m_buttonLayout = new QVBoxLayout(m_buttonContainer);
    m_buttonLayout->setContentsMargins(8, 12, 8, 12);
    m_buttonLayout->setSpacing(10);

    // 头像和用户名区域
    QWidget* headerWidget = new QWidget(m_buttonContainer);
     headerWidget->setObjectName("headerWidget");
    //headerWidget->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 15, 0, 15);
    headerLayout->setSpacing(8);

    m_avatarLabel = new QLabel(headerWidget);
    m_avatarLabel->setFixedSize(m_avatarSize, m_avatarSize);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setVisible(false);

    m_usernameLabel = new QLabel("未登录", headerWidget);
    m_usernameLabel->setAlignment(Qt::AlignCenter);
    m_usernameLabel->setVisible(false);

    headerLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    headerLayout->addWidget(m_usernameLabel, 0, Qt::AlignHCenter);

    // 顶部按钮容器
    m_topButtons = new QWidget(m_buttonContainer);
     m_topButtons->setObjectName("topButtons");
    //m_topButtons->setAttribute(Qt::WA_StyledBackground, true);
    m_topLayout = new QVBoxLayout(m_topButtons);
    m_topLayout->setContentsMargins(0, 0, 0, 0);
    m_topLayout->setSpacing(10);
    m_topLayout->setAlignment(Qt::AlignTop);

    // 底部按钮容器
    m_bottomContainer = new QWidget(m_buttonContainer);
    m_bottomContainer->setObjectName("bottomContainer");
    //m_bottomContainer->setAttribute(Qt::WA_StyledBackground, true);
    m_bottomLayout = new QVBoxLayout(m_bottomContainer);
    m_bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomLayout->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);

    // 弹性空间
    m_spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

    // 创建按钮
    createButtons();

    // 组装顶部按钮
    m_topLayout->addWidget(m_themeButton);
    m_topLayout->addWidget(m_collectButton);
    m_topLayout->addWidget(m_vocabularyButton);
    m_topLayout->addWidget(m_logoutButton);
    m_topLayout->addWidget(m_deleteButton);

    // 组装底部按钮
    m_bottomLayout->addWidget(m_toggleButton);

    // 按顺序添加到主布局
    m_buttonLayout->addWidget(headerWidget, 0, Qt::AlignHCenter);
    m_buttonLayout->addWidget(m_topButtons);
    m_buttonLayout->addSpacerItem(m_spacer);
    m_buttonLayout->addWidget(m_bottomContainer);

     mainLayout()->addWidget(m_buttonContainer);

    // 通过动态属性驱动QSS状态
    updateSidebarContent();

     //强制应用 QSS 样式
     if (QStyle* s = style()) {
         s->unpolish(this);
         s->polish(this);
     }
}

void SystemSidebar::createButtons()
{
    // 清理旧按钮
    if (m_themeButton) m_themeButton->deleteLater();
    if (m_logoutButton) m_logoutButton->deleteLater();
    if (m_deleteButton) m_deleteButton->deleteLater();
    if (m_collectButton) m_collectButton->deleteLater();
    if (m_vocabularyButton) m_vocabularyButton->deleteLater();
    if (m_toggleButton) m_toggleButton->deleteLater();

    // 创建新按钮
    m_themeButton = new QPushButton(this);
    m_logoutButton = new QPushButton(this);
    m_deleteButton = new QPushButton(this);
    m_collectButton = new QPushButton(this);
    m_vocabularyButton = new QPushButton(this);
    m_toggleButton = new QPushButton(this);

    // 设置对象名供QSS使用
    m_themeButton->setObjectName("themeButton");
    m_logoutButton->setObjectName("logoutButton");
    m_deleteButton->setObjectName("deleteButton");
    m_collectButton->setObjectName("collectButton");
    m_vocabularyButton->setObjectName("vocabularyButton");
    m_toggleButton->setObjectName("toggleButton");

    // 设置属性标记
    m_themeButton->setProperty("sidebarButton", true);
    m_logoutButton->setProperty("sidebarButton", true);
    m_deleteButton->setProperty("sidebarButton", true);
    m_collectButton->setProperty("sidebarButton", true);
    m_vocabularyButton->setProperty("sidebarButton", true);
    m_toggleButton->setProperty("sidebarButton", true);

    // 设置光标
    m_themeButton->setCursor(Qt::PointingHandCursor);
    m_logoutButton->setCursor(Qt::PointingHandCursor);
    m_deleteButton->setCursor(Qt::PointingHandCursor);
    m_collectButton->setCursor(Qt::PointingHandCursor);
    m_vocabularyButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setCursor(Qt::PointingHandCursor);

    // 连接信号
    connect(m_themeButton, &QPushButton::clicked, this, &SystemSidebar::handleThemeButtonClicked);
    connect(m_logoutButton, &QPushButton::clicked, this, &SystemSidebar::handleLogoutButtonClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &SystemSidebar::handleDeleteAccountButtonClicked);
    connect(m_collectButton, &QPushButton::clicked, this, &SystemSidebar::handleCollectButtonClicked);
    connect(m_vocabularyButton, &QPushButton::clicked, this, &SystemSidebar::handleVocabularyButtonClicked);
    connect(m_toggleButton, &QPushButton::clicked, this, [this]() { toggle(); });


}

void SystemSidebar::setupConnections()
{
    if (!m_userSession) return;

    connect(m_userSession, &UserSession::loginSuccess,
            this, &SystemSidebar::updateUserInfo);
    connect(m_userSession, &UserSession::logoutSuccess,
            this, &SystemSidebar::onUserLogoutSuccess);
    connect(m_userSession, &UserSession::loginFailed,
            this, &SystemSidebar::onUserLoginFailed);
    connect(m_userSession, &UserSession::unregisterSuccess,
            this, &SystemSidebar::onUserUnregisterSuccess);
    connect(m_userSession, &UserSession::unregisterFailed,
            this, &SystemSidebar::onUserUnregisterFailed);
}

void SystemSidebar::setupAnimations()
{
    m_widthAnimation->setDuration(250);
    m_widthAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_widthAnimation, &QPropertyAnimation::finished,
            this, &SystemSidebar::onAnimationFinished);
}

// ============ 展开/收起 ============

void SystemSidebar::setExpanded(bool expanded) { if (m_expanded == expanded) return;

    m_expanded = expanded;

    updateSidebarContent();

    // 设置动态属性驱动QSS状态切换
    setProperty("expanded", expanded);
    //【关键修改】刷新样式，包括所有子控件
    // 仅 polish(this) 不够，因为 QSS 选择器依赖父属性，子控件需要重新计算样式
    if (QStyle* s = style()) {
        s->unpolish(this);
        s->polish(this);
    }
    // 递归刷新子控件样式
    for (QWidget* child : findChildren<QWidget*>()) {
        if (child->style()) {
            child->style()->unpolish(child);
            child->style()->polish(child);
        }
    }

    startExpandAnimation(expanded);
    emit expandedChanged(expanded);
}

void SystemSidebar::toggle() { setExpanded(!m_expanded); }

void SystemSidebar::setSidebarWidth(int width) { if (m_currentWidth != width) { m_currentWidth = width; setFixedWidth(width); } }

void SystemSidebar::startExpandAnimation(bool expand) {
    if (!m_widthAnimation) return;
    if (m_widthAnimation->state() == QPropertyAnimation::Running) { m_widthAnimation->stop(); }
    int endWidth = expand ? EXPANDED_WIDTH : COLLAPSED_WIDTH;
    m_widthAnimation->setStartValue(m_currentWidth);
    m_widthAnimation->setEndValue(endWidth);
    m_widthAnimation->start();

    qDebug() << "启动侧边栏动画：" << m_currentWidth << " -> " << endWidth;
}

void SystemSidebar::onAnimationFinished()
{
    updateSidebarContent();
}

// ============ 内容更新（文本/可见性，不涉及颜色样式） ============

void SystemSidebar::updateSidebarContent()
{
    if (!m_themeButton || !m_logoutButton || !m_deleteButton || !m_toggleButton) {
        return;
    }

    bool loggedIn = m_userSession && m_userSession->isLoggedIn();
    UITheme currentTheme = ThemeManager::instance()->currentTheme();

    if (m_expanded) {
        // 展开状态：显示完整文本
        m_themeButton->setText(currentTheme  == UITheme::Light ? "🌙 暗色模式" : "☀️ 亮色模式");
        m_collectButton->setText("❤️ 查看收藏");
        m_vocabularyButton->setText("⭐ 查看生词本");
        m_logoutButton->setText(loggedIn ? "🚪 退出登录" : "🚪 未登录");
        m_deleteButton->setText("🗑️ 注销账号");
        m_toggleButton->setText("⚙️ 收起");

        // 显示所有控件
        m_themeButton->setVisible(true);
        m_logoutButton->setVisible(true);
        m_deleteButton->setVisible(true);
        m_collectButton->setVisible(true);
        m_vocabularyButton->setVisible(true);
        m_usernameLabel->setVisible(loggedIn);
        m_avatarLabel->setVisible(loggedIn);
    } else {
        // 收起状态：隐藏文字，只留图标
        // 不要 setVisible(false)，而是 setText("")，利用 QSS 显示图标
        m_themeButton->setText("");
        m_collectButton->setText("");
        m_vocabularyButton->setText("");
        m_logoutButton->setText("");
        m_deleteButton->setText("");

        // 确保按钮不可见，以便 QSS 的 min-width 生效
        m_themeButton->setVisible(false);
        m_logoutButton->setVisible(false);
        m_deleteButton->setVisible(false);
        m_collectButton->setVisible(false);
        m_vocabularyButton->setVisible(false);

        // 头像用户名隐藏
        m_usernameLabel->setVisible(false);
        m_avatarLabel->setVisible(false);

        // Toggle 按钮显示汉堡菜单
         m_toggleButton->setText("☰");  // 清空文字，用 QSS 的 qproperty-text 或 icon
        m_toggleButton->setVisible(true);
    }

    // 工具提示
    m_themeButton->setToolTip(m_expanded ? "" : "切换主题");
    m_logoutButton->setToolTip(m_expanded ? "" : "退出登录");
    m_deleteButton->setToolTip(m_expanded ? "" : "注销账号");
    m_collectButton->setToolTip(m_expanded ? "" : "查看收藏");
    m_vocabularyButton->setToolTip(m_expanded ? "" : "查看生词本");
    m_toggleButton->setToolTip(m_expanded ? "收起侧边栏" : "展开侧边栏");

    // 按钮启用状态
    m_logoutButton->setEnabled(loggedIn);
    m_deleteButton->setEnabled(loggedIn);

    update();
}

// ============ 事件处理 ============

void SystemSidebar::resizeEvent(QResizeEvent* event)
{
    BaseWidget::resizeEvent(event);
    if (m_buttonContainer) {
        m_buttonContainer->setFixedWidth(width());
    }
}

void SystemSidebar::onThemeChanged(UITheme theme)
{
    Q_UNUSED(theme);
    // QSS 已自动处理颜色，这里只更新按钮文本中的图标
    updateSidebarContent();
}

// ============ 按钮点击处理 ============

void SystemSidebar::handleThemeButtonClicked()
{
    ThemeManager::instance()->toggleTheme();
    emit themeToggleRequested();
    showMessage("主题切换中...", false, 800);
}

void SystemSidebar::handleLogoutButtonClicked()
{
    if (m_userSession && m_userSession->isLoggedIn()) {
        m_userSession->logout();
    } else {
        showMessage("当前无用户登录", false, 1500);
    }
}

void SystemSidebar::handleDeleteAccountButtonClicked()
{
    if (m_userSession && m_userSession->isLoggedIn()) {
        UserData currentUser = m_userSession->currentUser();
        auto reply = QMessageBox::question(this, "确认注销",
                                           QString("确定要注销账户 '%1' 吗？此操作不可撤销！").arg(currentUser.username()),
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            bool ok;
            QString password = QInputDialog::getText(this, "确认密码",
                                                     "请输入密码以确认注销:", QLineEdit::Password, "", &ok);

            if (ok && !password.isEmpty()) {
                m_userSession->unregisterUser(currentUser.username(), password);
                showMessage("正在注销账户...", false, 2000);
            } else {
                showMessage("取消注销操作", false, 1500);
            }
        }
    } else {
        showMessage("请先登录后再操作", false, 1500);
    }
}

void SystemSidebar::handleCollectButtonClicked()
{
    emit showCollectPageRequested();
}

void SystemSidebar::handleVocabularyButtonClicked()
{
    emit showVocabularyPageRequested();
}

// ============ UserSession 回调 ============

void SystemSidebar::onUserLoggedIn(const UserData& user)
{
    Q_UNUSED(user);
    updateUserButtonsState();
}

void SystemSidebar::onUserLoginFailed(const QString& error)
{
    showMessage("登录失败：" + error, true, 2000);
}

void SystemSidebar::onUserLogoutSuccess()
{
    showMessage("已成功退出登录", false, 1500);
    updateUserButtonsState();
}

void SystemSidebar::onUserUnregisterSuccess(const QString& username, int userId)
{
    Q_UNUSED(userId);
    showMessage(QString("账户 '%1' 注销成功！").arg(username), false, 2000);
    updateUserButtonsState();
}

void SystemSidebar::onUserUnregisterFailed(const QString& error)
{
    showMessage("注销失败：" + error, true, 2000);
}

void SystemSidebar::onUserChanged(const UserData& user)
{
    updateUserInfo(user);
    updateUserButtonsState();
}

void SystemSidebar::updateUserButtonsState()
{
    updateSidebarContent();
}

void SystemSidebar::updateUserInfo(const UserData& user)
{
    if (!m_avatarLabel || !m_usernameLabel) return;

    QString username = user.username();

    if (user.isLoggedIn()) {
        m_usernameLabel->setText(username);

        QString avatarPath = m_userSession ? m_userSession->getUserAvatarPath() : "";
        QPixmap pixmap(avatarPath);

        if (!pixmap.isNull()) {
            QPixmap roundedPixmap(m_avatarSize, m_avatarSize);
            roundedPixmap.fill(Qt::transparent);
            QPainter painter(&roundedPixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(0, 0, m_avatarSize, m_avatarSize, m_avatarSize / 2, m_avatarSize / 2);
            painter.setClipPath(path);
            painter.drawPixmap(0, 0, pixmap.scaled(m_avatarSize, m_avatarSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            m_avatarLabel->setPixmap(roundedPixmap);
            m_avatarLabel->setStyleSheet(""); // 清除可能存在的文字样式
        } else {
            m_avatarLabel->setPixmap(QPixmap());
            m_avatarLabel->setText(username.left(1).toUpper());
        }
    } else {
        m_usernameLabel->clear();
        m_avatarLabel->clear();
        m_avatarLabel->setText("");
    }

    updateSidebarContent();
}
