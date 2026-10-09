#include "theme_manager.h"
#include <QApplication>
#include <QFile>
#include <QDebug>

QRegularExpression ThemeManager::s_placeholderRegex(R"(\{\{(\w+[-\w]*)\}\})");

ThemeManager* ThemeManager::instance() {
    static ThemeManager inst;
    return &inst;
}

ThemeManager::ThemeManager() {
    loadColorPalette();
}

void ThemeManager::loadColorPalette() {
    // 亮色主题色板
    m_lightPalette = {
        // 【修改】：bg-primary 保持纯白，bg-secondary 改为更淡的灰（#F8F9FA），形成“纸张”感
        // 或者改为更深的灰（#E5E7EB），形成“阴影”感。这里推荐用更淡的灰，看起来更现代。
        // 但为了区分明显，我们让侧边栏比主内容稍微“重”一点点，或者用边框区分。
        // 方案：侧边栏 #F0F2F5 (淡灰)，主内容 #FFFFFF。
        {"bg-primary", "#FFFFFF"},
        {"bg-secondary", "#F3F4F6"}, // 改为淡灰色，与白色区分

        {"bg-tertiary", "#E5E7EB"}, // 调整 tertiary 以保持层级
        {"text-primary", "#111827"}, // 加深文字，提高对比
        {"text-secondary", "#6B7280"},
        {"text-disabled", "#9CA3AF"},

        // 【修改】：边框颜色加深，确保在淡灰背景上能看见
        {"border-color", "#E5E7EB"},
        {"border-disabled", "#F3F4F6"},

        {"btn-primary-bg", "#3B82F6"}, {"btn-primary-hover", "#2563EB"}, {"btn-primary-pressed", "#1D4ED8"},
        {"btn-primary-text", "#FFFFFF"},
        {"btn-secondary-bg", "#4B5563"}, {"btn-secondary-hover", "#374151"}, {"btn-secondary-text", "#FFFFFF"},
        {"input-bg", "#FFFFFF"}, {"input-border", "#D1D5DB"}, {"input-focus", "#3B82F6"},
        {"scroll-bg", "#F3F4F6"}, {"scroll-handle", "#D1D5DB"}, {"scroll-hover", "#9CA3AF"},
        {"error-bg", "#EF4444"}, {"error-text", "#FFFFFF"}, {"error-border", "#DC2626"}
    };

    // 暗色主题色板
    m_darkPalette = {
        // 【修改】：主内容保持 #1E1E24，侧边栏改为更深的 #121216，形成强烈的层次感
        {"bg-primary", "#1E1E24"},
        {"bg-secondary", "#121216"}, // 更深的黑色，与主内容明显区分

        {"bg-tertiary", "#2A2A35"},
        {"text-primary", "#F3F4F6"},
        {"text-secondary", "#9CA3AF"},
        {"text-disabled", "#6B7280"},

        // 【修改】：边框颜色调整，在深黑背景上显示为深灰色线条
        {"border-color", "#2A2A35"},
        {"border-disabled", "#1F2937"},

        {"btn-primary-bg", "#3B82F6"}, {"btn-primary-hover", "#2563EB"}, {"btn-primary-pressed", "#1D4ED8"},
        {"btn-primary-text", "#FFFFFF"},
        {"btn-secondary-bg", "#4B5563"}, {"btn-secondary-hover", "#374151"}, {"btn-secondary-text", "#FFFFFF"},
        {"input-bg", "#1F2937"}, {"input-border", "#374151"}, {"input-focus", "#3B82F6"},
        {"scroll-bg", "#121216"}, {"scroll-handle", "#374151"}, {"scroll-hover", "#4B5563"},
        {"error-bg", "#EF4444"}, {"error-text", "#FFFFFF"}, {"error-border", "#B91C1C"}
    };
}

void ThemeManager::setTheme(UITheme theme) {
    if (m_currentTheme == theme) return;
    m_currentTheme = theme;
    QString qss = generateQSS();
    if (qApp) qApp->setStyleSheet(qss);
    emit themeChanged(theme);
}

void ThemeManager::toggleTheme() {
    setTheme(m_currentTheme == UITheme::Light ? UITheme::Dark : UITheme::Light);
}

QString ThemeManager::getColor(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    const auto& palette = (m_currentTheme == UITheme::Light) ? m_lightPalette : m_darkPalette;
    return palette.value(key, "#000000");
}

QString ThemeManager::generateQSS() const {

    const QString themeName = (m_currentTheme == UITheme::Light) ? "light" : "dark";

    // 1. 构造 QFile 对象
    QFile file(QString(":/styles/%1.qss").arg(themeName));

    qDebug() << "加载的样式表路径：" << file.fileName();
    // 2. 以只读文本模式打开
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open QSS file:" << themeName;
        return "";
    }

    // 3. 读取全部内容（readAll 返回 QByteArray，需要转为 QString）
    QString qssTemplate = QString::fromUtf8(file.readAll());
    file.close();

    if (qssTemplate.isEmpty()) {
        qWarning() << "QSS template is empty:" << themeName;
        return "";
    }

    QString result = qssTemplate;
    auto it = s_placeholderRegex.globalMatch(result);
    while (it.hasNext()) {
        auto match = it.next();
        QString key = match.captured(1);
        QString color = getColor(key);
        result.replace(match.captured(0), color);
    }
    return result;
}
