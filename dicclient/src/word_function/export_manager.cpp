#include "export_manager.h"
#include "view/collecte_page.h"
#include"view/vocabulary_page.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

ExportManager* ExportManager::s_instance = nullptr;

ExportManager* ExportManager::instance()
{
    if (!s_instance) {
        s_instance = new ExportManager();
    }
    return s_instance;
}

ExportManager::ExportManager(QObject* parent)
    : QObject(parent)
{
}

ExportManager::~ExportManager()
{
}

// void ExportManager::bindToCollectePage(CollectePage* page)
// {
//     collecte_page=page;
//     // 连接收藏页面的导出信号到管理器的槽
//     // connect(collecte_page, &CollectePage::exportRequested,
//     //         this, &ExportManager::onExportRequested);

// }

// void ExportManager::bindToVocabularyPage(VocabularyPage* page)
// {
//     vocabulary_page=page;
//     // connect(vocabulary_page,&VocabularyPage::exportRequested,
//     //         this,&ExportManager::onExportRequested);
// }

void ExportManager::onExportRequested(ExportFormat format, const QList<WordData>& wordList, const QString &titile)
{
    qDebug()<<"ExportManager::onExportRequested";
    // 选择保存路径
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/" + getDefaultFileName(format,titile);

    QString filter = getFilter(format);
    QString filePath = QFileDialog::getSaveFileName(
        nullptr,
        format == ExportFormat::Excel ? "导出Excel" : "导出PDF",
        defaultPath,
        filter
        );

    if (filePath.isEmpty()) {
        return;  // 用户取消
    }

    emit exportStarted(format);

    bool success = false;
    if (format == ExportFormat::Excel) {
        success = doExportExcel(wordList, filePath,titile);
    } else {
        success = doExportPdf(wordList, filePath,titile);
    }

    emit exportFinished(success, success ? "导出成功" : "导出失败");
}

bool ExportManager::doExportExcel(const QList<WordData>& list, const QString& filePath,const QString &titile)
{
    qDebug() << "导出Excel:" << filePath << "数量:" << list.size();

    if (list.isEmpty()) {
        return false;
    }

    try {
        xlnt::workbook wb;
        auto ws = wb.active_sheet();
        ws.title(titile.toStdString());

        // === 设置表头 ===
        ws.cell("A1").value("单词");
        ws.cell("B1").value("音标");
        ws.cell("C1").value("释义");
        ws.cell("D1").value("例句");
        ws.cell("E1").value("翻译");
        ws.cell("F1").value("收藏");
        ws.cell("G1").value("生词");

        // === 表头样式 ===
        auto header_font = xlnt::font().bold(true).size(12);
        for (int col = 1; col <= 7; col++) {
            auto cell = ws.cell(col, 1);
            cell.font(header_font);
            cell.alignment(xlnt::alignment().horizontal(xlnt::horizontal_alignment::center));
        }

        // === 写入数据 ===
        int row = 2;
        for (const WordData& wordData : list) {
            ws.cell(1, row).value(wordData.word.toStdString());
            ws.cell(2, row).value(wordData.phonetic.toStdString());
            ws.cell(3, row).value(wordData.meaning.toStdString());
            ws.cell(4, row).value(wordData.example.toStdString());
            ws.cell(5, row).value(wordData.translation.toStdString());
            ws.cell(6, row).value(wordData.isCollected ? "是" : "否");
            ws.cell(7, row).value(wordData.isVocabulary ? "是" : "否");
            row++;
        }

        // === 调整列宽 ===
        ws.column_properties("A").width = 15;  // 单词
        ws.column_properties("B").width = 15;  // 音标
        ws.column_properties("C").width = 30;  // 释义
        ws.column_properties("D").width = 40;  // 例句
        ws.column_properties("E").width = 30;  // 翻译
        ws.column_properties("F").width = 10;  // 收藏
        ws.column_properties("G").width = 10;  // 生词

        // === 设置数据行样式 ===
        auto data_font = xlnt::font().size(11);
        for (int r = 2; r < row; r++) {
            for (int col = 1; col <= 7; col++) {
                auto cell = ws.cell(col, r);
                cell.font(data_font);
                cell.alignment(xlnt::alignment().horizontal(xlnt::horizontal_alignment::left));
            }
        }

        // === 保存文件 ===
        wb.save(filePath.toStdString());

        return true;

    } catch (const std::exception& e) {
        qCritical() << "Excel导出错误:" << e.what();
        return false;
    }
}

bool ExportManager::doExportPdf(const QList<WordData>& list, const QString& filePath, const QString &titile)
{
    qDebug() << "导出PDF:" << filePath << "数量:" << list.size();

    if (list.isEmpty()) {
        return false;
    }

    try {
        // 创建 QTextDocument
        QTextDocument document;

        // 构建 HTML 内容
        QString html;
        html += "<html><head><style>";
        html += "body { font-family: Arial, sans-serif; }";
        html += "table { width: 100%; border-collapse: collapse; }";
        html += "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }";
        html += "th { background-color: #f2f2f2; }";
        html += "h1 { text-align: center; }";
        html += "</style></head><body>";
        html += QString("<h1>%1</h1>").arg(titile.toHtmlEscaped());
        html += "<table>";
        html += "<tr>"
                "<th>单词</th>"
                "<th>音标</th>"
                "<th>释义</th>"
                "<th>例句</th>"
                "<th>翻译</th>"
                "<th>收藏</th>"
                "<th>生词</th>"
                "</tr>";

        for (const WordData& wordData : list) {
            html += QString("<tr>"
                            "<td>%1</td>"
                            "<td>%2</td>"
                            "<td>%3</td>"
                            "<td>%4</td>"
                            "<td>%5</td>"
                            "<td>%6</td>"
                            "<td>%7</td>"
                            "</tr>")
                        .arg(wordData.word.toHtmlEscaped())
                        .arg(wordData.phonetic.toHtmlEscaped())
                        .arg(wordData.meaning.toHtmlEscaped())
                        .arg(wordData.example.toHtmlEscaped())
                        .arg(wordData.translation.toHtmlEscaped())
                        .arg(wordData.isCollected ? "是" : "否")
                        .arg(wordData.isVocabulary ? "是" : "否");
        }

        html += "</table></body></html>";
        document.setHtml(html);

        // 创建 PDF 打印机
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(filePath);
        printer.setPageSize(QPageSize(QPageSize::A4));

        // 打印到 PDF
        document.print(&printer);
        return true;

    } catch (const std::exception& e) {
        qCritical() << "PDF导出错误:" << e.what();
        return false;
    }
}
QString ExportManager::getDefaultFileName(ExportFormat format, const QString &title) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return format == ExportFormat::Excel
               ? title + timestamp + ".xlsx"
               : title + timestamp + ".pdf";
}

QString ExportManager::getFilter(ExportFormat format) const
{
    return format == ExportFormat::Excel
               ? "Excel文件 (*.xlsx)"
               : "PDF文件 (*.pdf)";
}
