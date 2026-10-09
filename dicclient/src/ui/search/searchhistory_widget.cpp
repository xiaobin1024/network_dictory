#include "searchhistory_widget.h"
#include <QApplication>
#include <QListWidgetItem>
#include <QScreen>
#include <algorithm>

SearchHistoryWidget::SearchHistoryWidget(QWidget* parent, UserSession* userSession)
    : SearchBase(parent)
    , m_userSession(userSession)
{
    initHistoryUI();
    setupHistoryConnections();

}

SearchHistoryWidget::~SearchHistoryWidget()
{
    qApp->removeEventFilter(this);  // 【新增】防止 dangling pointer
    // if (m_historyContainer) {
    //     m_historyContainer->deleteLater();
    // }
}

/* ==================== UI 初始化 ==================== */

void SearchHistoryWidget::initHistoryUI()
{
    if (!inputField()) {
        qCritical() << "SearchHistoryWidget::initHistoryUI - 输入框未初始化";
        return;
    }

    // 【关键】改用 Qt::Tool：顶层浮层，不抢焦点，不会自动关闭
    m_historyContainer = new QWidget(this);
    m_historyContainer->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_historyContainer->setAttribute(Qt::WA_ShowWithoutActivating, true);  // 显示时不激活窗口
    m_historyContainer->setAttribute(Qt::WA_TranslucentBackground, false);
    m_historyContainer->setObjectName("historyContainer");
    m_historyContainer->setVisible(false);

    // 历史列表
    m_historyList = new QListWidget(m_historyContainer);
    m_historyList->setObjectName("historyList");
    m_historyList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyList->setSelectionMode(QListWidget::SingleSelection);
    m_historyList->setFocusPolicy(Qt::NoFocus);

    // 清除按钮
    m_clearHistoryButton = new QPushButton(QStringLiteral("清除历史记录"), m_historyContainer);
    m_clearHistoryButton->setObjectName("clearHistoryButton");
    m_clearHistoryButton->setCursor(Qt::PointingHandCursor);

    // 折叠按钮
    m_foldButton = new QPushButton(QStringLiteral("▲ 折叠"), m_historyContainer);
    m_foldButton->setObjectName("foldHistoryButton");
    m_foldButton->setCursor(Qt::PointingHandCursor);
    m_foldButton->setMinimumWidth(70);  // 防止被压缩到 0
    m_foldButton->setVisible(false);

    // 【关键】按钮行布局
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);
    buttonLayout->addWidget(m_clearHistoryButton, 1);
    buttonLayout->addWidget(m_foldButton);

    // 【关键】主垂直布局 —— 必须包含列表 + 按钮行布局
    m_historyLayout = new QVBoxLayout(m_historyContainer);
    m_historyLayout->setContentsMargins(8, 8, 8, 8);
    m_historyLayout->setSpacing(4);
    m_historyLayout->addWidget(m_historyList, 0);        // stretch = 0，不拉伸
    m_historyLayout->addLayout(buttonLayout);            // ← 最容易遗漏！必须加！

    // 添加到 SearchBase 的垂直布局
    //mainLayout()->addWidget(m_historyContainer);
}

void SearchHistoryWidget::setupHistoryConnections()
{
    if (!inputField()) {
        qWarning() << "SearchHistoryWidget::setupHistoryConnections - 输入框未初始化";
        return;
    }

    // ========== 1. 焦点驱动（外部进入输入框时触发）==========
    connect(this, &SearchBase::inputFocused, this, [this]() {
        if (!m_historyItems.isEmpty() && keyword().isEmpty()) {
            showHistory();
        }
    });


    // ========== 2. 文本变化驱动（Backspace 删空时触发）==========
    connect(this, &SearchBase::keywordChanged, this, [this](const QString& text) {
        if (text.isEmpty() && inputField()->hasFocus() && !m_historyItems.isEmpty()) {
            showHistory();
        } else if (!text.isEmpty() && m_historyVisible) {
            hideHistory();
        }
    });

    // ========== 3. 搜索完成 → 添加历史 ==========
    connect(this, &SearchBase::searchRequested, this, [this](const QString& keyword) {
        if (keyword.trimmed().isEmpty()) return;
        addHistory(keyword);
        if (m_userSession && m_userSession->isLoggedIn()) {
            m_userSession->addSearchHistory(keyword);
        }
    });

    // ========== 4. 清除按钮 ==========
    connect(m_clearHistoryButton, &QPushButton::clicked, this, [this]() {
        clearHistory();
        hideHistory();
        clearInput();
    });

    // ========== 5. 历史项点击 ==========
    connect(m_historyList, &QListWidget::itemClicked,
            this, &SearchHistoryWidget::onHistoryItemClicked);

    // ========== 6. 登录成功 ==========
    if (m_userSession) {
        connect(m_userSession, &UserSession::loginSuccess,
                this, &SearchHistoryWidget::onLoginSuccess);
    }

    // ========== 7. 全局鼠标点击监听（点击历史列表外部时隐藏）==========
    qApp->installEventFilter(this);
    //监听 Popup 自身的 Hide 事件，修正状态同步
    m_historyContainer->installEventFilter(this);

    // 折叠按钮点击
    connect(m_foldButton, &QPushButton::clicked,
            this, &SearchHistoryWidget::onFoldClicked);
}
/* ==================== 主题切换 ==================== */

