#include "ImageViewer.h"
#include "MediaUtils.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QFont>
#include <QImageReader>
#include <QMovie>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QPixmapCache>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QSet>
#include <cmath>

namespace {
constexpr int    kFadeMs   = 180;   // image cross-in fade duration
constexpr int    kZoomMs   = 160;   // smooth zoom animation duration
constexpr double kMinScale = 0.02;  // zoom-out floor
constexpr double kMaxScale = 100.0; // zoom-in ceiling
constexpr int    kResampleDelayMs = 180;
constexpr qint64 kMaxSourcePixels = 24LL * 1024 * 1024;
constexpr qint64 kMaxResamplePixels = 16LL * 1024 * 1024;
}

ImageViewer::ImageViewer(QWidget *parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    connect(&ThemeManager::instance(), &ThemeManager::backgroundColorChanged,
            this, [this](const QColor &) { viewport()->update(); });

    m_resampleTimer = new QTimer(this);
    m_resampleTimer->setSingleShot(true);
    m_resampleTimer->setInterval(kResampleDelayMs);
    connect(m_resampleTimer, &QTimer::timeout,
            this, &ImageViewer::scheduleHighQualityResample);
}

bool ImageViewer::loadImage(const QString &filePath)
{
    QPixmap pm;
    QMovie *movie = nullptr;
    QImageReader animationReader(filePath);
    const bool isAnimated = animationReader.canRead() && animationReader.supportsAnimation();

    if (isAnimated) {
        movie = new QMovie(filePath, QByteArray(), this);
        movie->setCacheMode(QMovie::CacheAll);
        if (!movie->isValid() || !movie->jumpToFrame(0)
            || (pm = movie->currentPixmap()).isNull()) {
            const QString error = movie->lastErrorString();
            delete movie;
            clearImage(tr("This image could not be opened"), error);
            return false;
        }
    } else {
        // Use the shared pixmap cache: neighbours preloaded by MainWindow make
        // paging feel instant; a cache miss decodes on demand. Animated GIFs
        // bypass this cache because a QPixmap only represents one frame.
        const QString cacheKey = MediaUtils::imageCacheKey(filePath);
        if (!QPixmapCache::find(cacheKey, &pm)) {
            QImageReader reader(filePath);
            reader.setAutoTransform(true);
            QImage img = reader.read();
            if (img.isNull()) {
                clearImage(tr("This image could not be opened"), reader.errorString());
                return false;
            }
            pm = QPixmap::fromImage(img);
            QPixmapCache::insert(cacheKey, pm);
        }
    }

    if (m_zoomAnim) m_zoomAnim->stop();
    if (m_fadeAnim) m_fadeAnim->stop();
    if (m_movie) {
        m_movie->stop();
        delete m_movie;
    }
    scene()->clear();
    m_pixmapItem = scene()->addPixmap(pm);
    scene()->setSceneRect(m_pixmapItem->boundingRect());
    m_movie = movie;
    const qint64 imagePixels = qint64(pm.width()) * pm.height();
    m_sourceImage = !m_movie && imagePixels <= kMaxSourcePixels
        ? pm.toImage() : QImage();
    m_pixelArtCandidate = looksLikePixelArt(m_sourceImage);
    m_usingResampledPixmap = false;
    ++m_resampleGeneration;

    if (m_movie) {
        connect(m_movie, &QMovie::frameChanged, this, [this, movie](int) {
            if (m_movie != movie || !m_pixmapItem)
                return;
            const QPixmap frame = movie->currentPixmap();
            if (frame.isNull())
                return;
            const QRectF oldRect = m_pixmapItem->boundingRect();
            m_pixmapItem->setPixmap(frame);
            if (m_pixmapItem->boundingRect() != oldRect)
                scene()->setSceneRect(m_pixmapItem->boundingRect());
        });
        m_movie->start();
    }

    m_currentFile = filePath;
    m_hasImage = true;
    m_rotation = 0.0;
    m_scaleFactor = 1.0;
    m_fitMode = true;
    m_emptyTitle.clear();
    m_emptyDetails.clear();
    updateInterpolation();

    // Fade-in transition so switching images is a smooth cross-in, not a jump.
    // Cancel any in-flight fade first: on rapid paging, overlapping animations
    // would fight over the current item's opacity and cause visible flicker.
    m_pixmapItem->setOpacity(0.0);
    auto *fade = new QVariantAnimation(this);
    m_fadeAnim = fade;
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setDuration(kFadeMs);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        if (m_pixmapItem) m_pixmapItem->setOpacity(v.toDouble());
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    // Defer fitToWindow until widget has its final layout size
    QTimer::singleShot(0, this, [this] { fitToWindow(); });
    return true;
}

