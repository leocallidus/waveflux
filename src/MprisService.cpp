#include "MprisService.h"

#include "AudioEngine.h"
#include "PlaybackController.h"
#include "TrackModel.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusAbstractAdaptor>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QDebug>
#include <QDBusMessage>
#include <QFileInfo>
#include <QGuiApplication>
#include <QModelIndex>
#include <QUrl>
#include <QWindow>

namespace {
constexpr qlonglong kUsPerMs = 1000;
const QString kMprisPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString kDbusPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

bool seekDiagEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("WAVEFLUX_SEEK_DIAG");
    return enabled;
}
}

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")

    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    explicit MprisRootAdaptor(MprisService *service)
        : QDBusAbstractAdaptor(service)
    {
    }

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("WaveFlux"); }
    QString desktopEntry() const { return QStringLiteral("waveflux"); }
    QStringList supportedUriSchemes() const { return {QStringLiteral("file")}; }
    QStringList supportedMimeTypes() const
    {
        return {
            QStringLiteral("audio/mpeg"),
            QStringLiteral("audio/flac"),
            QStringLiteral("audio/x-flac"),
            QStringLiteral("audio/ogg"),
            QStringLiteral("audio/wav"),
            QStringLiteral("audio/x-wav"),
            QStringLiteral("audio/aac"),
            QStringLiteral("audio/mp4"),
            QStringLiteral("audio/x-m4a")
        };
    }

public slots:
    void Raise()
    {
        const auto windows = QGuiApplication::topLevelWindows();
        for (QWindow *window : windows) {
            if (!window || !window->isVisible()) {
                continue;
            }
            window->raise();
            window->requestActivate();
            break;
        }
    }

    void Quit()
    {
        QCoreApplication::quit();
    }

};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)

public:
    explicit MprisPlayerAdaptor(MprisService *service)
        : QDBusAbstractAdaptor(service)
        , m_service(service)
    {
    }

    QString playbackStatus() const { return m_service->playbackStatus(); }
    QString loopStatus() const { return m_service->loopStatus(); }
    void setLoopStatus(const QString &value) { m_service->setLoopStatus(value); }
    double rate() const { return m_service->rate(); }
    void setRate(double value) { m_service->setRate(value); }
    double minimumRate() const { return 0.25; }
    double maximumRate() const { return 2.0; }
    bool shuffle() const { return m_service->shuffle(); }
    void setShuffle(bool enabled) { m_service->setShuffle(enabled); }
    QVariantMap metadata() const { return m_service->metadata(); }
    double volume() const { return m_service->volume(); }
    void setVolume(double value) { m_service->setVolume(value); }
    qlonglong position() const { return m_service->positionUs(); }
    bool canGoNext() const { return m_service->canGoNext(); }
    bool canGoPrevious() const { return m_service->canGoPrevious(); }
    bool canPlay() const { return m_service->canPlay(); }
    bool canPause() const { return m_service->canPause(); }
    bool canSeek() const { return m_service->canSeek(); }
    bool canControl() const { return m_service->canControl(); }

public slots:
    void Next() { m_service->next(); }
    void Previous() { m_service->previous(); }
    void Pause() { m_service->pause(); }
    void PlayPause() { m_service->playPause(); }
    void Stop() { m_service->stop(); }
    void Play() { m_service->play(); }
    void Seek(qlonglong offsetUs) { m_service->seek(offsetUs); }
    void SetPosition(const QDBusObjectPath &trackId, qlonglong positionUs)
    {
        m_service->setPosition(trackId, positionUs);
    }
    void OpenUri(const QString &uri) { m_service->openUri(uri); }

signals:
    void Seeked(qlonglong positionUs);

private:
    MprisService *m_service = nullptr;
};

