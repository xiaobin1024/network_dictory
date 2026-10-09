#include "login_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QRegularExpressionValidator>
#include <QDebug>
#include <QShowEvent>
#include <QHideEvent>
#include<QStyle>

LoginPage::LoginPage(QWidget* parent)
    : BaseWidget(parent)
    , m_userSession(UserSession::instance())
    , m_layoutSetupDone(false)
{
    qDebug() << "LoginPage 创建";
    setObjectName("LoginPage");
    setWindowTitle("登录 - 网络词典");

    // 初始化 UserSession（保留旧版逻辑）
    if (!m_userSession->isInitialized()) {
        m_userSession->initialize();
    }

    setupLayout();
    connectSignals();
    setFocusToFirstField();
}

LoginPage::~LoginPage()
{
    qDebug() << "LoginPage 销毁";
    if (m_userSession) {
        m_userSession->disconnect(this);
    }
}

/* =========================================================
   布局（新版卡片式结构，全部使用 objectName，无硬编码样式）
   ========================================================= */
void LoginPage::setupLayout()
{
    if (m_layoutSetupDone) return;
    qDebug() << "LoginPage::setupLayout()";

    // 主布局：让卡片在页面正中央
    mainLayout()->setAlignment(Qt::AlignCenter);
    mainLayout()->setContentsMargins(0, 0, 0, 0);

    // 登录卡片：固定宽度，内部紧凑排列
    auto* card = new QWidget(this);
    card->setObjectName("loginCard");
    card->setFixedWidth(400);           // 固定卡片宽度，不再随窗口拉伸

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 32, 32, 32);
    cardLayout->setSpacing(16);
    cardLayout->setAlignment(Qt::AlignTop);

    // 标题
    m_titleLabel = new QLabel("用户登录", card);
    m_titleLabel->setObjectName("loginTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_titleLabel);

    // 表单区域
    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setContentsMargins(0, 8, 0, 0);

    auto addField = [&](const QString& objPrefix, QLineEdit** input, QLabel** err,
                        const QString& hint, bool isPwd = false)
    {
        auto* line = new QLineEdit(card);
        line->setObjectName(QString("input_%1").arg(objPrefix));
        line->setPlaceholderText(hint);
        line->setFixedHeight(40);       // 统一输入框高度
        if (isPwd) line->setEchoMode(QLineEdit::Password);

        QString labelText = (objPrefix == "account") ? "用户名" : "密码";
        formLayout->addRow(labelText, line);
        *input = line;

        auto* errLabel = new QLabel(card);
        errLabel->setObjectName(QString("err_%1").arg(objPrefix));
        errLabel->setWordWrap(true);
        errLabel->setVisible(false);
        formLayout->addRow("", errLabel);
        *err = errLabel;
    };

    addField("account",  &m_usernameEdit, &m_usernameErr, "请输入用户名");
    addField("password", &m_passwordEdit, &m_passwordErr, "请输入密码", true);

    // 保留旧版用户名正则验证器
    QRegularExpression regex("^[a-zA-Z0-9_]{3,20}$");
    auto* validator = new QRegularExpressionValidator(regex, this);
    m_usernameEdit->setValidator(validator);

    connect(m_usernameEdit, &QLineEdit::textChanged, this, &LoginPage::onInputTextChanged);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &LoginPage::onInputTextChanged);
    // // 失去焦点时做字段级校验（比实时输入更友好）
    // connect(m_usernameEdit, &QLineEdit::editingFinished,
    //         this, &LoginPage::onUsernameBlur);
    // connect(m_passwordEdit, &QLineEdit::editingFinished,
    //         this, &LoginPage::onPasswordBlur);

    // 用事件过滤器替代 editingFinished，确保焦点离开一定触发校验
    m_usernameEdit->installEventFilter(this);
    m_passwordEdit->installEventFilter(this);


    cardLayout->addLayout(formLayout);

    // 记住我 & 忘记密码 行
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 4, 0, 0);
    bottomRow->setSpacing(0);

    m_rememberMeCheckBox = new QCheckBox("记住登录状态", card);
    m_rememberMeCheckBox->setObjectName("rememberCheckbox");
    m_rememberMeCheckBox->setChecked(true);
    bottomRow->addWidget(m_rememberMeCheckBox);
    bottomRow->addStretch();

    m_forgotPasswordButton = new QPushButton("忘记密码?", card);
    m_forgotPasswordButton->setObjectName("forgotPasswordLink");
    m_forgotPasswordButton->setFlat(true);
    m_forgotPasswordButton->setCursor(Qt::PointingHandCursor);
    connect(m_forgotPasswordButton, &QPushButton::clicked,
            this, &LoginPage::onForgotPasswordClicked);
    bottomRow->addWidget(m_forgotPasswordButton);
    cardLayout->addLayout(bottomRow);

    // 登录按钮：固定宽度，居中，不拉伸
    auto* btnRow = new QHBoxLayout();
    btnRow->setAlignment(Qt::AlignCenter);
    m_loginButton = new QPushButton("登录", card);
    m_loginButton->setObjectName("loginSubmitBtn");
    m_loginButton->setFixedSize(280, 44);   // 固定按钮尺寸，不再填充
    m_loginButton->setEnabled(false);
    connect(m_loginButton, &QPushButton::clicked,
            this, &LoginPage::onLoginButtonClicked);
    btnRow->addWidget(m_loginButton);
    cardLayout->addLayout(btnRow);

    // 切换注册链接
    auto* switchRow = new QHBoxLayout();
    switchRow->setAlignment(Qt::AlignCenter);
    m_switchLabel = new QLabel("还没有账号？<a href='register'>立即注册</a>", card);
    m_switchLabel->setObjectName("switchRegisterLink");
    m_switchLabel->setTextFormat(Qt::RichText);
    m_switchLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_switchLabel->setOpenExternalLinks(false);
    connect(m_switchLabel, &QLabel::linkActivated,
            this, &LoginPage::onRegisterButtonClicked);
    switchRow->addWidget(m_switchLabel);
    cardLayout->addLayout(switchRow);

    mainLayout()->addWidget(card, 0, Qt::AlignCenter);
    m_layoutSetupDone = true;
}

