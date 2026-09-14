#include "FlashViewStyle.h"
#include "ImageViewer.h"
#include "MainWindow.h"
#include "MediaUtils.h"
#include "ThemeManager.h"
#include "ThumbnailBar.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMovie>
#include <QPainter>
#include <QSettings>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
#include <QWheelEvent>

#ifdef FLASHVIEW_HAVE_PREVIEWER
#include "PreviewWindow.h"
#include "PreviewerService.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QUrl>

// Captures the D-Bus signal the preview sends back to the file manager.
class SelectionEventRecorder : public QObject
{
    Q_OBJECT

public:
    QVector<uint> directions;

public slots:
    void record(uint direction) { directions.append(direction); }
};
#endif

class FlashViewTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void recognizesFormatsAndUsesNaturalOrder();
    void imageViewerClearsStaleContentOnDecodeFailure();
    void animatedGifAdvancesFrames();
    void fitToWindowNeverUpscalesSmallImages();
    void interpolationModesRemainResponsive();
    void wheelZoomSettingWorksInsideTheViewer();
    void thumbnailsLoadProgressively();
    void mainWindowTracksFolderChanges();
    void browsingOrderFollowsTheChosenSortKey();
    void deleteKeyRemovesTheFileAndShowsTheNextOne();
    void toolbarActionsHaveClearVisualSemantics();
#ifdef FLASHVIEW_HAVE_PREVIEWER
    void previewServiceAnswersTheFileManager();
    void previewServiceDegradesInsteadOfFailing();
#endif
    void generateDocumentationScreenshots();

private:
    QTemporaryDir m_settingsDirectory;
};

void FlashViewTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
    QCoreApplication::setOrganizationName(QStringLiteral("FlashViewTests"));
    QCoreApplication::setApplicationName(QStringLiteral("FlashViewTests"));
    qApp->setStyle(new FlashViewStyle);
}

void FlashViewTests::recognizesFormatsAndUsesNaturalOrder()
{
    QVERIFY(MediaUtils::isImageFile(QStringLiteral("holiday.JpG")));
    QVERIFY(MediaUtils::isVideoFile(QStringLiteral("clip.WEBM")));
    QVERIFY(!MediaUtils::isSupportedFile(QStringLiteral("notes.txt")));
    QVERIFY(MediaUtils::imageExtensions().contains(QStringLiteral("png")));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString disguisedPath = directory.filePath(QStringLiteral("image.data"));
    QImage disguised(3, 2, QImage::Format_RGB32);
    disguised.fill(Qt::cyan);
    QVERIFY(disguised.save(disguisedPath, "PNG"));
    QVERIFY(MediaUtils::isImageFile(disguisedPath));

    QStringList names = {
        QStringLiteral("image10.png"),
        QStringLiteral("image2.png"),
        QStringLiteral("image1.png")
    };
    MediaUtils::naturalSort(names);
    QCOMPARE(names, QStringList({
        QStringLiteral("image1.png"),
        QStringLiteral("image2.png"),
        QStringLiteral("image10.png")
    }));
}

void FlashViewTests::imageViewerClearsStaleContentOnDecodeFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString validPath = directory.filePath(QStringLiteral("valid.png"));
    QImage image(64, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(74, 128, 224));
    QVERIFY(image.save(validPath));

    const QString brokenPath = directory.filePath(QStringLiteral("broken.png"));
    QFile brokenFile(brokenPath);
    QVERIFY(brokenFile.open(QIODevice::WriteOnly));
    QCOMPARE(brokenFile.write("not an image"), qint64(12));
    brokenFile.close();

    ImageViewer viewer;
    QVERIFY(viewer.loadImage(validPath));
    QVERIFY(viewer.hasImage());
    QCOMPARE(viewer.imageSize(), QSize(64, 48));

    QVERIFY(!viewer.loadImage(brokenPath));
    QVERIFY(!viewer.hasImage());
    QVERIFY(viewer.currentFile().isEmpty());
    QCOMPARE(viewer.imageSize(), QSize());
}