MprisService::MprisService(AudioEngine *audioEngine,
                           TrackModel *trackModel,
                           PlaybackController *playbackController,
                           QObject *parent)
    : QObject(parent)
    , m_audioEngine(audioEngine)
    , m_trackModel(trackModel)
    , m_playbackController(playbackController)
    , m_connection(QDBusConnection::sessionBus())
{
    m_rootAdaptor = new MprisRootAdaptor(this);
    m_playerAdaptor = new MprisPlayerAdaptor(this);

    connectSignals();
    if (registerService()) {
        QVariantMap changed;
        changed.insert(QStringLiteral("PlaybackStatus"), playbackStatus());
        changed.insert(QStringLiteral("LoopStatus"), loopStatus());
        changed.insert(QStringLiteral("Rate"), rate());
        changed.insert(QStringLiteral("Shuffle"), shuffle());
        changed.insert(QStringLiteral("Metadata"), metadata());
        changed.insert(QStringLiteral("Volume"), volume());
        changed.insert(QStringLiteral("Position"), positionUs());
        changed.insert(QStringLiteral("CanGoNext"), canGoNext());
        changed.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
        changed.insert(QStringLiteral("CanPlay"), canPlay());
        changed.insert(QStringLiteral("CanPause"), canPause());
        changed.insert(QStringLiteral("CanSeek"), canSeek());
        changed.insert(QStringLiteral("CanControl"), canControl());
        emitPlayerPropertiesChanged(changed);
    }
}

MprisService::~MprisService()
{
    unregisterService();
}

bool MprisService::registerService()
{
    if (!m_connection.isConnected()) {
        qWarning() << "[MPRIS] Session bus is not connected";
        return false;
    }

    QDBusConnectionInterface *interface = m_connection.interface();
    if (!interface) {
        qWarning() << "[MPRIS] D-Bus connection interface is unavailable";
        return false;
    }

    const auto reply = interface->registerService(m_serviceName);
    if (reply != QDBusConnectionInterface::ServiceRegistered) {
        qWarning() << "[MPRIS] Failed to register service" << m_serviceName
                   << "reply:" << reply
                   << "error:" << interface->lastError().message();
        return false;
    }

    const bool registeredObject = m_connection.registerObject(
        m_objectPath,
        this,
        QDBusConnection::ExportAdaptors);
    if (!registeredObject) {
        interface->unregisterService(m_serviceName);
        qWarning() << "[MPRIS] Failed to register object at" << m_objectPath;
        return false;
    }

    m_registered = true;
    return true;
}

void MprisService::unregisterService()
{
    if (!m_registered) {
        return;
    }

    m_connection.unregisterObject(m_objectPath);
    if (QDBusConnectionInterface *interface = m_connection.interface()) {
        interface->unregisterService(m_serviceName);
    }
    m_registered = false;
}

void MprisService::emitPlayerPropertiesChanged(const QVariantMap &changedProperties) const
{
    if (!m_registered || changedProperties.isEmpty()) {
        return;
    }

    QDBusMessage signal = QDBusMessage::createSignal(
        m_objectPath,
        kDbusPropertiesInterface,
        QStringLiteral("PropertiesChanged"));
    signal << kMprisPlayerInterface << changedProperties << QStringList{};
    m_connection.send(signal);
}

void MprisService::emitSeeked(qlonglong positionUs)
{
    if (!m_registered || !m_playerAdaptor) {
        return;
    }
    emit m_playerAdaptor->Seeked(positionUs);
}

QString MprisService::playbackStatus() const
{
    if (!m_audioEngine) {
        return QStringLiteral("Stopped");
    }

    switch (m_audioEngine->state()) {
    case AudioEngine::PlayingState:
        return QStringLiteral("Playing");
    case AudioEngine::PausedState:
    case AudioEngine::ReadyState:
        return QStringLiteral("Paused");
    case AudioEngine::StoppedState:
    case AudioEngine::EndedState:
    case AudioEngine::ErrorState:
    default:
        return QStringLiteral("Stopped");
    }
}

QString MprisService::loopStatus() const
{
    if (!m_playbackController) {
        return QStringLiteral("None");
    }

    switch (m_playbackController->repeatMode()) {
    case PlaybackController::RepeatOne:
        return QStringLiteral("Track");
    case PlaybackController::RepeatAll:
        return QStringLiteral("Playlist");
    case PlaybackController::RepeatOff:
    default:
        return QStringLiteral("None");
    }
}

double MprisService::rate() const
{
    return m_audioEngine ? m_audioEngine->playbackRate() : 1.0;
}

