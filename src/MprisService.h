#ifndef MPRISSERVICE_H
#define MPRISSERVICE_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QVariantMap>

class AudioEngine;
class PlaybackController;
class TrackModel;
class MprisRootAdaptor;
class MprisPlayerAdaptor;

class MprisService : public QObject
{
    Q_OBJECT

public:
    explicit MprisService(AudioEngine *audioEngine,
                          TrackModel *trackModel,
                          PlaybackController *playbackController,
                          QObject *parent = nullptr);
    ~MprisService() override;

private:
    friend class MprisRootAdaptor;
    friend class MprisPlayerAdaptor;

    bool registerService();
    void unregisterService();

    void emitPlayerPropertiesChanged(const QVariantMap &changedProperties) const;
    void emitSeeked(qlonglong positionUs);

    QString playbackStatus() const;
    QString loopStatus() const;
    double rate() const;
    bool shuffle() const;
    QVariantMap metadata() const;
    double volume() const;
    qlonglong positionUs() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const;
    bool canPause() const;
    bool canSeek() const;
    bool canControl() const;

    void setLoopStatus(const QString &loopStatus);
    void setRate(double rate);
    void setShuffle(bool enabled);
    void setVolume(double volume);

    void next();
    void previous();
    void pause();
    void playPause();
    void stop();
    void play();
    void seek(qlonglong offsetUs);
    void setPosition(const QDBusObjectPath &trackId, qlonglong positionUs);
    void openUri(const QString &uri);

    QString currentTrackObjectPath() const;
    QString trackObjectPathFor(const QString &filePath, int index) const;
    void connectSignals();

    AudioEngine *m_audioEngine = nullptr;
    TrackModel *m_trackModel = nullptr;
    PlaybackController *m_playbackController = nullptr;

    QDBusConnection m_connection;
    bool m_registered = false;
    QString m_serviceName = QStringLiteral("org.mpris.MediaPlayer2.waveflux");
    QString m_objectPath = QStringLiteral("/org/mpris/MediaPlayer2");

    MprisRootAdaptor *m_rootAdaptor = nullptr;
    MprisPlayerAdaptor *m_playerAdaptor = nullptr;
    qint64 m_lastTrackChangeWallClockMs = 0;
};

#endif // MPRISSERVICE_H