void FlashViewTests::animatedGifAdvancesFrames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // A looped 1x1 GIF with two independently timed frames.
    const QByteArray gif = QByteArray::fromHex(
        "47494638396101000100800000000000ffffff"
        "21ff0b4e45545343415045322e300301000000"
        "21f90400050000002c0000000001000100000202440100"
        "21f90400050000002c00000000010001000002024c0100"
        "3b");
    QFile file(directory.filePath(QStringLiteral("animated.gif")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(gif), gif.size());
    file.close();

    ImageViewer viewer;
    QVERIFY(viewer.loadImage(file.fileName()));
    auto *movie = viewer.findChild<QMovie *>();
    QVERIFY(movie);
    QVERIFY(movie->isValid());
    QTRY_VERIFY_WITH_TIMEOUT(movie->currentFrameNumber() >= 1, 1000);
    QVERIFY(viewer.hasImage());
    QCOMPARE(viewer.imageSize(), QSize(1, 1));
}

void FlashViewTests::fitToWindowNeverUpscalesSmallImages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString smallPath = directory.filePath(QStringLiteral("small.png"));
    QImage small(80, 60, QImage::Format_RGB32);
    small.fill(Qt::cyan);
    QVERIFY(small.save(smallPath));

    const QString largePath = directory.filePath(QStringLiteral("large.png"));
    QImage large(1600, 1200, QImage::Format_RGB32);
    large.fill(Qt::blue);
    QVERIFY(large.save(largePath));

    ImageViewer viewer;
    viewer.resize(400, 300);
    viewer.show();

    QVERIFY(viewer.loadImage(smallPath));
    QTRY_COMPARE_WITH_TIMEOUT(viewer.currentScale(), 1.0, 500);

    viewer.zoomIn();
    QTRY_VERIFY_WITH_TIMEOUT(viewer.currentScale() > 1.0, 500);

    QVERIFY(viewer.loadImage(largePath));
    QTRY_VERIFY_WITH_TIMEOUT(viewer.currentScale() < 1.0, 500);
}

void FlashViewTests::interpolationModesRemainResponsive()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("sample.png"));
    QImage image(96, 64, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x)
            image.setPixelColor(x, y, QColor(x * 2, y * 3, (x + y) * 2));
    }
    QVERIFY(image.save(imagePath));

    ImageViewer viewer;
    viewer.resize(480, 320);
    viewer.show();
    QVERIFY(viewer.loadImage(imagePath));
    QTRY_COMPARE_WITH_TIMEOUT(viewer.currentScale(), 1.0, 500);

    viewer.setInterpolationMode(ImageViewer::InterpolationMode::Nearest);
    QVERIFY(!(viewer.renderHints() & QPainter::SmoothPixmapTransform));
    viewer.setInterpolationMode(ImageViewer::InterpolationMode::Smooth);
    QVERIFY(viewer.renderHints() & QPainter::SmoothPixmapTransform);

    viewer.zoomIn();
    QTRY_VERIFY_WITH_TIMEOUT(viewer.currentScale() > 1.0, 500);
    QTRY_VERIFY_WITH_TIMEOUT(([&viewer, &image] {
        const QList<QGraphicsItem *> items = viewer.scene()->items();
        if (items.isEmpty())
            return false;
        auto *pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem *>(items.first());
        return pixmapItem && pixmapItem->pixmap().width() > image.width();
    }()), 1500);
    QCOMPARE(viewer.imageSize(), image.size());

    const QString pixelArtPath = directory.filePath(QStringLiteral("pixel-art.png"));
    QImage pixelArt(32, 32, QImage::Format_RGB32);
    pixelArt.fill(Qt::magenta);
    QVERIFY(pixelArt.save(pixelArtPath));
    viewer.setInterpolationMode(ImageViewer::InterpolationMode::Auto);
    QVERIFY(viewer.loadImage(pixelArtPath));
    viewer.zoomIn();
    QTRY_VERIFY_WITH_TIMEOUT(viewer.currentScale() > 1.0, 500);
    QVERIFY(!(viewer.renderHints() & QPainter::SmoothPixmapTransform));
}

