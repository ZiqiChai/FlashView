#include "ImageViewer.h"
#include "MainWindow.h"
#include "MediaUtils.h"
#include "ThumbnailBar.h"

#include <QFile>
#include <QImage>
#include <QListView>
#include <QSettings>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>

class FlashViewTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void recognizesFormatsAndUsesNaturalOrder();
    void imageViewerClearsStaleContentOnDecodeFailure();
    void wheelZoomSettingWorksInsideTheViewer();
    void thumbnailsLoadProgressively();
    void mainWindowTracksFolderChanges();

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
}

void FlashViewTests::recognizesFormatsAndUsesNaturalOrder()
{
    QVERIFY(MediaUtils::isImageFile(QStringLiteral("holiday.JpG")));
    QVERIFY(MediaUtils::isVideoFile(QStringLiteral("clip.WEBM")));
    QVERIFY(!MediaUtils::isSupportedFile(QStringLiteral("notes.txt")));

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

QTEST_MAIN(FlashViewTests)
#include "FlashViewTests.moc"