void ImageViewer::clearImage(const QString &title, const QString &details)
{
    if (m_zoomAnim) m_zoomAnim->stop();
    if (m_fadeAnim) m_fadeAnim->stop();
    if (m_movie) {
        m_movie->stop();
        delete m_movie;
    }
    if (m_resampleTimer) m_resampleTimer->stop();
    ++m_resampleGeneration;
    m_sourceImage = QImage();
    m_usingResampledPixmap = false;

    scene()->clear();
    m_pixmapItem = nullptr;
    m_hasImage = false;
    m_fitMode = true;
    m_currentFile.clear();
    m_scaleFactor = 1.0;
    m_rotation = 0.0;
    m_panning = false;
    m_emptyTitle = title;
    m_emptyDetails = details;

    resetTransform();
    scene()->setSceneRect(QRectF());
    updateCursor();
    viewport()->update();
}

void ImageViewer::zoomIn()
{
    m_fitMode = false;
    animateScaleTo(m_scaleFactor * 1.25);
}

void ImageViewer::setInterpolationMode(InterpolationMode mode)
{
    if (m_interpolationMode == mode)
        return;
    m_interpolationMode = mode;
    ++m_resampleGeneration;
    restoreSourcePixmap();
    updateInterpolation();
    if (m_hasImage && !m_movie && !useNearestNeighbor())
        m_resampleTimer->start();
}

void ImageViewer::zoomOut()
{
    m_fitMode = false;
    animateScaleTo(m_scaleFactor / 1.25);
}

void ImageViewer::fitToWindow()
{
    if (!m_hasImage) return;
    m_fitMode = true;
    applyFitTransform();
    updateCursor();
}

void ImageViewer::applyFitTransform()
{
    ++m_resampleGeneration;
    restoreSourcePixmap();
    resetTransform();
    rotate(m_rotation);
    fitInView(sceneRect(), Qt::KeepAspectRatio);
    // transform = rotation * scale; uniform scale magnitude = hypot(m11, m12)
    m_scaleFactor = std::hypot(transform().m11(), transform().m12());
    // Fitting may enlarge a small image. Keep the default view pixel-perfect;
    // users can still zoom beyond 100% explicitly.
    if (m_scaleFactor > 1.0) {
        resetTransform();
        rotate(m_rotation);
        m_scaleFactor = 1.0;
    }
    emit scaleChanged(m_scaleFactor);
    updateInterpolation();
    if (!m_movie && !useNearestNeighbor())
        m_resampleTimer->start();
}

void ImageViewer::actualSize()
{
    if (!m_hasImage) return;
    m_fitMode = false;
    animateScaleTo(1.0);
}

void ImageViewer::rotateLeft()
{
    m_rotation -= 90;
    if (m_rotation < 0) m_rotation += 360;
    if (m_fitMode) fitToWindow();
    else applyTransform();
}

void ImageViewer::rotateRight()
{
    m_rotation += 90;
    if (m_rotation >= 360) m_rotation -= 360;
    if (m_fitMode) fitToWindow();
    else applyTransform();
}

double ImageViewer::currentScale() const
{
    return m_scaleFactor;
}

QSize ImageViewer::imageSize() const
{
    if (!m_hasImage || !m_pixmapItem) return {};
    if (!m_sourceImage.isNull())
        return m_sourceImage.size();
    return m_pixmapItem->pixmap().size();
}

