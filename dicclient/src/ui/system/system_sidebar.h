#ifndef SYSTEM_SIDEBAR_H
#define SYSTEM_SIDEBAR_H

#include "base/base_widget.h"
#include "user_session.h"
#include <QPropertyAnimation>
#include <QPointer>

class QPushButton;
class QVBoxLayout;
class QLabel;
class QSpacerItem;

/**
 * @brief 系统功能侧边栏
 *
 * 继承自BaseWidget，提供：
 * 1. 主题切换按钮（通过ThemeManager全局切换）
 * 2. 退出登录/注销账号按钮
 * 3. 收藏单词/生词本页面跳转
 * 4. 用户头像和用户名显示
 * 5. 展开/收起动画
 * 6. 自适应主题的QSS样式（通过动态属性驱动）
 */
class SystemSidebar : public BaseWidget
{
    Q_OBJECT
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(int sidebarWidth READ currentWidth WRITE setSidebarWidth)

public:
    explicit SystemSidebar(QWidget* parent = nullptr, UserSession* userSession = nullptr);
    ~SystemSidebar() override;

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    void toggle();

    int currentWidth() const { return m_currentWidth; }
    void setSidebarWidth(int width);

    // 尺寸常量
    static constexpr int EXPANDED_WIDTH = 180;
    static constexpr int COLLAPSED_WIDTH = 50;

signals:
    void expandedChanged(bool expanded);
    void logoutRequested();
    void deleteAccountRequested();
    void loginRequested(const QString& username, const QString& password);
    void registerRequested(const QString& username, const QString& password);
    void showCollectPageRequested();
    void showVocabularyPageRequested();
    void themeToggleRequested();

public slots:
    void onUserLoggedIn(const UserData& user);
    void onUserLoginFailed(const QString& error);
    void onUserLogoutSuccess();
    void onUserUnregisterSuccess(const QString& username, int userId);
    void onUserUnregisterFailed(const QString& error);
    void onUserChanged(const UserData& user);

private slots:
    void handleThemeButtonClicked();
    void handleLogoutButtonClicked();
    void handleDeleteAccountButtonClicked();
    void handleCollectButtonClicked();
    void handleVocabularyButtonClicked();
    void onAnimationFinished();
    void updateUserInfo(const UserData& user);
    void updateUserButtonsState();

private:
    void setupUI();
    void setupConnections();
    void createButtons();
    void setupAnimations();
    void startExpandAnimation(bool expand);
    void updateSidebarContent();

    void resizeEvent(QResizeEvent* event) override;
    void onThemeChanged(UITheme theme) override;

    // 成员变量
    UserSession* m_userSession = nullptr;

    bool m_expanded = false;
    int m_currentWidth = COLLAPSED_WIDTH;

    // 布局
    QVBoxLayout* m_buttonLayout = nullptr;
    QWidget* m_buttonContainer = nullptr;
    QWidget* m_topButtons = nullptr;
    QVBoxLayout* m_topLayout = nullptr;
    QWidget* m_bottomContainer = nullptr;
    QVBoxLayout* m_bottomLayout = nullptr;
    QSpacerItem* m_spacer = nullptr;

    // 按钮组件
    QPushButton* m_themeButton = nullptr;
    QPushButton* m_logoutButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_toggleButton = nullptr;
    QPushButton* m_collectButton = nullptr;
    QPushButton* m_vocabularyButton = nullptr;

    // 动画
    QPropertyAnimation* m_widthAnimation = nullptr;

    // 用户信息显示
    QLabel* m_avatarLabel = nullptr;
    QLabel* m_usernameLabel = nullptr;
    int m_avatarSize = 70;
};

#endif // SYSTEM_SIDEBAR_H
