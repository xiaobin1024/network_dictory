#include "search_base.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QShortcut>

// 私有数据类
class SearchBase::SearchBasePrivate {
public:
    QString lastSearchKeyword;  // 上次搜索的关键词
    bool isSearching = false;   // 是否正在搜索
};

SearchBase::SearchBase(QWidget* parent)
    : BaseWidget(parent)
    , d(new SearchBasePrivate())
{
    setObjectName("searchBase");
    setupLayout();
    setupConnections();
}

SearchBase::~SearchBase() {
    delete d;
}

void SearchBase::setupLayout()
{
    if (!mainLayout()) {
        qCritical() << "BaseWidget mainLayout is null! SearchBase init failed.";
        return;
    }

    // 与新版 BaseWidget 布局风格保持一致
    mainLayout()->setContentsMargins(0, 0, 0, 0);

    auto* hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(12, 8, 12, 8);
    hLayout->setSpacing(10);

    // 1. 输入框（始终占据主要空间）
    m_input = new QLineEdit(this);
    m_input->setObjectName("searchInput");
    m_input->setPlaceholderText(tr("搜索单词..."));
    m_input->setClearButtonEnabled(false); // 使用自定义清除按钮以便 QSS 完全控制
    hLayout->addWidget(m_input, 1);        // 输入框占满剩余空间

    // 2. 搜索按钮（固定位置，不受清除按钮显隐影响）
    m_searchButton = new QPushButton(this);
    m_searchButton->setObjectName("searchSubmitBtn");
    m_searchButton->setFixedSize(36, 36);
    m_searchButton->setCursor(Qt::PointingHandCursor);
    m_searchButton->setToolTip(tr("搜索"));
    hLayout->addWidget(m_searchButton);

    // 3. 清除按钮（最右侧，显隐不影响搜索按钮位置）
    m_clearButton = new QPushButton(this);
    m_clearButton->setObjectName("searchClearBtn");
    m_clearButton->setFixedSize(28, 28);
    m_clearButton->setVisible(false);
    m_clearButton->setCursor(Qt::PointingHandCursor);
    m_clearButton->setToolTip(tr("清空"));
    hLayout->addWidget(m_clearButton);

    mainLayout()->addLayout(hLayout);

    // 事件过滤器：监听输入框焦点
    m_input->installEventFilter(this);

    // 使用 Unicode 字符替代图标资源
    m_searchButton->setText(QStringLiteral("🔍"));
    m_clearButton->setText(QStringLiteral("✕"));
}

void SearchBase::setupConnections() {
    if (!m_input || !m_searchButton || !m_clearButton) {
        qWarning() << "SearchBase::setupConnections - widgets not initialized";
        return;
    }

    connect(m_input, &QLineEdit::textChanged, this, &SearchBase::onTextChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &SearchBase::onReturnPressed);
    connect(m_searchButton, &QPushButton::clicked, this, &SearchBase::onSearchClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &SearchBase::onClearClicked);

    // 快捷键：Ctrl+F 聚焦搜索框
    auto* focusShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this);
    connect(focusShortcut, &QShortcut::activated, this, &SearchBase::setFocusToInput);

    // 快捷键：Esc 清除输入
    auto* clearShortcut = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(clearShortcut, &QShortcut::activated, this, &SearchBase::clearInput);

}

void SearchBase::onThemeChanged(UITheme theme) {
    //BaseWidget::onThemeChanged(theme);
    // 所有颜色、边框、背景等样式均由 QSS 文件控制，
    // 此处仅保留钩子，供未来需要代码级主题切换时使用。
    Q_UNUSED(theme)
}

void SearchBase::updateClearButtonVisibility() {
    bool hasText = !m_input->text().trimmed().isEmpty();
    m_clearButton->setVisible(hasText && m_showClearButton);
}

void SearchBase::onTextChanged(const QString& text) {
    updateClearButtonVisibility();
    emit keywordChanged(text.trimmed());
}

void SearchBase::onSearchClicked() {
    qDebug() << ">>> onSearchClicked triggered";
    QString keyword = m_input->text().trimmed();
    if (!keyword.isEmpty()) {
        d->lastSearchKeyword = keyword;
        d->isSearching = true;
        emit searchRequested(keyword);
    }
}

void SearchBase::onClearClicked() {
    clearInput();
    m_input->setFocus();
}

void SearchBase::onReturnPressed() {
    onSearchClicked();
}

QString SearchBase::keyword() const {
    return m_input->text().trimmed();
}

void SearchBase::setKeyword(const QString& keyword) {
    if (m_input->text() != keyword) {
        m_input->setText(keyword);
        updateClearButtonVisibility();
    }
}

QString SearchBase::placeholder() const {
    return m_input->placeholderText();
}

void SearchBase::setPlaceholder(const QString& placeholder) {
    m_input->setPlaceholderText(placeholder);
}

void SearchBase::setShowClearButton(bool show) {
    if (m_showClearButton != show) {
        m_showClearButton = show;
        updateClearButtonVisibility();
    }
}

void SearchBase::setShowSearchButton(bool show) {
    if (m_showSearchButton != show) {
        m_showSearchButton = show;
        m_searchButton->setVisible(show);
    }
}

void SearchBase::setFocusToInput() {
    m_input->setFocus();
    m_input->selectAll();
}

void SearchBase::doSearch() {
    onSearchClicked();
}

void SearchBase::clearInput() {
    m_input->clear();
    updateClearButtonVisibility();
    emit keywordChanged(QString());
    emit clearClicked();
}


bool SearchBase::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_input) {
        if (event->type() == QEvent::FocusIn) {
            emit inputFocused();
        } else if (event->type() == QEvent::FocusOut) {
            emit inputLostFocus();
        }
    }
    return false; // 不拦截事件，继续传递
}
