#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QPointer>
#include <QImage>

class QVariantAnimation;
class QMovie;

class ImageViewer : public QGraphicsView
{
    Q_OBJECT

public:
    enum class InterpolationMode { Auto = 0, Smooth = 1, Nearest = 2 };
    Q_ENUM(InterpolationMode)

    explicit ImageViewer(QWidget *parent = nullptr);

    bool loadImage(const QString &filePath);
    void clearImage(const QString &title = QString(), const QString &details = QString());
    void setWheelZoomEnabled(bool enabled) { m_wheelZoomEnabled = enabled; }
    void setInterpolationMode(InterpolationMode mode);
    InterpolationMode interpolationMode() const { return m_interpolationMode; }
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void actualSize();
    void rotateLeft();
    void rotateRight();

    bool hasImage() const { return m_hasImage; }
    QString currentFile() const { return m_currentFile; }
    double currentScale() const;
    QSize imageSize() const;

signals:
    void scaleChanged(double scale);
    void navigateNext();
    void navigatePrev();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    void applyTransform();
    void applyFitTransform();
    void animateScaleTo(double target);
    void updateInterpolation();
    void scheduleHighQualityResample();
    void restoreSourcePixmap();
    bool useNearestNeighbor() const;
    static bool looksLikePixelArt(const QImage &image);
    void updateCursor();
    bool isPannable() const;

    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    bool m_hasImage = false;
    bool m_fitMode = true;             // auto re-fit on window resize
    QString m_currentFile;
    double m_scaleFactor = 1.0;
    double m_rotation = 0.0;
    bool m_panning = false;
    bool m_wheelZoomEnabled = false;
    QPoint m_panStartPos;
    QString m_emptyTitle;
    QString m_emptyDetails;
    QPointer<QVariantAnimation> m_zoomAnim;  // smooth zoom animation
    QPointer<QVariantAnimation> m_fadeAnim;  // image cross-in fade
    QPointer<QMovie> m_movie;                // animated GIF playback
    QImage m_sourceImage;
    InterpolationMode m_interpolationMode = InterpolationMode::Auto;
    QTimer *m_resampleTimer = nullptr;
    quint64 m_resampleGeneration = 0;
    bool m_pixelArtCandidate = false;
    bool m_usingResampledPixmap = false;
};

#endif // IMAGEVIEWER_H
