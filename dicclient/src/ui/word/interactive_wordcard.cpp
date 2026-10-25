#include "word/interactive_wordcard.h"
#include"base/theme_manager.h"
#include"QStyle"
#include <QMenu>
#include <QAction>
#include <QIcon>

InteractiveWordCard::InteractiveWordCard(const WordData& data,
                                         WordCollect* wordCollector,
                                         WordVocabulary* wordVocabulary,
                                         QWidget* parent)
    : WordCard(data, parent)
    , m_wordData(data)
    , m_wordCollector(wordCollector)
    , m_wordVocabulary(wordVocabulary)
{
    setObjectName("interactiveWordCard");

    m_isFavorite = data.isCollected;
    m_isAddedToVocabulary = data.isVocabulary;

    // 初始化语音引擎
    m_speech = new QTextToSpeech(this);
    m_speech->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));

    setupInteractiveUI();
    setupConnections();

    // 词典补齐音标/例句/翻译后，例句是查词的主内容之一。
    // 卡片默认折叠例句的话，用户查完还得双击一次才看得到，等于白查，
    // 所以带例句的词条默认展开，没有例句的保持折叠（展开也只是多一片空白）。
    if (!data.example.isEmpty() || !data.translation.isEmpty()) {
        setExpanded(true);
    }

    updateButtonIcons();   // 初始化图标
    updateButtonStates();  // 初始化状态

    // 【新增】强制刷新 QSS
    if (QStyle* s = style()) {
        s->unpolish(this);
        s->polish(this);
    }

    // 【新增】递归刷新子控件
    for (QWidget* child : findChildren<QWidget*>()) {
        if (child->style()) {
            child->style()->unpolish(child);
            child->style()->polish(child);
        }
    }
}

void InteractiveWordCard::setupInteractiveUI()
{
    // 创建按钮容器
    QWidget* buttonContainer = new QWidget(this);
    buttonContainer->setObjectName("cardButtonContainer");

    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->setSpacing(10);

    // 创建按钮（文本为空，图标由 updateButtonIcons 设置）
    m_favoriteButton = new QPushButton(buttonContainer);
    m_favoriteButton->setObjectName("cardFavoriteBtn");
    m_favoriteButton->setFixedSize(32, 32);
    m_favoriteButton->setCursor(Qt::PointingHandCursor);
    m_favoriteButton->setProperty("actionType", "favorite");
    m_favoriteButton->setText("");  // 文本为空，只用图标

    m_pronunciationButton = new QPushButton(buttonContainer);
    m_pronunciationButton->setObjectName("cardPronunciationBtn");
    m_pronunciationButton->setFixedSize(32, 32);
    m_pronunciationButton->setCursor(Qt::PointingHandCursor);
    m_pronunciationButton->setProperty("actionType", "pronunciation");
    m_pronunciationButton->setText("");

    m_addToVocabularyButton = new QPushButton(buttonContainer);
    m_addToVocabularyButton->setObjectName("cardAddVocabBtn");
    m_addToVocabularyButton->setFixedSize(32, 32);
    m_addToVocabularyButton->setCursor(Qt::PointingHandCursor);
    m_addToVocabularyButton->setProperty("actionType", "vocabulary");
    m_addToVocabularyButton->setText("");

    m_moreActionsButton = new QPushButton(buttonContainer);
    m_moreActionsButton->setObjectName("cardMoreBtn");
    m_moreActionsButton->setFixedSize(32, 32);
    m_moreActionsButton->setCursor(Qt::PointingHandCursor);
    m_moreActionsButton->setProperty("actionType", "more");
    m_moreActionsButton->setText("");

    // 【修复】方案1：所有按钮平均分配空间
    buttonLayout->addWidget(m_favoriteButton, 1);        // 1 份空间
    buttonLayout->addWidget(m_pronunciationButton, 1);   // 1 份空间
    buttonLayout->addWidget(m_addToVocabularyButton, 1); // 1 份空间
    buttonLayout->addWidget(m_moreActionsButton, 1);     // 1 份空间

    // 添加到主布局（WordCard 的 m_mainLayout）
    mainLayout()->addWidget(buttonContainer);

    // 创建菜单
    createMoreActionsMenu();
}


void InteractiveWordCard::setupConnections()
{
    connect(m_favoriteButton, &QPushButton::clicked,
            this, &InteractiveWordCard::onFavoriteButtonClicked);
    connect(m_pronunciationButton, &QPushButton::clicked,
            this, &InteractiveWordCard::onPronunciationButtonClicked);
    connect(m_addToVocabularyButton, &QPushButton::clicked,
            this, &InteractiveWordCard::onAddToVocabularyButtonClicked);
    connect(m_moreActionsButton, &QPushButton::clicked,
            this, &InteractiveWordCard::onMoreActionsButtonClicked);

    if (m_wordCollector) {
        connect(m_wordCollector, &WordCollect::collectStatusChanged,
                this, &InteractiveWordCard::onCollectStatusChanged);
    }
    if (m_wordVocabulary) {
        connect(m_wordVocabulary, &WordVocabulary::vocabularyStatusChanged,
                this, &InteractiveWordCard::onVocabularyStatusChanged);
    }
}

