#ifndef PAGE_CONTROLLER_H
#define PAGE_CONTROLLER_H

#include "base/base_widget.h"
#include"base/theme_manager.h"
#include "login_page.h"
#include "register_page.h"
#include "main_page.h"
#include "collecte_page.h"
#include "vocabulary_page.h"
#include "export_manager.h"
#include <QStackedWidget>

class PageController : public BaseWidget
{
    Q_OBJECT

public:
    explicit PageController(QWidget* parent = nullptr);
    ~PageController() override;

    enum PageType {
        Login = 0,
        Register = 1,
        Main = 2,
        Collect = 3,
        Vocabulary = 4
    };

    void setCurrentPage(PageType pageType);
    LoginPage* getLoginPage() const { return m_loginPage; }
    RegisterPage* getRegisterPage() const { return m_registerPage; }
    MainPage* getMainPage() const { return m_mainPage; }

signals:
    void loginSuccess(const UserData& user);
    void registerSuccess(const UserData& user);

private slots:
    void handleLoginSuccess(const UserData& user);
    void handleRegisterSuccess(const UserData& user);
    void handleNavigateToRegister();
    void handleNavigateToLogin();
    void handleNavigateToMain();
    void handleNavigateToCollect();
    void handleNavigateToVocabulary();
    void handleCollectToMain();
    void handleVocabularyToMain();

private:
    void setupPages();
    void connectPageSignals();

    QStackedWidget* m_stackedWidget{nullptr};
    LoginPage* m_loginPage{nullptr};
    RegisterPage* m_registerPage{nullptr};
    MainPage* m_mainPage{nullptr};
    CollectePage* m_collectPage{nullptr};
    VocabularyPage* m_vocabularyPage{nullptr};
};

#endif // PAGE_CONTROLLER_H
