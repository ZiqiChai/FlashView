#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

// A slider where clicking anywhere on the groove jumps the handle straight
// to that position, instead of QSlider's default single-step page behavior.
class ClickableSlider : public QSlider
{
    Q_OBJECT

public:
    explicit ClickableSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int valueForPosition(const QPoint &pos) const;
};

// A flat, borderless round button that paints its own glyph (play, pause,
// speaker, mute) instead of relying on text/emoji, which render off-center
// or get clipped inside a fixed-size circular button.
class IconButton : public QPushButton
{
    Q_OBJECT

public:
    enum class Icon { Play, Pause, VolumeHigh, VolumeMuted };

    explicit IconButton(QWidget *parent = nullptr);

    void setIcon(Icon icon);
    void setAccentColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Icon m_icon = Icon::Play;
    QColor m_iconColor = QColor(230, 230, 236);
};

class VideoPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer();

    void loadVideo(const QString &filePath);
    void stop();
    void togglePlayback();
    bool isPlaying() const;
    bool hasVideo() const { return !m_currentFile.isEmpty(); }
    QString currentFile() const { return m_currentFile; }
    void retranslateUi();

signals:
    void playbackError(const QString &message);

protected:
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void playPause();
    void seekTo(int position);
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void updatePlaybackState(QMediaPlayer::PlaybackState state);

private:
    void ensurePlayerReady();
    void applyControlsTheme();
    QString formatTime(qint64 ms) const;

    QVBoxLayout *m_layout = nullptr;
    QLabel *m_placeholder = nullptr;
    QMediaPlayer *m_player = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QWidget *m_controlsWidget = nullptr;
    ClickableSlider *m_seekSlider = nullptr;
    IconButton *m_playBtn = nullptr;
    IconButton *m_muteBtn = nullptr;
    QLabel *m_timeLabel = nullptr;
    ClickableSlider *m_volumeSlider = nullptr;
    QString m_currentFile;
    bool m_seeking = false;
};

#endif // VIDEOPLAYER_H