void SearchHistoryWidget::onThemeChanged(UITheme theme)
{
    SearchBase::onThemeChanged(theme);
    Q_UNUSED(theme)
}

/* ==================== 显示控制 ==================== */

void SearchHistoryWidget::showHistory()
{
    if (m_historyItems.isEmpty()) return;
    if (m_historyVisible) return;

    // 先刷新内容，确保 sizeHint 最新
    updateHistoryList();
    adjustHistoryPosition();

     m_historyContainer->show();  //Popup 用 show() 而非 setVisible(true)
    m_historyContainer->raise();
     m_historyVisible = true;

}

void SearchHistoryWidget::hideHistory()
{
    if (!m_historyVisible) return;
    m_historyContainer->hide();  // 【关键】Popup 用 hide()
    m_historyVisible = false;
}

bool SearchHistoryWidget::isHistoryVisible() const
{
    return m_historyVisible;
}

void SearchHistoryWidget::adjustHistoryPosition()
{
    if (!m_historyContainer || !inputField()) return;
    if (layout()) layout()->activate();

    // 【关键】Popup 是顶层窗口，位置用全局坐标 + move()
    QPoint globalPos = inputField()->mapToGlobal(QPoint(0, inputField()->height()));
    int w = inputField()->width();

    // 先让布局计算一次自然高度
    m_historyContainer->adjustSize();
    int h = m_historyContainer->height();
    const int maxH = 5 * 30 + 50;  // 5条 + 按钮区
    if (h > maxH) h = maxH;

    int x = globalPos.x();
    int y = globalPos.y() + 2;

    QRect screenRect = screen()->availableGeometry();

    // 下方空间不足 → 显示在输入框上方
    if (y + h > screenRect.bottom()) {
        y = inputField()->mapToGlobal(QPoint(0, 0)).y() - h - 2;
    }

    // 右边界超出 → 左移
    if (x + w > screenRect.right()) {
        x = screenRect.right() - w;
    }

    // 【关键】Popup 用 move() + setFixedSize()，不用 setGeometry()
    m_historyContainer->move(x, y);
    m_historyContainer->setFixedSize(w, h);
}

/* ==================== 历史记录管理 ==================== */

void SearchHistoryWidget::addHistory(const QString& keyword)
{
    if (keyword.trimmed().isEmpty()) return;
    updateLocalHistory(keyword);
    updateHistoryList();
}

void SearchHistoryWidget::updateLocalHistory(const QString& keyword)
{
    auto it = std::find_if(m_historyItems.begin(), m_historyItems.end(),
                           [&keyword](const HistoryItem& item) {
                               return item.keyword == keyword;
                           });

    if (it != m_historyItems.end()) {
        it->searchCount++;
        it->timestamp = QDateTime::currentDateTime();
    } else {
        HistoryItem newItem;
        newItem.keyword = keyword;
        newItem.timestamp = QDateTime::currentDateTime();
        newItem.searchCount = 1;
        m_historyItems.append(newItem);
    }

    ensureHistoryOrder();

    if (m_historyItems.size() > m_maxHistoryCount) {
        m_historyItems = m_historyItems.mid(0, m_maxHistoryCount);
    }
}