void FlashViewTests::wheelZoomSettingWorksInsideTheViewer()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath = directory.filePath(QStringLiteral("zoom.png"));
    QImage image(400, 300, QImage::Format_RGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(imagePath));

    ImageViewer viewer;
    viewer.resize(300, 220);
    viewer.show();
    QVERIFY(viewer.loadImage(imagePath));
    QTest::qWait(30);
    const double before = viewer.currentScale();

    viewer.setWheelZoomEnabled(true);
    QWheelEvent wheel(
        QPointF(120, 100), QPointF(120, 100),
        QPoint(), QPoint(0, 120),
        Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewer.viewport(), &wheel);

    QVERIFY(viewer.currentScale() > before);
    QVERIFY(wheel.isAccepted());
}

void FlashViewTests::thumbnailsLoadProgressively()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QStringList names;
    for (int i = 0; i < 3; ++i) {
        const QString name = QStringLiteral("thumb%1.png").arg(i);
        QImage image(320 + i * 10, 180 + i * 10, QImage::Format_RGB32);
        image.fill(QColor::fromHsv(i * 80, 180, 220));
        QVERIFY(image.save(directory.filePath(name)));
        names.append(name);
    }

    ThumbnailBar bar;
    bar.setDirectory(directory.path(), names);
    auto *view = bar.findChild<QListView *>();
    QVERIFY(view);
    auto *model = qobject_cast<QStandardItemModel *>(view->model());
    QVERIFY(model);
    QCOMPARE(model->rowCount(), names.size());

    for (int row = 0; row < names.size(); ++row) {
        QTRY_VERIFY_WITH_TIMEOUT(
            !model->item(row)->data(Qt::DecorationRole).value<QIcon>().isNull(),
            3000);
    }
}

void FlashViewTests::mainWindowTracksFolderChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QImage image(80, 60, QImage::Format_RGB32);
    image.fill(QColor(74, 128, 224));
    const QString image2 = directory.filePath(QStringLiteral("image2.png"));
    const QString image10 = directory.filePath(QStringLiteral("image10.png"));
    QVERIFY(image.save(image10));
    QVERIFY(image.save(image2));

    MainWindow window;
    window.openDirectory(directory.path());
    auto *viewer = window.findChild<ImageViewer *>();
    auto *thumbnailBar = window.findChild<ThumbnailBar *>();
    QVERIFY(viewer);
    QVERIFY(thumbnailBar);
    QVERIFY(viewer->currentFile().endsWith(QStringLiteral("image2.png")));

    QVERIFY(QFile::remove(image2));
    QTRY_VERIFY_WITH_TIMEOUT(
        viewer->currentFile().endsWith(QStringLiteral("image10.png")), 3000);

    QVERIFY(QFile::remove(image10));
    QTRY_VERIFY_WITH_TIMEOUT(!viewer->hasImage(), 3000);
    QCOMPARE(thumbnailBar->currentIndex(), -1);
}

void FlashViewTests::browsingOrderFollowsTheChosenSortKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // "newest" is deliberately the last name alphabetically and the smallest
    // file, so every sort key produces a distinguishable order.
    const QDateTime base(QDate(2024, 5, 1), QTime(12, 0));
    const QVector<QPair<QString, int>> files = {
        {QStringLiteral("alpha.png"), 400},
        {QStringLiteral("bravo.png"), 200},
        {QStringLiteral("newest.png"), 60}
    };
    for (int i = 0; i < files.size(); ++i) {
        QImage image(files.at(i).second, files.at(i).second, QImage::Format_RGB32);
        image.fill(QColor::fromHsv(i * 90, 200, 210));
        const QString path = directory.filePath(files.at(i).first);
        QVERIFY(image.save(path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(base.addDays(i * 3),
                                 QFileDevice::FileModificationTime));
    }

    const QDir dir(directory.path());
    const QStringList names = {QStringLiteral("bravo.png"),
                               QStringLiteral("newest.png"),
                               QStringLiteral("alpha.png")};

    QStringList sorted = names;
    MediaUtils::sortFiles(dir, sorted, MediaUtils::SortKey::Name, false);
    QCOMPARE(sorted, QStringList({QStringLiteral("alpha.png"),
                                  QStringLiteral("bravo.png"),
                                  QStringLiteral("newest.png")}));

    sorted = names;
    MediaUtils::sortFiles(dir, sorted, MediaUtils::SortKey::Name, true);
    QCOMPARE(sorted, QStringList({QStringLiteral("newest.png"),
                                  QStringLiteral("bravo.png"),
                                  QStringLiteral("alpha.png")}));

    sorted = names;
    MediaUtils::sortFiles(dir, sorted, MediaUtils::SortKey::ModifiedTime, true);
    QCOMPARE(sorted, QStringList({QStringLiteral("newest.png"),
                                  QStringLiteral("bravo.png"),
                                  QStringLiteral("alpha.png")}));

    sorted = names;
    MediaUtils::sortFiles(dir, sorted, MediaUtils::SortKey::Size, false);
    QCOMPARE(sorted.first(), QStringLiteral("newest.png"));

    // The pinned order is what the window browses with, so the file opened
    // first and the wheel neighbours match the order shown in the thumbnails.
    QSettings settings;
    settings.setValue("sortKey", int(MediaUtils::SortKey::ModifiedTime));
    settings.setValue("sortDescending", true);
    settings.sync();

    MainWindow window;
    window.openDirectory(directory.path());
    auto *viewer = window.findChild<ImageViewer *>();
    QVERIFY(viewer);
    QVERIFY(viewer->currentFile().endsWith(QStringLiteral("newest.png")));

    settings.remove("sortKey");
    settings.remove("sortDescending");
    settings.sync();
}

