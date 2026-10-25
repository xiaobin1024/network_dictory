#include "collecte_page.h"
#include "word/interactive_wordcard.h"

#include <QDebug>
#include <QShowEvent>

CollectePage::CollectePage(QWidget* parent)
    : BaseWidget(parent)
    , m_wordCollect(WordCollect::instance())
    , m_wordVocabulary(WordVocabulary::instance())
    , m_exportManager(ExportManager::instance())
{
    qDebug() << "CollectePage 创建";
    setObjectName("collectPage");
    setWindowTitle("我的收藏");

    setupUI();
    setupConnections();
}

CollectePage::~CollectePage()
{
    qDebug() << "CollectePage 销毁";
}

void CollectePage::setupUI()
{
    auto* layout = mainLayout();
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    // 顶部工具栏：返回按钮 + 标题（左），刷新 + 导出按钮（右）
    auto* headerBar = new QWidget(this);
    headerBar->setObjectName("collectHeaderBar");

    auto* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(10);

    m_backButton = new QPushButton("←", headerBar);
    m_backButton->setObjectName("collectBackBtn");
    m_backButton->setFixedSize(36, 36);
    m_backButton->setToolTip("返回主页面");
    m_backButton->setCursor(Qt::PointingHandCursor);

    m_titleLabel = new QLabel("我的收藏", headerBar);
    m_titleLabel->setObjectName("collectTitleLabel");
    m_titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_refreshButton = new QPushButton("🔄", headerBar);
    m_refreshButton->setObjectName("collectRefreshBtn");
    m_refreshButton->setFixedSize(36, 36);
    m_refreshButton->setToolTip("刷新");
    m_refreshButton->setCursor(Qt::PointingHandCursor);

    m_exportExcelButton = new QPushButton("📊 导出Excel", headerBar);
    m_exportExcelButton->setObjectName("collectExportExcelBtn");
    m_exportExcelButton->setCursor(Qt::PointingHandCursor);

    m_exportPdfButton = new QPushButton("📄 导出PDF", headerBar);
    m_exportPdfButton->setObjectName("collectExportPdfBtn");
    m_exportPdfButton->setCursor(Qt::PointingHandCursor);

    headerLayout->addWidget(m_backButton);
    headerLayout->addSpacing(4);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_refreshButton);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(m_exportExcelButton);
    headerLayout->addWidget(m_exportPdfButton);

    layout->addWidget(headerBar);

    // 中间区域：滚动单词卡片列表
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("collectScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_contentWidget = new QWidget();
    m_contentWidget->setObjectName("collectContentWidget");

    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(24, 24, 24, 24);
    m_contentLayout->setSpacing(16);

    m_scrollArea->setWidget(m_contentWidget);
    layout->addWidget(m_scrollArea, 1);
}

void CollectePage::setupConnections()
{
    connect(m_wordCollect, &WordCollect::collectListSuccess,
            this, &CollectePage::onCollectListSuccess);
    connect(m_wordCollect, &WordCollect::collectListFailed,
            this, &CollectePage::onCollectListFailed);
    connect(m_exportManager, &ExportManager::exportFinished,
            this, &CollectePage::onExportResult);

    connect(m_backButton, &QPushButton::clicked,
            this, &CollectePage::onBackButtonClicked);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &CollectePage::requestQueryCollectList);
    connect(m_exportExcelButton, &QPushButton::clicked,
            this, &CollectePage::onExportExcelClicked);
    connect(m_exportPdfButton, &QPushButton::clicked,
            this, &CollectePage::onExportPdfClicked);
}

void CollectePage::showEvent(QShowEvent* event)
{
    BaseWidget::showEvent(event);
    requestQueryCollectList();
}

void CollectePage::requestQueryCollectList()
{
    m_currentWordList.clear();
    m_wordCollect->queryCollectList();
}

void CollectePage::onCollectListSuccess(const QList<WordData>& wordList)
{
    m_currentWordList = wordList;

    // 清理旧卡片
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // 空列表提示
    if (wordList.isEmpty()) {
        auto* emptyLabel = new QLabel("暂无收藏单词，快去搜索并收藏吧", m_contentWidget);
        emptyLabel->setObjectName("collectEmptyLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(emptyLabel, 1, Qt::AlignCenter);
        return;
    }

    // 加载单词卡片
    for (const WordData& wordData : wordList) {
        auto* card = new InteractiveWordCard(
            wordData, m_wordCollect, m_wordVocabulary, m_contentWidget);
        card->setObjectName("collectWordCard");
        card->setMinimumWidth(400);
        card->setMaximumWidth(800);
        m_contentLayout->addWidget(card, 0, Qt::AlignHCenter);
    }

    m_contentLayout->addStretch();
}

void CollectePage::onCollectListFailed(const QString& errorMessage)
{
    showMessage("显示收藏单词内容出错: " + errorMessage, true, 2000);
}

void CollectePage::onBackButtonClicked()
{
    emit showMainPageRequested();
}

void CollectePage::onExportExcelClicked()
{
    if (m_currentWordList.isEmpty()) {
        showMessage("当前没有可导出的收藏单词", false, 2000);
        return;
    }
    emit exportRequested(ExportFormat::Excel, m_currentWordList, QStringLiteral("收藏单词"));
}

void CollectePage::onExportPdfClicked()
{
    if (m_currentWordList.isEmpty()) {
        showMessage("当前没有可导出的收藏单词", false, 2000);
        return;
    }
    emit exportRequested(ExportFormat::PDF, m_currentWordList, QStringLiteral("收藏单词"));
}

void CollectePage::onExportResult(bool success, const QString& message)
{
    if (success) {
        showMessage(message, false, 2000);
    } else {
        showMessage(message, true, 2000);
    }
}

void CollectePage::onThemeChanged(UITheme theme)
{
    Q_UNUSED(theme);
    // 样式已由全局 QSS 自动处理
}
