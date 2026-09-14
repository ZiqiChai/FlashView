#include "MainWindow.h"
#include "ImageViewer.h"
#include "MediaUtils.h"
#include "VideoPlayer.h"
#include "ThumbnailBar.h"
#include "SettingsDialog.h"
#include "ThemeManager.h"

#include <QActionGroup>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLocale>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QImageReader>
#include <QMouseEvent>
#include <QToolButton>
#include <QPixmapCache>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFont>
#include <QIcon>
#include <QtConcurrent>

#include <cmath>

namespace {

class CompactToolBar final : public QToolBar
{
public:
    using QToolBar::QToolBar;

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QToolBar::event(event);
        if (event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::Resize
            || event->type() == QEvent::Show) {
            centerItemsVertically();
        }
        return handled;
    }

private:
    void centerItemsVertically()
    {
        const auto children =
            findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget *child : children) {
            const bool isButton = qobject_cast<QToolButton *>(child);
            const bool isSeparator =
                QByteArray(child->metaObject()->className()).contains("Separator");
            if ((!isButton && !isSeparator) || !child->isVisible())
                continue;

            QRect geometry = child->geometry();
            geometry.moveTop((height() - geometry.height()) / 2);
            child->setGeometry(geometry);
        }
    }
};

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // A larger pixmap cache lets a few full-resolution images (current +
    // neighbours) stay resident so paging back and forth is instant.
    QPixmapCache::setCacheLimit(131072); // 128 MB

    setupActions();
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    loadSettings();
    applyTheme();

    // Toolbar symbols are themed icons; recolour them on theme change.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::refreshToolbarIcons);
    refreshToolbarIcons();

    // Debounced neighbour preloading: only decodes when the user pauses,
    // so fast paging never blocks on a background decode.
    m_preloadTimer = new QTimer(this);
    m_preloadTimer->setSingleShot(true);
    connect(m_preloadTimer, &QTimer::timeout, this, &MainWindow::preloadNeighbors);

    // Hide the cursor after a short idle while in fullscreen, for a clean,
    // distraction-free viewing experience; any movement reveals it again.
    m_idleCursorTimer = new QTimer(this);
    m_idleCursorTimer->setSingleShot(true);
    m_idleCursorTimer->setInterval(1500);
    connect(m_idleCursorTimer, &QTimer::timeout, this, [this] {
        if (isFullScreen() && !m_cursorHidden) {
            QApplication::setOverrideCursor(Qt::BlankCursor);
            m_cursorHidden = true;
        }
    });

    // Keep an open folder in sync with files added, removed or replaced by
    // another application. A short debounce coalesces editors' rename/write
    // sequences into one refresh.
    m_fileSystemWatcher = new QFileSystemWatcher(this);
    m_directoryRefreshTimer = new QTimer(this);
    m_directoryRefreshTimer->setSingleShot(true);
    m_directoryRefreshTimer->setInterval(180);
    connect(m_directoryRefreshTimer, &QTimer::timeout,
            this, &MainWindow::refreshDirectoryContents);
    connect(m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged,
            this, [this] { m_directoryRefreshTimer->start(); });
    connect(m_fileSystemWatcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
        QTimer::singleShot(180, this, [this, path] {
            if (m_currentIndex < 0 || m_currentIndex >= m_fileList.size())
                return;
            const QString currentPath =
                m_currentDir.absoluteFilePath(m_fileList[m_currentIndex]);
            if (QFileInfo::exists(currentPath) && currentPath == path) {
                m_currentLoadedPath.clear();
                loadFile(m_currentIndex);
            } else {
                refreshDirectoryContents();
            }
        });
    });

    setAcceptDrops(true);
    setMinimumSize(800, 600);
    resize(1200, 800);
    setWindowTitle("FlashView");
    updateActions();
    QTimer::singleShot(0, this, &MainWindow::showRuntimeSupportWarning);
}

