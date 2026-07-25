#include "ThumbnailBar.h"
#include "MediaUtils.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QImageReader>
#include <QPainter>
#include <QFileInfo>
#include <QStyledItemDelegate>
#include <QScrollBar>
#include <QWheelEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QtConcurrent>

const int THUMB_SIZE = 80;

class ThumbnailDelegate : public QStyledItemDelegate
{
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        QRect r = option.rect.adjusted(4, 4, -4, -4);

        // Selection highlight
        if (option.state & QStyle::State_Selected) {
            painter->setPen(QPen(QColor(74, 128, 224), 2));
            QColor highlight = ThemeManager::instance().currentTheme() == ThemeManager::Dark
                ? QColor(46, 46, 72, 180) : QColor(238, 242, 255, 200);
            painter->setBrush(highlight);
            painter->drawRoundedRect(r.adjusted(-2, -2, 2, 2), 8, 8);
        } else if (option.state & QStyle::State_MouseOver) {
            painter->setPen(Qt::NoPen);
            QColor hover = ThemeManager::instance().currentTheme() == ThemeManager::Dark
                ? QColor(37, 37, 56, 120) : QColor(245, 245, 250, 160);
            painter->setBrush(hover);
            painter->drawRoundedRect(r.adjusted(-2, -2, 2, 2), 8, 8);
        }

        // Thumbnail
        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            // Draw pixmap at its natural size, centered in the cell
            QList<QSize> sizes = icon.availableSizes();
            QPixmap pm = sizes.isEmpty()
                ? icon.pixmap(r.size())
                : icon.pixmap(sizes.first());
            // Only scale down if larger than cell, always keep aspect ratio
            if (pm.width() > r.width() || pm.height() > r.height())
                pm = pm.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QRect pmRect(QPoint(0, 0), pm.size());
            pmRect.moveCenter(r.center());
            painter->drawPixmap(pmRect, pm);
        } else {
            // Placeholder for video or unsupported
            painter->setPen(Qt::NoPen);
            QColor bg = ThemeManager::instance().currentTheme() == ThemeManager::Dark
                ? QColor(34, 34, 54) : QColor(240, 240, 246);
            painter->setBrush(bg);
            painter->drawRoundedRect(r, 8, 8);
            painter->setPen(ThemeManager::instance().currentTheme() == ThemeManager::Dark
                ? QColor(100, 100, 140) : QColor(160, 160, 180));
            painter->setFont(QFont("", 9));
            QString name = index.data(Qt::DisplayRole).toString();
            painter->drawText(r, Qt::AlignCenter, name.length() > 8 ? name.left(8) + "…" : name);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(THUMB_SIZE + 16, THUMB_SIZE + 16);
    }
};

ThumbnailBar::ThumbnailBar(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(0);

    m_model = new QStandardItemModel(this);
    m_listView = new QListView(this);
    m_listView->setObjectName("ThumbnailListView");
    m_listView->setModel(m_model);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setWrapping(false);
    m_listView->setMovement(QListView::Static);
    m_listView->setIconSize(QSize(THUMB_SIZE, THUMB_SIZE));
    m_listView->setFixedHeight(THUMB_SIZE + 28);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Per-pixel scrolling gives smooth, non-jumpy horizontal motion.
    m_listView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setItemDelegate(new ThumbnailDelegate);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setMouseTracking(true);
    // Route wheel events to horizontal scrolling (see eventFilter).
    m_listView->viewport()->installEventFilter(this);

    // Apply background based on theme
    auto &tm = ThemeManager::instance();
    QString bg = tm.currentTheme() == ThemeManager::Dark ? "#1c1c28" : "#ffffff";
    QString border = tm.currentTheme() == ThemeManager::Dark ? "#2a2a3c" : "#eaeaf0";
    m_listView->setStyleSheet(QString(
        "QListView#ThumbnailListView { background: %1; border-top: 1px solid %2; }")
        .arg(bg, border));

    connect(&tm, &ThemeManager::themeChanged, this, [this](ThemeManager::Theme t) {
        QString bg = t == ThemeManager::Dark ? "#1c1c28" : "#ffffff";
        QString border = t == ThemeManager::Dark ? "#2a2a3c" : "#eaeaf0";
        m_listView->setStyleSheet(QString(
            "QListView#ThumbnailListView { background: %1; border-top: 1px solid %2; }")
            .arg(bg, border));
    });

    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex &index) {
        m_currentIndex = index.row();
        emit fileSelected(m_currentIndex);
    });

    layout->addWidget(m_listView);
}

