#include "VideoPlayer.h"
#include "ThemeManager.h"
#include <QSettings>
#include <QWheelEvent>

VideoPlayer::VideoPlayer(QWidget *parent) : QWidget(parent)
{
    // Only create the placeholder; actual video widgets created lazily
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_layout = layout;

    // Placeholder label shown until a video is loaded
    m_placeholder = new QLabel(this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setStyleSheet("background-color: black; color: #555; font-size: 14px;");
    retranslateUi();
    layout->addWidget(m_placeholder, 1);
}

void VideoPlayer::retranslateUi()
{
    if (m_placeholder)
        m_placeholder->setText(QString::fromUtf8("\xe2\x96\xb6  ") + tr("No video loaded"));
    if (m_playBtn) {
        m_playBtn->setToolTip(tr("Play / Pause"));
        m_playBtn->setAccessibleName(tr("Play / Pause"));
    }
    if (m_muteBtn) {
        m_muteBtn->setToolTip(tr("Mute / Unmute"));
        m_muteBtn->setAccessibleName(tr("Mute / Unmute"));
    }
    if (m_seekSlider)
        m_seekSlider->setAccessibleName(tr("Playback position"));
    if (m_volumeSlider)
        m_volumeSlider->setAccessibleName(tr("Volume"));
}

VideoPlayer::~VideoPlayer()
{
    stop();
}

void VideoPlayer::ensurePlayerReady()
{
    if (m_player) return;

    // Remove placeholder
    if (m_placeholder) {
        m_layout->removeWidget(m_placeholder);
        delete m_placeholder;
        m_placeholder = nullptr;
    }

    // Video widget
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setStyleSheet("background-color: black;");
    m_layout->insertWidget(0, m_videoWidget, 1);

    // Controls bar
    auto *controlsWidget = new QWidget(this);
    controlsWidget->setFixedHeight(48);
    controlsWidget->setObjectName("videoControls");
    auto *controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(12, 4, 12, 4);
    controlsLayout->setSpacing(8);

    m_playBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xb6"), this);
    m_playBtn->setFixedSize(36, 36);
    m_playBtn->setStyleSheet("QPushButton { font-size: 16px; border-radius: 18px; }");
    controlsLayout->addWidget(m_playBtn);

    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setStyleSheet("QLabel { font-size: 12px; font-family: monospace; min-width: 100px; }");
    controlsLayout->addWidget(m_timeLabel);

    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 0);
    m_seekSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height: 4px; border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }
    )");
    controlsLayout->addWidget(m_seekSlider, 1);

    m_muteBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8a"), this);
    m_muteBtn->setFixedSize(36, 36);
    m_muteBtn->setStyleSheet("QPushButton { font-size: 14px; border-radius: 18px; }");
    controlsLayout->addWidget(m_muteBtn);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    QSettings settings;
    const int savedVolume = settings.value(QStringLiteral("videoVolume"), 70).toInt();
    const bool savedMuted = settings.value(QStringLiteral("videoMuted"), false).toBool();
    m_volumeSlider->setValue(qBound(0, savedVolume, 100));
    m_volumeSlider->setFixedWidth(80);
    controlsLayout->addWidget(m_volumeSlider);

    m_layout->addWidget(controlsWidget);

    // Media player
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(m_volumeSlider->value() / 100.0);
    m_audioOutput->setMuted(savedMuted);
    m_muteBtn->setText(savedMuted
        ? QString::fromUtf8("\xf0\x9f\x94\x87")
        : QString::fromUtf8("\xf0\x9f\x94\x8a"));
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    // Connections
    connect(m_playBtn, &QPushButton::clicked, this, &VideoPlayer::playPause);
    connect(m_seekSlider, &QSlider::sliderMoved, this, &VideoPlayer::seekTo);
    connect(m_seekSlider, &QSlider::sliderPressed, this, [this]() { m_seeking = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this]() {
        m_seeking = false;
        seekTo(m_seekSlider->value());
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayer::updatePosition);
    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPlayer::updateDuration);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayer::mediaStatusChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &VideoPlayer::updatePlaybackState);
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &errorText) {
        const QString message = errorText.isEmpty()
            ? tr("The video could not be played")
            : errorText;
        emit playbackError(message);
    });

    connect(m_muteBtn, &QPushButton::clicked, this, [this]() {
        if (!m_audioOutput) return;
        m_audioOutput->setMuted(!m_audioOutput->isMuted());
        QSettings().setValue(QStringLiteral("videoMuted"), m_audioOutput->isMuted());
        m_muteBtn->setText(m_audioOutput->isMuted()
            ? QString::fromUtf8("\xf0\x9f\x94\x87")
            : QString::fromUtf8("\xf0\x9f\x94\x8a"));
    });
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_audioOutput)
            m_audioOutput->setVolume(v / 100.0);
        QSettings().setValue(QStringLiteral("videoVolume"), v);
    });
    retranslateUi();
}

void VideoPlayer::loadVideo(const QString &filePath)
{
    ensurePlayerReady();
    m_currentFile = filePath;
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
}

void VideoPlayer::stop()
{
    if (m_player) m_player->stop();
    if (m_playBtn) m_playBtn->setText(QString::fromUtf8("\xe2\x96\xb6"));
}

bool VideoPlayer::isPlaying() const
{
    return m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
}

void VideoPlayer::togglePlayback()
{
    playPause();
}

void VideoPlayer::playPause()
{
    if (!m_player) return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void VideoPlayer::seekTo(int position)
{
    if (m_player) m_player->setPosition(position);
}

void VideoPlayer::updatePosition(qint64 position)
{
    if (!m_player) return;
    if (!m_seeking)
        m_seekSlider->setValue(static_cast<int>(position));
    m_timeLabel->setText(formatTime(position) + " / " + formatTime(m_player->duration()));
}

void VideoPlayer::updateDuration(qint64 duration)
{
    m_seekSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayer::mediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        m_playBtn->setText(QString::fromUtf8("\xe2\x96\xb6"));
    }
}

void VideoPlayer::updatePlaybackState(QMediaPlayer::PlaybackState state)
{
    if (!m_playBtn)
        return;
    m_playBtn->setText(state == QMediaPlayer::PlayingState
        ? QString::fromUtf8("\xe2\x8f\xb8")
        : QString::fromUtf8("\xe2\x96\xb6"));
}

void VideoPlayer::wheelEvent(QWheelEvent *event)
{
    if (!m_volumeSlider || !m_player) {
        event->ignore();
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        if (m_volumeSlider) {
            int delta = event->angleDelta().y() / 120;
            int newVol = qBound(0, m_volumeSlider->value() + delta * 5, 100);
            m_volumeSlider->setValue(newVol);
        }
        event->accept();
        return;
    }

    // Normal wheel gestures belong to the window's next/previous navigation.
    event->ignore();
}

QString VideoPlayer::formatTime(qint64 ms) const
{
    int s = static_cast<int>(ms / 1000);
    int m = s / 60;
    s %= 60;
    int h = m / 60;
    m %= 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}
