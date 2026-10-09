#include "register_page.h"
#include "base/theme_manager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDebug>
#include <QPixmap>
#include"QStyle"

RegisterPage::RegisterPage(QWidget* parent)
    : BaseWidget(parent)
    , m_userSession(UserSession::instance())
    , m_signalsConnected(false)
    , m_processingRegistration(false)
    , m_initializing(false)
{
    qDebug() << "RegisterPage 创建";
    setObjectName("RegisterPage");
    setWindowTitle("注册 - 网络词典");

    if (!m_userSession->isInitialized()) {
        m_userSession->initialize();
    }

    setupUI();
    connectUserSessionSignals();
}

RegisterPage::~RegisterPage()
{
    qDebug() << "RegisterPage 销毁";
    if (m_userSession) {
        m_userSession->disconnect(this);
    }
}

/* =========================================================
   界面构建
   ========================================================= */
void RegisterPage::setupUI()
{
    auto* mainLayout = this->mainLayout();
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setContentsMargins(0, 0, 0, 0);          // 与登录页统一

    // 卡片容器
    auto* card = new QWidget(this);
    card->setObjectName("registerCard");
    card->setFixedWidth(400);                            // 与登录页统一固定宽度

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 32, 32, 32);       // 与登录页统一
    cardLayout->setSpacing(16);                          // 与登录页统一
    cardLayout->setAlignment(Qt::AlignTop);              // 与登录页统一

    // 标题
    m_titleLabel = new QLabel("用户注册", card);
    m_titleLabel->setObjectName("registerTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_titleLabel);

    // 头像选择行（注册页特有，保持但调整边距与登录页 bottomRow 一致）
    auto* avatarLayout = new QHBoxLayout();
    avatarLayout->setSpacing(10);
    avatarLayout->setAlignment(Qt::AlignLeft);
    avatarLayout->setContentsMargins(0, 4, 0, 0);

    m_avatarLabel = new QLabel("🧑", card);
    m_avatarLabel->setObjectName("avatarLabel");
    m_avatarLabel->setFixedSize(60, 60);
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    m_avatarButton = new QPushButton("选择图片 📷", card);
    m_avatarButton->setObjectName("avatarButton");
    m_avatarButton->setCursor(Qt::PointingHandCursor);
    connect(m_avatarButton, &QPushButton::clicked, this, &RegisterPage::onAvatarSelected);

    avatarLayout->addWidget(m_avatarLabel);
    avatarLayout->addWidget(m_avatarButton);
    avatarLayout->addStretch();
    cardLayout->addLayout(avatarLayout);

    // 表单
    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(8);                           // 与登录页统一
    formLayout->setLabelAlignment(Qt::AlignLeft);        // 【修改】左对齐，与登录页统一
    formLayout->setFormAlignment(Qt::AlignLeft);         // 【新增】与登录页统一
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow); // 【修改】与登录页统一
    formLayout->setContentsMargins(0, 8, 0, 0);          // 【新增】与登录页统一

    // 用户名
    m_usernameInput = new QLineEdit(card);
    m_usernameInput->setObjectName("input_username");
    m_usernameInput->setPlaceholderText("请输入用户名");
    m_usernameInput->setFixedHeight(40);                 // 【新增】统一输入框高度
    // 移除最小/最大宽度限制，由 400px 卡片和 ExpandingFieldsGrow 自然约束
    QRegularExpression usernameRegex("^[a-zA-Z0-9_]{3,20}$");
    m_usernameInput->setValidator(new QRegularExpressionValidator(usernameRegex, this));
    formLayout->addRow("用户名", m_usernameInput);
    m_usernameErr = new QLabel(card);
    m_usernameErr->setObjectName("err_username");
    m_usernameErr->setWordWrap(true);
    m_usernameErr->setVisible(false);
    formLayout->addRow("", m_usernameErr);

    // 邮箱
    m_emailInput = new QLineEdit(card);
    m_emailInput->setObjectName("input_email");
    m_emailInput->setPlaceholderText("请输入邮箱地址");
    m_emailInput->setFixedHeight(40);
    QRegularExpression emailRegex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    m_emailInput->setValidator(new QRegularExpressionValidator(emailRegex, this));
    formLayout->addRow("邮箱", m_emailInput);
    m_emailErr = new QLabel(card);
    m_emailErr->setObjectName("err_email");
    m_emailErr->setWordWrap(true);
    m_emailErr->setVisible(false);
    formLayout->addRow("", m_emailErr);

    // 密码
    m_passwordInput = new QLineEdit(card);
    m_passwordInput->setObjectName("input_password");
    m_passwordInput->setPlaceholderText("请输入密码");
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setFixedHeight(40);
    formLayout->addRow("密码", m_passwordInput);
    m_passwordErr = new QLabel(card);
    m_passwordErr->setObjectName("err_password");
    m_passwordErr->setWordWrap(true);
    m_passwordErr->setVisible(false);
    formLayout->addRow("", m_passwordErr);

    // 确认密码
    m_confirmInput = new QLineEdit(card);
    m_confirmInput->setObjectName("input_confirm");
    m_confirmInput->setPlaceholderText("请再次输入密码");
    m_confirmInput->setEchoMode(QLineEdit::Password);
    m_confirmInput->setFixedHeight(40);
    formLayout->addRow("确认密码", m_confirmInput);
    m_confirmErr = new QLabel(card);
    m_confirmErr->setObjectName("err_confirm");
    m_confirmErr->setWordWrap(true);
    m_confirmErr->setVisible(false);
    formLayout->addRow("", m_confirmErr);

    cardLayout->addLayout(formLayout);

    // 条款与协议行（参考登录页 bottomRow 风格）
    auto* termsLayout = new QHBoxLayout();
    termsLayout->setContentsMargins(0, 4, 0, 0);         // 与登录页统一
    termsLayout->setSpacing(0);                          // 与登录页统一

    m_termsBox = new QCheckBox("我已阅读并同意用户协议和隐私政策", card);
    m_termsBox->setObjectName("termsCheckbox");
    termsLayout->addWidget(m_termsBox);
    termsLayout->addStretch();

    m_termsButton = new QPushButton("查看用户协议", card);
    m_termsButton->setObjectName("termsButton");
    m_termsButton->setFlat(true);
    m_termsButton->setCursor(Qt::PointingHandCursor);
    termsLayout->addWidget(m_termsButton);
    cardLayout->addLayout(termsLayout);

    // 注册按钮：固定 280×44，居中，不拉伸（与登录页完全一致）
    auto* btnRow = new QHBoxLayout();
    btnRow->setAlignment(Qt::AlignCenter);
    m_submitBtn = new QPushButton("注册", card);
    m_submitBtn->setObjectName("registerSubmitBtn");
    m_submitBtn->setFixedSize(280, 44);                  // 【修改】与登录页统一
    m_submitBtn->setEnabled(false);
    connect(m_submitBtn, &QPushButton::clicked, this, &RegisterPage::onRegisterClicked);
    btnRow->addWidget(m_submitBtn);
    cardLayout->addLayout(btnRow);

    // 切换登录链接（参考登录页 switchRow，修正 HTML 并统一交互行为）
    auto* switchRow = new QHBoxLayout();
    switchRow->setAlignment(Qt::AlignCenter);
    m_switchLabel = new QLabel("已有账户？<a href='login'>去登录</a>", card); // 【修正】去掉多余的 <
    m_switchLabel->setObjectName("switchLoginLink");
    m_switchLabel->setTextFormat(Qt::RichText);
    m_switchLabel->setTextInteractionFlags(Qt::TextBrowserInteraction); // 【新增】与登录页统一
    m_switchLabel->setOpenExternalLinks(false);                         // 【新增】与登录页统一
    connect(m_switchLabel, &QLabel::linkActivated, this, &RegisterPage::onSwitchToLoginClicked);
    switchRow->addWidget(m_switchLabel);
    cardLayout->addLayout(switchRow);

    mainLayout->addWidget(card, 0, Qt::AlignCenter);

    // 输入实时校验与错误清除
    connect(m_usernameInput, &QLineEdit::textChanged, this, &RegisterPage::clearAllErrors);
    connect(m_emailInput,    &QLineEdit::textChanged, this, &RegisterPage::clearAllErrors);
    connect(m_passwordInput, &QLineEdit::textChanged, this, &RegisterPage::onPasswordTextChanged);
    connect(m_confirmInput,  &QLineEdit::textChanged, this, &RegisterPage::onConfirmPasswordTextChanged);
    connect(m_termsBox,      &QCheckBox::stateChanged, this, &RegisterPage::onTermsAgreedChanged);
    connect(m_termsButton,   &QPushButton::clicked,   this, &RegisterPage::onTermsAgreedChanged);
}

