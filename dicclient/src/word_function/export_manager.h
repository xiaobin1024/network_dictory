#ifndef EXPORT_MANAGER_H
#define EXPORT_MANAGER_H

#include <QObject>
#include <QList>
#include <xlnt/xlnt.hpp>
#include <QTextDocument>
#include <QPrinter>
#include <QPageSize>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include "word_data.h"


// 前向声明，避免循环包含
// class CollectePage;
// class VocabularyPage;
enum class ExportFormat {
    Excel,
    PDF
};

class ExportManager : public QObject
{
    Q_OBJECT

public:
    static ExportManager* instance();

    // 绑定到收藏页面，连接信号
    // void bindToCollectePage(CollectePage* page);
    // void bindToVocabularyPage(VocabularyPage* page);

public slots:
    void onExportRequested(ExportFormat format, const QList<WordData>& wordList, const QString &titile);

signals:
    void exportStarted(ExportFormat format);
    void exportFinished(bool success, const QString& message);


private:
    explicit ExportManager(QObject* parent = nullptr);
    ~ExportManager();

    bool doExportExcel(const QList<WordData>& list, const QString& filePath, const QString &titile);
    bool doExportPdf(const QList<WordData>& list, const QString& filePath,const QString& titile);

    QString getDefaultFileName(ExportFormat format ,const QString& title) const;
    QString getFilter(ExportFormat format) const;

    static ExportManager* s_instance;

    // CollectePage* collecte_page{nullptr};
    // VocabularyPage* vocabulary_page{nullptr};
};

#endif // EXPORT_MANAGER_H
