#include "ThemeManager.h"
#include <QApplication>
#include <QSettings>

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent)
{
    QSettings s;
    m_theme = static_cast<Theme>(s.value("theme", Dark).toInt());
    m_bgColor = s.value("backgroundColor", QColor(24, 24, 32)).value<QColor>();
    applyPalette();
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;
    if (theme == Dark)
        m_bgColor = QColor(24, 24, 32);
    else
        m_bgColor = QColor(250, 250, 252);
    QSettings s;
    s.setValue("theme", static_cast<int>(theme));
    s.setValue("backgroundColor", m_bgColor);
    applyPalette();
    emit themeChanged(theme);
    emit backgroundColorChanged(m_bgColor);
}

void ThemeManager::setBackgroundColor(const QColor &color)
{
    if (m_bgColor == color) return;
    m_bgColor = color;
    QSettings s;
    s.setValue("backgroundColor", color);
    emit backgroundColorChanged(color);
}

QString ThemeManager::themeStyleSheet() const
{
    if (m_theme == Dark) {
        // ── Dark Theme ──
        // Palette: deep charcoal + soft blue accent
        return R"(
            /* ── Base ── */
            QMainWindow { background-color: #181820; color: #d8d8e8; }

            /* ── Menu Bar ── */
            QMenuBar {
                background-color: #1c1c28;
                color: #a8a8c0;
                border-bottom: 1px solid #2a2a3c;
                padding: 2px 0;
            }
            QMenuBar::item {
                padding: 5px 12px;
                border-radius: 6px;
                margin: 2px 1px;
            }
            QMenuBar::item:selected { background-color: #2a2a40; color: #e8e8f4; }
            QMenuBar::item:pressed  { background-color: #4a80e0; color: #fff; }

            /* ── Drop Menus ── */
            QMenu {
                background-color: #202030;
                color: #d0d0e0;
                border: 1px solid #303048;
                border-radius: 10px;
                padding: 6px 4px;
            }
            QMenu::item {
                padding: 6px 28px 6px 12px;
                border-radius: 6px;
                margin: 1px 2px;
            }
            QMenu::item:selected { background-color: #2e2e48; color: #fff; }
            QMenu::item:disabled { color: #555568; }
            QMenu::separator { height: 1px; background: #2a2a40; margin: 4px 12px; }
            QMenu::indicator { width: 16px; height: 16px; margin-left: 6px; }
            QMenu::indicator:checked { image: none; background: #4a80e0; border: 2px solid #4a80e0; border-radius: 3px; }

            /* ── Toolbar ── */
            QToolBar {
                background-color: #1c1c28;
                border: none;
                border-bottom: 1px solid #2a2a3c;
                spacing: 4px;
                padding: 2px 10px;
            }
            QToolButton {
                background: transparent;
                color: #b0b0c8;
                border: none;
                border-radius: 7px;
                padding: 2px;
                min-width: 34px;
                min-height: 32px;
            }
            QToolButton:hover   { background-color: #2a2a40; color: #e8e8f4; }
            QToolButton:pressed { background-color: #4a80e0; color: #fff; }
            QToolButton:checked { background-color: #2a3a58; color: #6aa0f0; }
            QToolBar::separator {
                width: 1px; background: #2a2a3c;
                margin: 4px 6px;
            }

            /* ── Status Bar ── */
            QStatusBar {
                background-color: #1c1c28;
                color: #7878a0;
                border-top: 1px solid #2a2a3c;
                font-size: 12px;
                padding: 0 4px;
                min-height: 26px;
            }
            QStatusBar QLabel {
                color: #8888a8;
                padding: 2px 8px;
            }
            QStatusBar QLabel#statusSegment {
                border-left: 1px solid #2a2a3c;
                color: #9898b8;
            }
            QStatusBar QLabel#statusFileInfo {
                color: #b0b0c8;
            }

            /* ── Scroll Bars ── */
            QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; border-radius: 4px; }
            QScrollBar::handle:horizontal { background: #3a3a54; border-radius: 4px; min-width: 28px; }
            QScrollBar::handle:horizontal:hover { background: #4a4a68; }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
            QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
            QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; border-radius: 4px; }
            QScrollBar::handle:vertical { background: #3a3a54; border-radius: 4px; min-height: 28px; }
            QScrollBar::handle:vertical:hover { background: #4a4a68; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

            /* ── Dialog ── */
            QDialog { background-color: #1e1e2c; color: #d8d8e8; }

            /* ── Labels ── */
            QLabel { color: #d0d0e0; }

            /* ── Buttons ── */
            QPushButton {
                background-color: #4a80e0;
                color: #fff;
                border: none;
                border-radius: 8px;
                padding: 8px 20px;
                font-weight: 500;
            }
            QPushButton:hover   { background-color: #5a90f0; }
            QPushButton:pressed { background-color: #3a70d0; }
            QPushButton:disabled { background-color: #2a2a3c; color: #555568; }

            /* ── ComboBox ── */
            QComboBox {
                background-color: #222236;
                color: #d0d0e0;
                border: 1px solid #303048;
                border-radius: 8px;
                padding: 6px 12px;
                min-height: 24px;
            }
            QComboBox:hover { border-color: #4a80e0; }
            QComboBox::drop-down { border: none; width: 20px; }
            QComboBox QAbstractItemView {
                background-color: #222236;
                color: #d0d0e0;
                selection-background-color: #2e2e48;
                border: 1px solid #303048;
                border-radius: 8px;
                padding: 4px;
                outline: none;
            }

            /* ── Line Edit ── */
            QLineEdit {
                background-color: #222236;
                color: #d0d0e0;
                border: 1px solid #303048;
                border-radius: 8px;
                padding: 7px 12px;
            }
            QLineEdit:focus { border-color: #4a80e0; }

            /* ── Group Box ── */
            QGroupBox {
                color: #b0b0c8;
                border: 1px solid #2a2a3c;
                border-radius: 10px;
                margin-top: 14px;
                padding-top: 16px;
            }
            QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; }

            /* ── Tabs ── */
            QTabWidget::pane {
                border: 1px solid #2a2a3c;
                border-radius: 8px;
                background: #1e1e2c;
            }
            QTabBar::tab {
                background: #1c1c28;
                color: #7878a0;
                border: none;
                padding: 9px 20px;
                border-top-left-radius: 8px;
                border-top-right-radius: 8px;
                margin-right: 2px;
            }
            QTabBar::tab:hover { color: #b0b0c8; background: #222236; }
            QTabBar::tab:selected { background: #1e1e2c; color: #e0e0f0; }

            /* ── List Widget ── */
            QListWidget {
                background-color: #1e1e2c;
                color: #d0d0e0;
                border: 1px solid #2a2a3c;
                border-radius: 8px;
                outline: none;
            }
            QListWidget::item { padding: 6px 8px; border-radius: 6px; margin: 1px 4px; }
            QListWidget::item:selected { background-color: #2e2e48; color: #fff; }
            QListWidget::item:hover { background-color: #252538; }

            /* ── CheckBox ── */
            QCheckBox { color: #d0d0e0; spacing: 10px; }
            QCheckBox::indicator {
                width: 18px; height: 18px;
                border-radius: 5px;
                border: 2px solid #3a3a54;
                background: #222236;
            }
            QCheckBox::indicator:hover { border-color: #4a80e0; }
            QCheckBox::indicator:checked {
                background-color: #4a80e0;
                border-color: #4a80e0;
            }

            /* ── Key Sequence Edit ── */
            QKeySequenceEdit {
                background-color: #222236;
                color: #d0d0e0;
                border: 1px solid #303048;
                border-radius: 8px;
                padding: 5px 8px;
            }
            QKeySequenceEdit:focus { border-color: #4a80e0; }

            /* ── Slider (for video controls) ── */
            QSlider::groove:horizontal { background: #2a2a3c; height: 4px; border-radius: 2px; }
            QSlider::handle:horizontal {
                background: #4a80e0; width: 14px; height: 14px;
                margin: -5px 0; border-radius: 7px;
            }
            QSlider::handle:horizontal:hover { background: #5a90f0; }
            QSlider::sub-page:horizontal { background: #4a80e0; border-radius: 2px; }
        )";
    } else {
        // ── Light Theme ──
        // Palette: clean white + refined blue accent
        return R"(
            /* ── Base ── */
            QMainWindow { background-color: #fafafa; color: #1f1f2e; }

            /* ── Menu Bar ── */
            QMenuBar {
                background-color: #ffffff;
                color: #5a5a6e;
                border-bottom: 1px solid #eaeaf0;
                padding: 2px 0;
            }
            QMenuBar::item {
                padding: 5px 12px;
                border-radius: 6px;
                margin: 2px 1px;
            }
            QMenuBar::item:selected { background-color: #f0f0f6; color: #1f1f2e; }
            QMenuBar::item:pressed  { background-color: #4a80e0; color: #fff; }

            /* ── Drop Menus ── */
            QMenu {
                background-color: #ffffff;
                color: #3a3a4e;
                border: 1px solid #e4e4ee;
                border-radius: 10px;
                padding: 6px 4px;
            }
            QMenu::item {
                padding: 6px 28px 6px 12px;
                border-radius: 6px;
                margin: 1px 2px;
            }
            QMenu::item:selected { background-color: #eef2ff; color: #1f1f2e; }
            QMenu::item:disabled { color: #b8b8c8; }
            QMenu::separator { height: 1px; background: #eaeaf0; margin: 4px 12px; }
            QMenu::indicator { width: 16px; height: 16px; margin-left: 6px; }
            QMenu::indicator:checked { image: none; background: #4a80e0; border: 2px solid #4a80e0; border-radius: 3px; }

            /* ── Toolbar ── */
            QToolBar {
                background-color: #ffffff;
                border: none;
                border-bottom: 1px solid #eaeaf0;
                spacing: 4px;
                padding: 2px 10px;
            }
            QToolButton {
                background: transparent;
                color: #6a6a80;
                border: none;
                border-radius: 7px;
                padding: 2px;
                min-width: 34px;
                min-height: 32px;
            }
            QToolButton:hover   { background-color: #f0f0f6; color: #1f1f2e; }
            QToolButton:pressed { background-color: #4a80e0; color: #fff; }
            QToolButton:checked { background-color: #e8eeff; color: #4a80e0; }
            QToolBar::separator {
                width: 1px; background: #eaeaf0;
                margin: 4px 6px;
            }

            /* ── Status Bar ── */
            QStatusBar {
                background-color: #ffffff;
                color: #8a8a9e;
                border-top: 1px solid #eaeaf0;
                font-size: 12px;
                padding: 0 4px;
                min-height: 26px;
            }
            QStatusBar QLabel {
                color: #8a8a9e;
                padding: 2px 8px;
            }
            QStatusBar QLabel#statusSegment {
                border-left: 1px solid #eaeaf0;
                color: #6a6a80;
            }
            QStatusBar QLabel#statusFileInfo {
                color: #4a4a60;
            }

            /* ── Scroll Bars ── */
            QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; border-radius: 4px; }
            QScrollBar::handle:horizontal { background: #d0d0dc; border-radius: 4px; min-width: 28px; }
            QScrollBar::handle:horizontal:hover { background: #b0b0c0; }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
            QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
            QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; border-radius: 4px; }
            QScrollBar::handle:vertical { background: #d0d0dc; border-radius: 4px; min-height: 28px; }
            QScrollBar::handle:vertical:hover { background: #b0b0c0; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

            /* ── Dialog ── */
            QDialog { background-color: #fafafa; color: #1f1f2e; }

            /* ── Labels ── */
            QLabel { color: #3a3a4e; }

            /* ── Buttons ── */
            QPushButton {
                background-color: #4a80e0;
                color: #fff;
                border: none;
                border-radius: 8px;
                padding: 8px 20px;
                font-weight: 500;
            }
            QPushButton:hover   { background-color: #3a70d0; }
            QPushButton:pressed { background-color: #2a60c0; }
            QPushButton:disabled { background-color: #e8e8f0; color: #b8b8c8; }

            /* ── ComboBox ── */
            QComboBox {
                background-color: #ffffff;
                color: #3a3a4e;
                border: 1px solid #d8d8e4;
                border-radius: 8px;
                padding: 6px 12px;
                min-height: 24px;
            }
            QComboBox:hover { border-color: #4a80e0; }
            QComboBox::drop-down { border: none; width: 20px; }
            QComboBox QAbstractItemView {
                background-color: #ffffff;
                color: #3a3a4e;
                selection-background-color: #eef2ff;
                border: 1px solid #e4e4ee;
                border-radius: 8px;
                padding: 4px;
                outline: none;
            }

            /* ── Line Edit ── */
            QLineEdit {
                background-color: #ffffff;
                color: #3a3a4e;
                border: 1px solid #d8d8e4;
                border-radius: 8px;
                padding: 7px 12px;
            }
            QLineEdit:focus { border-color: #4a80e0; }

            /* ── Group Box ── */
            QGroupBox {
                color: #5a5a6e;
                border: 1px solid #e4e4ee;
                border-radius: 10px;
                margin-top: 14px;
                padding-top: 16px;
            }
            QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; }

            /* ── Tabs ── */
            QTabWidget::pane {
                border: 1px solid #e4e4ee;
                border-radius: 8px;
                background: #ffffff;
            }
            QTabBar::tab {
                background: #f0f0f6;
                color: #8a8a9e;
                border: none;
                padding: 9px 20px;
                border-top-left-radius: 8px;
                border-top-right-radius: 8px;
                margin-right: 2px;
            }
            QTabBar::tab:hover { color: #5a5a6e; background: #e8e8f0; }
            QTabBar::tab:selected { background: #ffffff; color: #1f1f2e; }

            /* ── List Widget ── */
            QListWidget {
                background-color: #ffffff;
                color: #3a3a4e;
                border: 1px solid #e4e4ee;
                border-radius: 8px;
                outline: none;
            }
            QListWidget::item { padding: 6px 8px; border-radius: 6px; margin: 1px 4px; }
            QListWidget::item:selected { background-color: #eef2ff; color: #1f1f2e; }
            QListWidget::item:hover { background-color: #f5f5fa; }

            /* ── CheckBox ── */
            QCheckBox { color: #3a3a4e; spacing: 10px; }
            QCheckBox::indicator {
                width: 18px; height: 18px;
                border-radius: 5px;
                border: 2px solid #c8c8d8;
                background: #ffffff;
            }
            QCheckBox::indicator:hover { border-color: #4a80e0; }
            QCheckBox::indicator:checked {
                background-color: #4a80e0;
                border-color: #4a80e0;
            }

            /* ── Key Sequence Edit ── */
            QKeySequenceEdit {
                background-color: #ffffff;
                color: #3a3a4e;
                border: 1px solid #d8d8e4;
                border-radius: 8px;
                padding: 5px 8px;
            }
            QKeySequenceEdit:focus { border-color: #4a80e0; }

            /* ── Slider (for video controls) ── */
            QSlider::groove:horizontal { background: #e0e0ea; height: 4px; border-radius: 2px; }
            QSlider::handle:horizontal {
                background: #4a80e0; width: 14px; height: 14px;
                margin: -5px 0; border-radius: 7px;
            }
            QSlider::handle:horizontal:hover { background: #3a70d0; }
            QSlider::sub-page:horizontal { background: #4a80e0; border-radius: 2px; }
        )";
    }
}

void ThemeManager::applyPalette() const
{
    auto *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) return;

    QPalette pal;
    if (m_theme == Dark) {
        QColor base(24, 24, 32);       // window / title bar
        QColor surface(28, 28, 40);
        QColor text(216, 216, 232);
        QColor dimText(168, 168, 192);
        QColor highlight(74, 128, 224);

        pal.setColor(QPalette::Window,          base);
        pal.setColor(QPalette::WindowText,      text);
        pal.setColor(QPalette::Base,            surface);
        pal.setColor(QPalette::AlternateBase,   base);
        pal.setColor(QPalette::Text,            text);
        pal.setColor(QPalette::Button,          surface);
        pal.setColor(QPalette::ButtonText,      text);
        pal.setColor(QPalette::Highlight,       highlight);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::ToolTipBase,     surface);
        pal.setColor(QPalette::ToolTipText,     text);
        pal.setColor(QPalette::PlaceholderText, dimText);
        pal.setColor(QPalette::Link,            highlight);
        pal.setColor(QPalette::Disabled, QPalette::Text,       QColor(80, 80, 110));
        pal.setColor(QPalette::Disabled, QPalette::WindowText,  QColor(80, 80, 110));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText,  QColor(80, 80, 110));
    } else {
        QColor base(250, 250, 252);
        QColor surface(255, 255, 255);
        QColor text(31, 31, 46);
        QColor dimText(120, 120, 140);
        QColor highlight(74, 128, 224);

        pal.setColor(QPalette::Window,          base);
        pal.setColor(QPalette::WindowText,      text);
        pal.setColor(QPalette::Base,            surface);
        pal.setColor(QPalette::AlternateBase,   base);
        pal.setColor(QPalette::Text,            text);
        pal.setColor(QPalette::Button,          surface);
        pal.setColor(QPalette::ButtonText,      text);
        pal.setColor(QPalette::Highlight,       highlight);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::ToolTipBase,     surface);
        pal.setColor(QPalette::ToolTipText,     text);
        pal.setColor(QPalette::PlaceholderText, dimText);
        pal.setColor(QPalette::Link,            highlight);
        pal.setColor(QPalette::Disabled, QPalette::Text,       QColor(160, 160, 176));
        pal.setColor(QPalette::Disabled, QPalette::WindowText,  QColor(160, 160, 176));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText,  QColor(160, 160, 176));
    }
    app->setPalette(pal);
}
