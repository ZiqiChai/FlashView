#include "PreviewWindow.h"

#include "ImageViewer.h"
#include "MediaUtils.h"
#include "ThemeManager.h"
#include "VideoPlayer.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOption>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr int kHeaderHeight = 38;
constexpr double kMaxScreenFraction = 0.8;
} // namespace

PreviewWindow::PreviewWindow(QWidget *parent)
    : QWidget(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setupUi();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this] { applyTheme(); });

    // Follow the viewing preferences the user already set in the main window.
    QSettings settings;
    m_imageViewer->setWheelZoomEnabled(settings.value("wheelZoomMode", false).toBool());
    m_imageViewer->setInterpolationMode(static_cast<ImageViewer::InterpolationMode>(
        qBound(0, settings.value("interpolationMode", 0).toInt(), 2)));
}

PreviewWindow::~PreviewWindow()
{
    delete m_foreignParent.data();
}

void PreviewWindow::setupUi()
{
    setWindowTitle(tr("Preview"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("previewHeader"));
    m_header->setFixedHeight(kHeaderHeight);
    m_header->installEventFilter(this);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(12, 0, 8, 0);
    headerLayout->setSpacing(8);

    m_titleLabel = new QLabel(m_header);
    m_titleLabel->setObjectName(QStringLiteral("previewTitle"));
    // Ignored width lets a long file name shrink instead of pushing the
    // buttons out of the header.
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerLayout->addWidget(m_titleLabel, 1);

    m_openButton = new QPushButton(tr("Open"), m_header);
    m_openButton->setToolTip(tr("Open in FlashView (Enter)"));
    m_openButton->setFocusPolicy(Qt::NoFocus);
    connect(m_openButton, &QPushButton::clicked, this, &PreviewWindow::openInFullViewer);
    headerLayout->addWidget(m_openButton);

    m_closeButton = new QPushButton(QStringLiteral("\u2715"), m_header);
    m_closeButton->setToolTip(tr("Close (Space or Esc)"));
    m_closeButton->setFixedWidth(34);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    connect(m_closeButton, &QPushButton::clicked, this, &PreviewWindow::closeRequested);
    headerLayout->addWidget(m_closeButton);

    layout->addWidget(m_header);

    m_stack = new QStackedWidget(this);
    m_imageViewer = new ImageViewer;
    // The scroll area must not take focus, otherwise it consumes the arrow
    // keys the preview needs to forward to the file manager.
    m_imageViewer->setFocusPolicy(Qt::NoFocus);
    m_videoPlayer = new VideoPlayer;
    m_stack->addWidget(m_imageViewer); // index 0
    m_stack->addWidget(m_videoPlayer); // index 1
    layout->addWidget(m_stack, 1);

    // Paging inside the preview is the file manager's decision, so relay the
    // viewer's own navigation gestures as selection requests too.
    connect(m_imageViewer, &ImageViewer::navigateNext, this,
            [this] { emit selectionRequested(Right); });
    connect(m_imageViewer, &ImageViewer::navigatePrev, this,
            [this] { emit selectionRequested(Left); });

    setupActions();
}

void PreviewWindow::setupActions()
{
    // Window-level shortcuts rather than key handlers: the image view is a
    // scroll area that would otherwise swallow the arrow keys for panning.
    auto addShortcut = [this](const QList<QKeySequence> &keys, auto &&handler) {
        auto *action = new QAction(this);
        action->setShortcuts(keys);
        action->setShortcutContext(Qt::WindowShortcut);
        connect(action, &QAction::triggered, this, handler);
        addAction(action);
        return action;
    };

    addShortcut({QKeySequence(Qt::Key_Space), QKeySequence(Qt::Key_Escape)},
                [this] { emit closeRequested(); });
    addShortcut({QKeySequence(Qt::Key_Left)}, [this] { emit selectionRequested(Left); });
    addShortcut({QKeySequence(Qt::Key_Right)}, [this] { emit selectionRequested(Right); });
    addShortcut({QKeySequence(Qt::Key_Up)}, [this] { emit selectionRequested(Up); });
    addShortcut({QKeySequence(Qt::Key_Down)}, [this] { emit selectionRequested(Down); });
    addShortcut({QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)},
                [this] { openInFullViewer(); });
    addShortcut({QKeySequence(Qt::Key_F)}, [this] {
        if (m_stack->currentIndex() == 0)
            m_imageViewer->fitToWindow();
    });
    addShortcut({QKeySequence(Qt::Key_1)}, [this] {
        if (m_stack->currentIndex() == 0)
            m_imageViewer->actualSize();
    });
}

void PreviewWindow::applyTheme()
{
    const bool dark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;
    const QString headerBackground = dark ? QStringLiteral("#22222e") : QStringLiteral("#f0f0f4");
    const QString headerText = dark ? QStringLiteral("#d8d8e8") : QStringLiteral("#1f1f2e");
    const QString border = dark ? QStringLiteral("#3a3a4c") : QStringLiteral("#d0d0dc");
    const QString btnHover = dark ? QStringLiteral("#32323e") : QStringLiteral("#e2e2ea");
    const QString btnPressed = dark ? QStringLiteral("#3c3c4a") : QStringLiteral("#d4d4de");

    setStyleSheet(ThemeManager::instance().themeStyleSheet()
        + QStringLiteral(R"(
            PreviewWindow { background-color: %1; border: 1px solid %3; }
            QWidget#previewHeader { background-color: %1; border-bottom: 1px solid %3; }
            QLabel#previewTitle { color: %2; font-size: 13px; padding: 0; }
            QWidget#previewHeader QPushButton {
                background-color: transparent;
                color: %2;
                border: none;
                border-radius: 6px;
                padding: 4px 12px;
                font-size: 12px;
                font-weight: 400;
                min-height: 0;
            }
            QWidget#previewHeader QPushButton:hover { background-color: %4; }
            QWidget#previewHeader QPushButton:pressed { background-color: %5; }
        )").arg(headerBackground, headerText, border, btnHover, btnPressed));
}