void MainWindow::setupActions()
{
    m_openFileAction = new QAction(tr("Open &File..."), this);
    m_openFileAction->setShortcut(QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    m_openDirAction = new QAction(tr("Open &Directory..."), this);
    m_openDirAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(m_openDirAction, &QAction::triggered, this, &MainWindow::onOpenDirectory);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QMainWindow::close);

    m_nextAction = new QAction(tr("&Next"), this);
    m_nextAction->setShortcut(Qt::Key_Right);
    connect(m_nextAction, &QAction::triggered, this, &MainWindow::onNextFile);

    m_prevAction = new QAction(tr("&Previous"), this);
    m_prevAction->setShortcut(Qt::Key_Left);
    connect(m_prevAction, &QAction::triggered, this, &MainWindow::onPrevFile);

    m_zoomInAction = new QAction(tr("Zoom &In"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, [this]() {
        if (m_stackedWidget->currentIndex() == 0) m_imageViewer->zoomIn();
    });

    m_zoomOutAction = new QAction(tr("Zoom &Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, [this]() {
        if (m_stackedWidget->currentIndex() == 0) m_imageViewer->zoomOut();
    });

    m_fitAction = new QAction(tr("&Fit Window"), this);
    m_fitAction->setShortcut(Qt::Key_F);
    connect(m_fitAction, &QAction::triggered, this, [this]() {
        if (m_stackedWidget->currentIndex() == 0) m_imageViewer->fitToWindow();
    });

    m_actualSizeAction = new QAction(tr("&Actual Size"), this);
    m_actualSizeAction->setShortcut(Qt::Key_1);
    connect(m_actualSizeAction, &QAction::triggered, this, [this]() {
        if (m_stackedWidget->currentIndex() == 0) m_imageViewer->actualSize();
    });

    m_rotateLeftAction = new QAction(tr("Rotate &Left"), this);
    m_rotateLeftAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(m_rotateLeftAction, &QAction::triggered, this, [this]() {
        if (m_stackedWidget->currentIndex() == 0) m_imageViewer->rotateLeft();
    });

    m_rotateRightAction = new QAction(tr("Rotate &Right"), this);
    m_rotateRightAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(m_rotateRightAction, &QAction::triggered, this, [this]() {
        if (m_stackedWidget->currentIndex() == 0) m_imageViewer->rotateRight();
    });

    m_fullscreenAction = new QAction(tr("F&ullscreen"), this);
    m_fullscreenAction->setCheckable(true);
    m_fullscreenAction->setShortcut(Qt::Key_F11);
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::onToggleFullscreen);

    m_thumbAction = new QAction(tr("Thumbnail &Bar"), this);
    m_thumbAction->setShortcut(Qt::Key_T);
    connect(m_thumbAction, &QAction::triggered, this, &MainWindow::onToggleThumbnails);

    m_deleteAction = new QAction(tr("&Delete"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::onDeleteFile);

    // Sorting is pinned by the user: a file manager's own order is private to
    // it, so matching it automatically is not possible.
    auto *sortGroup = new QActionGroup(this);
    sortGroup->setExclusive(true);
    const QStringList sortNames = {tr("&Name"), tr("&Modified Time"),
                                   tr("&Created Time"), tr("&File Size"),
                                   tr("File &Type")};
    m_sortKeyActions.clear();
    for (int key = 0; key < sortNames.size(); ++key) {
        auto *action = new QAction(sortNames.at(key), this);
        action->setCheckable(true);
        action->setActionGroup(sortGroup);
        connect(action, &QAction::triggered, this, [this, key] {
            applySortOrder(key, m_sortDescending);
        });
        m_sortKeyActions.append(action);
    }

    auto *orderGroup = new QActionGroup(this);
    orderGroup->setExclusive(true);
    m_sortAscendingAction = new QAction(tr("&Ascending"), this);
    m_sortAscendingAction->setCheckable(true);
    m_sortAscendingAction->setActionGroup(orderGroup);
    connect(m_sortAscendingAction, &QAction::triggered, this, [this] {
        applySortOrder(m_sortKey, false);
    });
    m_sortDescendingAction = new QAction(tr("D&escending"), this);
    m_sortDescendingAction->setCheckable(true);
    m_sortDescendingAction->setActionGroup(orderGroup);
    connect(m_sortDescendingAction, &QAction::triggered, this, [this] {
        applySortOrder(m_sortKey, true);
    });

    m_settingsAction = new QAction(tr("&Settings..."), this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

    m_darkThemeAction = new QAction(tr("&Dark Theme"), this);
    m_darkThemeAction->setCheckable(true);
    connect(m_darkThemeAction, &QAction::triggered, this, [this]() {
        ThemeManager::instance().setTheme(ThemeManager::Dark);
        m_darkThemeAction->setChecked(true);
        m_lightThemeAction->setChecked(false);
        applyTheme();
    });

    m_lightThemeAction = new QAction(tr("&Light Theme"), this);
    m_lightThemeAction->setCheckable(true);
    connect(m_lightThemeAction, &QAction::triggered, this, [this]() {
        ThemeManager::instance().setTheme(ThemeManager::Light);
        m_darkThemeAction->setChecked(false);
        m_lightThemeAction->setChecked(true);
        applyTheme();
    });

    m_langZhAction = new QAction(tr("中文"), this);
    m_langZhAction->setCheckable(true);
    connect(m_langZhAction, &QAction::triggered, this, [this]() {
        switchLanguage(0);
    });

    m_langEnAction = new QAction(tr("English"), this);
    m_langEnAction->setCheckable(true);
    connect(m_langEnAction, &QAction::triggered, this, [this]() {
        switchLanguage(1);
    });
}

void MainWindow::setupUI()
{
    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Stacked widget for image/video
    m_stackedWidget = new QStackedWidget;
    m_imageViewer = new ImageViewer;
    m_videoPlayer = new VideoPlayer;
    m_stackedWidget->addWidget(m_imageViewer);  // index 0
    m_stackedWidget->addWidget(m_videoPlayer);   // index 1

    layout->addWidget(m_stackedWidget, 1);

    // Thumbnail bar
    m_thumbnailBar = new ThumbnailBar;
    layout->addWidget(m_thumbnailBar);
    connect(m_thumbnailBar, &ThumbnailBar::fileSelected, this, &MainWindow::onThumbnailSelected);

    setCentralWidget(centralWidget);

    // Connect image viewer scale changes
    connect(m_imageViewer, &ImageViewer::scaleChanged, this, [this](double scale) {
        m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(scale * 100)));
    });

    // Mouse side buttons on the image page navigate between files.
    connect(m_imageViewer, &ImageViewer::navigateNext, this, &MainWindow::onNextFile);
    connect(m_imageViewer, &ImageViewer::navigatePrev, this, &MainWindow::onPrevFile);
    connect(m_videoPlayer, &VideoPlayer::playbackError, this, [this](const QString &message) {
        if (m_statusBar)
            m_statusBar->showMessage(tr("Video playback failed: %1").arg(message), 6000);
    });
}

