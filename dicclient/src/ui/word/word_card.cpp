#include "word_card.h"
#include <QMouseEvent>
#include <QRegularExpression>
#include <QDebug>

WordCard::WordCard(const WordData& data, QWidget* parent)
    : BaseWidget(parent)
    , m_data(data)
{
    setObjectName("wordCard");
    setupUI();
    updateContent();
}

void WordCard::setupUI()
{
    // 设置卡片尺寸约束
    setMinimumWidth(260);
    setMinimumHeight(140);
    setMaximumWidth(1000);

    // 配置主布局
    mainLayout()->setContentsMargins(0, 0, 0, 0);
    mainLayout()->setSpacing(0);

    // 【新增】创建内容容器，设置内边距
    QWidget* contentWidget = new QWidget(this);
    contentWidget->setObjectName("cardContent");
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 16, 16, 8);  // 上 右 下 左
    contentLayout->setSpacing(8);

    // 创建标签（使用对象名供 QSS 选择）
    m_wordLabel = new QLabel(contentWidget);
    m_wordLabel->setObjectName("cardWordLabel");
    m_wordLabel->setWordWrap(true);

    m_phoneticLabel = new QLabel(contentWidget);
    m_phoneticLabel->setObjectName("cardPhoneticLabel");

    m_definitionLabel = new QLabel(contentWidget);
    m_definitionLabel->setObjectName("cardDefinitionLabel");
    m_definitionLabel->setWordWrap(true);
    m_definitionLabel->setTextFormat(Qt::RichText);

    m_exampleLabel = new QLabel(contentWidget);
    m_exampleLabel->setObjectName("cardExampleLabel");
    m_exampleLabel->setWordWrap(true);
    m_exampleLabel->hide();

    m_translationLabel = new QLabel(contentWidget);
    m_translationLabel->setObjectName("cardTranslationLabel");
    m_translationLabel->setWordWrap(true);
    m_translationLabel->hide();

    // 添加到布局
    // 【修改】添加到 contentLayout 而不是 mainLayout
    contentLayout->addWidget(m_wordLabel);
    contentLayout->addWidget(m_phoneticLabel);
    contentLayout->addWidget(m_definitionLabel);
    contentLayout->addWidget(m_exampleLabel);
    contentLayout->addWidget(m_translationLabel);

     mainLayout()->addWidget(contentWidget);
}

void WordCard::setWordData(const WordData& data)
{
    m_data = data;
    updateContent();
}

void WordCard::setExpanded(bool expanded)
{
    if (m_expanded == expanded) return;

    m_expanded = expanded;

    // 控制例句与翻译的显示/隐藏
    if (m_expanded && !m_data.example.isEmpty()) {
        m_exampleLabel->show();
    } else {
        m_exampleLabel->hide();
    }

    if (m_expanded && !m_data.translation.isEmpty()) {
        m_translationLabel->show();
    } else {
        m_translationLabel->hide();
    }

    updateGeometry();
    updateCardSize();
}

/* 通过各标签的实际换行高度精确计算卡片高度，避免长例句被裁剪 */
void WordCard::updateCardSize()
{
    if (width() <= 0) return;

    // 内容区可显示宽度 = 卡片宽度 - 左右边距(16*2)
    const int contentWidth = qMax(80, width() - 32);

    int h = 24;  // 内容区上(16)下(8)内边距
    auto addLabel = [&](QLabel* label, bool visible) {
        if (!label || !visible || label->text().isEmpty()) return;
        h += label->heightForWidth(contentWidth) + 8;  // 标签高度 + 间距
    };

    addLabel(m_wordLabel, true);
    addLabel(m_phoneticLabel, true);
    addLabel(m_definitionLabel, true);
    addLabel(m_exampleLabel, m_expanded);
    addLabel(m_translationLabel, m_expanded);

    // 交互卡片底部的按钮容器
    if (QWidget* btnContainer = findChild<QWidget*>("cardButtonContainer")) {
        h += btnContainer->sizeHint().height();
    }

    h = qMax(160, h);
    if (h != minimumHeight()) {
        setMinimumHeight(h);
    }
}

void WordCard::resizeEvent(QResizeEvent* event)
{
    BaseWidget::resizeEvent(event);   // 保留消息悬浮层定位
    updateCardSize();
}

