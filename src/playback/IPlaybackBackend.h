#ifndef IPLAYBACKBACKEND_H
#define IPLAYBACKBACKEND_H

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace WaveFlux {

enum class PlaybackBackendState {
    Stopped,
    Playing,
    Paused,
    Ready,
    Ended,
    Error
};

struct PlaybackMetadata {
    QString title;
    QString artist;
    QString album;
    QString sourceFormat;
    QString trackerType;
    QString trackerMessage;
    int channelCount = 0;
    int patternCount = 0;
    int instrumentCount = 0;

    bool operator==(const PlaybackMetadata &other) const = default;
};

struct PlaybackBackendCapabilities {
    bool seek = false;
    bool waveform = false;
    bool spectrum = false;
    bool equalizer = false;
    bool reverse = false;
    bool gapless = false;
    bool rate = false;
    bool pitch = false;
    bool rateWithPitchChange = false;
    bool timeStretch = false;
    bool pitchShift = false;
    bool remoteSources = false;

    bool operator==(const PlaybackBackendCapabilities &other) const = default;
};

class IPlaybackBackend : public QObject
{
    Q_OBJECT

public:
    explicit IPlaybackBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IPlaybackBackend() override = default;

    virtual void load(const QString &source) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 positionMs) = 0;
    virtual qint64 position() const = 0;
    virtual qint64 duration() const = 0;
    virtual PlaybackBackendState state() const = 0;
    virtual PlaybackMetadata metadata() const = 0;
    virtual PlaybackBackendCapabilities capabilities() const = 0;
    virtual void setVolume(double volume) = 0;

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void stateChanged(WaveFlux::PlaybackBackendState state);
    void error(const QString &message);
    void endOfStream();
    void metadataChanged(const WaveFlux::PlaybackMetadata &metadata);
};

} // namespace WaveFlux

Q_DECLARE_METATYPE(WaveFlux::PlaybackBackendState)
Q_DECLARE_METATYPE(WaveFlux::PlaybackMetadata)
Q_DECLARE_METATYPE(WaveFlux::PlaybackBackendCapabilities)

#endif // IPLAYBACKBACKEND_H