void MainWindow::setupMenuBar()
{
    QMenuBar *mb = menuBar();
    mb->clear(); // Remove old menus to prevent duplicates on language switch

    // File menu
    QMenu *fileMenu = mb->addMenu(tr("&File"));
    fileMenu->addAction(m_openFileAction);
    fileMenu->addAction(m_openDirAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    // View menu
    QMenu *viewMenu = mb->addMenu(tr("&View"));
    viewMenu->addAction(m_prevAction);
    viewMenu->addAction(m_nextAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_fitAction);
    viewMenu->addAction(m_actualSizeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_rotateLeftAction);
    viewMenu->addAction(m_rotateRightAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fullscreenAction);
    viewMenu->addAction(m_thumbAction);
    viewMenu->addSeparator();
    QMenu *sortSubMenu = viewMenu->addMenu(tr("S&ort By"));
    for (QAction *action : std::as_const(m_sortKeyActions))
        sortSubMenu->addAction(action);
    sortSubMenu->addSeparator();
    sortSubMenu->addAction(m_sortAscendingAction);
    sortSubMenu->addAction(m_sortDescendingAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_deleteAction);

    // Settings menu (contains theme, language, preferences)
    QMenu *settingsMenu = mb->addMenu(tr("&Settings"));
    // Theme submenu
    QMenu *themeSubMenu = settingsMenu->addMenu(tr("&Theme"));
    themeSubMenu->addAction(m_darkThemeAction);
    themeSubMenu->addAction(m_lightThemeAction);
    // Language submenu
    QMenu *langSubMenu = settingsMenu->addMenu(tr("&Language"));
    langSubMenu->addAction(m_langZhAction);
    langSubMenu->addAction(m_langEnAction);
    settingsMenu->addSeparator();
    settingsMenu->addAction(m_settingsAction);
}

void MainWindow::setupToolBar()
{
    auto *tb = new CompactToolBar(tr("Main"), this);
    addToolBar(tb);
    m_mainToolBar = tb;
    tb->setObjectName("MainToolBar");
    tb->setMovable(false);
    tb->setIconSize(QSize(24, 24));
    tb->setFixedHeight(40);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_toolGlyphs.clear();
    auto registerIcon = [this](QAction *action, const QString &name) {
        m_toolGlyphs.append({action, name});
    };

    // Every action uses the same 24-unit stroke icon system. Actions that only
    // appear in menus are registered too, keeping dropdowns visually coherent.
    registerIcon(m_openFileAction, QStringLiteral("file"));
    registerIcon(m_openDirAction, QStringLiteral("folder"));
    registerIcon(m_exitAction, QStringLiteral("exit"));
    registerIcon(m_prevAction, QStringLiteral("previous"));
    registerIcon(m_nextAction, QStringLiteral("next"));
    registerIcon(m_zoomInAction, QStringLiteral("zoom-in"));
    registerIcon(m_zoomOutAction, QStringLiteral("zoom-out"));
    registerIcon(m_fitAction, QStringLiteral("fit"));
    registerIcon(m_actualSizeAction, QStringLiteral("actual"));
    registerIcon(m_rotateLeftAction, QStringLiteral("rotate-left"));
    registerIcon(m_rotateRightAction, QStringLiteral("rotate-right"));
    registerIcon(m_fullscreenAction, QStringLiteral("fullscreen"));
    registerIcon(m_thumbAction, QStringLiteral("thumbnails"));
    registerIcon(m_deleteAction, QStringLiteral("delete"));
    registerIcon(m_settingsAction, QStringLiteral("settings"));

    tb->addAction(m_openFileAction);
    tb->addAction(m_openDirAction);
    tb->addSeparator();
    tb->addAction(m_prevAction);
    tb->addAction(m_nextAction);
    tb->addSeparator();
    tb->addAction(m_zoomInAction);
    tb->addAction(m_zoomOutAction);
    tb->addAction(m_fitAction);
    tb->addSeparator();
    tb->addAction(m_rotateLeftAction);
    tb->addAction(m_rotateRightAction);
    tb->addSeparator();
    tb->addAction(m_fullscreenAction);
    tb->addAction(m_thumbAction);
    tb->addSeparator();
    tb->addAction(m_deleteAction);

    refreshActionTooltips();
}

QIcon MainWindow::glyphIcon(const QString &name, const QColor &color) const
{
    // All icons share a 24-unit grid, 1.8-unit rounded stroke and a common
    // optical box. This keeps their perceived size stable in toolbars and menus.
    const int logical = 24;
    const qreal dpr = devicePixelRatioF();
    QPixmap pm(qRound(logical * dpr), qRound(logical * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QPen pen(color, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (name == QStringLiteral("file")) {
        QPainterPath page;
        page.moveTo(6, 2.5);
        page.lineTo(14.5, 2.5);
        page.lineTo(19.5, 7.5);
        page.lineTo(19.5, 21.5);
        page.lineTo(6, 21.5);
        page.closeSubpath();
        p.drawPath(page);
        p.drawLine(QPointF(14.5, 2.5), QPointF(14.5, 7.5));
        p.drawLine(QPointF(14.5, 7.5), QPointF(19.5, 7.5));
        p.drawLine(QPointF(9, 12), QPointF(16.5, 12));
        p.drawLine(QPointF(9, 16), QPointF(16.5, 16));
    } else if (name == QStringLiteral("folder")) {
        QPainterPath folder;
        folder.moveTo(2.5, 7);
        folder.lineTo(9.5, 7);
        folder.lineTo(12, 9.5);
        folder.lineTo(21.5, 9.5);
        folder.lineTo(20, 20);
        folder.lineTo(2.5, 20);
        folder.closeSubpath();
        p.drawPath(folder);
        p.drawLine(QPointF(2.8, 9.5), QPointF(20.8, 9.5));
    } else if (name == QStringLiteral("previous")
               || name == QStringLiteral("next")) {
        const bool previous = name == QStringLiteral("previous");
        QPainterPath chevron;
        chevron.moveTo(previous ? 15.5 : 8.5, 5);
        chevron.lineTo(previous ? 8.5 : 15.5, 12);
        chevron.lineTo(previous ? 15.5 : 8.5, 19);
        p.drawPath(chevron);
    } else if (name == QStringLiteral("zoom-in")
               || name == QStringLiteral("zoom-out")) {
        p.drawEllipse(QRectF(3, 3, 13.5, 13.5));
        p.drawLine(QPointF(15.2, 15.2), QPointF(21, 21));
        p.drawLine(QPointF(7, 9.75), QPointF(12.5, 9.75));
        if (name == QStringLiteral("zoom-in"))
            p.drawLine(QPointF(9.75, 7), QPointF(9.75, 12.5));
    } else if (name == QStringLiteral("fit")) {
        // Nested frames read as media contained by the available window,
        // while staying clearly distinct from fullscreen's open corners.
        p.drawRoundedRect(QRectF(2.5, 4.5, 19, 15), 1.5, 1.5);
        p.drawRoundedRect(QRectF(6.5, 7.5, 11, 9), 1, 1);
    } else if (name == QStringLiteral("actual")) {
        p.drawRoundedRect(QRectF(3, 3, 18, 18), 2, 2);
        QFont font = p.font();
        font.setPixelSize(8);
        font.setWeight(QFont::DemiBold);
        p.setFont(font);
        p.drawText(QRectF(4, 4, 16, 16), Qt::AlignCenter, QStringLiteral("1:1"));
    } else if (name == QStringLiteral("rotate-left")
               || name == QStringLiteral("rotate-right")) {
        const bool left = name == QStringLiteral("rotate-left");
        QPainterPath arc;
        arc.moveTo(left ? 5 : 19, 8);
        arc.cubicTo(left ? 8 : 16, 3, left ? 16 : 8, 3, left ? 19 : 5, 9);
        arc.cubicTo(left ? 22 : 2, 15, left ? 18 : 6, 20, 12, 20);
        p.drawPath(arc);
        QPainterPath arrow;
        arrow.moveTo(left ? 5 : 19, 8);
        arrow.lineTo(left ? 5.5 : 18.5, 3.5);
        arrow.moveTo(left ? 5 : 19, 8);
        arrow.lineTo(left ? 9.5 : 14.5, 7.5);
        p.drawPath(arrow);
    } else if (name == QStringLiteral("fullscreen")) {
        p.drawLine(QPointF(4, 9), QPointF(4, 4));
        p.drawLine(QPointF(4, 4), QPointF(9, 4));
        p.drawLine(QPointF(15, 4), QPointF(20, 4));
        p.drawLine(QPointF(20, 4), QPointF(20, 9));
        p.drawLine(QPointF(20, 15), QPointF(20, 20));
        p.drawLine(QPointF(20, 20), QPointF(15, 20));
        p.drawLine(QPointF(9, 20), QPointF(4, 20));
        p.drawLine(QPointF(4, 20), QPointF(4, 15));
    } else if (name == QStringLiteral("thumbnails")) {
        p.drawRoundedRect(QRectF(2.5, 6, 19, 12), 1.5, 1.5);
        p.drawLine(QPointF(8.8, 6), QPointF(8.8, 18));
        p.drawLine(QPointF(15.2, 6), QPointF(15.2, 18));
    } else if (name == QStringLiteral("delete")) {
        p.drawLine(QPointF(3.5, 6.5), QPointF(20.5, 6.5));
        p.drawLine(QPointF(9, 6.5), QPointF(9, 3.5));
        p.drawLine(QPointF(9, 3.5), QPointF(15, 3.5));
        p.drawLine(QPointF(15, 3.5), QPointF(15, 6.5));
        QPainterPath bin;
        bin.moveTo(5.5, 6.5);
        bin.lineTo(6.8, 20.5);
        bin.lineTo(17.2, 20.5);
        bin.lineTo(18.5, 6.5);
        p.drawPath(bin);
        p.drawLine(QPointF(10, 10), QPointF(10.4, 17));
        p.drawLine(QPointF(14, 10), QPointF(13.6, 17));
    } else if (name == QStringLiteral("exit")) {
        p.drawLine(QPointF(10, 3), QPointF(4, 3));
        p.drawLine(QPointF(4, 3), QPointF(4, 21));
        p.drawLine(QPointF(4, 21), QPointF(10, 21));
        p.drawLine(QPointF(8, 12), QPointF(21, 12));
        p.drawLine(QPointF(17, 8), QPointF(21, 12));
        p.drawLine(QPointF(21, 12), QPointF(17, 16));
    } else if (name == QStringLiteral("settings")) {
        p.drawEllipse(QRectF(8.5, 8.5, 7, 7));
        p.drawEllipse(QRectF(4.5, 4.5, 15, 15));
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < 8; ++i) {
            const double angle = i * pi / 4.0;
            p.drawLine(QPointF(12 + std::cos(angle) * 7.5,
                               12 + std::sin(angle) * 7.5),
                       QPointF(12 + std::cos(angle) * 10,
                               12 + std::sin(angle) * 10));
        }
    }
    p.end();
    return QIcon(pm);
}

void MainWindow::refreshToolbarIcons()
{
    const QColor c = ThemeManager::instance().currentTheme() == ThemeManager::Dark
        ? QColor(220, 220, 235) : QColor(60, 60, 70);
    for (const auto &entry : m_toolGlyphs)
        entry.first->setIcon(glyphIcon(entry.second, c));
}

void MainWindow::setupStatusBar()
{
    m_statusBar = statusBar();
    m_statusBar->setObjectName("MainStatusBar");

    m_fileInfoLabel = new QLabel;
    m_fileInfoLabel->setObjectName("statusFileInfo");
    m_imageSizeLabel = new QLabel;
    m_imageSizeLabel->setObjectName("statusSegment");
    m_imageSizeLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel = new QLabel;
    m_zoomLabel->setObjectName("statusSegment");
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_indexLabel = new QLabel;
    m_indexLabel->setObjectName("statusSegment");
    m_indexLabel->setAlignment(Qt::AlignCenter);

    m_statusBar->addWidget(m_fileInfoLabel, 1);
    m_statusBar->addPermanentWidget(m_imageSizeLabel);
    m_statusBar->addPermanentWidget(m_zoomLabel);
    m_statusBar->addPermanentWidget(m_indexLabel);

    m_imageSizeLabel->setFixedWidth(130);
    m_zoomLabel->setFixedWidth(70);
    m_indexLabel->setFixedWidth(90);

    // The zoom segment doubles as a one-click "reset to 100%" control.
    m_zoomLabel->setCursor(Qt::PointingHandCursor);
    m_zoomLabel->setToolTip(tr("Click to reset zoom to 100%"));
    m_zoomLabel->installEventFilter(this);
}

void MainWindow::openFile(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        m_statusBar->showMessage(tr("File not found: %1").arg(filePath), 5000);
        return;
    }
    if (!MediaUtils::isSupportedFile(fi.absoluteFilePath())) {
        m_statusBar->showMessage(tr("Unsupported media format: %1").arg(fi.suffix()), 5000);
        return;
    }

    m_currentDir = fi.absoluteDir();
    updateFileList();
    watchCurrentDirectory();

    int idx = m_fileList.indexOf(fi.fileName());
    if (idx >= 0) {
        m_currentLoadedPath.clear();
        loadFile(idx);
    } else {
        m_statusBar->showMessage(tr("The selected file is not readable"), 5000);
    }
}

void MainWindow::openDirectory(const QString &dirPath)
{
    QDir directory(dirPath);
    if (!directory.exists() || !QFileInfo(dirPath).isReadable()) {
        m_statusBar->showMessage(tr("Folder not found or not readable: %1").arg(dirPath), 5000);
        return;
    }

    m_currentDir = directory;
    m_currentLoadedPath.clear();
    m_currentIndex = -1;
    updateFileList();
    watchCurrentDirectory();
    if (!m_fileList.isEmpty()) {
        loadFile(0);
    } else {
        clearCurrentView(tr("No supported media files in this folder"),
                         tr("Drop a file here or choose another folder"));
    }
}

void MainWindow::onOpenFile()
{
    QString extFilters;
    for (const QString &ext : supportedExtensions())
        extFilters += "*." + ext + " ";
    const QString startPath = m_currentDir.path() != QStringLiteral(".") && m_currentDir.exists()
        ? m_currentDir.absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), startPath,
        tr("Media Files (%1);;All Files (*)").arg(extFilters.trimmed()));
    if (!filePath.isEmpty())
        openFile(filePath);
}

