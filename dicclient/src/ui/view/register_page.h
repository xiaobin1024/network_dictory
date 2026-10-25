#ifndef REGISTER_PAGE_H
#define REGISTER_PAGE_H

#include "base/base_widget.h"
#include "user_session.h"
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QShowEvent>
#include <QHideEvent>

class RegisterPage : public BaseWidget
{
    Q_OBJECT

public:
    explicit RegisterPage(QWidget* parent = nullptr);
    ~RegisterPage() override;

    void setRegisterData(const QString& username = "", const QString& email = "");
    void setFocusToFirstField();

signals:
    void registerSuccess(const UserData& user);
    void navigateToLogin();
    void navigateToMain();

private slots:
    void onRegisterClicked();
    void onSwitchToLoginClicked();
    void onTermsAgreedChanged();
    void onAvatarSelected();

    void onPasswordTextChanged(const QString& text);
    void onConfirmPasswordTextChanged(const QString& text);
    void clearAllErrors();

    // UserSession 信号处理
    void onRegisterSuccess(const QString& username);
    void onRegisterFailed(const QString& error);
    void onUserChanged(const UserData& user);

protected:
    void onThemeChanged(UITheme theme) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setupUI();
    void connectUserSessionSignals();
    bool validateInputs();
    void showFieldError(QLineEdit* field, QLabel* label, const QString& msg);
    void clearFieldError(QLineEdit* field, QLabel* label);
    void setFormEnabled(bool enabled);
    void validateInput();

    QLabel* m_titleLabel=nullptr;

    // 头像
    QLabel* m_avatarLabel = nullptr;
    QPushButton* m_avatarButton = nullptr;
    QString m_avatarPath;

    // 表单输入
    QLineEdit* m_usernameInput = nullptr;
    QLineEdit* m_emailInput = nullptr;
    QLineEdit* m_passwordInput = nullptr;
    QLineEdit* m_confirmInput = nullptr;
    QCheckBox* m_termsBox = nullptr;

    // 错误提示
    QLabel* m_usernameErr = nullptr;
    QLabel* m_emailErr = nullptr;
    QLabel* m_passwordErr = nullptr;
    QLabel* m_confirmErr = nullptr;

    // 操作按钮
    QPushButton* m_submitBtn = nullptr;
    QPushButton* m_termsButton = nullptr;   // 查看用户协议
    QLabel* m_switchLabel = nullptr;        // 切换登录

    // 服务与状态
    UserSession* m_userSession = nullptr;
    bool m_signalsConnected = false;
    bool m_processingRegistration = false;
    bool m_initializing = false;
};

#endif // REGISTER_PAGE_H