void SearchHistoryWidget::ensureHistoryOrder()
{
    std::sort(m_historyItems.begin(), m_historyItems.end(),
              [](const HistoryItem& a, const HistoryItem& b) {
                  return a.timestamp > b.timestamp;
              });
}

void SearchHistoryWidget::clearHistory()
{
    m_historyItems.clear();
    m_historyList->clear();

    if (m_userSession && m_userSession->isLoggedIn()) {
        m_userSession->clearSearchHistory();
    }

    // 【关键】彻底重置折叠状态
    m_isFolded = false;
    m_historyList->setMaximumHeight(QWIDGETSIZE_MAX);

    // 按钮状态由 updateFoldButtonState 控制，这里不需要手动 setVisible

    m_historyCleared = true;
    emit historyCleared();
}

int SearchHistoryWidget::historyCount() const
{
    return m_historyItems.size();
}

QStringList SearchHistoryWidget::getHistory() const
{
    QStringList result;
    for (const auto& item : m_historyItems) {
        result.append(item.keyword);
    }
    return result;
}

void SearchHistoryWidget::setMaxHistoryCount(int count)
{
    if (count > 0 && count != m_maxHistoryCount) {
        m_maxHistoryCount = count;
        if (m_historyItems.size() > m_maxHistoryCount) {
            m_historyItems = m_historyItems.mid(0, m_maxHistoryCount);
            updateHistoryList();
        }
    }
}

int SearchHistoryWidget::maxHistoryCount() const
{
    return m_maxHistoryCount;
}


/* ==================== 槽函数 ==================== */

void SearchHistoryWidget::onHistoryItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    QString keyword = item->text();

    // 复用父类方法：设置关键词 + 触发搜索 + 聚焦
    setKeyword(keyword);
    doSearch();           // ← SearchBase 提供：发射 searchRequested(keyword)
    hideHistory();
}

void SearchHistoryWidget::onClearHistoryClicked()
{
    clearHistory();
    hideHistory();
    m_historyCleared = true;
}

void SearchHistoryWidget::onLoginSuccess(const UserData& user)
{
    if (user.isLoggedIn()) {
        fetchHistoryFromSession();
    }
}

/* ==================== 列表更新与数据转换 ==================== */