void ThumbnailBar::setDirectory(const QString &dirPath, const QStringList &fileNames)
{
    m_dirPath = dirPath;
    m_fileNames = fileNames;
    m_currentIndex = -1;
    generateThumbnails();
}

void ThumbnailBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_model->rowCount()) return;
    m_currentIndex = index;
    m_listView->setCurrentIndex(m_model->index(index, 0));
    smoothScrollToCenter(index);
}

void ThumbnailBar::smoothScrollToCenter(int index)
{
    const QModelIndex mi = m_model->index(index, 0);
    const QRect vr = m_listView->visualRect(mi);
    QScrollBar *sb = m_listView->horizontalScrollBar();

    // If the item hasn't been laid out yet (far offscreen), jump directly.
    if (vr.width() <= 0) {
        m_listView->scrollTo(mi, QAbstractItemView::PositionAtCenter);
        return;
    }

    const int itemCenter = sb->value() + vr.center().x();
    int target = itemCenter - m_listView->viewport()->width() / 2;
    target = qBound(sb->minimum(), target, sb->maximum());

    if (target == sb->value()) return;

    if (m_scrollAnim) m_scrollAnim->stop();
    auto *anim = new QPropertyAnimation(sb, "value", this);
    m_scrollAnim = anim;
    anim->setStartValue(sb->value());
    anim->setEndValue(target);
    anim->setDuration(220);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ThumbnailBar::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    if (visible && m_model->rowCount() > 0 && m_currentIndex >= 0) {
        m_listView->scrollTo(m_model->index(m_currentIndex, 0), QAbstractItemView::PositionAtCenter);
    }
}

void ThumbnailBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

bool ThumbnailBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_listView->viewport() && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(event);
        // Map vertical wheel to smooth horizontal scrolling; keep native
        // behaviour for genuine horizontal wheels/trackpad gestures.
        const int dy = we->angleDelta().y();
        const int dx = we->angleDelta().x();
        if (dy != 0 && dx == 0) {
            QScrollBar *sb = m_listView->horizontalScrollBar();
            // Only consume the wheel when there is actually room to scroll;
            // otherwise let it fall through to window-level page navigation.
            if (sb->maximum() > 0) {
                sb->setValue(sb->value() - dy);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ThumbnailBar::generateThumbnails()
{
    ++m_generation;
    if (m_thumbnailWatcher) {
        disconnect(m_thumbnailWatcher, nullptr, this, nullptr);
        m_thumbnailWatcher->cancel();
        m_thumbnailWatcher->deleteLater();
        m_thumbnailWatcher = nullptr;
    }

    m_model->clear();
    if (m_fileNames.isEmpty()) return;

    QStringList imagePaths;
    imagePaths.reserve(m_fileNames.size());

    for (const QString &fileName : m_fileNames) {
        auto *item = new QStandardItem();
        item->setEditable(false);
        item->setText(fileName);
        item->setToolTip(fileName);
        m_model->appendRow(item);

        const QString absolutePath = QDir(m_dirPath).absoluteFilePath(fileName);
        imagePaths.append(MediaUtils::isImageFile(fileName) ? absolutePath : QString());
    }

    // Decode thumbnails away from the GUI thread. Results are applied one by
    // one, so a large folder appears progressively and remains interactive.
    const quint64 generation = m_generation;
    auto future = QtConcurrent::mapped(imagePaths, [](const QString &filePath) {
        if (filePath.isEmpty())
            return QImage();

        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        const QSize originalSize = reader.size();
        if (originalSize.isValid()) {
            const int maxDimension = THUMB_SIZE * 2;
            if (originalSize.width() > maxDimension || originalSize.height() > maxDimension) {
                QSize decodeSize = originalSize;
                decodeSize.scale(maxDimension, maxDimension, Qt::KeepAspectRatio);
                reader.setScaledSize(decodeSize);
            }
        }

        const QImage image = reader.read();
        if (image.isNull())
            return QImage();
        return image.scaled(QSize(THUMB_SIZE, THUMB_SIZE),
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
    });

    auto *watcher = new QFutureWatcher<QImage>(this);
    m_thumbnailWatcher = watcher;
    connect(watcher, &QFutureWatcher<QImage>::resultReadyAt, this,
            [this, watcher, generation](int index) {
        if (generation != m_generation || watcher != m_thumbnailWatcher)
            return;
        QStandardItem *item = m_model->item(index);
        if (!item)
            return;
        const QImage image = watcher->resultAt(index);
        if (!image.isNull())
            item->setData(QIcon(QPixmap::fromImage(image)), Qt::DecorationRole);
    });
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher] {
        if (watcher == m_thumbnailWatcher)
            m_thumbnailWatcher = nullptr;
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}
