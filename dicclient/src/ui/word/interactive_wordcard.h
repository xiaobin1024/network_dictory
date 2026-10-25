#ifndef INTERACTIVE_WORD_CARD_H
#define INTERACTIVE_WORD_CARD_H

#include "word_card.h"
#include "word_data.h"
#include "word_collect.h"
#include "word_vocabulary.h"
#include <QTextToSpeech>
#include"QPushButton"
#include"QHBoxLayout"
#include <QMenu>



/**
 * @brief 交互式单词卡片
 *
 * 在 WordCard 基础上添加：
 * 1. 收藏按钮（带图标，不同状态不同图标）
 * 2. 发音按钮（带图标）
 * 3. 添加到生词本按钮（带图标，不同状态不同图标）
 * 4. 更多操作菜单（带图标）
 */
class InteractiveWordCard : public WordCard
{
    Q_OBJECT

public:
    explicit InteractiveWordCard(const WordData& data,
                                 WordCollect* wordCollector,
                                 WordVocabulary* wordVocabulary,
                                 QWidget* parent = nullptr);

    bool isFavorite() const { return m_isFavorite; }
    bool isAddedToVocabulary() const { return m_isAddedToVocabulary; }

    void setFavorite(bool favorite);
    void setAddedToVocabulary(bool added);

signals:
    void favoriteToggled(const QString& word, bool favorite);
    void pronunciationRequested(const QString& word);
    void addToVocabularyRequested(const WordData& data);
    void removeFromVocabularyRequested(const QString& word);
    void editRequested(const WordData& data);
    void shareRequested(const WordData& data);

protected:
    void onThemeChanged(UITheme theme) override;

private slots:
    void onFavoriteButtonClicked();
    void onPronunciationButtonClicked();
    void onAddToVocabularyButtonClicked();
    void onMoreActionsButtonClicked();
    void onCollectStatusChanged(QString& word, bool isCollected);
    void onVocabularyStatusChanged(const QString& word, bool isVocabulary);

private:
    void setupInteractiveUI();
    void setupConnections();
    void updateButtonIcons();      // 更新图标资源
    void updateButtonStates();     // 更新动态属性和提示
    void createMoreActionsMenu();

    // 按钮
    QPushButton* m_favoriteButton = nullptr;
    QPushButton* m_pronunciationButton = nullptr;
    QPushButton* m_addToVocabularyButton = nullptr;
    QPushButton* m_moreActionsButton = nullptr;

    // 状态
    bool m_isFavorite = false;
    bool m_isAddedToVocabulary = false;

    // 数据与业务对象
    WordData m_wordData;
    WordCollect* m_wordCollector = nullptr;
    WordVocabulary* m_wordVocabulary = nullptr;

    // 语音
    QTextToSpeech* m_speech = nullptr;

    // 菜单
    QMenu* m_moreActionsMenu = nullptr;
};

#endif // INTERACTIVE_WORD_CARD_H
