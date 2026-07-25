#ifndef THUMBNAILBAR_H
#define THUMBNAILBAR_H

#include <QWidget>
#include <QListView>
#include <QStandardItemModel>
#include <QFutureWatcher>
#include <QImage>
#include <QDir>
#include <QPointer>

class QPropertyAnimation;

class ThumbnailBar : public QWidget
{
    Q_OBJECT

public:
    explicit ThumbnailBar(QWidget *parent = nullptr);

    void setDirectory(const QString &dirPath, const QStringList &fileNames);
    void setCurrentIndex(int index);
    void setVisible(bool visible);
    int currentIndex() const { return m_currentIndex; }

signals:
    void fileSelected(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void generateThumbnails();
    void smoothScrollToCenter(int index);

    QListView *m_listView = nullptr;
    QStandardItemModel *m_model = nullptr;
    QString m_dirPath;
    QStringList m_fileNames;
    int m_currentIndex = -1;
    QPointer<QPropertyAnimation> m_scrollAnim; // smooth centering animation
    QPointer<QFutureWatcher<QImage>> m_thumbnailWatcher;
    quint64 m_generation = 0;
};

#endif // THUMBNAILBAR_H
