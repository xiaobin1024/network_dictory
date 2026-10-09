#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include "base/base_widget.h"
#include "user_session.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

class LoginPage : public BaseWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);
    ~LoginPage() override;

    // 预填充登录数据（保留旧版接口）
    void setLoginData(const QString& username = "", bool rememberMe = false);
    void setFocusToFirstField();

signals:
    // 页面导航信号（完全保留旧版信号名）
    void loginSuccess(const UserData& user);
    void navigateToRegister();
    void navigateToMain();

protected:
    // 样式与生命周期（适配新版 BaseWidget）
    void updateWidgetStyles();
    void onThemeChanged(UITheme theme) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // 按钮槽（保留旧版业务）
    void onLoginButtonClicked();
    void onRegisterButtonClicked();
    void onForgotPasswordClicked();

    // UserSession 信号槽（完全保留）
    void onLoginSuccess(const UserData& user);
    void onLoginFailed(const QString& error);

    // 新版字段级错误清除（QSS 动态属性联动）
    void onInputTextChanged();
    void clearAllErrors();

    void onUsernameBlur();
    void onPasswordBlur();

private:
    void setupLayout();
    void connectSignals();
    void validateInput();
    void setControlsEnabled(bool enabled);

    // 字段级错误提示（新版样式能力）
    void showFieldError(QLineEdit* field, QLabel* label, const QString& msg);
    void clearFieldError(QLineEdit* field, QLabel* label);

    // UI 控件（新版布局结构 + 旧版语义变量名）
    QLineEdit*  m_usernameEdit{nullptr};
    QLineEdit*  m_passwordEdit{nullptr};
    QCheckBox*  m_rememberMeCheckBox{nullptr};
    QPushButton* m_loginButton{nullptr};
    QPushButton* m_forgotPasswordButton{nullptr};
    QLabel*     m_titleLabel{nullptr};
    QLabel*     m_switchLabel{nullptr};

    QLabel*     m_usernameErr{nullptr};   // 用户名错误提示
    QLabel*     m_passwordErr{nullptr};   // 密码错误提示

    // 服务与状态（完全保留旧版成员）
    UserSession* m_userSession{nullptr};
    bool m_initializing{false};
    bool m_layoutSetupDone{false};
    UserData m_lastLoggedInUser;
    bool m_processingLogin{false};
    bool m_signalsConnected{false};
};

#endif // LOGIN_PAGE_H
