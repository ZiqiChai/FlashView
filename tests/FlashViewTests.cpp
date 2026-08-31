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
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
#include <QWheelEvent>

class FlashViewTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void recognizesFormatsAndUsesNaturalOrder();
    void imageViewerClearsStaleContentOnDecodeFailure();
    void animatedGifAdvancesFrames();
    void fitToWindowNeverUpscalesSmallImages();
    void wheelZoomSettingWorksInsideTheViewer();
    void thumbnailsLoadProgressively();
    void mainWindowTracksFolderChanges();
    void toolbarActionsHaveClearVisualSemantics();
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

    auto captureWindow = [&](ThemeManager::Theme theme, const QString &fileName,
                             bool captureViewMenu) {
        ThemeManager::instance().setTheme(theme);
        MainWindow window;
        window.resize(1200, 800);
        window.openFile(mediaPath);
        window.show();
        QTest::qWait(300);
        QApplication::processEvents();

        QPixmap screenshot = window.grab();
        if (captureViewMenu) {
            QAction *viewAction = window.menuBar()->actions().at(1);
            QMenu *viewMenu = viewAction->menu();
            viewMenu->popup(window.mapToGlobal(
                QPoint(window.menuBar()->actionGeometry(viewAction).left(),
                       window.menuBar()->height())));
            QTest::qWait(80);
            const QPixmap menu = viewMenu->grab();
            viewMenu->hide();

            QPainter painter(&screenshot);
            painter.drawPixmap(
                window.menuBar()->actionGeometry(viewAction).left(),
                window.menuBar()->height(), menu);
        }

        QVERIFY(screenshot.save(
            QDir(outputDirectory).filePath(fileName), "PNG"));
    };

    captureWindow(ThemeManager::Dark,
                  QStringLiteral("flashview-dark.png"), false);
    captureWindow(ThemeManager::Light,
                  QStringLiteral("flashview-light.png"), false);
    captureWindow(ThemeManager::Dark,
                  QStringLiteral("flashview-view-menu.png"), true);
}

QTEST_MAIN(FlashViewTests)
#include "FlashViewTests.moc"
