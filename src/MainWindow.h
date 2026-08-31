#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QDir>
#include <QTranslator>
#include <QElapsedTimer>
#include <QIcon>
#include <QVector>
#include <QPair>

class QTimer;
class QFileSystemWatcher;
class QToolBar;

class ImageViewer;
class VideoPlayer;
class ThumbnailBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openFile(const QString &filePath);
    void openDirectory(const QString &dirPath);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onOpenFile();
    void onOpenDirectory();
    void onNextFile();
    void onPrevFile();
    void onThumbnailSelected(int index);
    void onSettings();
    void onToggleFullscreen();
    void onToggleThumbnails();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupActions();
    void loadSettings();
    void saveSettings();
    void applyTheme();
    void loadFile(int index);
    void updateFileList();
    void updateStatusBar();
    void preloadNeighbors();
    void refreshDirectoryContents();
    void watchCurrentDirectory();
    void watchCurrentFile(const QString &filePath);
    void clearCurrentView(const QString &title = QString(), const QString &details = QString());
    void updateActions();
    void refreshActionTooltips();
    void switchLanguage(int langIndex, bool persist = true);
    void retranslateUi();
    void refreshToolbarIcons();
    void showRuntimeSupportWarning();
    QIcon glyphIcon(const QString &glyph, const QColor &color) const;
    bool isVideoFile(const QString &fileName) const;
    QStringList supportedExtensions() const;
    QString formatFileSize(qint64 bytes) const;

    // Widgets
    QStackedWidget *m_stackedWidget = nullptr;
    ImageViewer *m_imageViewer = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;
    ThumbnailBar *m_thumbnailBar = nullptr;
    QStatusBar *m_statusBar = nullptr;
    QToolBar *m_mainToolBar = nullptr;
    QLabel *m_fileInfoLabel = nullptr;
    QLabel *m_imageSizeLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_indexLabel = nullptr;

    // Actions
    QAction *m_openFileAction = nullptr;
    QAction *m_openDirAction = nullptr;
    QAction *m_exitAction = nullptr;
    QAction *m_nextAction = nullptr;
    QAction *m_prevAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_actualSizeAction = nullptr;
    QAction *m_rotateLeftAction = nullptr;
    QAction *m_rotateRightAction = nullptr;
    QAction *m_fullscreenAction = nullptr;
    QAction *m_thumbAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QAction *m_lightThemeAction = nullptr;
    QAction *m_langZhAction = nullptr;
    QAction *m_langEnAction = nullptr;

    // State
    QDir m_currentDir;
    QStringList m_fileList;
    int m_currentIndex = -1;
    QString m_currentLoadedPath;   // guards against redundant reloads
    QTimer *m_preloadTimer = nullptr; // debounced neighbour preloading
    quint64 m_preloadGeneration = 0;
    QTimer *m_idleCursorTimer = nullptr; // fullscreen idle cursor hide
    QTimer *m_directoryRefreshTimer = nullptr;
    QFileSystemWatcher *m_fileSystemWatcher = nullptr;
    bool m_cursorHidden = false;
    bool m_wasMaximizedBeforeFullscreen = false;
    QByteArray m_normalGeometry;
    QByteArray m_normalWindowState;
    bool m_thumbnailsVisible = true;
    bool m_wheelZoomMode = false;
    int m_interpolationMode = 0;
    int m_language = 0; // 0=zh, 1=en
    QTranslator m_translator;
    QVector<QPair<QAction *, QString>> m_toolGlyphs; // toolbar glyphs, re-themed on theme change
    // Wheel navigation: accumulator-based for smooth mouse/trackpad
    int m_scrollAccumulator = 0;
    QElapsedTimer m_scrollDecayTimer;
    static constexpr int SCROLL_THRESHOLD = 120; // one wheel notch
    static constexpr int SCROLL_DECAY_MS = 300;  // reset after pause
};

#endif // MAINWINDOW_H