bool PreviewWindow::showFile(const QString &filePath, const QString &windowHandle)
{
    applyTransientParent(windowHandle);

    const QFileInfo info(filePath);
    m_currentFile = filePath;
    m_titleLabel->setText(info.fileName());

    bool supported = true;
    if (!info.exists() || !info.isReadable()) {
        showUnsupported(tr("This file is no longer available"), info.fileName(),
                        filePath);
        supported = false;
    } else if (MediaUtils::isVideoFile(filePath)) {
        m_stack->setCurrentIndex(1);
        m_videoPlayer->loadVideo(filePath);
        resizeToContent(QSize(960, 540));
    } else if (MediaUtils::isImageFile(filePath) && m_imageViewer->loadImage(filePath)) {
        m_videoPlayer->stop();
        m_stack->setCurrentIndex(0);
        resizeToContent(m_imageViewer->imageSize());
    } else {
        showUnsupported(tr("FlashView previews images and videos"),
                        tr("Press Enter to open %1 with another application")
                            .arg(info.fileName()),
                        filePath);
        supported = false;
    }

    show();
    raise();
    activateWindow();
    return supported;
}

void PreviewWindow::showUnsupported(const QString &title, const QString &details,
                                    const QString &associatedFile)
{
    m_videoPlayer->stop();
    m_stack->setCurrentIndex(0);
    m_imageViewer->clearImage(title, details);
    m_currentFile = associatedFile;
    m_titleLabel->setText(associatedFile.isEmpty()
                          ? title : QFileInfo(associatedFile).fileName());
    resizeToContent(QSize(720, 420));
    show();
    raise();
    activateWindow();
}

bool PreviewWindow::showsMedia() const
{
    return m_stack->currentIndex() == 1 ? m_videoPlayer->hasVideo()
                                        : m_imageViewer->hasImage();
}

void PreviewWindow::resizeToContent(const QSize &content)
{
    const QScreen *screen = m_foreignParent ? m_foreignParent->screen() : nullptr;
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry();
    const QSize bounds(qRound(available.width() * kMaxScreenFraction),
                       qRound(available.height() * kMaxScreenFraction - kHeaderHeight));

    // Shrink oversized media to fit the screen, but never inflate a small
    // image into a mostly empty window.
    QSize media = content.isEmpty() ? QSize(720, 480) : content;
    if (media.width() > bounds.width() || media.height() > bounds.height())
        media.scale(bounds, Qt::KeepAspectRatio);
    media = media.expandedTo(QSize(480, 320));

    const QSize target(media.width(), media.height() + kHeaderHeight);
    setGeometry(QRect(available.center() - QPoint(target.width() / 2, target.height() / 2),
                      target));
}

void PreviewWindow::applyTransientParent(const QString &handle)
{
    // Handles look like "x11:1a00007" (hexadecimal XID) or "wayland:<token>".
    // Anything else, including an empty handle, simply leaves the overlay
    // unparented, which still works - it just no longer follows the caller.
    if (!handle.startsWith(QStringLiteral("x11:")) || handle == m_parentHandle)
        return;

    QString id = handle.mid(4);
    if (id.startsWith(QStringLiteral("0x")))
        id = id.mid(2);
    bool ok = false;
    const WId parentId = id.toULongLong(&ok, 16);
    if (!ok || parentId == 0)
        return;

    delete m_foreignParent.data();
    m_foreignParent = QWindow::fromWinId(parentId);
    if (!m_foreignParent)
        return;

    winId(); // realize our own window before parenting it
    if (QWindow *own = windowHandle())
        own->setTransientParent(m_foreignParent);
    m_parentHandle = handle;
}

void PreviewWindow::paintEvent(QPaintEvent *)
{
    // Required for a plain QWidget subclass to honour its own style sheet.
    QStyleOption option;
    option.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
}

void PreviewWindow::openInFullViewer()
{
    if (!m_currentFile.isEmpty()) {
        const QFileInfo info(m_currentFile);
        if (MediaUtils::isSupportedFile(m_currentFile)) {
            QProcess::startDetached(QApplication::applicationFilePath(),
                                    {info.absoluteFilePath()});
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
        }
    }
    emit closeRequested();
}

void PreviewWindow::keyPressEvent(QKeyEvent *event)
{
    // Window shortcuts only fire while the window manager treats this window
    // as active. Replaying the key against the same actions keeps the preview
    // usable when it does not, such as under a window manager that refuses to
    // focus a borderless window.
    const QKeySequence pressed(event->key()
                               | (event->modifiers() & ~Qt::KeypadModifier).toInt());
    for (QAction *action : actions()) {
        if (action->shortcuts().contains(pressed)) {
            action->trigger();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void PreviewWindow::hideEvent(QHideEvent *event)
{
    // Audio must not outlive a dismissed preview.
    m_videoPlayer->stop();
    QWidget::hideEvent(event);
}

bool PreviewWindow::eventFilter(QObject *watched, QEvent *event)
{
    // A borderless window gets no title bar from the window manager, so the
    // header doubles as the drag handle.
    if (watched == m_header) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOrigin = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(mouse->globalPosition().toPoint() - m_dragOrigin);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
    }
    return QWidget::eventFilter(watched, event);
}