void FlashViewTests::deleteKeyRemovesTheFileAndShowsTheNextOne()
{
    // The trash lives on the home volume; prefer a folder there so deleting
    // does not fall back to a confirmation this test cannot answer.
    QTemporaryDir homeDirectory(QDir::homePath() + QStringLiteral("/flashview-test-"));
    QTemporaryDir tempDirectory;
    QTemporaryDir &directory =
        homeDirectory.isValid() ? homeDirectory : tempDirectory;
    QVERIFY(directory.isValid());

    QFile probe(directory.filePath(QStringLiteral("probe.bin")));
    QVERIFY(probe.open(QIODevice::WriteOnly));
    probe.write("probe");
    probe.close();
    if (!probe.moveToTrash())
        QSKIP("This filesystem offers no trash folder");
    QFile::remove(probe.fileName());

    QImage image(48, 36, QImage::Format_RGB32);
    image.fill(QColor(74, 128, 224));
    const QStringList names = {QStringLiteral("one.png"),
                               QStringLiteral("two.png")};
    for (const QString &name : names)
        QVERIFY(image.save(directory.filePath(name)));

    QSettings settings;
    settings.setValue("confirmDelete", false);
    settings.sync();

    MainWindow window;
    window.openDirectory(directory.path());
    auto *viewer = window.findChild<ImageViewer *>();
    auto *thumbnailBar = window.findChild<ThumbnailBar *>();
    QVERIFY(viewer);
    QVERIFY(thumbnailBar);
    QVERIFY(viewer->currentFile().endsWith(QStringLiteral("one.png")));

    QAction *deleteAction = nullptr;
    for (QAction *action : window.findChildren<QAction *>()) {
        if (action->shortcut() == QKeySequence(QKeySequence::Delete))
            deleteAction = action;
    }
    QVERIFY(deleteAction);
    QVERIFY(deleteAction->isEnabled());

    deleteAction->trigger();
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("one.png"))));
    QTRY_VERIFY_WITH_TIMEOUT(
        viewer->currentFile().endsWith(QStringLiteral("two.png")), 3000);

    deleteAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!viewer->hasImage(), 3000);
    QCOMPARE(thumbnailBar->currentIndex(), -1);
    QVERIFY(!deleteAction->isEnabled());

    // Leave the user's trash as clean as the test found it.
    const QString trash = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
    for (const QString &name : names) {
        QFile::remove(trash + QStringLiteral("/files/") + name);
        QFile::remove(trash + QStringLiteral("/info/") + name
                      + QStringLiteral(".trashinfo"));
    }
    settings.remove("confirmDelete");
    settings.sync();
}

