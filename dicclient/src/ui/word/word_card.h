#ifndef WORD_CARD_H
#define WORD_CARD_H

#include "base/base_widget.h"
#include "word_data.h"
#include <QWidget>

/**
 * @brief 单词卡片组件
 *
 * 继承自 BaseWidget，用于显示单词的基本信息。
 * 特性：
 * 1. 支持双击展开/折叠例句
 * 2. 响应主题切换（QSS 动态属性驱动）
 * 3. 词性高亮显示
 * 4. 尺寸自适应内容
 */
class WordCard : public BaseWidget
{
    Q_OBJECT

public:
    explicit WordCard(const WordData& data, QWidget* parent = nullptr);

    void setWordData(const WordData& data);
    const WordData& getWordData() const { return m_data; }

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onThemeChanged(UITheme theme) override;

private:
    void setupUI();
    void updateContent();
    void updateCardSize();
    QString parseMeaningWithStyle(const QString& meaning) const;

    WordData m_data;
    bool m_expanded = false;

    // UI 组件
    QLabel* m_wordLabel = nullptr;
    QLabel* m_phoneticLabel = nullptr;
    QLabel* m_definitionLabel = nullptr;
    QLabel* m_exampleLabel = nullptr;
    QLabel* m_translationLabel = nullptr;
};

#endif // WORD_CARD_H