bool MprisService::shuffle() const
{
    return m_playbackController ? m_playbackController->shuffleEnabled() : false;
}

QVariantMap MprisService::metadata() const
{
    QVariantMap map;
    const QString filePath = m_trackModel
        ? m_trackModel->getFilePath(m_trackModel->currentIndex())
        : QString();
    const int index = m_trackModel ? m_trackModel->currentIndex() : -1;
    const QString trackPath = trackObjectPathFor(filePath, index);
    map.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(trackPath)));

    if (!m_trackModel || index < 0) {
        return map;
    }

    const QModelIndex modelIndex = m_trackModel->index(index, 0);
    QString title = m_trackModel->data(modelIndex, TrackModel::TitleRole).toString();
    if (title.isEmpty()) {
        title = m_trackModel->data(modelIndex, TrackModel::DisplayNameRole).toString();
    }
    const QString artist = m_trackModel->data(modelIndex, TrackModel::ArtistRole).toString();
    const QString album = m_trackModel->data(modelIndex, TrackModel::AlbumRole).toString();
    const qint64 durationMs = m_trackModel->data(modelIndex, TrackModel::DurationRole).toLongLong();

    if (!title.isEmpty()) {
        map.insert(QStringLiteral("xesam:title"), title);
    } else if (!filePath.isEmpty()) {
        map.insert(QStringLiteral("xesam:title"), QFileInfo(filePath).fileName());
    }
    if (!artist.isEmpty()) {
        map.insert(QStringLiteral("xesam:artist"), QStringList{artist});
    }
    if (!album.isEmpty()) {
        map.insert(QStringLiteral("xesam:album"), album);
    }
    if (!filePath.isEmpty()) {
        map.insert(QStringLiteral("xesam:url"), QUrl::fromLocalFile(filePath).toString());
    }
    if (durationMs > 0) {
        map.insert(QStringLiteral("mpris:length"), static_cast<qlonglong>(durationMs * kUsPerMs));
    }

    return map;
}

double MprisService::volume() const
{
    return m_audioEngine ? m_audioEngine->volume() : 1.0;
}

qlonglong MprisService::positionUs() const
{
    if (!m_audioEngine) {
        return 0;
    }
    return static_cast<qlonglong>(qMax<qint64>(0, m_audioEngine->position()) * kUsPerMs);
}

bool MprisService::canGoNext() const
{
    return m_playbackController ? m_playbackController->canGoNext() : false;
}

bool MprisService::canGoPrevious() const
{
    return m_playbackController ? m_playbackController->canGoPrevious() : false;
}

bool MprisService::canPlay() const
{
    return m_trackModel && m_trackModel->rowCount() > 0;
}

bool MprisService::canPause() const
{
    return canPlay();
}

bool MprisService::canSeek() const
{
    return canPlay();
}

bool MprisService::canControl() const
{
    return true;
}

void MprisService::setLoopStatus(const QString &loopStatusValue)
{
    if (!m_playbackController) {
        return;
    }

    if (loopStatusValue == QStringLiteral("Track")) {
        m_playbackController->setRepeatMode(PlaybackController::RepeatOne);
    } else if (loopStatusValue == QStringLiteral("Playlist")) {
        m_playbackController->setRepeatMode(PlaybackController::RepeatAll);
    } else {
        m_playbackController->setRepeatMode(PlaybackController::RepeatOff);
    }
}

void MprisService::setRate(double rateValue)
{
    if (!m_audioEngine) {
        return;
    }
    m_audioEngine->setPlaybackRate(rateValue);
}

void MprisService::setShuffle(bool enabled)
{
    if (!m_playbackController) {
        return;
    }
    m_playbackController->setShuffleEnabled(enabled);
}

void MprisService::setVolume(double volumeValue)
{
    if (!m_audioEngine) {
        return;
    }
    m_audioEngine->setVolume(volumeValue);
}

void MprisService::next()
{
    if (!m_playbackController) {
        return;
    }
    m_playbackController->nextTrack();
}

void MprisService::previous()
{
    if (!m_playbackController) {
        return;
    }
    m_playbackController->previousTrack();
}

