#include "VideoPlayer.h"
#include "ThemeManager.h"
#include <QSettings>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionSlider>
#include <utility>

// ── ClickableSlider ──────────────────────────────────────────────────────

ClickableSlider::ClickableSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
}

int ClickableSlider::valueForPosition(const QPoint &pos) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    if (orientation() == Qt::Horizontal) {
        const int span = groove.width() - handle.width();
        const int offset = pos.x() - groove.left() - handle.width() / 2;
        return QStyle::sliderValueFromPosition(minimum(), maximum(), offset, span, 0);
    }
    const int span = groove.height() - handle.height();
    const int offset = pos.y() - groove.top() - handle.height() / 2;
    return QStyle::sliderValueFromPosition(minimum(), maximum(), offset, span, 0);
}

void ClickableSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QSlider::mousePressEvent(event);
        return;
    }

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    // Clicking the handle itself keeps the normal drag behavior.
    if (handle.contains(event->pos())) {
        QSlider::mousePressEvent(event);
        return;
    }

    const int newValue = valueForPosition(event->pos());
    setSliderPosition(newValue);
    setValue(newValue);
    emit sliderMoved(newValue);
    event->accept();
}

// ── IconButton ───────────────────────────────────────────────────────────

IconButton::IconButton(QWidget *parent) : QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
}

void IconButton::setIcon(Icon icon)
{
    if (m_icon == icon) return;
    m_icon = icon;
    update();
}