void ImageViewer::applyTransform()
{
    ++m_resampleGeneration;
    restoreSourcePixmap();
    resetTransform();
    rotate(m_rotation);
    scale(m_scaleFactor, m_scaleFactor);
    emit scaleChanged(m_scaleFactor);
    updateInterpolation();
    if (!m_movie && !useNearestNeighbor())
        m_resampleTimer->start();
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) || m_wheelZoomEnabled) {
        // Zoom centred on the cursor for a natural, direct-manipulation feel.
        if (!m_hasImage) {
            event->ignore();
            return;
        }
        m_fitMode = false;
        if (m_zoomAnim) m_zoomAnim->stop();
        double steps = event->angleDelta().y() / 120.0;
        if (qFuzzyIsNull(steps))
            steps = event->pixelDelta().y() / 80.0;
        if (qFuzzyIsNull(steps)) {
            event->ignore();
            return;
        }
        const double target = qBound(kMinScale, m_scaleFactor * std::pow(1.25, steps), kMaxScale);
        const double factor = target / m_scaleFactor;
        ++m_resampleGeneration;
        restoreSourcePixmap();
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        scale(factor, factor);
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        m_scaleFactor = target;
        emit scaleChanged(m_scaleFactor);
        updateInterpolation();
        if (!m_movie && !useNearestNeighbor())
            m_resampleTimer->start();
        updateCursor();
        event->accept();
        return;
    }

    // Check if image is scrollable (larger than viewport)
    bool canScrollH = horizontalScrollBar()->maximum() > 0;
    bool canScrollV = verticalScrollBar()->maximum() > 0;

    if (canScrollH || canScrollV) {
        // Image is larger than viewport: scroll within image
        QGraphicsView::wheelEvent(event);
    } else {
        // Image fits in viewport: let parent handle page navigation
        event->ignore();
    }
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    // Mouse side buttons page like a browser's back/forward.
    if (event->button() == Qt::BackButton) { emit navigatePrev(); event->accept(); return; }
    if (event->button() == Qt::ForwardButton) { emit navigateNext(); event->accept(); return; }
    if (event->button() == Qt::LeftButton && isPannable()) {
        m_panning = true;
        m_panStartPos = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_panStartPos;
        m_panStartPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        updateCursor();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_hasImage && event->button() == Qt::LeftButton) {
        // Toggle between fit-to-window and 100% actual size.
        if (m_fitMode) { m_fitMode = false; animateScaleTo(1.0); }
        else fitToWindow();
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    // Keep the image fitted while the window is being resized/maximised.
    if (m_hasImage && m_fitMode)
        applyFitTransform();
    updateCursor();
}