void MprisService::pause()
{
    if (!m_audioEngine) {
        return;
    }
    m_audioEngine->pause();
}

void MprisService::playPause()
{
    if (!m_audioEngine) {
        return;
    }
    if (!canPlay()) {
        return;
    }
    if (m_trackModel && m_trackModel->currentIndex() < 0) {
        m_trackModel->setCurrentIndex(0);
    }
    m_audioEngine->togglePlayPause();
}

void MprisService::stop()
{
    if (!m_audioEngine) {
        return;
    }
    m_audioEngine->stop();
}

void MprisService::play()
{
    if (!m_audioEngine || !canPlay()) {
        return;
    }

    if (m_trackModel && m_trackModel->currentIndex() < 0) {
        m_trackModel->setCurrentIndex(0);
    }
    m_audioEngine->play();
}

void MprisService::seek(qlonglong offsetUs)
{
    if (!m_playbackController || !m_audioEngine || !canSeek()) {
        return;
    }

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][MPRIS] relative seek request"
            << "offsetUs=" << offsetUs
            << "offsetMs=" << (offsetUs / kUsPerMs)
            << "currentFile=" << (m_audioEngine ? m_audioEngine->currentFile() : QString())
            << "positionMs=" << (m_audioEngine ? m_audioEngine->position() : 0);
    }

    // Delegate to PlaybackController for cue-aware seeking.
    // MPRIS offset is in microseconds; seekRelative expects milliseconds.
    m_playbackController->seekRelative(offsetUs / kUsPerMs);
    emitSeeked(positionUs());
}

void MprisService::setPosition(const QDBusObjectPath &trackId, qlonglong positionUsValue)
{
    if (!m_audioEngine || !canSeek()) {
        return;
    }

    if (trackId.path() != currentTrackObjectPath()) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][MPRIS] absolute seek ignored: track id mismatch"
                << "requestedTrackId=" << trackId.path()
                << "currentTrackId=" << currentTrackObjectPath()
                << "positionUs=" << positionUsValue;
        }
        return;
    }

    const qlonglong boundedPositionUs = qMax<qlonglong>(0, positionUsValue);
    const qint64 requestedMs = boundedPositionUs / kUsPerMs;
    const qint64 currentMs = qMax<qint64>(0, m_audioEngine->position());
    const qint64 durationMs = qMax<qint64>(0, m_audioEngine->duration());
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool shortlyAfterTrackChange = (m_lastTrackChangeWallClockMs > 0)
        && ((nowMs - m_lastTrackChangeWallClockMs) <= 5000);
    const qint64 deltaMs = qAbs(requestedMs - currentMs);

    if (shortlyAfterTrackChange && deltaMs > 15000) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][MPRIS] absolute seek ignored shortly after track change"
                << "requestedMs=" << requestedMs
                << "currentMs=" << currentMs
                << "durationMs=" << durationMs
                << "deltaMs=" << deltaMs
                << "elapsedSinceTrackChangeMs=" << (nowMs - m_lastTrackChangeWallClockMs);
        }
        return;
    }

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][MPRIS] absolute seek request"
            << "requestedMs=" << requestedMs
            << "currentMs=" << currentMs
            << "durationMs=" << durationMs
            << "shortlyAfterTrackChange=" << shortlyAfterTrackChange
            << "deltaMs=" << deltaMs;
    }

    m_audioEngine->seekWithSource(requestedMs, QStringLiteral("mpris.set_position"));
    emitSeeked(boundedPositionUs);
}

void MprisService::openUri(const QString &uri)
{
    if (!m_audioEngine || uri.isEmpty()) {
        return;
    }

    const QUrl url(uri);
    if (url.isLocalFile()) {
        m_audioEngine->loadFile(url.toLocalFile());
    } else if (url.scheme() == QStringLiteral("file")) {
        m_audioEngine->loadFile(QUrl::fromPercentEncoding(url.path().toUtf8()));
    }
}

QString MprisService::currentTrackObjectPath() const
{
    const QString filePath = m_trackModel
        ? m_trackModel->getFilePath(m_trackModel->currentIndex())
        : QString();
    const int index = m_trackModel ? m_trackModel->currentIndex() : -1;
    return trackObjectPathFor(filePath, index);
}