void FlashViewTests::toolbarActionsHaveClearVisualSemantics()
{
    MainWindow window;
    window.show();
    QTest::qWait(30);

    auto *toolbar = window.findChild<QToolBar *>(QStringLiteral("MainToolBar"));
    QVERIFY(toolbar);
    const int toolbarHeight = toolbar->height();
    QVERIFY2(toolbarHeight <= 46,
             qPrintable(QStringLiteral("The toolbar height is %1 px").arg(toolbarHeight)));
    for (QToolButton *button : toolbar->findChildren<QToolButton *>()) {
        if (!button->isVisible())
            continue;
        const int topGap = button->geometry().top();
        const int bottomGap = toolbar->height() - button->geometry().bottom() - 1;
        QVERIFY2(qAbs(topGap - bottomGap) <= 1,
                 qPrintable(QStringLiteral("Toolbar gaps are %1 px / %2 px")
                                .arg(topGap).arg(bottomGap)));
    }

    auto actionWithShortcut = [&window](const QKeySequence &shortcut) {
        for (QAction *action : window.findChildren<QAction *>()) {
            if (action->shortcut() == shortcut)
                return action;
        }
        return static_cast<QAction *>(nullptr);
    };

    QAction *openFile = actionWithShortcut(QKeySequence::Open);
    QAction *openFolder = actionWithShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    QAction *fitWindow = actionWithShortcut(QKeySequence(Qt::Key_F));
    QAction *fullscreen = actionWithShortcut(QKeySequence(Qt::Key_F11));
    QAction *thumbnails = actionWithShortcut(QKeySequence(Qt::Key_T));
    QVERIFY(openFile);
    QVERIFY(openFolder);
    QVERIFY(fitWindow);
    QVERIFY(fullscreen);
    QVERIFY(thumbnails);

    QVERIFY(!openFile->icon().isNull());
    QVERIFY(!openFolder->icon().isNull());
    QVERIFY(openFile->icon().cacheKey() != openFolder->icon().cacheKey());
    QVERIFY(fitWindow->icon().cacheKey() != fullscreen->icon().cacheKey());
    QVERIFY(!thumbnails->isCheckable());
}

#ifdef FLASHVIEW_HAVE_PREVIEWER
void FlashViewTests::previewServiceAnswersTheFileManager()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        QSKIP("No session bus available for the preview service");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("preview.png"));
    QImage image(320, 240, QImage::Format_RGB32);
    image.fill(QColor(74, 128, 224));
    QVERIFY(image.save(imagePath));

    // A private name keeps the test from taking over the desktop's previewer.
    const QString serviceName =
        QStringLiteral("org.gnome.NautilusPreviewer.FlashViewTest");
    PreviewerService service;
    service.setIdleTimeoutMinutes(0);
    QString message;
    QCOMPARE(service.registerOnBus(&message, serviceName),
             PreviewerService::Registration::Registered);

    QDBusInterface previewer(serviceName, PreviewerService::objectPath(),
                             PreviewerService::interfaceName(), bus);
    QVERIFY(previewer.isValid());

    SelectionEventRecorder recorder;
    QVERIFY(bus.connect(serviceName, PreviewerService::objectPath(),
                        PreviewerService::interfaceName(),
                        QStringLiteral("SelectionEvent"),
                        &recorder, SLOT(record(uint))));

    const QString uri = QUrl::fromLocalFile(imagePath).toString();
    QDBusReply<void> shown = previewer.call(QStringLiteral("ShowFile"), uri,
                                            QString(), false);
    QVERIFY2(shown.isValid(), qPrintable(shown.error().message()));

    PreviewWindow *window = service.previewWindow();
    QVERIFY(window);
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(window->currentFile(), imagePath);
    QVERIFY(window->showsMedia());
    QCOMPARE(previewer.property("Visible").toBool(), true);

    // Arrow keys hand the browsing order back to the file manager instead of
    // paging locally, so the preview follows the order it displays.
    QTest::keyClick(window, Qt::Key_Right);
    QTRY_COMPARE(recorder.directions.size(), 1);
    QCOMPARE(recorder.directions.constFirst(), uint(PreviewWindow::Right));
    QTest::keyClick(window, Qt::Key_Up);
    QTRY_COMPARE(recorder.directions.size(), 2);
    QCOMPARE(recorder.directions.at(1), uint(PreviewWindow::Up));

    // The preview key pressed again on the same file dismisses the preview.
    QDBusReply<void> toggled = previewer.call(QStringLiteral("ShowFile"), uri,
                                              QString(), true);
    QVERIFY(toggled.isValid());
    QTRY_VERIFY(!window->isVisible());
    QCOMPARE(previewer.property("Visible").toBool(), false);

    // Escape closes it as well, and the legacy interface still works.
    QDBusInterface legacy(serviceName, PreviewerService::objectPath(),
                          QStringLiteral("org.gnome.NautilusPreviewer"), bus);
    QDBusReply<void> legacyShown =
        legacy.call(QStringLiteral("ShowFile"), uri, 0, false);
    QVERIFY2(legacyShown.isValid(), qPrintable(legacyShown.error().message()));
    QTRY_VERIFY(window->isVisible());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->isVisible());
}

