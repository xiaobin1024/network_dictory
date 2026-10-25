#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QRegularExpression>

 enum class UITheme { Light, Dark };

class ThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int currentTheme READ currentThemeInt WRITE setCurrentThemeInt NOTIFY themeChanged)

public:


    static ThemeManager* instance();
    ~ThemeManager() override = default;

    UITheme currentTheme() const { return m_currentTheme; }
    int currentThemeInt() const { return static_cast<int>(m_currentTheme); }
    void setCurrentThemeInt(int theme) { setTheme(static_cast<UITheme>(theme)); }

    void setTheme(UITheme theme);
    void toggleTheme();
    QString getColor(const QString& key) const;

signals:
     void themeChanged(UITheme newTheme);

private:
     ThemeManager();
     void loadColorPalette();
     QString generateQSS() const;

     UITheme m_currentTheme = UITheme::Dark;
     QMap<QString, QString> m_lightPalette;
     QMap<QString, QString> m_darkPalette;
     mutable QMutex m_mutex;
     static QRegularExpression s_placeholderRegex;
};

#endif // THEME_MANAGER_H