void RegisterPage::connectUserSessionSignals()
{
    if (m_signalsConnected) {
        qDebug() << "RegisterPage 信号已连接，跳过重复连接";
        return;
    }

    bool c1 = connect(m_userSession, &UserSession::registerSuccess,
                      this, &RegisterPage::onRegisterSuccess);
    bool c2 = connect(m_userSession, &UserSession::registerFailed,
                      this, &RegisterPage::onRegisterFailed);
    bool c3 = connect(m_userSession, &UserSession::userChanged,
                      this, &RegisterPage::onUserChanged);

    m_signalsConnected = true;
    qDebug() << "RegisterPage UserSession信号连接:"
             << "registerSuccess:" << c1
             << "registerFailed:" << c2
             << "userChanged:" << c3;
}

/* =========================================================
   业务逻辑
   ========================================================= */
void RegisterPage::onRegisterClicked()
{
    if (!validateInputs()) return;

    setFormEnabled(false);
    setUIState(UIState::Loading);
    showMessage("正在注册...", false, 0);

    QString username = m_usernameInput->text().trimmed();
    QString password = m_passwordInput->text();
    m_userSession->registerUser(username, password);
}

void RegisterPage::onSwitchToLoginClicked()
{
    emit navigateToLogin();
}

void RegisterPage::onTermsAgreedChanged()
{
    validateInput();
}