void MainWindow::onOpenDirectory()
{
    const QString startPath = m_currentDir.path() != QStringLiteral(".") && m_currentDir.exists()
        ? m_currentDir.absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString dirPath = QFileDialog::getExistingDirectory(
        this, tr("Open Directory"), startPath);
    if (!dirPath.isEmpty())
        openDirectory(dirPath);
}

void MainWindow::onNextFile()
{
    if (m_currentIndex < m_fileList.size() - 1) {
        m_currentIndex++;
        loadFile(m_currentIndex);
    } else if (!m_fileList.isEmpty() && m_statusBar) {
        m_statusBar->showMessage(tr("Last file"), 1200);
    }
}

void MainWindow::onPrevFile()
{
    if (m_currentIndex > 0) {
        m_currentIndex--;
        loadFile(m_currentIndex);
    } else if (!m_fileList.isEmpty() && m_statusBar) {
        m_statusBar->showMessage(tr("First file"), 1200);
    }
}

void MainWindow::onThumbnailSelected(int index)
{
    if (index < 0 || index >= m_fileList.size())
        return;
    loadFile(index);
}

void MainWindow::onDeleteFile()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_fileList.size())
        return;

    const int deletedIndex = m_currentIndex;
    const QString fileName = m_fileList[deletedIndex];
    const QString filePath = m_currentDir.absoluteFilePath(fileName);

    if (m_confirmDelete
        && QMessageBox::question(
               this, tr("Delete File"),
               tr("Move \"%1\" to the trash?").arg(fileName),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           != QMessageBox::Yes) {
        return;
    }

    // Release the file first: a playing video keeps its handle open, which
    // makes the removal fail or leaves the player pointing at a ghost.
    m_videoPlayer->stop();

    QFile file(filePath);
    if (!file.moveToTrash()) {
        // Volumes without a trash folder cannot hold the file for recovery,
        // so ask before removing it for good, whatever the confirm setting is.
        if (QMessageBox::warning(
                this, tr("Delete File"),
                tr("\"%1\" cannot be moved to the trash. Delete it permanently?")
                    .arg(fileName),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        if (!file.remove()) {
            m_statusBar->showMessage(
                tr("Could not delete %1: %2").arg(fileName, file.errorString()), 5000);
            return;
        }
    }

    // Update immediately rather than waiting for the folder watcher, so the
    // next file appears the moment the key is released.
    m_fileList.removeAt(deletedIndex);
    m_currentLoadedPath.clear();
    m_thumbnailBar->setDirectory(m_currentDir.absolutePath(), m_fileList);
    if (m_fileList.isEmpty()) {
        clearCurrentView(tr("No supported media files in this folder"),
                         tr("Drop a file here or choose another folder"));
    } else {
        loadFile(qMin(deletedIndex, m_fileList.size() - 1));
    }
    m_statusBar->showMessage(tr("Deleted %1").arg(fileName), 3000);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.setLanguageIndex(m_language);
    dlg.setThemeIndex(ThemeManager::instance().currentTheme() == ThemeManager::Dark ? 0 : 1);
    dlg.setBackgroundColor(ThemeManager::instance().backgroundColor());
    dlg.setThumbnailsVisible(m_thumbnailsVisible);
    dlg.setWheelZoomEnabled(m_wheelZoomMode);
    dlg.setInterpolationMode(m_interpolationMode);
    dlg.setSortKey(m_sortKey);
    dlg.setSortDescending(m_sortDescending);
    dlg.setConfirmDelete(m_confirmDelete);

    dlg.setKeyNext(m_nextAction->shortcut());
    dlg.setKeyPrev(m_prevAction->shortcut());
    dlg.setKeyZoomIn(m_zoomInAction->shortcut());
    dlg.setKeyZoomOut(m_zoomOutAction->shortcut());
    dlg.setKeyFitWindow(m_fitAction->shortcut());
    dlg.setKeyActualSize(m_actualSizeAction->shortcut());
    dlg.setKeyRotateLeft(m_rotateLeftAction->shortcut());
    dlg.setKeyRotateRight(m_rotateRightAction->shortcut());
    dlg.setKeyFullscreen(m_fullscreenAction->shortcut());
    dlg.setKeyDelete(m_deleteAction->shortcut());

    if (dlg.exec() == QDialog::Accepted) {
        // Apply language
        if (dlg.languageIndex() != m_language)
            switchLanguage(dlg.languageIndex());

        // Apply theme
        ThemeManager::instance().setTheme(
            dlg.themeIndex() == 0 ? ThemeManager::Dark : ThemeManager::Light);
        m_darkThemeAction->setChecked(dlg.themeIndex() == 0);
        m_lightThemeAction->setChecked(dlg.themeIndex() == 1);
        applyTheme();

        // Background color
        ThemeManager::instance().setBackgroundColor(dlg.backgroundColor());

        // Thumbnails
        m_thumbnailsVisible = dlg.thumbnailsVisible();
        m_thumbnailBar->setVisible(m_thumbnailsVisible);

        // Wheel zoom mode
        m_wheelZoomMode = dlg.wheelZoomEnabled();
        m_imageViewer->setWheelZoomEnabled(m_wheelZoomMode);

        m_interpolationMode = dlg.interpolationMode();
        m_imageViewer->setInterpolationMode(
            static_cast<ImageViewer::InterpolationMode>(m_interpolationMode));

        // Browsing order and delete confirmation
        m_confirmDelete = dlg.confirmDelete();
        applySortOrder(dlg.sortKey(), dlg.sortDescending(), false);

        // Shortcuts
        m_nextAction->setShortcut(dlg.keyNext());
        m_prevAction->setShortcut(dlg.keyPrev());
        m_zoomInAction->setShortcut(dlg.keyZoomIn());
        m_zoomOutAction->setShortcut(dlg.keyZoomOut());
        m_fitAction->setShortcut(dlg.keyFitWindow());
        m_actualSizeAction->setShortcut(dlg.keyActualSize());
        m_rotateLeftAction->setShortcut(dlg.keyRotateLeft());
        m_rotateRightAction->setShortcut(dlg.keyRotateRight());
        m_fullscreenAction->setShortcut(dlg.keyFullscreen());
        m_deleteAction->setShortcut(dlg.keyDelete());

        refreshActionTooltips();
        saveSettings();
    }
}

void MainWindow::onToggleFullscreen()
{
    if (isFullScreen()) {
        if (m_wasMaximizedBeforeFullscreen)
            showMaximized();
        else
            showNormal();
        menuBar()->show();
        statusBar()->show();
        if (m_mainToolBar) m_mainToolBar->show();
        m_thumbnailBar->setVisible(m_thumbnailsVisible);
        m_fullscreenAction->setChecked(false);
        qApp->removeEventFilter(this);
        if (m_idleCursorTimer) m_idleCursorTimer->stop();
        if (m_cursorHidden) { QApplication::restoreOverrideCursor(); m_cursorHidden = false; }
    } else {
        m_wasMaximizedBeforeFullscreen = isMaximized();
        m_normalGeometry = saveGeometry();
        m_normalWindowState = saveState();
        showFullScreen();
        menuBar()->hide();
        statusBar()->hide();
        if (m_mainToolBar) m_mainToolBar->hide();
        m_thumbnailBar->hide();
        m_fullscreenAction->setChecked(true);
        qApp->installEventFilter(this);
        if (m_idleCursorTimer) m_idleCursorTimer->start();
    }
}

void MainWindow::onToggleThumbnails()
{
    m_thumbnailsVisible = !m_thumbnailsVisible;
    m_thumbnailBar->setVisible(m_thumbnailsVisible);
    saveSettings();
}

void MainWindow::loadFile(int index)
{
    if (index < 0 || index >= m_fileList.size()) return;

    const QString filePath = m_currentDir.absoluteFilePath(m_fileList[index]);
    if (!QFileInfo::exists(filePath)) {
        refreshDirectoryContents();
        return;
    }
    m_currentIndex = index;

    // Skip redundant reloads (e.g. clicking the already-open thumbnail):
    // just keep the thumbnail selection in sync and avoid a flicker.
    if (filePath == m_currentLoadedPath) {
        m_thumbnailBar->setCurrentIndex(index);
        return;
    }
    m_currentLoadedPath = filePath;

    if (isVideoFile(m_fileList[index])) {
        m_stackedWidget->setCurrentIndex(1);
        m_videoPlayer->loadVideo(filePath);
    } else {
        // Stopping here is important: otherwise audio from the previous video
        // continues after its page is no longer visible.
        m_videoPlayer->stop();
        m_stackedWidget->setCurrentIndex(0);
        if (!m_imageViewer->loadImage(filePath)) {
            m_statusBar->showMessage(
                tr("Could not open %1").arg(m_fileList[index]), 5000);
        }
    }

    watchCurrentFile(filePath);
    m_thumbnailBar->setCurrentIndex(index);
    updateStatusBar();
    updateActions();
    setWindowTitle("FlashView - " + m_fileList[index]);

    // Schedule neighbour preloading after a short pause.
    ++m_preloadGeneration;
    if (m_preloadTimer) m_preloadTimer->start(150);
}

void MainWindow::preloadNeighbors()
{
    QStringList paths;
    for (const int index : {m_currentIndex + 1, m_currentIndex - 1}) {
        if (index < 0 || index >= m_fileList.size() || isVideoFile(m_fileList[index]))
            continue;
        const QString path = m_currentDir.absoluteFilePath(m_fileList[index]);
        QPixmap cached;
        if (!QPixmapCache::find(MediaUtils::imageCacheKey(path), &cached))
            paths.append(path);
    }
    if (paths.isEmpty())
        return;

    using PreloadResult = QVector<QPair<QString, QImage>>;
    const quint64 generation = m_preloadGeneration;
    auto future = QtConcurrent::run([paths] {
        PreloadResult results;
        results.reserve(paths.size());
        for (const QString &path : paths) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QImage image = reader.read();
            if (!image.isNull())
                results.append({MediaUtils::imageCacheKey(path), image});
        }
        return results;
    });

    auto *watcher = new QFutureWatcher<PreloadResult>(this);
    connect(watcher, &QFutureWatcher<PreloadResult>::finished, this,
            [this, watcher, generation] {
        if (generation == m_preloadGeneration) {
            const PreloadResult results = watcher->result();
            for (const auto &result : results)
                QPixmapCache::insert(result.first, QPixmap::fromImage(result.second));
        }
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void MainWindow::updateFileList()
{
    m_currentDir.setNameFilters({QStringLiteral("*")});
    m_currentDir.setSorting(QDir::NoSort);
    m_currentDir.setFilter(QDir::Files | QDir::Readable);

    const QStringList entries = m_currentDir.entryList();
    m_fileList.clear();
    m_fileList.reserve(entries.size());
    for (const QString &entry : entries) {
        if (MediaUtils::isSupportedFile(m_currentDir.absoluteFilePath(entry)))
            m_fileList.append(entry);
    }
    MediaUtils::sortFiles(m_currentDir, m_fileList,
                          MediaUtils::sortKeyFromInt(m_sortKey), m_sortDescending);
    m_thumbnailBar->setDirectory(m_currentDir.absolutePath(), m_fileList);
}

void MainWindow::applySortOrder(int sortKey, bool descending, bool persist)
{
    m_sortKey = qBound(0, sortKey, static_cast<int>(MediaUtils::SortKey::Type));
    m_sortDescending = descending;
    updateSortActions();

    if (!m_fileList.isEmpty()) {
        // Reordering must not change which file is on screen, only its place
        // in the sequence.
        const QString currentName =
            m_currentIndex >= 0 && m_currentIndex < m_fileList.size()
            ? m_fileList[m_currentIndex] : QString();
        updateFileList();
        const int preservedIndex = m_fileList.indexOf(currentName);
        if (preservedIndex >= 0) {
            m_currentIndex = preservedIndex;
            m_thumbnailBar->setCurrentIndex(preservedIndex);
            updateStatusBar();
            updateActions();
        } else {
            m_currentLoadedPath.clear();
            loadFile(qBound(0, m_currentIndex, m_fileList.size() - 1));
        }
    }

    if (persist)
        saveSettings();
}

void MainWindow::updateSortActions()
{
    for (int key = 0; key < m_sortKeyActions.size(); ++key)
        m_sortKeyActions[key]->setChecked(key == m_sortKey);
    m_sortAscendingAction->setChecked(!m_sortDescending);
    m_sortDescendingAction->setChecked(m_sortDescending);
}

void MainWindow::refreshDirectoryContents()
{
    const int previousIndex = m_currentIndex;
    const QString previousName =
        previousIndex >= 0 && previousIndex < m_fileList.size()
        ? m_fileList[previousIndex] : QString();

    updateFileList();
    watchCurrentDirectory();
    if (m_fileList.isEmpty()) {
        clearCurrentView(tr("No supported media files in this folder"),
                         tr("Drop a file here or choose another folder"));
        return;
    }

    const int preservedIndex = m_fileList.indexOf(previousName);
    if (preservedIndex >= 0) {
        m_currentIndex = preservedIndex;
        m_thumbnailBar->setCurrentIndex(preservedIndex);
        updateStatusBar();
        updateActions();
        watchCurrentFile(m_currentDir.absoluteFilePath(previousName));
        return;
    }

    const int fallbackIndex = qBound(0, previousIndex, m_fileList.size() - 1);
    m_currentLoadedPath.clear();
    loadFile(fallbackIndex);
}

void MainWindow::watchCurrentDirectory()
{
    if (!m_fileSystemWatcher)
        return;
    const QStringList watchedDirectories = m_fileSystemWatcher->directories();
    if (!watchedDirectories.isEmpty())
        m_fileSystemWatcher->removePaths(watchedDirectories);
    if (m_currentDir.exists())
        m_fileSystemWatcher->addPath(m_currentDir.absolutePath());
}

void MainWindow::watchCurrentFile(const QString &filePath)
{
    if (!m_fileSystemWatcher)
        return;
    const QStringList watchedFiles = m_fileSystemWatcher->files();
    if (!watchedFiles.isEmpty())
        m_fileSystemWatcher->removePaths(watchedFiles);
    if (QFileInfo::exists(filePath))
        m_fileSystemWatcher->addPath(filePath);
}

void MainWindow::clearCurrentView(const QString &title, const QString &details)
{
    if (m_preloadTimer)
        m_preloadTimer->stop();
    ++m_preloadGeneration;
    m_videoPlayer->stop();
    m_stackedWidget->setCurrentIndex(0);
    m_imageViewer->clearImage(title, details);
    m_currentIndex = -1;
    m_currentLoadedPath.clear();

    m_fileInfoLabel->clear();
    m_imageSizeLabel->setText(QStringLiteral("—"));
    m_zoomLabel->setText(QStringLiteral("—"));
    m_indexLabel->setText(QStringLiteral("0 / 0"));
    setWindowTitle(QStringLiteral("FlashView"));
    updateActions();
}

void MainWindow::updateStatusBar()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_fileList.size()) return;

    QFileInfo fi(m_currentDir.absoluteFilePath(m_fileList[m_currentIndex]));
    m_fileInfoLabel->setText(QString("  %1   %2")
        .arg(fi.fileName(), formatFileSize(fi.size())));

    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer->hasImage()) {
        QSize sz = m_imageViewer->imageSize();
        m_imageSizeLabel->setText(QString("%1 × %2").arg(sz.width()).arg(sz.height()));
        m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_imageViewer->currentScale() * 100)));
    } else {
        m_imageSizeLabel->setText("—");
        m_zoomLabel->setText("—");
    }

    m_indexLabel->setText(QString("%1 / %2")
        .arg(m_currentIndex + 1).arg(m_fileList.size()));
}

