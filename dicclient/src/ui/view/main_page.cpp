#include "main_page.h"
#include "user_session.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QLabel>
#include <QRegularExpressionValidator>
#include <QDebug>

MainPage::MainPage(QWidget* parent)
    : BaseWidget(parent)
    , m_userSession(UserSession::instance())
    , m_wordSearch(WordSearch::instance())
    , m_wordCollect(WordCollect::instance())
    , m_wordVocabulary(WordVocabulary::instance())
{
    qDebug() << "MainPage 创建";
    setObjectName("MainPage");
    setWindowTitle("主页面 - 网络词典");

    setupUI();
    setupConnections();
}

MainPage::~MainPage()
{
    qDebug() << "MainPage 销毁";
}

/* ================================================================
 *  界面组装（旧版业务逻辑保留，样式完全交由 QSS）
 * ================================================================ */
void MainPage::setupUI()
{
    // 使用 BaseWidget 提供的 mainLayout() 作为根垂直布局
    auto* rootLayout = new QHBoxLayout();
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    mainLayout()->addLayout(rootLayout);

    // ---------- 左侧内容区 ----------
    auto* contentArea = new QWidget(this);
    contentArea->setObjectName("mainContentArea");

    auto* contentAreaLayout = new QVBoxLayout(contentArea);
    contentAreaLayout->setContentsMargins(0, 0, 0, 0);
    contentAreaLayout->setSpacing(0);

    setupHeader();
    setupSearchSection();
    setupContentArea();

    contentAreaLayout->addWidget(m_searchTitleLabel);
    contentAreaLayout->addWidget(m_searchContainer);
    contentAreaLayout->addWidget(m_contentScrollArea, 1);
    contentAreaLayout->addStretch();

    // ---------- 右侧侧边栏 ----------
    setupSidebar();

    // 组装
    rootLayout->addWidget(contentArea, 1);
    rootLayout->addWidget(m_sidebar);
}

void MainPage::setupHeader()
{
    m_searchTitleLabel = new QLabel("网络词典", this);
    m_searchTitleLabel->setObjectName("searchTitleLabel");
    m_searchTitleLabel->setAlignment(Qt::AlignCenter);
    // 字体、颜色、边距等全部交由 light/dark.qss 控制
}