/* =========================================================
   信号连接（完全保留旧版 UserSession 连接逻辑）
   ========================================================= */
void LoginPage::connectSignals()
{
    if (m_signalsConnected) {
        qDebug() << "LoginPage 信号已连接，跳过重复连接";
        return;
    }

    bool c1 = connect(m_userSession, &UserSession::loginSuccess,
                      this, &LoginPage::onLoginSuccess);
    bool c2 = connect(m_userSession, &UserSession::loginFailed,
                      this, &LoginPage::onLoginFailed);
    m_signalsConnected = true;

    qDebug() << "LoginPage 信号连接状态:"
             << "loginSuccess:" << c1
             << "loginFailed:" << c2;
}

/* =========================================================
   业务槽（完全保留旧版处理流程，仅替换 showLoading/hideLoading
   为新版 BaseWidget 的 setUIState + 悬浮消息）
   ========================================================= */
void LoginPage::onLoginButtonClicked()
{
    clearAllErrors();

    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    bool rememberMe = m_rememberMeCheckBox->isChecked();

    // 空值字段级校验（新版错误提示样式）
    if (username.isEmpty() || password.isEmpty()) {
        if (username.isEmpty())
            showFieldError(m_usernameEdit, m_usernameErr, "用户名不能为空");
        if (password.isEmpty())
            showFieldError(m_passwordEdit, m_passwordErr, "密码不能为空");
        return;
    }

    // 保留旧版正则与长度校验
    QRegularExpression regex("^[a-zA-Z0-9_]{3,20}$");
    if (!regex.match(username).hasMatch()) {
        showFieldError(m_usernameEdit, m_usernameErr,
                       "用户名格式不正确（3-20个字符，字母数字下划线）");
        return;
    }
    if (password.length() < 6) {
        showFieldError(m_passwordEdit, m_passwordErr, "密码长度不能少于6位");
        return;
    }

    // 禁用控件并进入 Loading 态（适配新版 BaseWidget）
    setControlsEnabled(false);
    setUIState(UIState::Loading);
    showMessage("正在登录...", false, 0);

    m_processingLogin = true;
    m_userSession->login(username, password, rememberMe);
}