void InteractiveWordCard::setFavorite(bool favorite)
{
    if (m_isFavorite == favorite) return;

    m_isFavorite = favorite;
    updateButtonIcons();   // 图标变化
    updateButtonStates();  // 状态变化

    showMessage(favorite ? "已添加到收藏" : "已取消收藏", false, 1500);
}

void InteractiveWordCard::setAddedToVocabulary(bool added)
{
    if (m_isAddedToVocabulary == added) return;

    m_isAddedToVocabulary = added;
    updateButtonIcons();   // 图标变化
    updateButtonStates();  // 状态变化
}

// ============ 图标更新（核心修改） ============

void InteractiveWordCard::updateButtonIcons()
{
    QString themePrefix = (ThemeManager::instance()->currentTheme() == UITheme::Light) ? "light" : "dark";

    qDebug() << "InteractiveWordCard::updateButtonIcons():m_isFavorite" << m_isFavorite;

    // 收藏按钮图标：根据状态和主题切换
    if (m_isFavorite) {
        QIcon starIcon(QString(":/imgxx/%1/star_bright.png").arg(themePrefix));
        if (starIcon.isNull()) {
            // 备用：使用 Unicode 字符 + 设置激活属性
            m_favoriteButton->setText("★");
            m_favoriteButton->setIcon(QIcon());
            m_favoriteButton->setProperty("active", true);  // ← 设置激活属性
        } else {
            m_favoriteButton->setIcon(starIcon);
            qDebug()<<"starIcon: "<<starIcon;
            m_favoriteButton->setIconSize(QSize(30, 30));
            m_favoriteButton->setText("");
            m_favoriteButton->setProperty("active", true);  // ← 设置激活属性
        }
    } else {
        QIcon starIcon(QString(":/imgxxx/%1/star_gray.png").arg(themePrefix));
        if (starIcon.isNull()) {
            m_favoriteButton->setText("☆");
            m_favoriteButton->setIcon(QIcon());
            m_favoriteButton->setProperty("active", false);  // ← 设置未激活属性
        } else {
            m_favoriteButton->setIcon(starIcon);
            m_favoriteButton->setIconSize(QSize(30, 30));
            m_favoriteButton->setText("");
            m_favoriteButton->setProperty("active", false);  // ← 设置未激活属性
        }
    }

    // 发音按钮 - 固定图标
    QIcon speakerIcon(QString(":/icons/%1/speaker.svg").arg(themePrefix));
    if (speakerIcon.isNull()) {
        m_pronunciationButton->setText("🔊");
        m_pronunciationButton->setIcon(QIcon());
    } else {
        m_pronunciationButton->setIcon(speakerIcon);
        m_pronunciationButton->setIconSize(QSize(30, 30));
        m_pronunciationButton->setText("");
    }


    // 生词本按钮
    if (m_isAddedToVocabulary) {
        QIcon bookmarkIcon(QString(":/icons/%1/bookmark_filled.svg").arg(themePrefix));
        if (bookmarkIcon.isNull()) {
            m_addToVocabularyButton->setText("🔖");
            m_addToVocabularyButton->setIcon(QIcon());
            m_addToVocabularyButton->setProperty("active", true);  // ← 设置激活属性
        } else {
            m_addToVocabularyButton->setIcon(bookmarkIcon);
            m_addToVocabularyButton->setIconSize(QSize(30, 30));
            m_addToVocabularyButton->setText("");
            m_addToVocabularyButton->setProperty("active", true);  // ← 设置激活属性
        }
    } else {
        QIcon bookmarkIcon(QString(":/icons/%1/bookmark_empty.svg").arg(themePrefix));
        if (bookmarkIcon.isNull()) {
            m_addToVocabularyButton->setText("📑");
            m_addToVocabularyButton->setIcon(QIcon());
            m_addToVocabularyButton->setProperty("active", false);  // ← 设置未激活属性
        } else {
            m_addToVocabularyButton->setIcon(bookmarkIcon);
            m_addToVocabularyButton->setIconSize(QSize(30, 30));
            m_addToVocabularyButton->setText("");
            m_addToVocabularyButton->setProperty("active", false);  // ← 设置未激活属性
        }
    }

    // 更多操作按钮 - 固定图标
    QIcon moreIcon(QString(":/icons/%1/more.svg").arg(themePrefix));
    if (moreIcon.isNull()) {
        m_moreActionsButton->setText("⋯");
        m_moreActionsButton->setIcon(QIcon());

        QFont starFont = m_moreActionsButton->font();
        starFont.setPixelSize(30); // 像素大小（推荐）
        starFont.setBold(true);    // 加粗
        m_moreActionsButton->setFont(starFont);
    } else {
        m_moreActionsButton->setIcon(moreIcon);
        m_moreActionsButton->setIconSize(QSize(20, 20));
        m_moreActionsButton->setText("");
    }

    // 【重要】刷新样式
    // m_favoriteButton->style()->unpolish(m_favoriteButton);
    // m_favoriteButton->style()->polish(m_favoriteButton);
    // m_addToVocabularyButton->style()->unpolish(m_addToVocabularyButton);
    // m_addToVocabularyButton->style()->polish(m_addToVocabularyButton);
}

