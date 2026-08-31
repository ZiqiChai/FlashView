#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QHash>
#include <QLabel>
#include <QVector>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(480, 400);
    setupUI();
}

void SettingsDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    // General tab
    auto *generalTab = new QWidget;
    auto *generalLayout = new QFormLayout(generalTab);
    generalLayout->setSpacing(12);
    generalLayout->setContentsMargins(16, 16, 16, 16);

    m_langCombo = new QComboBox;
    m_langCombo->addItem("简体中文");
    m_langCombo->addItem("English");
    generalLayout->addRow(tr("Language / 语言:"), m_langCombo);

    m_themeCombo = new QComboBox;
    m_themeCombo->addItem(tr("Dark"));
    m_themeCombo->addItem(tr("Light"));
    generalLayout->addRow(tr("Theme:"), m_themeCombo);

    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setFixedSize(80, 32);
    m_bgColorBtn->setStyleSheet("border-radius: 6px;");
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_bgColor, this, tr("Background Color"));
        if (c.isValid()) {
            m_bgColor = c;
            m_bgColorBtn->setStyleSheet(
                QString("background-color: %1; border-radius: 6px; border: 2px solid #666;")
                .arg(c.name()));
        }
    });
    generalLayout->addRow(tr("Background:"), m_bgColorBtn);

    m_thumbCheck = new QCheckBox(tr("Show thumbnail bar"));
    generalLayout->addRow("", m_thumbCheck);

    m_wheelZoomCheck = new QCheckBox(tr("Scroll wheel to zoom (instead of navigate)"));
    generalLayout->addRow("", m_wheelZoomCheck);

    m_interpolationCombo = new QComboBox;
    m_interpolationCombo->addItem(tr("Auto (recommended)"));
    m_interpolationCombo->addItem(tr("Smooth (photos)"));
    m_interpolationCombo->addItem(tr("Nearest neighbor (pixel art)"));
    m_interpolationCombo->setToolTip(
        tr("High-quality resampling runs in the background after zooming stops."));
    generalLayout->addRow(tr("Scaling quality:"), m_interpolationCombo);

    m_tabs->addTab(generalTab, tr("General"));

    // Keys tab
    auto *keysTab = new QWidget;
    auto *keysLayout = new QFormLayout(keysTab);
    keysLayout->setSpacing(10);
    keysLayout->setContentsMargins(16, 16, 16, 16);

    m_keyNext = new QKeySequenceEdit;
    keysLayout->addRow(tr("Next file:"), m_keyNext);

    m_keyPrev = new QKeySequenceEdit;
    keysLayout->addRow(tr("Previous file:"), m_keyPrev);

    m_keyZoomIn = new QKeySequenceEdit;
    keysLayout->addRow(tr("Zoom in:"), m_keyZoomIn);

    m_keyZoomOut = new QKeySequenceEdit;
    keysLayout->addRow(tr("Zoom out:"), m_keyZoomOut);

    m_keyFitWindow = new QKeySequenceEdit;
    keysLayout->addRow(tr("Fit window:"), m_keyFitWindow);

    m_keyActualSize = new QKeySequenceEdit;
    keysLayout->addRow(tr("Actual size:"), m_keyActualSize);

    m_keyRotateLeft = new QKeySequenceEdit;
    keysLayout->addRow(tr("Rotate left:"), m_keyRotateLeft);

    m_keyRotateRight = new QKeySequenceEdit;
    keysLayout->addRow(tr("Rotate right:"), m_keyRotateRight);

    m_keyFullscreen = new QKeySequenceEdit;
    keysLayout->addRow(tr("Fullscreen:"), m_keyFullscreen);

    m_tabs->addTab(keysTab, tr("Shortcuts"));

    mainLayout->addWidget(m_tabs);

    // Button box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *restoreButton = buttonBox->addButton(
        tr("Restore Defaults"), QDialogButtonBox::ResetRole);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(restoreButton, &QPushButton::clicked, this, [this] {
        m_langCombo->setCurrentIndex(0);
        m_themeCombo->setCurrentIndex(0);
        setBackgroundColor(QColor(24, 24, 32));
        m_thumbCheck->setChecked(true);
        m_wheelZoomCheck->setChecked(false);
        m_interpolationCombo->setCurrentIndex(0);
        setKeyNext(QKeySequence(Qt::Key_Right));
        setKeyPrev(QKeySequence(Qt::Key_Left));
        setKeyZoomIn(QKeySequence::ZoomIn);
        setKeyZoomOut(QKeySequence::ZoomOut);
        setKeyFitWindow(QKeySequence(Qt::Key_F));
        setKeyActualSize(QKeySequence(Qt::Key_1));
        setKeyRotateLeft(QKeySequence(QStringLiteral("Ctrl+L")));
        setKeyRotateRight(QKeySequence(QStringLiteral("Ctrl+R")));
        setKeyFullscreen(QKeySequence(Qt::Key_F11));
    });
    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::accept()
{
    const QVector<QPair<QString, QKeySequence>> shortcuts = {
        {tr("Next file"), keyNext()},
        {tr("Previous file"), keyPrev()},
        {tr("Zoom in"), keyZoomIn()},
        {tr("Zoom out"), keyZoomOut()},
        {tr("Fit window"), keyFitWindow()},
        {tr("Actual size"), keyActualSize()},
        {tr("Rotate left"), keyRotateLeft()},
        {tr("Rotate right"), keyRotateRight()},
        {tr("Fullscreen"), keyFullscreen()}
    };

    QHash<QString, QString> owners;
    for (const auto &entry : shortcuts) {
        if (entry.second.isEmpty())
            continue;
        const QString portable =
            entry.second.toString(QKeySequence::PortableText);
        const auto existing = owners.constFind(portable);
        if (existing != owners.cend()) {
            QMessageBox::warning(
                this,
                tr("Shortcut conflict"),
                tr("%1 and %2 use the same shortcut (%3).")
                    .arg(*existing, entry.first,
                         entry.second.toString(QKeySequence::NativeText)));
            return;
        }
        owners.insert(portable, entry.first);
    }

    QDialog::accept();
}