void IconButton::setAccentColor(const QColor &color)
{
    if (m_iconColor == color) return;
    m_iconColor = color;
    update();
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect();
    const qreal side = qMin(r.width(), r.height());
    const QPointF center = r.center();

    // Subtle circular hover/pressed feedback, no accent color. Pressed
    // shrinks the highlight slightly so the click reads as a tap, not just
    // a color swap.
    if (isDown()) {
        p.setBrush(QColor(m_iconColor.red(), m_iconColor.green(), m_iconColor.blue(), 40));
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, side * 0.46, side * 0.46);
    } else if (underMouse()) {
        p.setBrush(QColor(m_iconColor.red(), m_iconColor.green(), m_iconColor.blue(), 22));
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, side / 2.0, side / 2.0);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(m_iconColor);

    // Half-extent of the tallest glyph, sized so every icon (including the
    // asymmetric speaker, which is wider than it is tall) fits inside the
    // circle with clear padding on every side.
    const qreal glyph = side * 0.23;
    const qreal corner = glyph * 0.16; // shared rounding radius, keeps every glyph looking hand-drawn rather than geometric

    switch (m_icon) {
    case Icon::Play: {
        // A rounded-corner triangle reads as a soft, deliberate glyph
        // instead of a sharp geometric wedge.
        QPainterPath path;
        const QPointF top(center.x() - glyph * 0.6, center.y() - glyph * 1.05);
        const QPointF bottom(center.x() - glyph * 0.6, center.y() + glyph * 1.05);
        const QPointF tip(center.x() + glyph * 0.95, center.y());

        auto roundedCorner = [&](const QPointF &a, const QPointF &b, const QPointF &c) {
            QLineF toA(b, a);
            QLineF toC(b, c);
            toA.setLength(corner);
            toC.setLength(corner);
            return std::make_pair(toA.p2(), toC.p2());
        };

        const auto [topIn, topOut] = roundedCorner(bottom, top, tip);
        const auto [bottomIn, bottomOut] = roundedCorner(top, bottom, tip);
        const auto [tipIn, tipOut] = roundedCorner(top, tip, bottom);

        path.moveTo(topIn);
        path.quadTo(top, topOut);
        path.lineTo(tipIn);
        path.quadTo(tip, tipOut);
        path.lineTo(bottomOut);
        path.quadTo(bottom, bottomIn);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Icon::Pause: {
        const qreal barWidth = glyph * 0.48;
        const qreal gap = glyph * 0.62;
        const qreal radius = barWidth * 0.4;
        p.drawRoundedRect(QRectF(center.x() - gap / 2 - barWidth, center.y() - glyph,
                                  barWidth, glyph * 2), radius, radius);
        p.drawRoundedRect(QRectF(center.x() + gap / 2, center.y() - glyph,
                                  barWidth, glyph * 2), radius, radius);
        break;
    }
    case Icon::VolumeHigh:
    case Icon::VolumeMuted: {
        // Build the speaker body + cone in an unshifted local coordinate
        // space first so the whole glyph's true width is known, then
        // translate it so that width is centered in the button -- rather
        // than anchoring the body on center and letting the asymmetric
        // cone/waves extend further right than left, which pushed the
        // wave arcs outside the circle and clipped them.
        const qreal bodyW = glyph * 0.5;
        const qreal bodyH = glyph * 0.9;
        const qreal coneW = glyph * 0.8;
        const bool muted = (m_icon == Icon::VolumeMuted);
        const qreal wavesW = muted ? glyph * 0.85 : glyph * 1.05;
        const qreal totalW = bodyW + coneW + wavesW;
        const qreal originX = center.x() - totalW / 2.0;
        const qreal bodyRadius = bodyW * 0.3;

        // One continuous rounded path for body + cone reads as a single
        // speaker silhouette instead of two overlapping primitives.
        QPainterPath speaker;
        QRectF body(originX, center.y() - bodyH / 2, bodyW, bodyH);
        speaker.addRoundedRect(body, bodyRadius, bodyRadius);

        QPainterPath cone;
        const qreal coneTipRound = glyph * 0.1;
        cone.moveTo(body.center().x(), body.top());
        cone.lineTo(body.right() + coneW - coneTipRound, center.y() - glyph + coneTipRound);
        cone.quadTo(body.right() + coneW, center.y() - glyph + coneTipRound * 0.4,
                    body.right() + coneW, center.y());
        cone.quadTo(body.right() + coneW, center.y() + glyph - coneTipRound * 0.4,
                    body.right() + coneW - coneTipRound, center.y() + glyph - coneTipRound);
        cone.lineTo(body.center().x(), body.bottom());
        cone.closeSubpath();
        speaker = speaker.united(cone);
        p.drawPath(speaker);

        QPen pen(m_iconColor);
        pen.setWidthF(qMax(1.2, side * 0.036));
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        const qreal x0 = body.right() + coneW + glyph * 0.22;
        if (!muted) {
            QPainterPath arc;
            arc.arcMoveTo(QRectF(x0, center.y() - glyph * 0.7, wavesW, glyph * 1.4), -58);
            arc.arcTo(QRectF(x0, center.y() - glyph * 0.7, wavesW, glyph * 1.4), -58, 116);
            p.drawPath(arc);
        } else {
            const qreal x1 = x0 + wavesW;
            p.drawLine(QPointF(x0, center.y() - glyph * 0.6), QPointF(x1, center.y() + glyph * 0.6));
            p.drawLine(QPointF(x0, center.y() + glyph * 0.6), QPointF(x1, center.y() - glyph * 0.6));
        }
        break;
    }
    }
}