void InteractiveWordCard::updateButtonStates()
{
    // 使用动态属性驱动 QSS 状态（颜色变化）
    m_favoriteButton->setProperty("active", m_isFavorite);
    m_addToVocabularyButton->setProperty("active", m_isAddedToVocabulary);

    // 更新工具提示
    m_favoriteButton->setToolTip(m_isFavorite ? "取消收藏" : "收藏");
    m_pronunciationButton->setToolTip("播放发音");
    m_addToVocabularyButton->setToolTip(m_isAddedToVocabulary ? "从生词本移除" : "添加到生词本");
    m_moreActionsButton->setToolTip("更多操作");

    m_favoriteButton->setProperty("hover", false);
    m_pronunciationButton->setProperty("hover", false);
    m_addToVocabularyButton->setProperty("hover", false);
    m_moreActionsButton->setProperty("hover",false);

    // 强制刷新样式
    if (QStyle* s = style()) {
        s->unpolish(m_favoriteButton);
        s->polish(m_favoriteButton);
        s->unpolish(m_addToVocabularyButton);
        s->polish(m_addToVocabularyButton);
    }
}

// ============ 按钮点击处理 ============

void InteractiveWordCard::onFavoriteButtonClicked()
{
    if (m_wordCollector) {
        m_wordCollector->collectWord(m_wordData);
    }
}

void InteractiveWordCard::onCollectStatusChanged(QString& word, bool isCollected)
{
    if (word != m_wordData.word) return;

    m_isFavorite = isCollected;
    m_wordData.isCollected = isCollected;
    updateButtonIcons();   // 更新图标
    updateButtonStates();  // 更新状态
    showMessage(isCollected ? "收藏成功" : "已取消收藏", false, 1500);
}

void InteractiveWordCard::onAddToVocabularyButtonClicked()
{
    if (m_wordVocabulary) {
        m_wordVocabulary->updateVocabularyStatus(m_wordData);
    }
}

void InteractiveWordCard::onVocabularyStatusChanged(const QString& word, bool isVocabulary)
{
    if (word != m_wordData.word) return;

    m_isAddedToVocabulary = isVocabulary;
    m_wordData.isVocabulary = isVocabulary;
    updateButtonIcons();   // 更新图标
    updateButtonStates();  // 更新状态
    showMessage(isVocabulary ? "添加生词成功" : "移除生词", false, 1500);
}

void InteractiveWordCard::onPronunciationButtonClicked()
{
    if (m_wordData.isValid()) {
        m_speech->say(m_wordData.word);
    }
}

void InteractiveWordCard::onMoreActionsButtonClicked()
{
    if (m_moreActionsMenu) {
        m_moreActionsMenu->popup(m_moreActionsButton->mapToGlobal(
            QPoint(0, m_moreActionsButton->height())));
    }
}

// ============ 菜单 ============

void InteractiveWordCard::createMoreActionsMenu()
{
    m_moreActionsMenu = new QMenu(this);
    m_moreActionsMenu->setObjectName("cardMoreMenu");

    QAction* editAction = new QAction("编辑单词", m_moreActionsMenu);
    QAction* shareAction = new QAction("分享", m_moreActionsMenu);
    QAction* noteAction = new QAction("添加笔记", m_moreActionsMenu);
    QAction* copyAction = new QAction("复制", m_moreActionsMenu);

    m_moreActionsMenu->addAction(editAction);
    m_moreActionsMenu->addAction(shareAction);
    m_moreActionsMenu->addAction(noteAction);
    m_moreActionsMenu->addSeparator();
    m_moreActionsMenu->addAction(copyAction);

    connect(editAction, &QAction::triggered, this, [this]() {
        emit editRequested(getWordData());
    });
    connect(shareAction, &QAction::triggered, this, [this]() {
        emit shareRequested(getWordData());
    });
    connect(noteAction, &QAction::triggered, this, [this]() {
        showMessage("笔记功能开发中...", false, 1500);
    });
    connect(copyAction, &QAction::triggered, this, [this]() {
        showMessage("已复制到剪贴板", false, 1000);
    });
}

// ============ 主题响应 ============

void InteractiveWordCard::onThemeChanged(UITheme theme)
{
    WordCard::onThemeChanged(theme);
    // 主题切换时更新图标（亮色/暗色图标可能不同）
    updateButtonIcons();
}