// Getters
int SettingsDialog::languageIndex() const { return m_langCombo->currentIndex(); }
int SettingsDialog::themeIndex() const { return m_themeCombo->currentIndex(); }
QColor SettingsDialog::backgroundColor() const { return m_bgColor; }
bool SettingsDialog::thumbnailsVisible() const { return m_thumbCheck->isChecked(); }
bool SettingsDialog::wheelZoomEnabled() const { return m_wheelZoomCheck->isChecked(); }
int SettingsDialog::interpolationMode() const { return m_interpolationCombo->currentIndex(); }

void SettingsDialog::setLanguageIndex(int idx) { m_langCombo->setCurrentIndex(idx); }
void SettingsDialog::setThemeIndex(int idx) { m_themeCombo->setCurrentIndex(idx); }
void SettingsDialog::setBackgroundColor(const QColor &c) {
    m_bgColor = c;
    m_bgColorBtn->setStyleSheet(
        QString("background-color: %1; border-radius: 6px; border: 2px solid #666;")
        .arg(c.name()));
}
void SettingsDialog::setThumbnailsVisible(bool v) { m_thumbCheck->setChecked(v); }
void SettingsDialog::setWheelZoomEnabled(bool v) { m_wheelZoomCheck->setChecked(v); }
void SettingsDialog::setInterpolationMode(int mode) {
    m_interpolationCombo->setCurrentIndex(qBound(0, mode, 2));
}

QKeySequence SettingsDialog::keyNext() const { return m_keyNext->keySequence(); }
QKeySequence SettingsDialog::keyPrev() const { return m_keyPrev->keySequence(); }
QKeySequence SettingsDialog::keyZoomIn() const { return m_keyZoomIn->keySequence(); }
QKeySequence SettingsDialog::keyZoomOut() const { return m_keyZoomOut->keySequence(); }
QKeySequence SettingsDialog::keyFitWindow() const { return m_keyFitWindow->keySequence(); }
QKeySequence SettingsDialog::keyActualSize() const { return m_keyActualSize->keySequence(); }
QKeySequence SettingsDialog::keyRotateLeft() const { return m_keyRotateLeft->keySequence(); }
QKeySequence SettingsDialog::keyRotateRight() const { return m_keyRotateRight->keySequence(); }
QKeySequence SettingsDialog::keyFullscreen() const { return m_keyFullscreen->keySequence(); }

void SettingsDialog::setKeyNext(const QKeySequence &ks) { m_keyNext->setKeySequence(ks); }
void SettingsDialog::setKeyPrev(const QKeySequence &ks) { m_keyPrev->setKeySequence(ks); }
void SettingsDialog::setKeyZoomIn(const QKeySequence &ks) { m_keyZoomIn->setKeySequence(ks); }
void SettingsDialog::setKeyZoomOut(const QKeySequence &ks) { m_keyZoomOut->setKeySequence(ks); }
void SettingsDialog::setKeyFitWindow(const QKeySequence &ks) { m_keyFitWindow->setKeySequence(ks); }
void SettingsDialog::setKeyActualSize(const QKeySequence &ks) { m_keyActualSize->setKeySequence(ks); }
void SettingsDialog::setKeyRotateLeft(const QKeySequence &ks) { m_keyRotateLeft->setKeySequence(ks); }
void SettingsDialog::setKeyRotateRight(const QKeySequence &ks) { m_keyRotateRight->setKeySequence(ks); }
void SettingsDialog::setKeyFullscreen(const QKeySequence &ks) { m_keyFullscreen->setKeySequence(ks); }
