#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include <QPointer>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QWindow;

class ImageViewer;
class VideoPlayer;

// Borderless preview overlay driven by a file manager. It shows one file at a
// time and leaves the browsing order to the caller, which is what keeps the
// arrow keys in step with the order the file manager itself displays.
class PreviewWindow : public QWidget
{
    Q_OBJECT

public:
    // Mirrors GtkDirectionType, the vocabulary of the file-manager protocol.
    enum Direction : uint { Up = 2, Down = 3, Left = 4, Right = 5 };

    explicit PreviewWindow(QWidget *parent = nullptr);
    ~PreviewWindow() override;

    // Returns false when the file is not a supported medium; the overlay then
    // explains itself instead of presenting an empty frame.
    bool showFile(const QString &filePath, const QString &windowHandle = QString());
    // Shows an explanation instead of media. The associated file, when given,
    // stays available so Enter can still hand it to another application.
    void showUnsupported(const QString &title, const QString &details,
                         const QString &associatedFile = QString());

    QString currentFile() const { return m_currentFile; }
    bool showsMedia() const;

signals:
    void closeRequested();
    void selectionRequested(uint direction);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupActions();
    void applyTheme();
    void applyTransientParent(const QString &handle);
    void resizeToContent(const QSize &content);
    void openInFullViewer();

    QStackedWidget *m_stack = nullptr;
    ImageViewer *m_imageViewer = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;
    QWidget *m_header = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    QString m_currentFile;
    QString m_parentHandle;
    QPointer<QWindow> m_foreignParent; // file-manager window we are transient to
    QPoint m_dragOrigin;
    bool m_dragging = false;
};

#endif // PREVIEWWINDOW_H