void FlashViewTests::previewServiceDegradesInsteadOfFailing()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        QSKIP("No session bus available for the preview service");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString textPath = directory.filePath(QStringLiteral("notes.txt"));
    QFile notes(textPath);
    QVERIFY(notes.open(QIODevice::WriteOnly));
    notes.write("not a medium");
    notes.close();

    {
        PreviewerService service;
        service.setIdleTimeoutMinutes(0);
        QCOMPARE(service.registerOnBus(
                     nullptr, QStringLiteral("org.gnome.NautilusPreviewer.FlashViewFallback")),
                 PreviewerService::Registration::Registered);

        // A file the viewer cannot decode, a location it cannot reach and a
        // file that disappeared must all end in an explanation, never in a
        // crash or an empty frame.
        service.showFile(QUrl::fromLocalFile(textPath).toString(), QString(), false);
        PreviewWindow *window = service.previewWindow();
        QVERIFY(window);
        QVERIFY(!window->showsMedia());
        QVERIFY(service.isVisible());

        service.showFile(QStringLiteral("sftp://example.invalid/photo.png"), QString(), false);
        QVERIFY(!window->showsMedia());

        service.showFile(
            QUrl::fromLocalFile(directory.filePath(QStringLiteral("gone.png"))).toString(),
            QString(), false);
        QVERIFY(!window->showsMedia());

        // An unusable window handle must not stop the preview from appearing.
        const QString imagePath = directory.filePath(QStringLiteral("ok.png"));
        QImage image(64, 64, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(imagePath));
        service.showFile(QUrl::fromLocalFile(imagePath).toString(),
                         QStringLiteral("wayland:nonsense"), false);
        QVERIFY(window->showsMedia());

        service.closePreview();
        QVERIFY(!service.isVisible());
    }

    // A name another previewer already owns is left alone, and the caller is
    // told why instead of ending up half-registered.
    const QString takenName = QStringLiteral("org.gnome.NautilusPreviewer.FlashViewTaken");
    const QString rivalConnection = QStringLiteral("flashview-rival-previewer");
    // A separate connection stands in for the other previewer: claiming the
    // name twice on one connection would simply succeed.
    QDBusConnection rival =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, rivalConnection);
    QVERIFY(rival.isConnected());
    QVERIFY(rival.registerService(takenName));

    PreviewerService blocked;
    blocked.setIdleTimeoutMinutes(0);
    QString message;
    QCOMPARE(blocked.registerOnBus(&message, takenName),
             PreviewerService::Registration::NameTaken);
    QVERIFY(!message.isEmpty());

    rival.unregisterService(takenName);
    QDBusConnection::disconnectFromBus(rivalConnection);
}
#endif