void MainWindow::updateActions()
{
    const bool hasSelection =
        m_currentIndex >= 0 && m_currentIndex < m_fileList.size();
    const bool hasImage =
        hasSelection && m_stackedWidget->currentIndex() == 0 && m_imageViewer->hasImage();

    m_prevAction->setEnabled(hasSelection && m_currentIndex > 0);
    m_nextAction->setEnabled(hasSelection && m_currentIndex + 1 < m_fileList.size());
    m_zoomInAction->setEnabled(hasImage);
    m_zoomOutAction->setEnabled(hasImage);
    m_fitAction->setEnabled(hasImage);
    m_actualSizeAction->setEnabled(hasImage);
    m_rotateLeftAction->setEnabled(hasImage);
    m_rotateRightAction->setEnabled(hasImage);
    m_thumbAction->setEnabled(!m_fileList.isEmpty());
    m_deleteAction->setEnabled(hasSelection);
    m_zoomLabel->setCursor(hasImage ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void MainWindow::switchLanguage(int langIndex, bool persist)
{
    m_language = langIndex;
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) return;

    app->removeTranslator(&m_translator);

    QString qmFile;
    if (langIndex == 0)
        qmFile = ":/i18n/flashview_zh.qm";
    else
        qmFile = ":/i18n/flashview_en.qm";

    if (m_translator.load(qmFile))
        app->installTranslator(&m_translator);

    m_langZhAction->setChecked(langIndex == 0);
    m_langEnAction->setChecked(langIndex == 1);

    // Re-translate the whole UI: action texts were baked in at construction
    // time, so installing a translator alone does not update them.
    retranslateUi();
    if (persist)
        saveSettings();
}

void MainWindow::retranslateUi()
{
    m_openFileAction->setText(tr("Open &File..."));
    m_openDirAction->setText(tr("Open &Directory..."));
    m_exitAction->setText(tr("E&xit"));
    m_nextAction->setText(tr("&Next"));
    m_prevAction->setText(tr("&Previous"));
    m_zoomInAction->setText(tr("Zoom &In"));
    m_zoomOutAction->setText(tr("Zoom &Out"));
    m_fitAction->setText(tr("&Fit Window"));
    m_actualSizeAction->setText(tr("&Actual Size"));
    m_rotateLeftAction->setText(tr("Rotate &Left"));
    m_rotateRightAction->setText(tr("Rotate &Right"));
    m_fullscreenAction->setText(tr("F&ullscreen"));
    m_thumbAction->setText(tr("Thumbnail &Bar"));
    m_deleteAction->setText(tr("&Delete"));
    m_settingsAction->setText(tr("&Settings..."));
    const QStringList sortNames = {tr("&Name"), tr("&Modified Time"),
                                   tr("&Created Time"), tr("&File Size"),
                                   tr("File &Type")};
    for (int key = 0; key < m_sortKeyActions.size(); ++key)
        m_sortKeyActions[key]->setText(sortNames.at(key));
    m_sortAscendingAction->setText(tr("&Ascending"));
    m_sortDescendingAction->setText(tr("D&escending"));
    m_darkThemeAction->setText(tr("&Dark Theme"));
    m_lightThemeAction->setText(tr("&Light Theme"));
    // Language entries stay self-named in their own language.
    m_langZhAction->setText(QStringLiteral("中文"));
    m_langEnAction->setText(QStringLiteral("English"));

    // Rebuild menus so the menu titles pick up the new language too.
    setupMenuBar();

    refreshActionTooltips();

    if (m_zoomLabel)
        m_zoomLabel->setToolTip(tr("Click to reset zoom to 100%"));
    if (auto *tb = findChild<QToolBar *>("MainToolBar"))
        tb->setWindowTitle(tr("Main"));

    // The empty-state hint in the viewer is drawn with tr(); repaint it.
    if (m_imageViewer)
        m_imageViewer->viewport()->update();
    if (m_videoPlayer)
        m_videoPlayer->retranslateUi();
}

void MainWindow::refreshActionTooltips()
{
    for (const auto &entry : m_toolGlyphs) {
        QAction *action = entry.first;
        QString name = action->text();
        name.remove('&');
        const QString shortcut =
            action->shortcut().toString(QKeySequence::NativeText);
        action->setToolTip(shortcut.isEmpty()
            ? name
            : QStringLiteral("%1  (%2)").arg(name, shortcut));
    }
}

void MainWindow::applyTheme()
{
    setStyleSheet(ThemeManager::instance().themeStyleSheet());
    m_darkThemeAction->setChecked(ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    m_lightThemeAction->setChecked(ThemeManager::instance().currentTheme() == ThemeManager::Light);
}

void MainWindow::loadSettings()
{
    QSettings s;
    const int defaultLanguage =
        QLocale::system().language() == QLocale::Chinese ? 0 : 1;
    m_language = qBound(0, s.value("language", defaultLanguage).toInt(), 1);
    m_thumbnailsVisible = s.value("thumbnailsVisible", true).toBool();
    m_wheelZoomMode = s.value("wheelZoomMode", false).toBool();
    m_imageViewer->setWheelZoomEnabled(m_wheelZoomMode);
    m_interpolationMode = qBound(0, s.value("interpolationMode", 0).toInt(), 2);
    m_imageViewer->setInterpolationMode(
        static_cast<ImageViewer::InterpolationMode>(m_interpolationMode));
    m_sortKey = qBound(0, s.value("sortKey", 0).toInt(),
                       static_cast<int>(MediaUtils::SortKey::Type));
    m_sortDescending = s.value("sortDescending", false).toBool();
    m_confirmDelete = s.value("confirmDelete", false).toBool();
    updateSortActions();

    m_thumbnailBar->setVisible(m_thumbnailsVisible);

    // Shortcuts from settings
    m_nextAction->setShortcut(s.value("keyNext", QKeySequence(Qt::Key_Right)).value<QKeySequence>());
    m_prevAction->setShortcut(s.value("keyPrev", QKeySequence(Qt::Key_Left)).value<QKeySequence>());
    m_zoomInAction->setShortcut(s.value("keyZoomIn", QKeySequence::ZoomIn).value<QKeySequence>());
    m_zoomOutAction->setShortcut(s.value("keyZoomOut", QKeySequence::ZoomOut).value<QKeySequence>());
    m_fitAction->setShortcut(s.value("keyFitWindow", QKeySequence(Qt::Key_F)).value<QKeySequence>());
    m_actualSizeAction->setShortcut(s.value("keyActualSize", QKeySequence(Qt::Key_1)).value<QKeySequence>());
    m_rotateLeftAction->setShortcut(s.value("keyRotateLeft", QKeySequence("Ctrl+L")).value<QKeySequence>());
    m_rotateRightAction->setShortcut(s.value("keyRotateRight", QKeySequence("Ctrl+R")).value<QKeySequence>());
    m_fullscreenAction->setShortcut(s.value("keyFullscreen", QKeySequence(Qt::Key_F11)).value<QKeySequence>());
    m_deleteAction->setShortcut(s.value("keyDelete", QKeySequence(QKeySequence::Delete)).value<QKeySequence>());
    refreshActionTooltips();

    // Restore window geometry
    if (s.contains("geometry"))
        restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState"))
        restoreState(s.value("windowState").toByteArray());

    // Language
    switchLanguage(m_language, false);
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("language", m_language);
    s.setValue("thumbnailsVisible", m_thumbnailsVisible);
    s.setValue("wheelZoomMode", m_wheelZoomMode);
    s.setValue("interpolationMode", m_interpolationMode);
    s.setValue("sortKey", m_sortKey);
    s.setValue("sortDescending", m_sortDescending);
    s.setValue("confirmDelete", m_confirmDelete);
    s.setValue("keyNext", m_nextAction->shortcut());
    s.setValue("keyPrev", m_prevAction->shortcut());
    s.setValue("keyZoomIn", m_zoomInAction->shortcut());
    s.setValue("keyZoomOut", m_zoomOutAction->shortcut());
    s.setValue("keyFitWindow", m_fitAction->shortcut());
    s.setValue("keyActualSize", m_actualSizeAction->shortcut());
    s.setValue("keyRotateLeft", m_rotateLeftAction->shortcut());
    s.setValue("keyRotateRight", m_rotateRightAction->shortcut());
    s.setValue("keyFullscreen", m_fullscreenAction->shortcut());
    s.setValue("keyDelete", m_deleteAction->shortcut());
    const bool fullscreenSnapshot =
        isFullScreen() && !m_normalGeometry.isEmpty() && !m_normalWindowState.isEmpty();
    s.setValue("geometry", fullscreenSnapshot ? m_normalGeometry : saveGeometry());
    s.setValue("windowState", fullscreenSnapshot ? m_normalWindowState : saveState());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        onToggleFullscreen();
        return;
    }

    // Paging shortcuts that complement the Left/Right action shortcuts.
    switch (event->key()) {
    case Qt::Key_PageDown:
        onNextFile();
        return;
    case Qt::Key_PageUp:
        onPrevFile();
        return;
    case Qt::Key_Home:
        if (!m_fileList.isEmpty()) { m_currentIndex = 0; loadFile(0); }
        return;
    case Qt::Key_End:
        if (!m_fileList.isEmpty()) { m_currentIndex = m_fileList.size() - 1; loadFile(m_currentIndex); }
        return;
    default:
        break;
    }

    if (event->key() == Qt::Key_Space
        && m_stackedWidget->currentIndex() == 1
        && m_videoPlayer->hasVideo()) {
        m_videoPlayer->togglePlayback();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+wheel always zooms
        if (m_stackedWidget->currentIndex() == 0) {
            int delta = event->angleDelta().y();
            if (delta > 0) m_imageViewer->zoomIn();
            else if (delta < 0) m_imageViewer->zoomOut();
        }
        event->accept();
        return;
    }

    if (m_wheelZoomMode) {
        // Wheel zoom mode: scroll to zoom
        if (m_stackedWidget->currentIndex() == 0) {
            int delta = event->angleDelta().y();
            if (delta > 0) m_imageViewer->zoomIn();
            else if (delta < 0) m_imageViewer->zoomOut();
        }
        event->accept();
    } else {
        // Normal mode: page navigation, robust across mice and trackpads.
        //
        // The angle delta per physical wheel notch is NOT guaranteed to be
        // 120: different mice report 100/120/140 etc. A fixed-threshold
        // accumulator that keeps the remainder therefore drifts, so some
        // notches flip a page and others don't (the "stutter" effect).
        //
        // Fix: handle the two input kinds separately.
        //   - Discrete mouse wheel (one notch per event): round the magnitude
        //     to whole notches (min 1) and flip immediately. Stateless, so a
        //     notch always flips exactly one page regardless of its exact size.
        //   - High-resolution wheel / trackpad (many tiny deltas): keep the
        //     smooth accumulator so small movements don't over-trigger.
        const QPoint pixelDelta = event->pixelDelta();
        const int angle = event->angleDelta().y();

        // A pause resets any residual accumulation.
        if (m_scrollDecayTimer.isValid() && m_scrollDecayTimer.elapsed() > SCROLL_DECAY_MS) {
            m_scrollAccumulator = 0;
        }
        m_scrollDecayTimer.restart();

        if (pixelDelta.isNull() && qAbs(angle) >= SCROLL_THRESHOLD / 2) {
            // Discrete mouse wheel: one physical notch per event.
            m_scrollAccumulator = 0;
            const int steps = qMax(1, qRound(qAbs(angle) / double(SCROLL_THRESHOLD)));
            for (int i = 0; i < steps; ++i) {
                if (angle > 0) onPrevFile();
                else           onNextFile();
            }
        } else if (angle != 0) {
            // High-resolution wheel / trackpad: smooth accumulation.
            // Reverse direction: cancel residual to avoid sluggish reversal.
            if ((angle > 0 && m_scrollAccumulator < 0) || (angle < 0 && m_scrollAccumulator > 0)) {
                m_scrollAccumulator = 0;
            }
            m_scrollAccumulator += angle;
            while (m_scrollAccumulator >= SCROLL_THRESHOLD) {
                m_scrollAccumulator -= SCROLL_THRESHOLD;
                onPrevFile();
            }
            while (m_scrollAccumulator <= -SCROLL_THRESHOLD) {
                m_scrollAccumulator += SCROLL_THRESHOLD;
                onNextFile();
            }
        }

        event->accept();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_cursorHidden) {
        QApplication::restoreOverrideCursor();
        m_cursorHidden = false;
    }
    saveSettings();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Clickable zoom segment: reset the image to 100%.
    if (watched == m_zoomLabel && event->type() == QEvent::MouseButtonPress) {
        if (m_stackedWidget->currentIndex() == 0 && m_imageViewer->hasImage())
            m_imageViewer->actualSize();
        return true;
    }
    // Fullscreen idle-cursor: any movement reveals the cursor and restarts
    // the hide countdown. The app-wide filter is only installed in fullscreen.
    if (isFullScreen() && event->type() == QEvent::MouseMove) {
        if (m_cursorHidden) { QApplication::restoreOverrideCursor(); m_cursorHidden = false; }
        if (m_idleCursorTimer) m_idleCursorTimer->start();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;
    for (const QUrl &url : event->mimeData()->urls()) {
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()
            || (info.isFile() && MediaUtils::isSupportedFile(info.absoluteFilePath()))) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString path = urls.first().toLocalFile();
        QFileInfo fi(path);
        if (fi.isDir())
            openDirectory(path);
        else
            openFile(path);
    }
}

bool MainWindow::isVideoFile(const QString &fileName) const
{
    const QFileInfo info(fileName);
    return MediaUtils::isVideoFile(info.isAbsolute()
        ? fileName : m_currentDir.absoluteFilePath(fileName));
}

QStringList MainWindow::supportedExtensions() const
{
    return MediaUtils::supportedExtensions();
}

void MainWindow::showRuntimeSupportWarning()
{
    QStringList missing = MediaUtils::missingModernImageFormats();
    missing.append(MediaUtils::missingCommonVideoCodecs());
    if (missing.isEmpty())
        return;
    m_statusBar->showMessage(
        tr("Limited media support; missing decoders: %1").arg(missing.join(", ")),
        12000);
}

QString MainWindow::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
