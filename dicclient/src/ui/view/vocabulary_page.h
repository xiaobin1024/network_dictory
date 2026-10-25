#ifndef VOCABULARY_PAGE_H
#define VOCABULARY_PAGE_H

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

class VocabularyPage : public BaseWidget
{
    Q_OBJECT

public:
    explicit VocabularyPage(QWidget* parent = nullptr);
    ~VocabularyPage() override;

signals:
    void showMainPageRequested();
    void exportRequested(ExportFormat format, const QList<WordData>& wordList, const QString& title);

private slots:
    void onVocabularyListSuccess(const QList<WordData>& wordList);
    void onVocabularyListFailed(const QString& errorMessage);
    void onBackButtonClicked();
    void onExportExcelClicked();
    void onExportPdfClicked();
    void onExportResult(bool success, const QString& message);
    void requestQueryVocabularyList();

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

#endif // VOCABULARY_PAGE_H