void ImageViewer::animateScaleTo(double target)
{
    if (!m_hasImage) return;
    target = qBound(kMinScale, target, kMaxScale);
    if (m_zoomAnim) m_zoomAnim->stop();
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    auto *anim = new QVariantAnimation(this);
    m_zoomAnim = anim;
    anim->setStartValue(m_scaleFactor);
    anim->setEndValue(target);
    anim->setDuration(kZoomMs);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_scaleFactor = v.toDouble();
        applyTransform();
    });
    connect(anim, &QVariantAnimation::finished, this, [this] { updateCursor(); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ImageViewer::updateCursor()
{
    if (!m_hasImage) { viewport()->setCursor(Qt::ArrowCursor); return; }
    viewport()->setCursor(isPannable() ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

bool ImageViewer::useNearestNeighbor() const
{
    return m_interpolationMode == InterpolationMode::Nearest
        || (m_interpolationMode == InterpolationMode::Auto
            && m_scaleFactor > 1.0 && m_pixelArtCandidate);
}

void ImageViewer::updateInterpolation()
{
    setRenderHint(QPainter::SmoothPixmapTransform, !useNearestNeighbor());
    if (m_pixmapItem) {
        m_pixmapItem->setTransformationMode(useNearestNeighbor()
            ? Qt::FastTransformation : Qt::SmoothTransformation);
    }
    if (useNearestNeighbor()) {
        if (m_resampleTimer) m_resampleTimer->stop();
        ++m_resampleGeneration;
        restoreSourcePixmap();
    }
    viewport()->update();
}

void ImageViewer::restoreSourcePixmap()
{
    if (!m_usingResampledPixmap || !m_pixmapItem || m_sourceImage.isNull())
        return;
    m_pixmapItem->setPixmap(QPixmap::fromImage(m_sourceImage));
    m_pixmapItem->setScale(1.0);
    scene()->setSceneRect(m_pixmapItem->boundingRect());
    m_usingResampledPixmap = false;
}

bool ImageViewer::looksLikePixelArt(const QImage &image)
{
    if (image.isNull() || image.width() > 512 || image.height() > 512)
        return false;
    const QImage sample = image.scaled(128, 128, Qt::KeepAspectRatio,
                                       Qt::FastTransformation)
                              .convertToFormat(QImage::Format_ARGB32);
    QSet<QRgb> colors;
    for (int y = 0; y < sample.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(sample.constScanLine(y));
        for (int x = 0; x < sample.width(); ++x) {
            colors.insert(line[x]);
            if (colors.size() > 256)
                return false;
        }
    }
    return true;
}

void ImageViewer::scheduleHighQualityResample()
{
    if (!m_hasImage || m_movie || m_sourceImage.isNull() || useNearestNeighbor())
        return;

    const double deviceScale = viewport()->devicePixelRatioF();
    double requestedScale = m_scaleFactor * deviceScale;
    if (qAbs(requestedScale - 1.0) < 0.08) {
        restoreSourcePixmap();
        return;
    }
    const qint64 sourcePixels = qint64(m_sourceImage.width()) * m_sourceImage.height();
    if (sourcePixels <= 0)
        return;
    requestedScale = qMin(requestedScale,
                          std::sqrt(double(kMaxResamplePixels) / sourcePixels));
    if (requestedScale <= 0.0)
        return;

    const QSize targetSize(qMax(1, qRound(m_sourceImage.width() * requestedScale)),
                           qMax(1, qRound(m_sourceImage.height() * requestedScale)));
    const QImage source = m_sourceImage;
    const quint64 generation = ++m_resampleGeneration;
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [this, watcher, generation, targetSize] {
        const QImage result = watcher->result();
        watcher->deleteLater();
        if (generation != m_resampleGeneration || result.isNull()
            || !m_pixmapItem || m_movie || useNearestNeighbor())
            return;
        QPixmap pixmap = QPixmap::fromImage(result);
        m_pixmapItem->setPixmap(pixmap);
        const double itemScale = double(m_sourceImage.width()) / targetSize.width();
        m_pixmapItem->setScale(itemScale);
        scene()->setSceneRect(m_pixmapItem->mapRectToScene(m_pixmapItem->boundingRect()));
        m_usingResampledPixmap = true;
        viewport()->update();
    });
    watcher->setFuture(QtConcurrent::run([source, targetSize] {
        return source.scaled(targetSize, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }));
}

bool ImageViewer::isPannable() const
{
    return horizontalScrollBar()->maximum() > 0 || verticalScrollBar()->maximum() > 0;
}

void ImageViewer::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, ThemeManager::instance().backgroundColor());
}

void ImageViewer::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (m_hasImage) return;

    // Empty-state guidance, drawn in viewport (device) coordinates so it stays
    // centred regardless of scene transform.
    painter->save();
    painter->resetTransform();
    const bool dark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;
    const QRect viewportRect = viewport()->rect();
    const QPoint center = viewportRect.center();
    const QColor accent(74, 128, 224);
    const QColor primary = dark ? QColor(205, 205, 222) : QColor(62, 62, 78);
    const QColor secondary = dark ? QColor(125, 125, 152) : QColor(130, 130, 148);

    // A small code-drawn media card keeps the empty state useful without
    // requiring a raster asset and scales cleanly on HiDPI displays.
    QRectF card(center.x() - 34, center.y() - 90, 68, 58);
    painter->setPen(QPen(accent, 2));
    painter->setBrush(dark ? QColor(35, 40, 58) : QColor(238, 243, 255));
    painter->drawRoundedRect(card, 10, 10);
    painter->drawEllipse(QPointF(card.left() + 19, card.top() + 18), 5, 5);
    QPainterPath mountains;
    mountains.moveTo(card.left() + 10, card.bottom() - 10);
    mountains.lineTo(card.left() + 27, card.top() + 31);
    mountains.lineTo(card.left() + 38, card.bottom() - 18);
    mountains.lineTo(card.left() + 48, card.top() + 27);
    mountains.lineTo(card.right() - 9, card.bottom() - 10);
    painter->drawPath(mountains);

    QFont headingFont = painter->font();
    headingFont.setPointSize(15);
    headingFont.setWeight(QFont::DemiBold);
    painter->setFont(headingFont);
    painter->setPen(primary);
    const QString title = m_emptyTitle.isEmpty()
        ? tr("Drop an image or video here")
        : m_emptyTitle;
    painter->drawText(QRect(24, center.y() - 18, viewportRect.width() - 48, 32),
                      Qt::AlignHCenter | Qt::AlignVCenter, title);

    QFont detailFont = painter->font();
    detailFont.setPointSize(10);
    detailFont.setWeight(QFont::Normal);
    painter->setFont(detailFont);
    painter->setPen(secondary);
    const QString details = m_emptyDetails.isEmpty()
        ? tr("or press Ctrl+O to choose a media file")
        : m_emptyDetails;
    painter->drawText(QRect(36, center.y() + 17, viewportRect.width() - 72, 48),
                      Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, details);
    painter->restore();
}
