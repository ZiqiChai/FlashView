#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QPalette>

class QApplication;

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme { Light, Dark };
    Q_ENUM(Theme)

    static ThemeManager &instance();

    Theme currentTheme() const { return m_theme; }
    QColor backgroundColor() const { return m_bgColor; }
    void setTheme(Theme theme);
    void setBackgroundColor(const QColor &color);
    QString themeStyleSheet() const;
    void applyPalette() const;

signals:
    void themeChanged(Theme theme);
    void backgroundColorChanged(const QColor &color);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    Theme m_theme = Dark;
    QColor m_bgColor = QColor(24, 24, 32);
};

#endif // THEMEMANAGER_H
