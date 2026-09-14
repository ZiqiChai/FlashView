#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    int languageIndex() const;
    int themeIndex() const;
    QColor backgroundColor() const;
    bool thumbnailsVisible() const;
    bool wheelZoomEnabled() const;
    int interpolationMode() const;
    int sortKey() const;
    bool sortDescending() const;
    bool confirmDelete() const;

    void setLanguageIndex(int idx);
    void setThemeIndex(int idx);
    void setBackgroundColor(const QColor &c);
    void setThumbnailsVisible(bool v);
    void setWheelZoomEnabled(bool v);
    void setInterpolationMode(int mode);
    void setSortKey(int key);
    void setSortDescending(bool descending);
    void setConfirmDelete(bool v);

    // Key sequences
    QKeySequence keyNext() const;
    QKeySequence keyPrev() const;
    QKeySequence keyZoomIn() const;
    QKeySequence keyZoomOut() const;
    QKeySequence keyFitWindow() const;
    QKeySequence keyActualSize() const;
    QKeySequence keyRotateLeft() const;
    QKeySequence keyRotateRight() const;
    QKeySequence keyFullscreen() const;
    QKeySequence keyDelete() const;

    void setKeyNext(const QKeySequence &ks);
    void setKeyPrev(const QKeySequence &ks);
    void setKeyZoomIn(const QKeySequence &ks);
    void setKeyZoomOut(const QKeySequence &ks);
    void setKeyFitWindow(const QKeySequence &ks);
    void setKeyActualSize(const QKeySequence &ks);
    void setKeyRotateLeft(const QKeySequence &ks);
    void setKeyRotateRight(const QKeySequence &ks);
    void setKeyFullscreen(const QKeySequence &ks);
    void setKeyDelete(const QKeySequence &ks);

public slots:
    void accept() override;

private:
    void setupUI();

    QTabWidget *m_tabs = nullptr;

    // General tab
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QPushButton *m_bgColorBtn = nullptr;
    QCheckBox *m_thumbCheck = nullptr;
    QCheckBox *m_wheelZoomCheck = nullptr;
    QComboBox *m_interpolationCombo = nullptr;
    QComboBox *m_sortKeyCombo = nullptr;
    QComboBox *m_sortOrderCombo = nullptr;
    QCheckBox *m_confirmDeleteCheck = nullptr;
    QColor m_bgColor;

    // Keys tab
    QKeySequenceEdit *m_keyNext = nullptr;
    QKeySequenceEdit *m_keyPrev = nullptr;
    QKeySequenceEdit *m_keyZoomIn = nullptr;
    QKeySequenceEdit *m_keyZoomOut = nullptr;
    QKeySequenceEdit *m_keyFitWindow = nullptr;
    QKeySequenceEdit *m_keyActualSize = nullptr;
    QKeySequenceEdit *m_keyRotateLeft = nullptr;
    QKeySequenceEdit *m_keyRotateRight = nullptr;
    QKeySequenceEdit *m_keyFullscreen = nullptr;
    QKeySequenceEdit *m_keyDelete = nullptr;
};

#endif // SETTINGSDIALOG_H