void FlashViewTests::generateDocumentationScreenshots()
{
    const QString outputDirectory =
        qEnvironmentVariable("FLASHVIEW_SCREENSHOT_DIR");
    if (outputDirectory.isEmpty())
        QSKIP("Set FLASHVIEW_SCREENSHOT_DIR");

    QVERIFY(QDir().mkpath(outputDirectory));

    // Generate a deterministic PNG solely as content for the real UI capture.
    // This avoids relying on optional image plugins in headless CI.
    const QString mediaPath =
        m_settingsDirectory.filePath(QStringLiteral("mountain-sunset.png"));
    QImage sample(1600, 1000, QImage::Format_RGB32);
    QPainter samplePainter(&sample);
    QLinearGradient sky(0, 0, 0, sample.height());
    sky.setColorAt(0.0, QColor(35, 48, 92));
    sky.setColorAt(0.55, QColor(113, 105, 166));
    sky.setColorAt(1.0, QColor(238, 165, 127));
    samplePainter.fillRect(sample.rect(), sky);
    samplePainter.setPen(Qt::NoPen);
    samplePainter.setBrush(QColor(255, 219, 145, 220));
    samplePainter.drawEllipse(QPointF(1260, 245), 105, 105);
    samplePainter.setBrush(QColor(68, 67, 108));
    samplePainter.drawPolygon(QPolygonF({
        QPointF(0, 760), QPointF(250, 480), QPointF(465, 680),
        QPointF(720, 370), QPointF(1040, 700), QPointF(1280, 455),
        QPointF(1600, 735), QPointF(1600, 1000), QPointF(0, 1000)
    }));
    samplePainter.setBrush(QColor(35, 43, 73));
    samplePainter.drawPolygon(QPolygonF({
        QPointF(0, 835), QPointF(320, 650), QPointF(580, 790),
        QPointF(900, 560), QPointF(1210, 810), QPointF(1450, 655),
        QPointF(1600, 760), QPointF(1600, 1000), QPointF(0, 1000)
    }));
    samplePainter.setBrush(QColor(20, 29, 50));
    samplePainter.drawRect(0, 865, 1600, 135);
    samplePainter.end();
    QVERIFY(sample.save(mediaPath, "PNG"));

    const QFont previousFont = qApp->font();
    qApp->setFont(QFont(QStringLiteral("Noto Sans CJK SC"), 10));

    auto captureWindow = [&](int language, ThemeManager::Theme theme,
                             const QString &stem) {
        // QSettings is already isolated in a temporary directory by initTestCase.
        QSettings settings;
        settings.clear();
        settings.setValue("language", language);
        settings.setValue("thumbnailsVisible", true);
        ThemeManager::instance().setTheme(theme);
        ThemeManager::instance().setBackgroundColor(theme == ThemeManager::Dark
            ? QColor(24, 24, 32) : QColor(250, 250, 252));
        MainWindow window;
        window.resize(1200, 800);
        window.openFile(mediaPath);
        window.show();
        QTest::qWait(300);
        QApplication::processEvents();

        // Assert actual visible text: a missing translation must fail the
        // capture instead of silently producing English screenshots for zh.
        QCOMPARE(window.menuBar()->actions().first()->text(),
                 language == 0 ? QStringLiteral("文件(&F)")
                               : QStringLiteral("&File"));
        auto *thumbnailView = window.findChild<ThumbnailBar *>()->findChild<QListView *>();
        QVERIFY(thumbnailView);
        QTRY_VERIFY_WITH_TIMEOUT(!thumbnailView->model()->index(0, 0)
            .data(Qt::DecorationRole).value<QIcon>().isNull(), 3000);
        // Transient machine-specific codec messages should not cover the
        // file information in documentation captures.
        window.statusBar()->clearMessage();
        window.menuBar()->setActiveAction(nullptr);
        QVERIFY(window.grab().save(QDir(outputDirectory).filePath(stem + ".png"), "PNG"));

        {
            QAction *viewAction = window.menuBar()->actions().at(1);
            QMenu *viewMenu = viewAction->menu();
            viewMenu->popup(window.mapToGlobal(
                QPoint(window.menuBar()->actionGeometry(viewAction).left(),
                       window.menuBar()->height())));
            QTest::qWait(80);
            QPixmap screenshot = window.grab();
            const QPixmap menu = viewMenu->grab();
            viewMenu->hide();

            QPainter painter(&screenshot);
            painter.drawPixmap(
                window.menuBar()->actionGeometry(viewAction).left(),
                window.menuBar()->height(), menu);
            painter.end();
            QVERIFY(screenshot.save(
                QDir(outputDirectory).filePath(stem + "-menu.png"), "PNG"));
        }
    };

    captureWindow(0, ThemeManager::Dark, QStringLiteral("flashview-zh-dark"));
    captureWindow(0, ThemeManager::Light, QStringLiteral("flashview-zh-light"));
    captureWindow(1, ThemeManager::Dark, QStringLiteral("flashview-en-dark"));
    captureWindow(1, ThemeManager::Light, QStringLiteral("flashview-en-light"));
    qApp->setFont(previousFont);
}

QTEST_MAIN(FlashViewTests)
#include "FlashViewTests.moc"
