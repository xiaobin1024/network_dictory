#include "page_controller.h"
#include <QVBoxLayout>
#include <QDebug>

PageController::PageController(QWidget* parent)
    : BaseWidget(parent)
{
    // 1. 初始化亮色主题（会自动加载 :/styles/light.qss）
    ThemeManager::instance()->setTheme(UITheme::Light);
    qDebug() << "初始主题已设置为 Light";

    qDebug() << "PageController 创建";

    // 基类已创建 m_mainLayout，此处直接设置对象名和窗口标题
    setObjectName("PageController");
    setWindowTitle("网络词典 - 登录/注册");

    // 创建堆栈控件并添加到基类提供的主布局
    m_stackedWidget = new QStackedWidget(this);
    mainLayout()->addWidget(m_stackedWidget);

    // 创建页面并建立信号连接
    setupPages();
    connectPageSignals();

    // 初始显示登录页面
    setCurrentPage(Login);
}

PageController::~PageController()
{
    qDebug() << "PageController 销毁";
}

void PageController::setupPages()
{
    // 创建页面
    m_loginPage = new LoginPage(this);
    m_registerPage = new RegisterPage(this);
    m_mainPage = new MainPage(this);
    m_collectPage = new CollectePage(this);
    m_vocabularyPage = new VocabularyPage(this);

    // 添加到堆栈控件
    m_stackedWidget->addWidget(m_loginPage);
    m_stackedWidget->addWidget(m_registerPage);
    m_stackedWidget->addWidget(m_mainPage);
    m_stackedWidget->addWidget(m_collectPage);
    m_stackedWidget->addWidget(m_vocabularyPage);

    // 绑定收藏/生词页面的导出请求到 ExportManager
    connect(m_collectPage, &CollectePage::exportRequested,
            ExportManager::instance(), &ExportManager::onExportRequested);
    connect(m_vocabularyPage, &VocabularyPage::exportRequested,
            ExportManager::instance(), &ExportManager::onExportRequested);
}

void PageController::connectPageSignals()
{
    qDebug() << "PageController::connectPageSignals()";

    // // 安全检查
    // if (!m_loginPage || !m_registerPage || !m_mainPage) {
    //     qWarning() << "PageController::connectPageSignals() - 页面对象未初始化";
    //     return;
    // }

    // 连接登录页面信号
    connect(m_loginPage, &LoginPage::loginSuccess,
            this, &PageController::handleLoginSuccess);
    connect(m_loginPage, &LoginPage::navigateToRegister,
            this, &PageController::handleNavigateToRegister);
    connect(m_loginPage, &LoginPage::navigateToMain,
            this, &PageController::handleNavigateToMain);

    // 连接注册页面信号
    connect(m_registerPage, &RegisterPage::registerSuccess,
            this, &PageController::handleRegisterSuccess);
    connect(m_registerPage, &RegisterPage::navigateToLogin,
            this, &PageController::handleNavigateToLogin);
    // connect(m_registerPage, &RegisterPage::navigateToMain,
    //         this, &PageController::handleNavigateToMain);

    // 连接主页面信号
    connect(m_mainPage, &MainPage::navigateToLogin,
            this, &PageController::handleNavigateToLogin);
    connect(m_mainPage, &MainPage::navigateToRegister,
            this, &PageController::handleNavigateToRegister);

    connect(m_mainPage, &MainPage::showCollectPageRequested,
            this, &PageController::handleNavigateToCollect);

    connect(m_collectPage, &CollectePage::showMainPageRequested,
            this, &PageController::handleCollectToMain);

    connect(m_mainPage, &MainPage::showVocabularyRequested,
            this, &PageController::handleNavigateToVocabulary);
    connect(m_vocabularyPage, &VocabularyPage::showMainPageRequested,
            this, &PageController::handleVocabularyToMain);
}

/* =========================================================
 *  核心修改：通过指针查索引，枚举值不再等于堆栈索引
 * ========================================================= */
void PageController::setCurrentPage(PageType pageType)
{
    QWidget* target = nullptr;

    switch (pageType) {
    case Login:      target = m_loginPage;      break;
    case Register:   target = m_registerPage;   break;
    case Main:       target = m_mainPage;       break;
    case Collect:    target = m_collectPage;    break;
    case Vocabulary: target = m_vocabularyPage; break;
    }

    if (!target) {
        qWarning() << "PageController: 页面类型" << pageType
                   << "尚未实现或已被注释，跳过跳转";
        return;
    }

    int idx = m_stackedWidget->indexOf(target);
    if (idx == -1) {
        qWarning() << "PageController: 页面类型" << pageType
                   << "的指针对象未添加到堆栈控件";
        return;
    }

    if (m_stackedWidget->currentIndex() == idx) {
        return;
    }

    m_stackedWidget->setCurrentIndex(idx);
    qDebug() << "PageController: 已切换到页面" << pageType << "，堆栈索引" << idx;
}

void PageController::handleLoginSuccess(const UserData& user)
{
    emit loginSuccess(user);
    setCurrentPage(Main);
}

void PageController::handleRegisterSuccess(const UserData& user)
{
    emit registerSuccess(user);
    setCurrentPage(Login);
}

void PageController::handleNavigateToRegister()
{
    setCurrentPage(Register);
}

void PageController::handleNavigateToLogin()
{
    setCurrentPage(Login);
}

void PageController::handleNavigateToMain()
{
    UserData currentUser = UserSession::instance()->currentUser();
    if (currentUser.isLoggedIn()) {
        emit loginSuccess(currentUser);
    }
}

void PageController::handleNavigateToCollect()
{
    qDebug() << "PageController::handleNavigateToCollect()";
    qDebug() << "准备跳转收藏页面";
    setCurrentPage(Collect);
}

void PageController::handleCollectToMain()
{
    qDebug() << "PageController::handleCollectToMain()";
    setCurrentPage(Main);
}

void PageController::handleNavigateToVocabulary()
{
    qDebug() << "PageController::handleNavigateToVocabulary()";
    qDebug() << "准备跳转生词本页面";
    setCurrentPage(Vocabulary);
}

void PageController::handleVocabularyToMain()
{
    qDebug() << "PageController::handleVocabularyToMain()";
    setCurrentPage(Main);
}