void RegisterPage::onAvatarSelected()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "选择头像", "", "Image Files (*.png *.jpg *.bmp)");
    if (!filePath.isEmpty()) {
        m_avatarPath = filePath;
        QPixmap pixmap(filePath);
        m_avatarLabel->setPixmap(pixmap.scaled(
            m_avatarLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// register_page.cpp
void RegisterPage::onRegisterSuccess(const QString& username)
{
    qDebug() << "RegisterPage::onRegisterSuccess 被调用，用户名:" << username;

    setUIState(UIState::Normal);
    clearMessage();
    setFormEnabled(true);

    showMessage("注册成功！欢迎加入我们", false, 2000);

    UserData user = m_userSession->currentUser();
    emit registerSuccess(user);
    qDebug() << "RegisterPage 发射 registerSuccess 信号，userId:" << user.userId();

    if (!m_avatarPath.isEmpty()) {
        bool ok = m_userSession->saveUserAvatar(m_avatarPath);
        if (!ok) {
            QMessageBox::warning(this, "提示", "头像保存失败，请在个人中心重试");
        }
    }

    emit navigateToLogin();
}

void RegisterPage::onRegisterFailed(const QString& error)
{
    setUIState(UIState::Normal);
    clearMessage();
    setFormEnabled(true);

    showMessage("注册失败：" + error, true, 3000);

    m_passwordInput->clear();
    m_confirmInput->clear();
    m_usernameInput->setFocus();
}

void RegisterPage::onUserChanged(const UserData& user)
{
    if (user.isLoggedIn()) {
        // 注册成功后会自动登录，可在此做额外处理
    } else if (!user.isValid()) {
        // 用户登出或注销，可做清理
    }
}

/* =========================================================
   校验与错误提示
   ========================================================= */
bool RegisterPage::validateInputs()
{
    clearAllErrors();

    QString username = m_usernameInput->text().trimmed();
    QString password = m_passwordInput->text();
    QString confirm  = m_confirmInput->text();
    QString email    = m_emailInput->text().trimmed();
    bool termsAgreed = m_termsBox->isChecked();

    bool ok = true;

    if (username.isEmpty()) {
        showFieldError(m_usernameInput, m_usernameErr, "请填写用户名");
        ok = false;
    } else if (username.length() < 3 || username.length() > 20) {
        showFieldError(m_usernameInput, m_usernameErr, "用户名长度应为3-20个字符");
        ok = false;
    } else {
        QRegularExpression re("^[a-zA-Z0-9_]{3,20}$");
        if (!re.match(username).hasMatch()) {
            showFieldError(m_usernameInput, m_usernameErr,
                           "用户名只能包含字母、数字和下划线");
            ok = false;
        }
    }

    if (email.isEmpty()) {
        showFieldError(m_emailInput, m_emailErr, "请填写邮箱");
        ok = false;
    } else {
        QRegularExpression re("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
        if (!re.match(email).hasMatch()) {
            showFieldError(m_emailInput, m_emailErr, "邮箱格式不正确");
            ok = false;
        }
    }

    if (password.isEmpty()) {
        showFieldError(m_passwordInput, m_passwordErr, "请填写密码");
        ok = false;
    } else if (password.length() < 6) {
        showFieldError(m_passwordInput, m_passwordErr, "密码长度至少6位");
        ok = false;
    }

    if (confirm.isEmpty()) {
        showFieldError(m_confirmInput, m_confirmErr, "请确认密码");
        ok = false;
    } else if (password != confirm) {
        showFieldError(m_confirmInput, m_confirmErr, "两次输入的密码不一致");
        ok = false;
    }

    if (!termsAgreed) {
        showMessage("请同意用户协议和隐私政策", true, 2000);
        ok = false;
    }

    return ok;
}

void RegisterPage::showFieldError(QLineEdit* field, QLabel* label, const QString& msg)
{
    if (!field || !label) return;
    field->setProperty("validationState", "error");
    field->style()->unpolish(field);
    field->style()->polish(field);

    label->setText(msg);
    label->setVisible(true);
    if (m_submitBtn) m_submitBtn->setEnabled(false);
}

void RegisterPage::clearFieldError(QLineEdit* field, QLabel* label)
{
    if (!field || !label) return;
    if (field->property("validationState").toString() == "error") {
        field->setProperty("validationState", "normal");
        field->style()->unpolish(field);
        field->style()->polish(field);
    }
    label->clear();
    label->setVisible(false);
    validateInput();   // 恢复按钮可用性
}

void RegisterPage::clearAllErrors()
{
    clearFieldError(m_usernameInput, m_usernameErr);
    clearFieldError(m_emailInput, m_emailErr);
    clearFieldError(m_passwordInput, m_passwordErr);
    clearFieldError(m_confirmInput, m_confirmErr);
}

void RegisterPage::onPasswordTextChanged(const QString&)
{
    clearFieldError(m_passwordInput, m_passwordErr);
    clearFieldError(m_confirmInput, m_confirmErr);
}

void RegisterPage::onConfirmPasswordTextChanged(const QString&)
{
    clearFieldError(m_confirmInput, m_confirmErr);
}

void RegisterPage::validateInput()
{
    if (!m_usernameInput || !m_passwordInput || !m_confirmInput ||
        !m_emailInput || !m_termsBox || !m_submitBtn) {
        return;
    }

    QString username = m_usernameInput->text().trimmed();
    QString password = m_passwordInput->text();
    QString confirm  = m_confirmInput->text();
    QString email    = m_emailInput->text().trimmed();

    bool valid = !username.isEmpty() && !password.isEmpty() &&
                 !confirm.isEmpty() && !email.isEmpty() &&
                 password == confirm &&
                 password.length() >= 6 &&
                 username.length() >= 3 && username.length() <= 20 &&
                 m_termsBox->isChecked();

    m_submitBtn->setEnabled(valid);
}

/* =========================================================
   辅助方法
   ========================================================= */
void RegisterPage::setFormEnabled(bool enabled)
{
    if (m_usernameInput) m_usernameInput->setEnabled(enabled);
    if (m_emailInput)    m_emailInput->setEnabled(enabled);
    if (m_passwordInput) m_passwordInput->setEnabled(enabled);
    if (m_confirmInput)  m_confirmInput->setEnabled(enabled);
    if (m_termsBox)      m_termsBox->setEnabled(enabled);
    if (m_avatarButton)  m_avatarButton->setEnabled(enabled);
    if (m_termsButton)   m_termsButton->setEnabled(enabled);

    if (enabled) validateInput();
    else if (m_submitBtn) m_submitBtn->setEnabled(false);
}

void RegisterPage::setRegisterData(const QString& username, const QString& email)
{
    if (!m_usernameInput || !m_emailInput) {
        qWarning() << "RegisterPage UI 未初始化完成";
        return;
    }

    m_initializing = true;
    m_usernameInput->setText(username);
    m_emailInput->setText(email);

    if (!username.isEmpty()) m_passwordInput->setFocus();
    else                     m_usernameInput->setFocus();

    validateInput();
    m_initializing = false;
}

void RegisterPage::setFocusToFirstField()
{
    if (m_usernameInput) m_usernameInput->setFocus();
}

/* =========================================================
   主题与生命周期
   ========================================================= */
void RegisterPage::onThemeChanged(UITheme theme)
{
    Q_UNUSED(theme);
    // 全局 QSS 已接管颜色、边框、焦点态等。
    // 注册页无额外自定义绘制资源，无需处理。
}

void RegisterPage::showEvent(QShowEvent* event)
{
    BaseWidget::showEvent(event);
    qDebug() << "RegisterPage 页面显示";
    if (m_usernameInput) m_usernameInput->setFocus();
}

void RegisterPage::hideEvent(QHideEvent* event)
{
    BaseWidget::hideEvent(event);
    qDebug() << "RegisterPage 页面隐藏";
}
