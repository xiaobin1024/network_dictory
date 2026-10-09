// search_history_widget.cpp
#include "search_history_widget.h"
#include <QSettings>
#include <QDateTime>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>

// 私有数据类
class SearchHistoryWidget::SearchHistoryWidgetPrivate
{
public:
    // 可以添加更多私有数据
};

SearchHistoryWidget::SearchHistoryWidget(QWidget* parent)
    : SearchBase(parent)
    , d(new SearchHistoryWidgetPrivate())
{
    // 初始化历史UI
    initHistoryUI();

    // 设置连接
    setupHistoryConnections();

    // 加载历史记录
    loadHistory();

    // 创建隐藏定时器
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(200);  // 200ms后隐藏

    connect(m_hideTimer, &QTimer::timeout, this, [this]() {
        if (m_historyVisible) {
            hideHistory();
        }
        });

    qDebug() << "SearchHistoryWidget 创建完成";
}

SearchHistoryWidget::~SearchHistoryWidget()
{
    // 保存历史记录
    saveHistory();

    // 清理弹出窗口
    if (m_historyContainer) {
        m_historyContainer->deleteLater();
    }

    delete d;
}

void SearchHistoryWidget::initHistoryUI()
{
    qDebug() << "初始化搜索历史UI";

    // 1. 创建独立的弹出窗口
    m_historyContainer = new QWidget(nullptr);  // 独立窗口，不设父对象
    m_historyContainer->setObjectName("historyContainer");
    m_historyContainer->setWindowFlags(
        Qt::Popup |
        Qt::FramelessWindowHint |
        Qt::NoDropShadowWindowHint
    );
    m_historyContainer->setAttribute(Qt::WA_TranslucentBackground);
    m_historyContainer->setAttribute(Qt::WA_ShowWithoutActivating);
    m_historyContainer->setAttribute(Qt::WA_DeleteOnClose, false);
    m_historyContainer->setVisible(false);

    // 2. 创建历史记录列表
    m_historyList = new QListWidget(m_historyContainer);
    m_historyList->setObjectName("historyList");
    m_historyList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyList->setSelectionMode(QListWidget::SingleSelection);
    m_historyList->setFocusPolicy(Qt::NoFocus);

    // 3. 创建清除历史按钮
    m_clearHistoryButton = new QPushButton(m_historyContainer);
    m_clearHistoryButton->setObjectName("clearHistoryButton");
    m_clearHistoryButton->setText("清除历史记录");
    m_clearHistoryButton->setCursor(Qt::PointingHandCursor);

    // 4. 创建弹出窗口内部的布局
    m_historyLayout = new QVBoxLayout(m_historyContainer);
    m_historyLayout->setContentsMargins(8, 8, 8, 8);
    m_historyLayout->setSpacing(4);
    m_historyLayout->addWidget(m_historyList, 1);
    m_historyLayout->addWidget(m_clearHistoryButton);

    // 5. 设置初始样式
    updateHistoryStyle();

    qDebug() << "搜索历史UI初始化完成";
    qDebug() << "历史容器父对象:" << m_historyContainer->parent();
    qDebug() << "历史容器窗口标志:" << m_historyContainer->windowFlags();
}

void SearchHistoryWidget::setupHistoryConnections()
{
    // 使用父类SearchBase的焦点信号
    connect(this, &SearchBase::inputFocused,
        this, &SearchHistoryWidget::onInputFocused);

    connect(this, &SearchBase::inputLostFocus,
        this, &SearchHistoryWidget::onInputLostFocus);

    // 搜索信号
    connect(this, &SearchBase::searchRequested,
        this, &SearchHistoryWidget::onSearchPerformed);

    // 历史项点击
    connect(m_historyList, &QListWidget::itemClicked,
        this, &SearchHistoryWidget::onHistoryItemClicked);

    // 清除历史按钮
    connect(m_clearHistoryButton, &QPushButton::clicked,
        this, &SearchHistoryWidget::onClearHistoryClicked);

    // 监听输入框文本变化
    connect(inputField(), &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty() && inputField()->hasFocus() && !m_historyItems.isEmpty()) {
            // 输入框为空且获得焦点，显示历史记录
            QTimer::singleShot(50, this, [this]() {
                if (inputField()->hasFocus()) {
                    showHistory();
                }
                });
        }
        else if (!text.isEmpty() && m_historyVisible) {
            // 当用户开始输入时，隐藏历史记录
            hideHistory();
        }
        });

    // 窗口激活状态变化
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        if (m_historyVisible) {
            // 检查焦点是否还在SearchHistoryWidget内
            bool focusInWidget = false;
            QWidget* widget = now;
            while (widget) {
                if (widget == this || widget == m_historyContainer) {
                    focusInWidget = true;
                    break;
                }
                widget = widget->parentWidget();
            }

            if (!focusInWidget) {
                hideHistory();
            }
        }
        });
}