void MainPage::setupSearchSection()
{
    // 搜索组件
    m_searchWidget = new SearchHistoryWidget(this, m_userSession);
    m_searchWidget->setObjectName("searchWidget");
    m_searchWidget->setMinimumHeight(50);
    m_searchWidget->setMinimumWidth(200);
    m_searchWidget->setPlaceholder("请输入要搜索的单词...");

    // 输入验证器（保留旧版业务）
    QRegularExpression regex("^[a-zA-Z0-9_\\s]+$");
    auto* validator = new QRegularExpressionValidator(regex, this);
    m_searchWidget->inputField()->setValidator(validator);

    // 搜索容器（用于控制整体宽度与边距）
    m_searchContainer = new QWidget(this);
    m_searchContainer->setObjectName("searchContainer");

    auto* searchContainerLayout = new QHBoxLayout(m_searchContainer);
    searchContainerLayout->setContentsMargins(20, 0, 20, 0);
    searchContainerLayout->setSpacing(0);
    searchContainerLayout->addWidget(m_searchWidget);

    m_searchContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MainPage::setupContentArea()
{
    // 滚动区域
    m_contentScrollArea = new QScrollArea(this);
    m_contentScrollArea->setObjectName("contentScrollArea");
    m_contentScrollArea->setWidgetResizable(true);
    m_contentScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_contentScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_contentScrollArea->setMinimumHeight(500);

    // 内容载体
    m_contentWidget = new QWidget();
    m_contentWidget->setObjectName("contentWidget");

    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(50, 50, 50, 50);
    m_contentLayout->setSpacing(20);

    m_contentScrollArea->setWidget(m_contentWidget);

    // 顶部占位，使欢迎信息视觉居中
    m_contentLayout->addStretch(7);

    auto* welcomeLabel = new QLabel("欢迎使用网络词典！", m_contentWidget);
    welcomeLabel->setObjectName("welcomeLabel");
    welcomeLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(welcomeLabel);

    m_contentLayout->addStretch(3);
}

void MainPage::setupSidebar()
{
    m_sidebar = new SystemSidebar(this, m_userSession);
    m_sidebar->setObjectName("systemSidebar");
    m_sidebar->setFixedWidth(50);
}

/* ================================================================
 *  信号连接（完全保留旧版业务逻辑）
 * ================================================================ */
void MainPage::setupConnections()
{
    qDebug() << "MainPage::setupConnections()";

    // 搜索框信号
    connect(m_searchWidget, &SearchHistoryWidget::keywordChanged,
            this, &MainPage::onKeywordChanged);
    connect(m_searchWidget, &SearchHistoryWidget::searchRequested,
            this, &MainPage::onSearchRequested);
    connect(m_searchWidget, &SearchHistoryWidget::historyItemClicked,
            this, &MainPage::onSearchRequested);

    // 用户会话：注销 / 注销账户后回到登录页
    connect(m_userSession, &UserSession::logoutSuccess,
            this, &MainPage::navigateToLogin);
    connect(m_userSession, &UserSession::unregisterSuccess,
            this, &MainPage::navigateToLogin);

    // 单词搜索服务
    connect(m_wordSearch, &WordSearch::searchSuccess,
            this, &MainPage::onWordSearchSuccess);
    connect(m_wordSearch, &WordSearch::searchFailed,
            this, &MainPage::onWordSearchFailed);

    connect(m_sidebar, &SystemSidebar::deleteAccountRequested,
            this, &MainPage::navigateToLogin);
    connect(m_sidebar, &SystemSidebar::showCollectPageRequested,
            this, &MainPage::showCollectPageRequested);
    connect(m_sidebar, &SystemSidebar::showVocabularyPageRequested,
            this, &MainPage::showVocabularyRequested);

    // 注入 UserSession（保留旧版初始化逻辑）
    m_wordSearch->setUserSession(m_userSession);
    m_wordCollect->setUserSession(m_userSession);
    m_wordVocabulary->setUserSession(m_userSession);
}

/* ================================================================
 *  业务槽函数（与旧版行为一致）
 * ================================================================ */
void MainPage::onSearchRequested(const QString& query)
{
    if (!query.isEmpty()) {
        m_wordSearch->searchWord(query);
    }
}

void MainPage::onWordSearchSuccess(const WordData& wordData)
{
    setCurrentWordCard(wordData);
}

void MainPage::onWordSearchFailed(const QString& error)
{
    showMessage("搜索失败: " + error, true, 2000);
}

void MainPage::onClearButtonClicked()
{
    if (m_searchWidget) {
        m_searchWidget->clearInput();
        m_searchWidget->setFocusToInput();
    }
}

void MainPage::setSearchPlaceholder(const QString& placeholder)
{
    if (m_searchWidget) {
        m_searchWidget->setPlaceholder(placeholder);
    }
}

void MainPage::setSearchResults(const QStringList& results)
{
    // 清理旧结果（保留首尾 stretch）
    while (m_contentLayout->count() > 1) {
        QLayoutItem* item = m_contentLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (results.isEmpty()) {
        auto* noResultsLabel = new QLabel("没有找到匹配的结果", m_contentWidget);
        noResultsLabel->setObjectName("noResultsLabel");
        noResultsLabel->setAlignment(Qt::AlignCenter);
        m_contentLayout->insertWidget(0, noResultsLabel);
        return;
    }

    for (const QString& result : results) {
        auto* resultLabel = new QLabel(result, m_contentWidget);
        resultLabel->setObjectName("resultLabel");
        m_contentLayout->insertWidget(0, resultLabel);
    }
}

void MainPage::setCurrentWordCard(const WordData& wordData)
{
    // 1. 移除旧卡片
    if (m_currentWordCard) {
        m_contentLayout->removeWidget(m_currentWordCard);
        m_currentWordCard->deleteLater();
        m_currentWordCard = nullptr;
    }

    // 2. 清空布局中的旧内容（welcomeLabel 和 stretch）
    //    注意：这会移除欢迎信息。如果你后续需要恢复欢迎页，
    //    建议改用下面的 QStackedWidget 方案。
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->hide();  // 先隐藏，避免闪烁
            item->widget()->deleteLater();
        }
        delete item;
    }

    // 3. 创建新卡片
    m_currentWordCard = new InteractiveWordCard(
        wordData, m_wordCollect, m_wordVocabulary, this);
    m_currentWordCard->setObjectName("currentWordCard");

    // 【修改】让卡片等比放大：占据内容展示区域约一半宽度，随窗口自适应
    updateCurrentCardWidth();

    // 4. 用 Stretch 实现垂直居中，AlignHCenter 实现水平居中
    m_contentLayout->addStretch(1);  // 上方弹性占位
    m_contentLayout->addWidget(m_currentWordCard, 0, Qt::AlignHCenter);
    m_contentLayout->addStretch(1);  // 下方弹性占位
}

void MainPage::updateCurrentCardWidth()
{
    if (!m_currentWordCard) return;

    // 以滚动区视口宽度计算可展示宽度（去掉内容边距 50*2）
    int viewportWidth = m_contentScrollArea ? m_contentScrollArea->viewport()->width() : 0;
    if (viewportWidth <= 0) {
        viewportWidth = 900;  // 布局未就绪时的兜底值
    }
    int contentWidth = qMax(0, viewportWidth - 100);
    // 卡片宽度约为展示区域的 7 成，避免中间区域太空，且随窗口自适应
    int targetWidth = contentWidth * 7 / 10;

    // 限制在合理范围内，避免过小或过大
    targetWidth = qBound(380, targetWidth, 760);
    m_currentWordCard->setFixedWidth(targetWidth);
}

void MainPage::resizeEvent(QResizeEvent* event)
{
    BaseWidget::resizeEvent(event);   // 保留消息悬浮层位置更新
    updateCurrentCardWidth();
}

void MainPage::onKeywordChanged(const QString& keyword)
{
    if (keyword.isEmpty() && m_currentWordCard) {
        m_contentLayout->removeWidget(m_currentWordCard);
        m_currentWordCard->deleteLater();
        m_currentWordCard = nullptr;
    }
}

/* ================================================================
 *  主题响应（新版 BaseWidget 保留的虚函数）
 * ================================================================ */
void MainPage::onThemeChanged(UITheme theme)
{
    // 全局 QSS 已自动处理背景、边框、字体、滚动条等样式。
    // 此处仅处理 QSS 无法覆盖的自定义属性（如第三方控件或动态绘制）。
    // if (m_currentWordCard) {
    //     m_currentWordCard->setUITheme(theme);
    // }
    Q_UNUSED(theme)

}
