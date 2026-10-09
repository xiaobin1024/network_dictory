#ifndef COLLECTE_PAGE_H
#define COLLECTE_PAGE_H

#include "base/base_widget.h"
#include "word_data.h"
#include "word_collect.h"
#include "word_vocabulary.h"
#include "export_manager.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

class CollectePage : public BaseWidget
{
    Q_OBJECT

public:
    explicit CollectePage(QWidget* parent = nullptr);
    ~CollectePage() override;

signals:
    void showMainPageRequested();
    void exportRequested(ExportFormat format, const QList<WordData>& wordList, const QString& title);

private slots:
    void onCollectListSuccess(const QList<WordData>& wordList);
    void onCollectListFailed(const QString& errorMessage);
    void onBackButtonClicked();
    void onExportExcelClicked();
    void onExportPdfClicked();
    void onExportResult(bool success, const QString& message);
    void requestQueryCollectList();

protected:
    void showEvent(QShowEvent* event) override;
    void onThemeChanged(UITheme theme) override;

private:
    void setupUI();
    void setupConnections();

    QList<WordData> m_currentWordList;  // 当前显示的单词列表
    QScrollArea* m_scrollArea{nullptr};
    QWidget* m_contentWidget{nullptr};
    QVBoxLayout* m_contentLayout{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_backButton{nullptr};
    QPushButton* m_refreshButton{nullptr};
    QPushButton* m_exportExcelButton{nullptr};
    QPushButton* m_exportPdfButton{nullptr};

    WordCollect* m_wordCollect{nullptr};
    WordVocabulary* m_wordVocabulary{nullptr};
    ExportManager* m_exportManager{nullptr};
};

#endif // COLLECTE_PAGE_H