void SearchHistoryWidget::onThemeChanged()
{
    // 先调用父类的主题更新
    SearchBase::onThemeChanged();

    // 更新历史列表样式
    updateHistoryStyle();
}

void SearchHistoryWidget::updateHistoryStyle()
{
    QString backgroundColor = getColor("surface");
    QString borderColor = getColor("border");
    QString textColor = getColor("text-primary");
    QString hoverColor = getColor("primary-light") + "20";
    QString selectedColor = getColor("primary-light") + "40";

    // 历史容器样式
    QString containerStyle = QString(
        "QWidget {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);"
        "}"
    ).arg(backgroundColor, borderColor);

    m_historyContainer->setStyleSheet(containerStyle);

    // 历史列表样式
    QString listStyle = QString(
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  background-color: transparent;"
        "  color: %1;"
        "  border-radius: 4px;"
        "  padding: 6px 8px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: %2;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: %3;"
        "}"
        "QScrollBar:vertical {"
        "  border: none;"
        "  background-color: transparent;"
        "  width: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: %4;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background-color: %5;"
        "}"
    ).arg(textColor, hoverColor, selectedColor,
        getColor("border"), getColor("text-secondary"));

    m_historyList->setStyleSheet(listStyle);

    // 清除按钮样式
    QString clearButtonStyle = QString(
        "QPushButton {"
        "  color: %1;"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 6px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "  border-color: %3;"
        "}"
    ).arg(getColor("text-secondary"), hoverColor, getColor("border"));

    m_clearHistoryButton->setStyleSheet(clearButtonStyle);
}

void SearchHistoryWidget::onInputFocused()
{
    qDebug() << "SearchHistoryWidget: 输入框获得焦点";

    if (m_autoShowHistory && !m_historyItems.isEmpty() && inputField()->text().isEmpty()) {
        QTimer::singleShot(50, this, [this]() {
            if (inputField()->hasFocus()) {
                showHistory();
            }
            });
    }

    // 停止隐藏定时器
    m_hideTimer->stop();
}

void SearchHistoryWidget::onInputLostFocus()
{
    qDebug() << "SearchHistoryWidget: 输入框失去焦点";

    if (m_autoHideHistory) {
        // 延迟隐藏，给用户点击历史项的时间
        m_hideTimer->start(300);
    }
}

void SearchHistoryWidget::keyPressEvent(QKeyEvent* event)
{
    // 按Esc键隐藏历史列表
    if (event->key() == Qt::Key_Escape && m_historyVisible) {
        hideHistory();
        event->accept();
        return;
    }

    // 按上下箭头在历史项中导航
    if (m_historyVisible && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        int currentRow = m_historyList->currentRow();
        int rowCount = m_historyList->count();

        if (event->key() == Qt::Key_Up) {
            currentRow = (currentRow - 1 + rowCount) % rowCount;
        }
        else if (event->key() == Qt::Key_Down) {
            currentRow = (currentRow + 1) % rowCount;
        }

        m_historyList->setCurrentRow(currentRow);
        m_historyList->scrollToItem(m_historyList->item(currentRow));
        event->accept();
        return;
    }

    // 按Enter选择当前历史项
    if (m_historyVisible && event->key() == Qt::Key_Return) {
        QListWidgetItem* currentItem = m_historyList->currentItem();
        if (currentItem) {
            onHistoryItemClicked(currentItem);
            event->accept();
            return;
        }
    }

    SearchBase::keyPressEvent(event);
}