QString MprisService::trackObjectPathFor(const QString &filePath, int index) const
{
    if (filePath.isEmpty() || index < 0) {
        return QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");
    }
    const QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("/org/mpris/MediaPlayer2/Track/%1").arg(QString::fromLatin1(hash));
}

void MprisService::connectSignals()
{
    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::stateChanged, this, [this]() {
            QVariantMap changed;
            changed.insert(QStringLiteral("PlaybackStatus"), playbackStatus());
            changed.insert(QStringLiteral("CanPause"), canPause());
            changed.insert(QStringLiteral("CanPlay"), canPlay());
            changed.insert(QStringLiteral("CanSeek"), canSeek());
            changed.insert(QStringLiteral("CanGoNext"), canGoNext());
            changed.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
            emitPlayerPropertiesChanged(changed);
        });

        connect(m_audioEngine, &AudioEngine::volumeChanged, this, [this]() {
            emitPlayerPropertiesChanged({{QStringLiteral("Volume"), volume()}});
        });

        connect(m_audioEngine, &AudioEngine::playbackRateChanged, this, [this]() {
            emitPlayerPropertiesChanged({{QStringLiteral("Rate"), rate()}});
        });

        connect(m_audioEngine, &AudioEngine::currentFileChanged, this, [this]() {
            m_lastTrackChangeWallClockMs = QDateTime::currentMSecsSinceEpoch();
            QVariantMap changed;
            changed.insert(QStringLiteral("Metadata"), metadata());
            changed.insert(QStringLiteral("Position"), positionUs());
            changed.insert(QStringLiteral("CanPlay"), canPlay());
            changed.insert(QStringLiteral("CanPause"), canPause());
            changed.insert(QStringLiteral("CanSeek"), canSeek());
            emitPlayerPropertiesChanged(changed);
            emitSeeked(positionUs());
        });

        connect(m_audioEngine, &AudioEngine::durationChanged, this, [this]() {
            emitPlayerPropertiesChanged({{QStringLiteral("Metadata"), metadata()}});
        });

        connect(m_audioEngine, &AudioEngine::metadataChanged, this, [this]() {
            emitPlayerPropertiesChanged({{QStringLiteral("Metadata"), metadata()}});
        });
    }

    if (m_trackModel) {
        connect(m_trackModel, &TrackModel::currentIndexChanged, this, [this]() {
            QVariantMap changed;
            changed.insert(QStringLiteral("Metadata"), metadata());
            changed.insert(QStringLiteral("CanPlay"), canPlay());
            changed.insert(QStringLiteral("CanPause"), canPause());
            changed.insert(QStringLiteral("CanSeek"), canSeek());
            changed.insert(QStringLiteral("CanGoNext"), canGoNext());
            changed.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
            emitPlayerPropertiesChanged(changed);
        });

        connect(m_trackModel, &TrackModel::countChanged, this, [this]() {
            QVariantMap changed;
            changed.insert(QStringLiteral("Metadata"), metadata());
            changed.insert(QStringLiteral("CanPlay"), canPlay());
            changed.insert(QStringLiteral("CanPause"), canPause());
            changed.insert(QStringLiteral("CanSeek"), canSeek());
            changed.insert(QStringLiteral("CanGoNext"), canGoNext());
            changed.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
            emitPlayerPropertiesChanged(changed);
        });
    }

    if (m_playbackController) {
        connect(m_playbackController, &PlaybackController::navigationStateChanged, this, [this]() {
            QVariantMap changed;
            changed.insert(QStringLiteral("CanGoNext"), canGoNext());
            changed.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
            emitPlayerPropertiesChanged(changed);
        });

        connect(m_playbackController, &PlaybackController::repeatModeChanged, this, [this]() {
            emitPlayerPropertiesChanged({{QStringLiteral("LoopStatus"), loopStatus()}});
        });

        connect(m_playbackController, &PlaybackController::shuffleEnabledChanged, this, [this]() {
            emitPlayerPropertiesChanged({{QStringLiteral("Shuffle"), shuffle()}});
        });
    }
}

#include "MprisService.moc"
