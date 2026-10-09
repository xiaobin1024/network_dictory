#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "base/base_widget.h"
#include "word/interactive_wordcard.h"
#include "system/system_sidebar.h"
#include "search/searchhistory_widget.h"
#include "base/theme_manager.h"
#include "word_search.h"
#include "word_collect.h"
#include "word_vocabulary.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QLabel>

class MainPage : public BaseWidget
{
    Q_OBJECT

public:
    explicit MainPage(QWidget* parent = nullptr);
    ~MainPage() override;

    void setSearchPlaceholder(const QString& placeholder);
    void setSearchResults(const QStringList& results);
    void setCurrentWordCard(const WordData& wordData);

    SearchHistoryWidget* getSearchWidget() const { return m_searchWidget; }

signals:
    void searchRequested(const QString& query);
    void navigateToLogin();
    void navigateToRegister();
    void showCollectPageRequested();
    void showVocabularyRequested();

private slots:
    void onSearchRequested(const QString& query);
    void onClearButtonClicked();
    void onWordSearchSuccess(const WordData& wordData);
    void onWordSearchFailed(const QString& error);
    void onKeywordChanged(const QString& keyword);

private:
    void setupUI();
    void setupHeader();
    void setupSearchSection();
    void setupContentArea();
    void setupSidebar();
    void setupConnections();

    void updateCurrentCardWidth();

    void onThemeChanged(UITheme theme) override;
    void resizeEvent(QResizeEvent* event) override;

    // UI 组件
    SearchHistoryWidget* m_searchWidget{nullptr};
    QWidget*             m_searchContainer{nullptr};
    QLabel*              m_searchTitleLabel{nullptr};
    QScrollArea*         m_contentScrollArea{nullptr};
    QWidget*             m_contentWidget{nullptr};
    QVBoxLayout*         m_contentLayout{nullptr};
    SystemSidebar*       m_sidebar{nullptr};

    // 当前显示的单词卡片
    InteractiveWordCard* m_currentWordCard{nullptr};

    // 服务对象
    UserSession*    m_userSession{nullptr};
    WordSearch*     m_wordSearch{nullptr};
    WordCollect*    m_wordCollect{nullptr};
    WordVocabulary* m_wordVocabulary{nullptr};
};

#endif // MAIN_PAGE_H