void LoginPage::onRegisterButtonClicked()
{
    emit navigateToRegister();
}

void LoginPage::onForgotPasswordClicked()
{
    showMessage("忘记密码功能开发中...", false, 1500);
}

void LoginPage::onLoginSuccess(const UserData& user)
{
    qDebug() << "LoginPage::onLoginSuccess 被调用，用户名:" << user.username();

    // 恢复状态（适配新版 BaseWidget 接口）
    setUIState(UIState::Normal);
    clearMessage();
    setControlsEnabled(true);

    showMessage(QString("登录成功！欢迎回来，%1").arg(user.username()), false, 1500);
    qDebug() << "user.id = " << user.userId();

    emit loginSuccess(user);
    qDebug() << "LoginPage 发射 loginSuccess 信号";

    m_lastLoggedInUser = user;
    m_processingLogin = false;

    emit navigateToMain();
}

void LoginPage::onLoginFailed(const QString& error)
{
    setUIState(UIState::Normal);
    clearMessage();
    setControlsEnabled(true);
    m_processingLogin = false;

    showMessage("登录失败：" + error, true, 3000);
    m_passwordEdit->clear();
    m_passwordEdit->setFocus();
}

/* =========================================================
   输入校验 & 控件状态（保留旧版逻辑）
   ========================================================= */
void LoginPage::validateInput()
{
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    bool valid = !username.isEmpty() && !password.isEmpty() &&
                 username.length() >= 3 && username.length() <= 20 &&
                 password.length() >= 6;

    m_loginButton->setEnabled(valid);
}

void LoginPage::setControlsEnabled(bool enabled)
{
    m_usernameEdit->setEnabled(enabled);
    m_passwordEdit->setEnabled(enabled);
    //m_rememberMeCheckBox->setEnabled(enabled);
    m_loginButton->setEnabled(enabled && !m_usernameEdit->text().trimmed().isEmpty()
                              && !m_passwordEdit->text().isEmpty());
    m_forgotPasswordButton->setEnabled(enabled);
    m_switchLabel->setEnabled(enabled);
}

void LoginPage::setLoginData(const QString& username, bool rememberMe)
{
    if (!m_usernameEdit || !m_rememberMeCheckBox) {
        qWarning() << "LoginPage UI 未初始化完成";
        return;
    }
    m_initializing = true;

    m_usernameEdit->setText(username);
    m_rememberMeCheckBox->setChecked(rememberMe);

    if (!username.isEmpty())
        m_passwordEdit->setFocus();
    else
        m_usernameEdit->setFocus();

    validateInput();
    m_initializing = false;
}

void LoginPage::setFocusToFirstField()
{
    if (m_usernameEdit) m_usernameEdit->setFocus();
}

/* =========================================================
   样式回调（全局 QSS 已接管，此处仅作空实现预留）
   ========================================================= */
void LoginPage::updateWidgetStyles()
{
    // 所有颜色、字体、边框均由 ThemeManager + QSS 控制。
    // 若未来需要替换 Unicode 图标或自定义绘制，可在此处理。
}

void LoginPage::onThemeChanged(UITheme theme)
{
    Q_UNUSED(theme);
    updateWidgetStyles();
}

/* =========================================================
   页面显示/隐藏（保留旧版自动登录逻辑）
   ========================================================= */