QSize WordCard::sizeHint() const
{
    int w = width() > 0 ? width() : 320;
    const int contentWidth = qMax(80, w - 32);

    int h = 24;  // 内容区上(16) + 下(8)内边距
    auto addLabel = [&](QLabel* label, bool visible) {
        if (!label || !visible || label->text().isEmpty()) return;
        h += label->heightForWidth(contentWidth) + 8;
    };

    addLabel(m_wordLabel, true);
    addLabel(m_phoneticLabel, true);
    addLabel(m_definitionLabel, true);
    addLabel(m_exampleLabel, m_expanded);
    addLabel(m_translationLabel, m_expanded);

    if (QWidget* btnContainer = findChild<QWidget*>("cardButtonContainer")) {
        h += btnContainer->sizeHint().height();
    }

    return QSize(w, qMax(160, h));
}

QSize WordCard::minimumSizeHint() const
{
    return QSize(260, 140);
}

void WordCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    BaseWidget::mousePressEvent(event);
}

void WordCard::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
        setExpanded(!m_expanded);
    }
    BaseWidget::mouseDoubleClickEvent(event);
}

void WordCard::onThemeChanged(UITheme theme)
{
    Q_UNUSED(theme);
    // QSS 自动处理颜色变化
    // 如需更新图标资源，可在此添加
}

void WordCard::updateContent()
{
    if (!m_data.isValid()) {
        m_wordLabel->setText("No data");
        m_phoneticLabel->setText("");
        m_definitionLabel->setText("");
        m_exampleLabel->setText("");
        m_exampleLabel->hide();
        m_translationLabel->setText("");
        m_translationLabel->hide();
        return;
    }

    // 更新单词
    m_wordLabel->setText(m_data.word);
    m_wordLabel->setToolTip(m_data.word);

    // 更新音标
    // 词典里的音标本身已经带斜杠（/ə'bændənmənt/），早期版本音标是空串，
    // 展示时才自己补一对斜杠。无条件拼接会出现 //ə'...// 这种双层斜杠，
    // 所以只在音标里本来没有斜杠时才补。
    if (!m_data.phonetic.isEmpty()) {
        m_phoneticLabel->setText(wrapPhonetic(m_data.phonetic));
        m_phoneticLabel->show();
    } else {
        m_phoneticLabel->hide();
    }

    // 更新释义（带词性高亮）
    QString styledMeaning = parseMeaningWithStyle(m_data.meaning);
    m_definitionLabel->setText(styledMeaning);
    m_definitionLabel->setToolTip(m_data.meaning);

    // 更新例句
    if (!m_data.example.isEmpty()) {
        m_exampleLabel->setText("\"" + m_data.example + "\"");
        m_exampleLabel->setToolTip("例句");
        if (m_expanded) m_exampleLabel->show();
    } else {
        m_exampleLabel->hide();
    }

    // 更新翻译
    if (!m_data.translation.isEmpty()) {
        m_translationLabel->setText(m_data.translation);
        m_translationLabel->setToolTip("翻译");
        if (m_expanded) m_translationLabel->show();
    } else {
        m_translationLabel->hide();
    }

    adjustSize();
    updateGeometry();
    updateCardSize();
}

/* 音标展示：已有斜杠就不重复包，避免 //ə'bændənmənt// */
QString WordCard::wrapPhonetic(const QString& phonetic) const
{
    if (phonetic.isEmpty()) {
        return QString();
    }
    if (phonetic.contains('/')) {
        return phonetic;
    }
    return "/" + phonetic + "/";
}

QString WordCard::parseMeaningWithStyle(const QString& meaning) const
{
    QRegularExpression re("(\\b(?:n\\.|adj\\.|prep\\.|adv\\.|v\\.|pron\\.|art\\.|num\\.|intj\\.|conj\\.|aux\\.|det\\.|mod\\.|part\\.)\\s+)");
    QRegularExpressionMatchIterator it = re.globalMatch(meaning);

    QString result;
    int lastPos = 0;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int startPos = match.capturedStart();
        int endPos = match.capturedEnd();

        if (startPos > lastPos) {
            result += meaning.mid(lastPos, startPos - lastPos);
        }

        QString pos = meaning.mid(startPos, endPos - startPos);
        // 使用 class 供 QSS 控制颜色，而非硬编码
        result += "<span class='pos-highlight'>" + pos + "</span>";

        int nextStartPos = it.hasNext() ? it.peekNext().capturedStart() : meaning.length();
        QString definition = meaning.mid(endPos, nextStartPos - endPos);
        result += definition;

        lastPos = nextStartPos;
    }

    if (lastPos < meaning.length()) {
        result += meaning.mid(lastPos);
    }

    return result;
}