void SearchHistoryWidget::updateHistoryList()
{
    m_historyList->clear();

    int displayCount = m_isFolded
                           ? qMin(FOLDED_COUNT, m_historyItems.size())
                           : m_historyItems.size();

    for (int i = 0; i < displayCount; ++i) {
        const auto& item = m_historyItems[i];
        QListWidgetItem* listItem = new QListWidgetItem(item.keyword);
        QString tooltip = QStringLiteral("搜索次数: %1\n最后搜索: %2")
                              .arg(item.searchCount)
                              .arg(item.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        listItem->setToolTip(tooltip);
        m_historyList->addItem(listItem);
    }

    // 【新增】根据折叠状态限制列表高度，使容器能收缩
    if (m_isFolded && m_historyItems.size() > FOLDED_COUNT) {
        int rowHeight = m_historyList->sizeHintForRow(0);
        if (rowHeight <= 0) rowHeight = 30; // 备用值
        int listHeight = FOLDED_COUNT * rowHeight + m_historyList->frameWidth() * 2;
        m_historyList->setMaximumHeight(listHeight);
    } else {
        m_historyList->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    // 【新增】强制容器重新计算自身大小
    m_historyContainer->adjustSize();

    updateFoldButtonState();

    if (m_historyItems.isEmpty() && m_historyVisible) {
        hideHistory();
    }
}
void SearchHistoryWidget::fetchHistoryFromSession()
{
    if (!m_userSession) {
        qDebug() << "错误：用户会话对象为空，无法获取历史记录";
        return;
    }
    if (!m_userSession->isLoggedIn()) {
        qDebug() << "警告：用户未登录，无法获取历史记录";
        return;
    }

    QJsonArray history = m_userSession->currentSearchHistory();
    m_historyItems = convertJsonToHistoryItems(history);
    updateHistoryList();
}

QList<HistoryItem> SearchHistoryWidget::convertJsonToHistoryItems(const QJsonArray& jsonArray)
{
    QList<HistoryItem> items;
    for (const QJsonValue& value : jsonArray) {
        if (!value.isObject()) continue;
        QJsonObject obj = value.toObject();
        if (!obj.contains(QStringLiteral("keyword")) || !obj.contains(QStringLiteral("timestamp"))) {
            qWarning() << "Invalid history item JSON, missing required fields";
            continue;
        }
        HistoryItem item;
        item.keyword = obj[QStringLiteral("keyword")].toString();
        QString timestampStr = obj[QStringLiteral("timestamp")].toString();
        item.timestamp = QDateTime::fromString(timestampStr, Qt::ISODate);
        if (!item.timestamp.isValid()) {
            qWarning() << "Invalid timestamp format:" << timestampStr;
            item.timestamp = QDateTime::currentDateTime();
        }
        item.searchCount = obj.contains(QStringLiteral("searchCount"))
                               ? obj[QStringLiteral("searchCount")].toInt(0) : 1;
        items.append(item);
    }
    return items;
}

void SearchHistoryWidget::onFoldClicked()
{
    m_isFolded = !m_isFolded;
    updateHistoryList();
    inputField()->setFocus();         // 【关键】焦点归还输入框，不再触发 inputLostFocus
}

void SearchHistoryWidget::updateFoldButtonState()
{
    bool canFold = m_historyItems.size() > FOLDED_COUNT;
    m_foldButton->setVisible(canFold);

    if (!canFold) {
        m_isFolded = false;
        return;
    }

    if (m_isFolded) {
        m_foldButton->setText(QStringLiteral("▼ 展开"));
        m_foldButton->setToolTip(QStringLiteral("展开显示全部 %1 条").arg(m_historyItems.size()));
    } else {
        m_foldButton->setText(QStringLiteral("▲ 折叠"));
        m_foldButton->setToolTip(QStringLiteral("折叠为显示前 %1 条").arg(FOLDED_COUNT));
    }
}

// bool SearchHistoryWidget::eventFilter(QObject* watched, QEvent* event)
// {
//     // 【简化】Qt::Popup 会自动处理点击外部关闭，只需保留点击输入框显示的逻辑
//     if (event->type() == QEvent::MouseButtonPress) {
//         QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
//         QPoint globalPos = mouseEvent->globalPos();

//         QRect inputRect(inputField()->mapToGlobal(QPoint(0, 0)), inputField()->size());

//         // 点击输入框，且为空，且当前未显示 → 显示
//         if (!m_historyVisible && inputRect.contains(globalPos)
//             && inputField()->text().isEmpty()
//             && !m_historyItems.isEmpty()) {
//             showHistory();
//         }
//     }

//     // 【删除】不再需要手动判断点击外部隐藏，Qt::Popup 自动处理
//     // 但保留父类的过滤
//     return SearchBase::eventFilter(watched, event);
// }

bool SearchHistoryWidget::eventFilter(QObject* watched, QEvent* event)
{
    // 【删除】不再需要监听 QEvent::Hide，Tool 窗口不会自动关闭

    if (event->type() == QEvent::MouseButtonPress) {
        // 【修复】仅当搜索组件本身可见时才响应全局点击。
        // 原因：搜索框通过 qApp->installEventFilter 监听全应用点击，而 mapToGlobal
        // 在组件被隐藏时依然有效。切换到收藏/生词本页面后 MainPage 虽已隐藏但仍
        // 占据原坐标，导致在其他页面点击搜索框原位置时会误触发 showHistory()。
        if (!isVisible()) {
            return SearchBase::eventFilter(watched, event);
        }

        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint globalPos = mouseEvent->globalPos();

        QRect historyRect(m_historyContainer->mapToGlobal(QPoint(0, 0)), m_historyContainer->size());
        QRect inputRect(inputField()->mapToGlobal(QPoint(0, 0)), inputField()->size());

        if (m_historyVisible) {
            // 点击外部（且不是输入框）→ 隐藏
            if (!historyRect.contains(globalPos) && !inputRect.contains(globalPos)) {
                hideHistory();
            }
        } else {
            // 点击输入框，且为空，且未显示 → 显示历史
            if (inputRect.contains(globalPos)
                && inputField()->text().isEmpty()
                && !m_historyItems.isEmpty()) {
                showHistory();
            }
        }
    }

    return SearchBase::eventFilter(watched, event);
}

void SearchHistoryWidget::hideEvent(QHideEvent* event)
{
    // 【修复】组件被隐藏（如切换页面）时关闭历史浮层，避免残留弹窗
    hideHistory();
    SearchBase::hideEvent(event);
}
