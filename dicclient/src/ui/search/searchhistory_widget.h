#ifndef SEARCH_HISTORY_WIDGET_H
#define SEARCH_HISTORY_WIDGET_H

#include "search_base.h"
#include "user_session.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QJsonArray>

struct HistoryItem {
    QString keyword;
    QDateTime timestamp;
    int searchCount = 0;
};

/**
 * @brief 搜索历史记录组件
 *
 * 继承 SearchBase，复用父类的搜索交互能力，仅扩展：
 * 1. 输入框获得焦点且为空时，弹出最近搜索记录
 * 2. 点击历史项自动填充并触发搜索
 * 3. 历史记录的增删、排序、持久化（通过 UserSession）
 */
class SearchHistoryWidget : public SearchBase {
    Q_OBJECT
public:
    explicit SearchHistoryWidget(QWidget* parent = nullptr, UserSession* userSession = nullptr);
    ~SearchHistoryWidget() override;

    void addHistory(const QString& keyword);
    void clearHistory();
    int historyCount() const;
    QStringList getHistory() const;

    void setMaxHistoryCount(int count);
    int maxHistoryCount() const;

    void showHistory();
    void hideHistory();
    bool isHistoryVisible() const;

    // 测试访问器
    QListWidget* getHistoryList() const { return m_historyList; }
    QWidget* getHistoryContainer() const { return m_historyContainer; }

signals:
    void historyItemClicked(const QString& keyword);
    void historyCleared();

protected:
    void onThemeChanged(UITheme theme) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onHistoryItemClicked(QListWidgetItem* item);
    void onClearHistoryClicked();
    void onLoginSuccess(const UserData& user);

private:
    void initHistoryUI();
    void setupHistoryConnections();
    void updateHistoryList();
    void adjustHistoryPosition();
    void ensureHistoryOrder();
    void fetchHistoryFromSession();
    void updateLocalHistory(const QString& keyword);
    QList<HistoryItem> convertJsonToHistoryItems(const QJsonArray& jsonArray);

    // UI 组件
    QListWidget* m_historyList = nullptr;
    QWidget* m_historyContainer = nullptr;
    QVBoxLayout* m_historyLayout = nullptr;
    QPushButton* m_clearHistoryButton = nullptr;


    QPushButton* m_foldButton = nullptr;   // 折叠/展开按钮
    bool m_isFolded = false;             // 当前是否处于折叠状态
    static constexpr int FOLDED_COUNT = 3; // 折叠时显示的记录数

    void onFoldClicked();                // 折叠按钮槽函数
    void updateFoldButtonState();        // 更新按钮文本和可见性


    // 历史记录存储
    QList<HistoryItem> m_historyItems;
    int m_maxHistoryCount = 5;
    bool m_historyVisible = false;
    bool m_historyCleared = false;


    // 用户会话
    UserSession* m_userSession = nullptr;
};

#endif // SEARCH_HISTORY_WIDGET_H