void LoginPage::showEvent(QShowEvent* event)
{
    BaseWidget::showEvent(event);
    qDebug() << "LoginPage 页面显示";

    if (!m_layoutSetupDone || !m_usernameEdit) {
        qWarning() << "LoginPage UI 未初始化完成";
        return;
    }

    if (m_userSession->isLoggedIn()) {
        // 已登录，直接进主页面
        UserData user = m_userSession->currentUser();
        emit loginSuccess(user);
    } else {
        // 【修改】不再每次都调用 autoLoginFromSavedSession
        // 只在程序启动时尝试一次；用户退出后不再尝试
        if (m_userSession->tryAutoLoginOnce()) {
            qDebug() << "LoginPage: 已触发自动登录，等待服务器响应";
        } else {
            m_usernameEdit->setFocus();
        }
    }
}

void LoginPage::hideEvent(QHideEvent* event)
{
    BaseWidget::hideEvent(event);
    qDebug() << "LoginPage 页面隐藏";
}


/* =========================================================
   字段级错误提示（配合 QSS 动态属性 validationState）
   ========================================================= */
void LoginPage::onUsernameBlur()
{
    QString username = m_usernameEdit->text().trimmed();
    qDebug() << ">>> onUsernameBlur, username=[" << username << "]";

    if (username.isEmpty()) {
        showFieldError(m_usernameEdit, m_usernameErr, "用户名不能为空");
        return;
    }

    QRegularExpression regex("^[a-zA-Z0-9_]{3,20}$");
    if (!regex.match(username).hasMatch()) {
        showFieldError(m_usernameEdit, m_usernameErr,
                       "用户名格式不正确（3-20个字符，字母数字下划线）");
    }
}

void LoginPage::onPasswordBlur()
{
    QString password = m_passwordEdit->text();
    if (password.isEmpty()){
        showFieldError(m_passwordEdit, m_passwordErr, "密码不能为空");
        return;
    }

    if (password.length() < 6) {
        showFieldError(m_passwordEdit, m_passwordErr,
                       "密码长度不能少于6位");
    }
}
void LoginPage::onInputTextChanged()
{
    // 获取发信号的输入框，只清它自己的红框
    auto* senderEdit = qobject_cast<QLineEdit*>(sender());
    if (senderEdit == m_usernameEdit)
        clearFieldError(m_usernameEdit, m_usernameErr);
    else if (senderEdit == m_passwordEdit)
        clearFieldError(m_passwordEdit, m_passwordErr);

    validateInput();
}

void LoginPage::clearAllErrors()
{
    clearFieldError(m_usernameEdit, m_usernameErr);
    clearFieldError(m_passwordEdit, m_passwordErr);
}

void LoginPage::showFieldError(QLineEdit* field, QLabel* label, const QString& msg)
{
    qDebug() << ">>> showFieldError called for" << field->objectName()
    << ", msg=" << msg;
    field->setProperty("validationState", "error");
    field->style()->unpolish(field);
    field->style()->polish(field);
    label->setText(msg);
    label->setVisible(true);
    qDebug() << ">>> label visible=" << label->isVisible()
             << ", text=" << label->text();
}


void LoginPage::clearFieldError(QLineEdit* field, QLabel* label)
{
     qDebug() << ">>> clearFieldError called for" << field->objectName();
    if (field->property("validationState").toString() == "error") {
        field->setProperty("validationState", "normal");
        field->style()->unpolish(field);
        field->style()->polish(field);
    }
    if (label) {
        label->clear();
        label->setVisible(false);
    }
}

bool LoginPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        if (watched == m_usernameEdit) {
            qDebug() << ">>> usernameEdit FocusOut";
            onUsernameBlur();
        } else if (watched == m_passwordEdit) {
            qDebug() << ">>> passwordEdit FocusOut";
            onPasswordBlur();
        }
    }
    return BaseWidget::eventFilter(watched, event);
}
