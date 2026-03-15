#include "MprisService.h"

#include "AudioEngine.h"
#include "PlaybackController.h"
#include "TrackModel.h"

MprisService::MprisService(AudioEngine *audioEngine,
                           TrackModel *trackModel,
                           PlaybackController *playbackController,
                           QObject *parent)
    : QObject(parent)
    , m_audioEngine(audioEngine)
    , m_trackModel(trackModel)
    , m_playbackController(playbackController)
{
}

MprisService::~MprisService() = default;

bool MprisService::registerService()
{
    return false;
}

void MprisService::unregisterService()
{
}

void MprisService::emitPlayerPropertiesChanged(const QVariantMap &changedProperties) const
{
    Q_UNUSED(changedProperties);
}

void MprisService::emitSeeked(qlonglong positionUs)
{
    Q_UNUSED(positionUs);
}

QString MprisService::playbackStatus() const
{
    return QStringLiteral("Stopped");
}

QString MprisService::loopStatus() const
{
    return QStringLiteral("None");
}

double MprisService::rate() const
{
    return 1.0;
}

bool MprisService::shuffle() const
{
    return false;
}

QVariantMap MprisService::metadata() const
{
    return {};
}

double MprisService::volume() const
{
    return 1.0;
}

qlonglong MprisService::positionUs() const
{
    return 0;
}

bool MprisService::canGoNext() const
{
    return false;
}

bool MprisService::canGoPrevious() const
{
    return false;
}

bool MprisService::canPlay() const
{
    return false;
}

bool MprisService::canPause() const
{
    return false;
}

bool MprisService::canSeek() const
{
    return false;
}

bool MprisService::canControl() const
{
    return false;
}

void MprisService::setLoopStatus(const QString &loopStatus)
{
    Q_UNUSED(loopStatus);
}

void MprisService::setRate(double rate)
{
    Q_UNUSED(rate);
}

void MprisService::setShuffle(bool enabled)
{
    Q_UNUSED(enabled);
}

void MprisService::setVolume(double volume)
{
    Q_UNUSED(volume);
}

void MprisService::next()
{
}

void MprisService::previous()
{
}

void MprisService::pause()
{
}

void MprisService::playPause()
{
}

void MprisService::stop()
{
}

void MprisService::play()
{
}

void MprisService::seek(qlonglong offsetUs)
{
    Q_UNUSED(offsetUs);
}

void MprisService::setPosition(const QDBusObjectPath &trackId, qlonglong positionUs)
{
    Q_UNUSED(trackId);
    Q_UNUSED(positionUs);
}

void MprisService::openUri(const QString &uri)
{
    Q_UNUSED(uri);
}

QString MprisService::currentTrackObjectPath() const
{
    return QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");
}

QString MprisService::trackObjectPathFor(const QString &filePath, int index) const
{
    Q_UNUSED(filePath);
    Q_UNUSED(index);
    return QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");
}

void MprisService::connectSignals()
{
}
