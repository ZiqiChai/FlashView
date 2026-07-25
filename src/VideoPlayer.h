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
    QString formatTime(qint64 ms) const;

    QVBoxLayout *m_layout = nullptr;
    QLabel *m_placeholder = nullptr;
    QMediaPlayer *m_player = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QSlider *m_seekSlider = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_muteBtn = nullptr;
    QLabel *m_timeLabel = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QString m_currentFile;
    bool m_seeking = false;
};

#endif // VIDEOPLAYER_H