// ── VideoPlayer ──────────────────────────────────────────────────────────

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
    controlsWidget->setFixedHeight(42);
    controlsWidget->setObjectName("videoControls");
    m_controlsWidget = controlsWidget;
    auto *controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(12, 4, 12, 4);
    controlsLayout->setSpacing(8);

    m_playBtn = new IconButton(this);
    m_playBtn->setIcon(IconButton::Icon::Play);
    m_playBtn->setFixedSize(28, 28);
    controlsLayout->addWidget(m_playBtn);

    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setObjectName("videoTimeLabel");
    m_timeLabel->setStyleSheet("QLabel { font-size: 12px; font-family: monospace; min-width: 100px; }");
    controlsLayout->addWidget(m_timeLabel);

    m_seekSlider = new ClickableSlider(Qt::Horizontal, this);
    m_seekSlider->setObjectName("videoSeekSlider");
    m_seekSlider->setRange(0, 0);
    controlsLayout->addWidget(m_seekSlider, 1);

    m_muteBtn = new IconButton(this);
    m_muteBtn->setFixedSize(28, 28);
    controlsLayout->addWidget(m_muteBtn);

    m_volumeSlider = new ClickableSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName("videoVolumeSlider");
    m_volumeSlider->setRange(0, 100);
    QSettings settings;
    const int savedVolume = settings.value(QStringLiteral("videoVolume"), 70).toInt();
    const bool savedMuted = settings.value(QStringLiteral("videoMuted"), false).toBool();
    m_volumeSlider->setValue(qBound(0, savedVolume, 100));
    m_volumeSlider->setFixedWidth(80);
    controlsLayout->addWidget(m_volumeSlider);

    m_layout->addWidget(controlsWidget);
    applyControlsTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &VideoPlayer::applyControlsTheme);

    // Media player
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(m_volumeSlider->value() / 100.0);
    m_audioOutput->setMuted(savedMuted);
    m_muteBtn->setIcon(savedMuted ? IconButton::Icon::VolumeMuted : IconButton::Icon::VolumeHigh);
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
        m_muteBtn->setIcon(m_audioOutput->isMuted()
            ? IconButton::Icon::VolumeMuted
            : IconButton::Icon::VolumeHigh);
    });
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_audioOutput)
            m_audioOutput->setVolume(v / 100.0);
        QSettings().setValue(QStringLiteral("videoVolume"), v);
    });
    retranslateUi();
}

void VideoPlayer::applyControlsTheme()
{
    if (!m_controlsWidget) return;

    const bool dark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    // Neutral, low-contrast palette on purpose: quicklook video controls sit
    // directly over black video content and should stay out of the way
    // rather than compete with it using the app's blue accent.
    const QString barBg = dark ? QStringLiteral("rgba(24, 24, 32, 220)") : QStringLiteral("rgba(245, 245, 247, 235)");
    const QString textColor = dark ? QStringLiteral("#d8d8e0") : QStringLiteral("#2a2a32");
    const QString grooveBg = dark ? QStringLiteral("#3a3a46") : QStringLiteral("#d4d4da");
    const QString filledBg = dark ? QStringLiteral("#9a9aa8") : QStringLiteral("#7a7a86");
    const QColor iconColor = dark ? QColor(224, 224, 230) : QColor(60, 60, 68);

    m_controlsWidget->setStyleSheet(QStringLiteral(R"(
        QWidget#videoControls { background-color: %1; }
        QLabel#videoTimeLabel { color: %2; background: transparent; }
    )").arg(barBg, textColor));

    const QString sliderStyle = QStringLiteral(R"(
        QSlider::groove:horizontal { background: %1; height: 4px; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }
        QSlider::add-page:horizontal { background: %1; border-radius: 2px; }
        QSlider::handle:horizontal {
            background: %2; width: 12px; height: 12px;
            margin: -4px 0; border-radius: 6px;
        }
        QSlider::handle:horizontal:hover { background: %3; }
    )").arg(grooveBg, filledBg, textColor);
    m_seekSlider->setStyleSheet(sliderStyle);
    m_volumeSlider->setStyleSheet(sliderStyle);

    // Buttons must stay flat and colorless: override the app-wide QPushButton
    // QSS (blue fill, padding, radius meant for dialog buttons) so the round
    // transport buttons render as bare circles with a painted glyph only.
    const QString buttonStyle = QStringLiteral(
        "QPushButton { background-color: transparent; border: none; padding: 0; }"
        "QPushButton:hover { background-color: transparent; }"
        "QPushButton:pressed { background-color: transparent; }");
    m_playBtn->setStyleSheet(buttonStyle);
    m_muteBtn->setStyleSheet(buttonStyle);
    m_playBtn->setAccentColor(iconColor);
    m_muteBtn->setAccentColor(iconColor);
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
    if (m_playBtn) m_playBtn->setIcon(IconButton::Icon::Play);
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
        m_playBtn->setIcon(IconButton::Icon::Play);
    }
}

void VideoPlayer::updatePlaybackState(QMediaPlayer::PlaybackState state)
{
    if (!m_playBtn)
        return;
    m_playBtn->setIcon(state == QMediaPlayer::PlayingState
        ? IconButton::Icon::Pause
        : IconButton::Icon::Play);
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