void SearchHistoryWidget::showHistory()
{
    if (m_historyItems.isEmpty() || !inputField()) {
        return;
    }

    if (!m_historyVisible) {
        // 调整位置
        adjustHistoryPosition();

        // 显示历史容器
        m_historyContainer->show();
        m_historyContainer->raise();

        m_historyVisible = true;

        // 取消隐藏定时器
        m_hideTimer->stop();

        qDebug() << "搜索历史列表显示";
    }
}

void SearchHistoryWidget::hideHistory()
{
    if (m_historyVisible) {
        m_historyContainer->hide();
        m_historyVisible = false;

        qDebug() << "搜索历史列表隐藏";
    }
}

void SearchHistoryWidget::adjustHistoryPosition()
{
    if (!m_historyContainer || !inputField()) {
        return;
    }

    // 获取输入框的全局坐标
    QPoint globalPos = inputField()->mapToGlobal(QPoint(0, inputField()->height()));

    // 获取屏幕可用区域
    QRect screenRect = QApplication::desktop()->availableGeometry(this);

    // 计算容器尺寸
    int containerWidth = inputField()->width();
    int itemHeight = 30;  // 每个历史项的高度
    int headerHeight = 40;  // 标题和按钮的高度
    int maxVisibleItems = qMin(m_historyItems.size(), 5);
    int containerHeight = maxVisibleItems * itemHeight + headerHeight;

    // 计算最佳位置
    int x = globalPos.x();
    int y = globalPos.y() + 2;  // 加2像素的间隔

    // 如果下方空间不足，显示在上方
    if (y + containerHeight > screenRect.bottom()) {
        y = inputField()->mapToGlobal(QPoint(0, 0)).y() - containerHeight - 2;
    }

    // 如果右侧超出，左移
    if (x + containerWidth > screenRect.right()) {
        x = screenRect.right() - containerWidth;
    }

    // 设置历史容器位置和大小
    m_historyContainer->setGeometry(x, y, containerWidth, containerHeight);
}

// ... 其余函数保持不变（addHistory, clearHistory, onHistoryItemClicked等）

void SearchHistoryWidget::adjustHistoryPosition()
{
    if (!m_historyContainer || !inputField()) {
        return;
    }

    // 获取输入框的全局坐标
    QPoint globalPos = inputField()->mapToGlobal(QPoint(0, inputField()->height()));

    // 获取屏幕可用区域 - Qt6 方式
    QScreen* currentScreen = nullptr;

    // 方法1：获取当前窗口所在的屏幕
    if (auto* window = windowHandle()) {
        currentScreen = window->screen();
    }

    // 方法2：如果方法1失败，获取包含输入框坐标的屏幕
    if (!currentScreen) {
        currentScreen = QApplication::screenAt(globalPos);
    }

    // 方法3：如果还是失败，使用主屏幕
    if (!currentScreen) {
        currentScreen = QApplication::primaryScreen();
    }

    if (!currentScreen) {
        qWarning() << "无法获取屏幕信息";
        return;
    }

    QRect screenRect = currentScreen->availableGeometry();

    // 计算容器尺寸
    int containerWidth = inputField()->width();
    int itemHeight = 30;  // 每个历史项的高度
    int headerHeight = 40;  // 标题和按钮的高度
    int maxVisibleItems = qMin(m_historyItems.size(), 5);
    int containerHeight = maxVisibleItems * itemHeight + headerHeight;

    // 限制最大高度，防止过长
    int maxHeight = static_cast<int>(screenRect.height() * 0.6);
    if (containerHeight > maxHeight) {
        containerHeight = maxHeight;
    }

    // 计算最佳位置
    int x = globalPos.x();
    int y = globalPos.y() + 2;  // 加2像素的间隔

    // 如果下方空间不足，显示在上方
    if (y + containerHeight > screenRect.bottom()) {
        y = inputField()->mapToGlobal(QPoint(0, 0)).y() - containerHeight - 2;
    }

    // 如果右侧超出，左移
    if (x + containerWidth > screenRect.right()) {
        x = screenRect.right() - containerWidth;
    }

    // 如果左侧超出，右移
    if (x < screenRect.left()) {
        x = screenRect.left();
    }

    // 如果顶部超出，显示在下方
    if (y < screenRect.top()) {
        y = inputField()->mapToGlobal(QPoint(0, inputField()->height())).y() + 2;
    }

    // 设置历史容器位置和大小
    m_historyContainer->setGeometry(x, y, containerWidth, containerHeight);
}