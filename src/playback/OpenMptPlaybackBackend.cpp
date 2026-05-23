#include "playback/OpenMptPlaybackBackend.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QVariantList>
#include <QUrl>
#include <QDebug>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <ostream>
#include <sstream>
#include <utility>

#include <libopenmpt/libopenmpt.hpp>

#include "playback/PlaybackBackendRouting.h"

namespace {

constexpr auto kPositionUpdateInterval = std::chrono::milliseconds(100);
constexpr qint64 kPcmTapIntervalMs = 66;
constexpr qint64 kPcmTapMaxFrames = 2048;

QAudioFormat openMptAudioFormat()
{
    QAudioFormat format;
    format.setSampleRate(WaveFlux::TrackerPcmEngine::kSampleRate);
    format.setChannelCount(WaveFlux::TrackerPcmEngine::kChannelCount);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

qint64 secondsToMs(double seconds)
{
    return std::max<qint64>(0, qRound64(seconds * 1000.0));
}

QString trackerLoadErrorPrefix(const QString &filePath)
{
    return QStringLiteral("Failed to load tracker module '%1'.").arg(filePath);
}

void storeMax(std::atomic<qint64> *target, qint64 value)
{
    qint64 previous = target->load(std::memory_order_relaxed);
    while (value > previous
           && !target->compare_exchange_weak(previous,
                                             value,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
    }
}

QVariantMap graphSnapshotToVariantMap(const WaveFlux::TrackerPcmEngineSnapshot &snapshot)
{
    return QVariantMap{
        {QStringLiteral("positionMs"), snapshot.positionMs},
        {QStringLiteral("decodePositionMs"), snapshot.decodePositionMs},
        {QStringLiteral("submittedFrames"), QVariant::fromValue<qulonglong>(snapshot.submittedFrames)},
        {QStringLiteral("consumedFrames"), QVariant::fromValue<qulonglong>(snapshot.consumedFrames)},
        {QStringLiteral("bufferedFrames"), QVariant::fromValue<qulonglong>(snapshot.bufferedFrames)},
        {QStringLiteral("activeGraphGeneration"), QVariant::fromValue<qulonglong>(snapshot.activeGraphGeneration)},
        {QStringLiteral("targetGraphGeneration"), QVariant::fromValue<qulonglong>(snapshot.targetGraphGeneration)},
        {QStringLiteral("lastFlushGeneration"), QVariant::fromValue<qulonglong>(snapshot.lastFlushGeneration)},
        {QStringLiteral("playbackRate"), snapshot.playbackRate},
        {QStringLiteral("pitchSemitones"), snapshot.pitchSemitones},
        {QStringLiteral("reversePlayback"), snapshot.reversePlayback},
        {QStringLiteral("reverseRatePitchStageConfigured"), snapshot.reverseRatePitchStageConfigured},
        {QStringLiteral("equalizerStageActive"), snapshot.equalizerStageActive},
        {QStringLiteral("equalizerBandCount"), snapshot.equalizerBandCount},
        {QStringLiteral("pendingEqualizerBandCount"), snapshot.pendingEqualizerBandCount},
        {QStringLiteral("pitchRampBlocksRemaining"), snapshot.pitchRampBlocksRemaining},
        {QStringLiteral("equalizerRampBlocksRemaining"), snapshot.equalizerRampBlocksRemaining},
        {QStringLiteral("limiterStageActive"), snapshot.limiterStageActive},
        {QStringLiteral("limiterEngaged"), snapshot.limiterEngaged},
        {QStringLiteral("endOfStream"), snapshot.endOfStream}
    };
}

bool isWindowsDriveLetterPath(const QString &path)
{
    return path.size() >= 3
        && path.at(0).isLetter()
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\'));
}

bool isSlashPrefixedWindowsDriveLetterPath(const QString &path)
{
    return path.size() >= 4
        && path.at(0) == QLatin1Char('/')
        && path.at(1).isLetter()
        && path.at(2) == QLatin1Char(':')
        && (path.at(3) == QLatin1Char('/') || path.at(3) == QLatin1Char('\\'));
}

bool isWindowsUncPath(const QString &path)
{
    return path.startsWith(QStringLiteral("\\\\")) || path.startsWith(QStringLiteral("//"));
}

} // namespace

namespace WaveFlux {

class OpenMptRenderDevice final : public QIODevice
{
public:
    explicit OpenMptRenderDevice(OpenMptPlaybackBackend *backend)
        : QIODevice(backend)
        , m_backend(backend)
    {
    }

    bool isSequential() const override
    {
        return true;
    }

    qint64 bytesAvailable() const override
    {
        return QIODevice::bytesAvailable() + m_backend->audioBytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        return m_backend->readAudioData(data, maxSize);
    }

    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

private:
    OpenMptPlaybackBackend *m_backend = nullptr;
};

struct OpenMptPlaybackBackend::LoadedTrackerModule {
    QString source;
    QString localPath;
    std::unique_ptr<std::ifstream> stream;
    std::unique_ptr<std::ostringstream> logStream;
    std::unique_ptr<openmpt::module> module;
    PlaybackMetadata metadata;
    qint64 durationMs = 0;
};

OpenMptPlaybackBackend::OpenMptPlaybackBackend(QObject *parent)
    : IPlaybackBackend(parent)
{
    m_positionTimer.setInterval(kPositionUpdateInterval);
    connect(&m_positionTimer, &QTimer::timeout, this, &OpenMptPlaybackBackend::emitPositionIfChanged);
    connect(&m_mediaDevices,
            &QMediaDevices::audioOutputsChanged,
            this,
            &OpenMptPlaybackBackend::handleAudioOutputsChanged);
}

OpenMptPlaybackBackend::~OpenMptPlaybackBackend()
{
    teardownAudioSink();
}

void OpenMptPlaybackBackend::load(const QString &source)
{
    teardownAudioSink();
    clearLoadedModule();
    resetDiagnostics();
    m_loadStartWallClockMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
    traceDiagnostics("load_started", source);

    QString errorMessage;
    std::unique_ptr<LoadedTrackerModule> loaded;
    QElapsedTimer gaplessPromotionTimer;
    if (preloadedNextMatches(source)) {
        gaplessPromotionTimer.start();
        loaded = std::move(m_preloadedNext);
        m_promotedPreloadedNextCount.fetch_add(1, std::memory_order_relaxed);
        traceDiagnostics("preloaded_next_promoted", loaded->localPath);
    } else {
        clearPreloadedNext();
        loaded = loadTrackerModuleForSource(source, &errorMessage);
        if (!loaded) {
            fail(errorMessage);
            return;
        }
    }

    const QString localPath = loaded->localPath;
    adoptLoadedModule(std::move(loaded));
    if (gaplessPromotionTimer.isValid()) {
        const qint64 latencyUs = gaplessPromotionTimer.nsecsElapsed() / 1000;
        m_lastGaplessTransitionLatencyUs.store(latencyUs, std::memory_order_relaxed);
        storeMax(&m_maxGaplessTransitionLatencyUs, latencyUs);
    }
    updatePosition(0);
    m_endOfStreamPending.store(false);

    m_lastEmittedPositionMs = 0;
    emit positionChanged(0);
    emit durationChanged(m_durationMs);
    emit metadataChanged(m_metadata);
    setState(PlaybackBackendState::Ready, true);
    traceDiagnostics("load_finished", localPath);
}

void OpenMptPlaybackBackend::play()
{
    if (!m_pcmEngine.hasModule()) {
        fail(QStringLiteral("Cannot start tracker playback before a module is loaded."), false);
        return;
    }

    if (m_state == PlaybackBackendState::Playing) {
        return;
    }

    if (m_state == PlaybackBackendState::Paused && m_audioSink) {
        m_audioSink->resume();
        m_positionTimer.start();
        setState(PlaybackBackendState::Playing);
        return;
    }

    if (m_state == PlaybackBackendState::Ended) {
        seek(0);
        if (m_state == PlaybackBackendState::Error) {
            return;
        }
    }

    if (!startAudioSink(false)) {
        return;
    }

    m_positionTimer.start();
    setState(PlaybackBackendState::Playing);
}

void OpenMptPlaybackBackend::pause()
{
    if (m_state != PlaybackBackendState::Playing || !m_audioSink) {
        return;
    }

    m_audioSink->suspend();
    m_positionTimer.stop();
    setState(PlaybackBackendState::Paused);
}

void OpenMptPlaybackBackend::stop()
{
    teardownAudioSink();
    m_pcmEngine.resetToStart();

    updatePosition(0);
    m_lastEmittedPositionMs = 0;
    emit positionChanged(0);
    setState(PlaybackBackendState::Stopped, m_state == PlaybackBackendState::Stopped);
}

void OpenMptPlaybackBackend::seek(qint64 positionMs)
{
    qint64 clampedPositionMs = qBound<qint64>(0, positionMs, m_durationMs);
    qint64 actualPositionMs = 0;
    QElapsedTimer seekTimer;
    seekTimer.start();

    if (!m_pcmEngine.hasModule()) {
        fail(QStringLiteral("Cannot seek tracker playback before a module is loaded."), false);
        return;
    }

    actualPositionMs = m_pcmEngine.seek(clampedPositionMs);
    const qint64 latencyUs = seekTimer.nsecsElapsed() / 1000;
    m_seekCount.fetch_add(1, std::memory_order_relaxed);
    m_lastSeekLatencyUs.store(latencyUs, std::memory_order_relaxed);
    storeMax(&m_maxSeekLatencyUs, latencyUs);
    updatePosition(actualPositionMs);
    m_lastEmittedPositionMs = actualPositionMs;
    emit positionChanged(actualPositionMs);
    m_endOfStreamPending.store(false);

    if (m_state == PlaybackBackendState::Playing || m_state == PlaybackBackendState::Paused) {
        const bool startSuspended = (m_state == PlaybackBackendState::Paused);
        if (!startAudioSink(startSuspended)) {
            return;
        }

        if (startSuspended) {
            m_positionTimer.stop();
        } else {
            m_positionTimer.start();
        }
    }
    traceDiagnostics("seek_finished",
                     QStringLiteral("targetMs=%1 actualMs=%2 latencyUs=%3")
                         .arg(clampedPositionMs)
                         .arg(actualPositionMs)
                         .arg(latencyUs));
}

qint64 OpenMptPlaybackBackend::position() const
{
    return m_pcmEngine.hasModule() ? m_pcmEngine.positionMs() : m_positionMs.load();
}

qint64 OpenMptPlaybackBackend::duration() const
{
    return m_durationMs;
}

PlaybackBackendState OpenMptPlaybackBackend::state() const
{
    return m_state;
}

PlaybackMetadata OpenMptPlaybackBackend::metadata() const
{
    return m_metadata;
}

PlaybackBackendCapabilities OpenMptPlaybackBackend::capabilities() const
{
    return PlaybackBackendCapabilities{
        .seek = true,
        .waveform = true,
        .spectrum = true,
        .equalizer = true,
        .reverse = false,
        .gapless = true,
        .rate = false,
        .pitch = false,
        .rateWithPitchChange = false,
        .timeStretch = false,
        .pitchShift = false,
        .remoteSources = false
    };
}

OpenMptPlaybackDiagnostics OpenMptPlaybackBackend::diagnosticsSnapshot() const
{
    return OpenMptPlaybackDiagnostics{
        .audioSinkUnderruns = m_audioSinkUnderruns.load(std::memory_order_relaxed),
        .bufferStarvationCount = m_bufferStarvationCount.load(std::memory_order_relaxed),
        .decodeErrors = m_decodeErrors.load(std::memory_order_relaxed),
        .seekCount = m_seekCount.load(std::memory_order_relaxed),
        .lastSeekLatencyUs = m_lastSeekLatencyUs.load(std::memory_order_relaxed),
        .maxSeekLatencyUs = m_maxSeekLatencyUs.load(std::memory_order_relaxed),
        .preloadedNextCount = m_preloadedNextCount.load(std::memory_order_relaxed),
        .promotedPreloadedNextCount =
            m_promotedPreloadedNextCount.load(std::memory_order_relaxed),
        .lastPreloadLatencyUs = m_lastPreloadLatencyUs.load(std::memory_order_relaxed),
        .maxPreloadLatencyUs = m_maxPreloadLatencyUs.load(std::memory_order_relaxed),
        .audioDeviceChangeCount = m_audioDeviceChangeCount.load(std::memory_order_relaxed),
        .audioDeviceLossCount = m_audioDeviceLossCount.load(std::memory_order_relaxed),
        .audioSinkErrorCount = m_audioSinkErrorCount.load(std::memory_order_relaxed),
        .audioRecoveryAttemptCount =
            m_audioRecoveryAttemptCount.load(std::memory_order_relaxed),
        .audioRecoverySuccessCount =
            m_audioRecoverySuccessCount.load(std::memory_order_relaxed),
        .audioSafeStopCount = m_audioSafeStopCount.load(std::memory_order_relaxed),
        .audioDeviceReopenCount = m_audioDeviceReopenCount.load(std::memory_order_relaxed),
        .lastAudioDeviceStatus = audioDeviceStatus(),
        .decodeStarvationCount = m_decodeStarvationCount.load(std::memory_order_relaxed),
        .dspReconfigurationCount = m_dspReconfigurationCount.load(std::memory_order_relaxed),
        .lastDspReconfigurationLatencyUs =
            m_lastDspReconfigurationLatencyUs.load(std::memory_order_relaxed),
        .maxDspReconfigurationLatencyUs =
            m_maxDspReconfigurationLatencyUs.load(std::memory_order_relaxed),
        .lastDspReconfigurationType = [this]() {
            QMutexLocker locker(&m_dspDiagnosticsMutex);
            return m_lastDspReconfigurationType;
        }(),
        .rateActivationCount = m_rateActivationCount.load(std::memory_order_relaxed),
        .lastRateActivationLatencyUs =
            m_lastRateActivationLatencyUs.load(std::memory_order_relaxed),
        .maxRateActivationLatencyUs =
            m_maxRateActivationLatencyUs.load(std::memory_order_relaxed),
        .pitchActivationCount = m_pitchActivationCount.load(std::memory_order_relaxed),
        .lastPitchActivationLatencyUs =
            m_lastPitchActivationLatencyUs.load(std::memory_order_relaxed),
        .maxPitchActivationLatencyUs =
            m_maxPitchActivationLatencyUs.load(std::memory_order_relaxed),
        .reverseActivationCount = m_reverseActivationCount.load(std::memory_order_relaxed),
        .lastReverseActivationLatencyUs =
            m_lastReverseActivationLatencyUs.load(std::memory_order_relaxed),
        .maxReverseActivationLatencyUs =
            m_maxReverseActivationLatencyUs.load(std::memory_order_relaxed),
        .lastGaplessTransitionLatencyUs =
            m_lastGaplessTransitionLatencyUs.load(std::memory_order_relaxed),
        .maxGaplessTransitionLatencyUs =
            m_maxGaplessTransitionLatencyUs.load(std::memory_order_relaxed),
        .loadToFirstAudioFramesMs = m_loadToFirstAudioFramesMs.load(std::memory_order_relaxed),
        .renderCallbackMaxDurationUs = m_renderCallbackMaxDurationUs.load(std::memory_order_relaxed),
        .graphSnapshot = m_pcmEngine.snapshot()
    };
}

QVariantMap OpenMptPlaybackBackend::diagnosticsExportSnapshot() const
{
    const OpenMptPlaybackDiagnostics diagnostics = diagnosticsSnapshot();

    const QVariantList graphOrder{
        QStringLiteral("decode"),
        QStringLiteral("seekResetBoundary"),
        QStringLiteral("reverseRatePitch"),
        QStringLiteral("equalizer"),
        QStringLiteral("spectrumTap"),
        QStringLiteral("outputClipGuard")
    };

    return QVariantMap{
        {QStringLiteral("component"), QStringLiteral("OpenMptPlaybackBackend")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("timestampMs"), QDateTime::currentMSecsSinceEpoch()},
        {QStringLiteral("state"), static_cast<int>(m_state)},
        {QStringLiteral("durationMs"), m_durationMs},
        {QStringLiteral("positionMs"), position()},
        {QStringLiteral("metadata"), QVariantMap{
            {QStringLiteral("title"), m_metadata.title},
            {QStringLiteral("sourceFormat"), m_metadata.sourceFormat},
            {QStringLiteral("trackerType"), m_metadata.trackerType},
            {QStringLiteral("channels"), m_metadata.channelCount},
            {QStringLiteral("patterns"), m_metadata.patternCount},
            {QStringLiteral("instruments"), m_metadata.instrumentCount}
        }},
        {QStringLiteral("audio"), QVariantMap{
            {QStringLiteral("audioSinkUnderruns"), QVariant::fromValue<qulonglong>(diagnostics.audioSinkUnderruns)},
            {QStringLiteral("bufferStarvationCount"), QVariant::fromValue<qulonglong>(diagnostics.bufferStarvationCount)},
            {QStringLiteral("decodeStarvationCount"), QVariant::fromValue<qulonglong>(diagnostics.decodeStarvationCount)},
            {QStringLiteral("decodeErrors"), QVariant::fromValue<qulonglong>(diagnostics.decodeErrors)},
            {QStringLiteral("renderCallbackMaxDurationUs"), diagnostics.renderCallbackMaxDurationUs},
            {QStringLiteral("loadToFirstAudioFramesMs"), diagnostics.loadToFirstAudioFramesMs}
        }},
        {QStringLiteral("device"), QVariantMap{
            {QStringLiteral("audioDeviceChangeCount"), QVariant::fromValue<qulonglong>(diagnostics.audioDeviceChangeCount)},
            {QStringLiteral("audioDeviceLossCount"), QVariant::fromValue<qulonglong>(diagnostics.audioDeviceLossCount)},
            {QStringLiteral("audioDeviceReopenCount"), QVariant::fromValue<qulonglong>(diagnostics.audioDeviceReopenCount)},
            {QStringLiteral("audioSinkErrorCount"), QVariant::fromValue<qulonglong>(diagnostics.audioSinkErrorCount)},
            {QStringLiteral("audioRecoveryAttemptCount"), QVariant::fromValue<qulonglong>(diagnostics.audioRecoveryAttemptCount)},
            {QStringLiteral("audioRecoverySuccessCount"), QVariant::fromValue<qulonglong>(diagnostics.audioRecoverySuccessCount)},
            {QStringLiteral("audioSafeStopCount"), QVariant::fromValue<qulonglong>(diagnostics.audioSafeStopCount)},
            {QStringLiteral("lastAudioDeviceStatus"), diagnostics.lastAudioDeviceStatus}
        }},
        {QStringLiteral("seek"), QVariantMap{
            {QStringLiteral("seekCount"), QVariant::fromValue<qulonglong>(diagnostics.seekCount)},
            {QStringLiteral("lastSeekLatencyUs"), diagnostics.lastSeekLatencyUs},
            {QStringLiteral("maxSeekLatencyUs"), diagnostics.maxSeekLatencyUs}
        }},
        {QStringLiteral("gapless"), QVariantMap{
            {QStringLiteral("preloadedNextCount"), QVariant::fromValue<qulonglong>(diagnostics.preloadedNextCount)},
            {QStringLiteral("promotedPreloadedNextCount"), QVariant::fromValue<qulonglong>(diagnostics.promotedPreloadedNextCount)},
            {QStringLiteral("lastPreloadLatencyUs"), diagnostics.lastPreloadLatencyUs},
            {QStringLiteral("maxPreloadLatencyUs"), diagnostics.maxPreloadLatencyUs},
            {QStringLiteral("lastGaplessTransitionLatencyUs"), diagnostics.lastGaplessTransitionLatencyUs},
            {QStringLiteral("maxGaplessTransitionLatencyUs"), diagnostics.maxGaplessTransitionLatencyUs}
        }},
        {QStringLiteral("dsp"), QVariantMap{
            {QStringLiteral("dspReconfigurationCount"), QVariant::fromValue<qulonglong>(diagnostics.dspReconfigurationCount)},
            {QStringLiteral("lastDspReconfigurationLatencyUs"), diagnostics.lastDspReconfigurationLatencyUs},
            {QStringLiteral("maxDspReconfigurationLatencyUs"), diagnostics.maxDspReconfigurationLatencyUs},
            {QStringLiteral("lastDspReconfigurationType"), diagnostics.lastDspReconfigurationType},
            {QStringLiteral("rateActivationCount"), QVariant::fromValue<qulonglong>(diagnostics.rateActivationCount)},
            {QStringLiteral("lastRateActivationLatencyUs"), diagnostics.lastRateActivationLatencyUs},
            {QStringLiteral("maxRateActivationLatencyUs"), diagnostics.maxRateActivationLatencyUs},
            {QStringLiteral("pitchActivationCount"), QVariant::fromValue<qulonglong>(diagnostics.pitchActivationCount)},
            {QStringLiteral("lastPitchActivationLatencyUs"), diagnostics.lastPitchActivationLatencyUs},
            {QStringLiteral("maxPitchActivationLatencyUs"), diagnostics.maxPitchActivationLatencyUs},
            {QStringLiteral("reverseActivationCount"), QVariant::fromValue<qulonglong>(diagnostics.reverseActivationCount)},
            {QStringLiteral("lastReverseActivationLatencyUs"), diagnostics.lastReverseActivationLatencyUs},
            {QStringLiteral("maxReverseActivationLatencyUs"), diagnostics.maxReverseActivationLatencyUs}
        }},
        {QStringLiteral("graphOrder"), graphOrder},
        {QStringLiteral("graph"), graphSnapshotToVariantMap(diagnostics.graphSnapshot)}
    };
}

bool OpenMptPlaybackBackend::preloadNext(const QString &source)
{
    if (preloadedNextMatches(source)) {
        return true;
    }

    QString errorMessage;
    QElapsedTimer preloadTimer;
    preloadTimer.start();
    std::unique_ptr<LoadedTrackerModule> loaded = loadTrackerModuleForSource(source, &errorMessage);
    const qint64 latencyUs = preloadTimer.nsecsElapsed() / 1000;
    m_lastPreloadLatencyUs.store(latencyUs, std::memory_order_relaxed);
    storeMax(&m_maxPreloadLatencyUs, latencyUs);

    if (!loaded) {
        clearPreloadedNext();
        traceDiagnostics("preload_next_failed", errorMessage);
        return false;
    }

    m_preloadedNext = std::move(loaded);
    m_preloadedNextCount.fetch_add(1, std::memory_order_relaxed);
    traceDiagnostics("preload_next_finished",
                     QStringLiteral("path=%1 latencyUs=%2")
                         .arg(m_preloadedNext->localPath)
                         .arg(latencyUs));
    return true;
}

bool OpenMptPlaybackBackend::hasPreloadedNext() const
{
    return m_preloadedNext != nullptr;
}

void OpenMptPlaybackBackend::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.25);
    if (m_audioSink) {
        m_audioSink->setVolume(m_volume);
    }
}

void OpenMptPlaybackBackend::setPlaybackRate(double playbackRate)
{
    if (!m_pcmEngine.supportsTimeStretch()) {
        return;
    }

    const qint64 currentPositionMs = position();
    QElapsedTimer dspTimer;
    dspTimer.start();
    m_pcmEngine.setPlaybackRate(playbackRate);
    const qint64 latencyUs = dspTimer.nsecsElapsed() / 1000;
    recordDspReconfiguration(QStringLiteral("rate"), latencyUs);
    updatePosition(currentPositionMs);
    m_lastEmittedPositionMs = currentPositionMs;
    emit positionChanged(currentPositionMs);
    m_endOfStreamPending.store(false);

    if (m_state == PlaybackBackendState::Playing || m_state == PlaybackBackendState::Paused) {
        const bool startSuspended = (m_state == PlaybackBackendState::Paused);
        startAudioSink(startSuspended);
        if (startSuspended) {
            m_positionTimer.stop();
        } else {
            m_positionTimer.start();
        }
    }

    traceDiagnostics("rate_changed",
                     QStringLiteral("playbackRate=%1 positionMs=%2")
                         .arg(playbackRate, 0, 'f', 2)
                         .arg(currentPositionMs)
                         + QStringLiteral(" latencyUs=%1").arg(latencyUs));
}

void OpenMptPlaybackBackend::setPitchSemitones(int pitchSemitones)
{
    if (!m_pcmEngine.supportsPitchShift()) {
        return;
    }

    const qint64 currentPositionMs = position();
    QElapsedTimer dspTimer;
    dspTimer.start();
    m_pcmEngine.setPitchSemitones(pitchSemitones);
    const qint64 latencyUs = dspTimer.nsecsElapsed() / 1000;
    recordDspReconfiguration(QStringLiteral("pitch"), latencyUs);
    updatePosition(currentPositionMs);
    m_lastEmittedPositionMs = currentPositionMs;
    emit positionChanged(currentPositionMs);
    m_endOfStreamPending.store(false);

    if (m_state == PlaybackBackendState::Playing || m_state == PlaybackBackendState::Paused) {
        const bool startSuspended = (m_state == PlaybackBackendState::Paused);
        startAudioSink(startSuspended);
        if (startSuspended) {
            m_positionTimer.stop();
        } else {
            m_positionTimer.start();
        }
    }

    traceDiagnostics("pitch_changed",
                     QStringLiteral("pitchSemitones=%1 positionMs=%2")
                         .arg(pitchSemitones)
                         .arg(currentPositionMs)
                         + QStringLiteral(" latencyUs=%1").arg(latencyUs));
}

void OpenMptPlaybackBackend::setReversePlayback(bool reversePlayback)
{
    if (!m_pcmEngine.supportsReversePlayback()) {
        return;
    }

    qint64 currentPositionMs = position();
    if (reversePlayback && currentPositionMs <= 0 && m_durationMs > 0) {
        currentPositionMs = m_durationMs;
    }

    QElapsedTimer dspTimer;
    dspTimer.start();
    m_pcmEngine.setReversePlayback(reversePlayback);
    const qint64 latencyUs = dspTimer.nsecsElapsed() / 1000;
    recordDspReconfiguration(QStringLiteral("reverse"), latencyUs);
    updatePosition(currentPositionMs);
    m_lastEmittedPositionMs = currentPositionMs;
    emit positionChanged(currentPositionMs);
    m_endOfStreamPending.store(false);

    if (m_state == PlaybackBackendState::Playing || m_state == PlaybackBackendState::Paused) {
        const bool startSuspended = (m_state == PlaybackBackendState::Paused);
        startAudioSink(startSuspended);
        if (startSuspended) {
            m_positionTimer.stop();
        } else {
            m_positionTimer.start();
        }
    }

    traceDiagnostics("reverse_changed",
                     QStringLiteral("reversePlayback=%1 positionMs=%2")
                         .arg(reversePlayback ? QStringLiteral("true") : QStringLiteral("false"))
                         .arg(currentPositionMs)
                         + QStringLiteral(" latencyUs=%1").arg(latencyUs));
}

void OpenMptPlaybackBackend::setPcmTapEnabled(bool enabled)
{
    m_pcmTapEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        m_lastPcmTapWallClockMs.store(0, std::memory_order_relaxed);
    }
}

void OpenMptPlaybackBackend::setEqualizerBands(const std::vector<double> &frequenciesHz,
                                               const std::vector<double> &gainsDb)
{
    QElapsedTimer dspTimer;
    dspTimer.start();
    m_pcmEngine.setEqualizerBands(frequenciesHz, gainsDb);
    recordDspReconfiguration(QStringLiteral("equalizer"), dspTimer.nsecsElapsed() / 1000);
}

void OpenMptPlaybackBackend::emitPositionIfChanged()
{
    const qint64 currentPositionMs = m_positionMs.load();
    if (currentPositionMs == m_lastEmittedPositionMs) {
        return;
    }

    m_lastEmittedPositionMs = currentPositionMs;
    emit positionChanged(currentPositionMs);
}

void OpenMptPlaybackBackend::handleAudioSinkStateChanged(QAudio::State state)
{
    if (!m_audioSink) {
        return;
    }

    if (state == QAudio::IdleState && m_endOfStreamPending.exchange(false)) {
        teardownAudioSink();
        updatePosition(m_durationMs);
        m_lastEmittedPositionMs = m_durationMs;
        emit positionChanged(m_durationMs);
        setState(PlaybackBackendState::Ended);
        emit endOfStream();
        return;
    }

    if (state == QAudio::IdleState) {
        m_audioSinkUnderruns.fetch_add(1, std::memory_order_relaxed);
        m_bufferStarvationCount.fetch_add(1, std::memory_order_relaxed);
        traceDiagnostics("audio_sink_underrun");
        return;
    }

    if (state == QAudio::StoppedState && m_audioSink->error() != QtAudio::NoError) {
        recoverFromAudioSinkError(
            QStringLiteral("Tracker audio output stopped with QtMultimedia error %1.")
                .arg(static_cast<int>(m_audioSink->error())));
    }
}

void OpenMptPlaybackBackend::handleAudioOutputsChanged()
{
    handleDefaultAudioOutputChanged(QMediaDevices::defaultAudioOutput());
}

void OpenMptPlaybackBackend::handleDefaultAudioOutputChanged(const QAudioDevice &device)
{
    if (device.isNull()) {
        if (m_state == PlaybackBackendState::Playing || m_state == PlaybackBackendState::Paused) {
            safeStopForAudioOutputLoss(
                QStringLiteral("Default audio output device is unavailable for tracker playback."));
        } else {
            m_audioDeviceLossCount.fetch_add(1, std::memory_order_relaxed);
            setAudioDeviceStatus(QStringLiteral("Default audio output device is unavailable."));
            traceDiagnostics("audio_device_unavailable");
        }
        return;
    }

    m_audioDeviceChangeCount.fetch_add(1, std::memory_order_relaxed);
    const QString status = QStringLiteral("Default audio output device available: %1")
                               .arg(device.description());
    setAudioDeviceStatus(status);
    traceDiagnostics("audio_device_changed", status);

    if (!m_pcmEngine.hasModule()) {
        return;
    }

    if (m_resumeAfterAudioDeviceRecovery) {
        const PlaybackBackendState targetState = m_recoverySuspendedAfterDeviceRecovery
            ? PlaybackBackendState::Paused
            : PlaybackBackendState::Playing;
        restartAudioSinkAfterDeviceChange(targetState,
                                          QStringLiteral("default_audio_output_recovered"));
        return;
    }

    if (m_state == PlaybackBackendState::Playing || m_state == PlaybackBackendState::Paused) {
        restartAudioSinkAfterDeviceChange(m_state,
                                          QStringLiteral("default_audio_output_changed"));
    }
}

QString OpenMptPlaybackBackend::resolveLocalTrackerPath(const QString &source, QString *errorMessage) const
{
    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker source path is empty.");
        }
        return {};
    }

    const QUrl sourceUrl(trimmedSource);
    QString localPath;
    if (isWindowsDriveLetterPath(trimmedSource) || isWindowsUncPath(trimmedSource)) {
        localPath = trimmedSource;
    } else if (isSlashPrefixedWindowsDriveLetterPath(trimmedSource)) {
        localPath = trimmedSource.mid(1);
    } else if (sourceUrl.isValid() && !sourceUrl.scheme().isEmpty()) {
        if (!sourceUrl.isLocalFile()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "OpenMPT playback supports only local files and file:// URLs in v1.");
            }
            return {};
        }

        localPath = sourceUrl.toLocalFile();
    } else {
        localPath = trimmedSource;
    }

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker source does not exist: %1").arg(localPath);
        }
        return {};
    }

    if (!fileInfo.isReadable()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker source is not readable: %1").arg(fileInfo.absoluteFilePath());
        }
        return {};
    }

    if (!isTrackerModuleSource(fileInfo.absoluteFilePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source is not a supported tracker module: %1")
                                .arg(fileInfo.absoluteFilePath());
        }
        return {};
    }

    return fileInfo.absoluteFilePath();
}

std::unique_ptr<OpenMptPlaybackBackend::LoadedTrackerModule>
OpenMptPlaybackBackend::loadTrackerModuleForSource(const QString &source, QString *errorMessage)
{
    const QString localPath = resolveLocalTrackerPath(source, errorMessage);
    if (localPath.isEmpty()) {
        return {};
    }

    auto loaded = std::make_unique<LoadedTrackerModule>();
    loaded->source = source;
    loaded->localPath = localPath;
    loaded->stream = std::make_unique<std::ifstream>(localPath.toStdString(), std::ios::binary);
    if (!loaded->stream->is_open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 File could not be opened for reading.")
                                .arg(trackerLoadErrorPrefix(localPath));
        }
        return {};
    }

    loaded->logStream = std::make_unique<std::ostringstream>();
    try {
        loaded->module =
            std::make_unique<openmpt::module>(*loaded->stream, *loaded->logStream);
        loaded->module->set_repeat_count(0);
    } catch (const openmpt::exception &exception) {
        m_decodeErrors.fetch_add(1, std::memory_order_relaxed);
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 %2")
                                .arg(trackerLoadErrorPrefix(localPath),
                                     QString::fromUtf8(exception.what()));
        }
        return {};
    } catch (const std::exception &exception) {
        m_decodeErrors.fetch_add(1, std::memory_order_relaxed);
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 %2")
                                .arg(trackerLoadErrorPrefix(localPath),
                                     QString::fromUtf8(exception.what()));
        }
        return {};
    } catch (...) {
        m_decodeErrors.fetch_add(1, std::memory_order_relaxed);
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("%1 Unknown libopenmpt error.").arg(trackerLoadErrorPrefix(localPath));
        }
        return {};
    }

    loaded->metadata.title = QString::fromUtf8(loaded->module->get_metadata("title").c_str());
    if (loaded->metadata.title.isEmpty()) {
        loaded->metadata.title = QFileInfo(localPath).completeBaseName();
    }
    loaded->metadata.artist = QString::fromUtf8(loaded->module->get_metadata("artist").c_str());
    loaded->metadata.sourceFormat = QFileInfo(localPath).suffix().trimmed().toLower();
    loaded->metadata.trackerType =
        QString::fromUtf8(loaded->module->get_metadata("type").c_str()).trimmed();
    loaded->metadata.trackerMessage =
        QString::fromUtf8(loaded->module->get_metadata("message_raw").c_str()).trimmed();
    loaded->metadata.channelCount = loaded->module->get_num_channels();
    loaded->metadata.patternCount = loaded->module->get_num_patterns();
    loaded->metadata.instrumentCount = loaded->module->get_num_instruments();
    loaded->durationMs = secondsToMs(loaded->module->get_duration_seconds());
    return loaded;
}

void OpenMptPlaybackBackend::adoptLoadedModule(std::unique_ptr<LoadedTrackerModule> loaded)
{
    if (!loaded) {
        return;
    }

    m_metadata = std::move(loaded->metadata);
    m_durationMs = loaded->durationMs;
    m_pcmEngine.load(std::move(loaded->module),
                     std::move(loaded->stream),
                     std::move(loaded->logStream),
                     m_durationMs);
}

bool OpenMptPlaybackBackend::preloadedNextMatches(const QString &source) const
{
    if (!m_preloadedNext) {
        return false;
    }

    QString errorMessage;
    const QString localPath = resolveLocalTrackerPath(source, &errorMessage);
    return !localPath.isEmpty() && localPath == m_preloadedNext->localPath;
}

void OpenMptPlaybackBackend::clearPreloadedNext()
{
    m_preloadedNext.reset();
}

void OpenMptPlaybackBackend::fail(const QString &message, bool clearLoadedState)
{
    traceDiagnostics("error", message);
    teardownAudioSink();
    if (clearLoadedState) {
        clearLoadedModule();
    }
    setState(PlaybackBackendState::Error, true);
    emit error(message);
}

void OpenMptPlaybackBackend::clearLoadedModule()
{
    m_pcmEngine.clear();

    m_metadata = {};
    m_durationMs = 0;
    updatePosition(0);
    m_endOfStreamPending.store(false);
}

bool OpenMptPlaybackBackend::startAudioSink(bool startSuspended)
{
    teardownAudioSink();

    const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull()) {
        m_audioDeviceLossCount.fetch_add(1, std::memory_order_relaxed);
        m_audioSafeStopCount.fetch_add(1, std::memory_order_relaxed);
        setAudioDeviceStatus(
            QStringLiteral("No default audio output device is available for tracker playback."));
        fail(QStringLiteral("No default audio output device is available for tracker playback."), false);
        return false;
    }

    const QAudioFormat format = openMptAudioFormat();
    if (!outputDevice.isFormatSupported(format)) {
        m_audioSafeStopCount.fetch_add(1, std::memory_order_relaxed);
        setAudioDeviceStatus(QStringLiteral(
            "Default audio output does not support tracker PCM format."));
        fail(QStringLiteral(
                 "Default audio output does not support the required tracker PCM format (48000 Hz, stereo, int16)."),
             false);
        return false;
    }

    m_audioDevice = std::make_unique<OpenMptRenderDevice>(this);
    if (!m_audioDevice->open(QIODevice::ReadOnly)) {
        m_audioDevice.reset();
        m_audioSinkErrorCount.fetch_add(1, std::memory_order_relaxed);
        setAudioDeviceStatus(QStringLiteral("Failed to open tracker PCM render device."));
        fail(QStringLiteral("Failed to open tracker PCM render device."), false);
        return false;
    }

    m_audioSink = std::make_unique<QAudioSink>(outputDevice, format, this);
    connect(m_audioSink.get(), &QAudioSink::stateChanged,
            this, &OpenMptPlaybackBackend::handleAudioSinkStateChanged);
    m_audioSink->setVolume(m_volume);
    m_endOfStreamPending.store(false);
    m_audioSink->start(m_audioDevice.get());

    if (m_audioSink->state() == QAudio::StoppedState && m_audioSink->error() != QtAudio::NoError) {
        const QString errorMessage = QStringLiteral(
            "Failed to start QtMultimedia audio sink for tracker playback (error %1).")
                                         .arg(static_cast<int>(m_audioSink->error()));
        teardownAudioSink();
        m_audioSinkErrorCount.fetch_add(1, std::memory_order_relaxed);
        setAudioDeviceStatus(errorMessage);
        fail(errorMessage, false);
        return false;
    }

    if (startSuspended) {
        m_audioSink->suspend();
    }

    m_audioDeviceReopenCount.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void OpenMptPlaybackBackend::safeStopForAudioOutputLoss(const QString &message)
{
    const bool wasPlaying = (m_state == PlaybackBackendState::Playing);
    const bool wasPaused = (m_state == PlaybackBackendState::Paused);
    m_resumeAfterAudioDeviceRecovery = wasPlaying || wasPaused;
    m_recoverySuspendedAfterDeviceRecovery = wasPaused;
    m_audioDeviceLossCount.fetch_add(1, std::memory_order_relaxed);
    m_audioSafeStopCount.fetch_add(1, std::memory_order_relaxed);
    setAudioDeviceStatus(message);
    traceDiagnostics("audio_device_safe_stop", message);

    teardownAudioSink();
    if (m_pcmEngine.hasModule()) {
        setState(PlaybackBackendState::Paused, true);
    } else {
        setState(PlaybackBackendState::Stopped, true);
    }
    emit error(message);
}

bool OpenMptPlaybackBackend::restartAudioSinkAfterDeviceChange(PlaybackBackendState targetState,
                                                               const QString &reason)
{
    if (!m_pcmEngine.hasModule()) {
        return false;
    }

    m_audioRecoveryAttemptCount.fetch_add(1, std::memory_order_relaxed);
    traceDiagnostics("audio_device_recovery_started", reason);
    const bool startSuspended = (targetState == PlaybackBackendState::Paused);
    if (!startAudioSink(startSuspended)) {
        return false;
    }

    if (startSuspended) {
        m_positionTimer.stop();
    } else {
        m_positionTimer.start();
    }

    m_resumeAfterAudioDeviceRecovery = false;
    m_recoverySuspendedAfterDeviceRecovery = false;
    m_audioRecoverySuccessCount.fetch_add(1, std::memory_order_relaxed);
    setAudioDeviceStatus(QStringLiteral("Tracker audio output recovered."));
    setState(targetState, true);
    traceDiagnostics("audio_device_recovery_finished", reason);
    return true;
}

bool OpenMptPlaybackBackend::recoverFromAudioSinkError(const QString &message)
{
    m_audioSinkErrorCount.fetch_add(1, std::memory_order_relaxed);
    setAudioDeviceStatus(message);
    traceDiagnostics("audio_sink_error", message);

    const PlaybackBackendState targetState =
        (m_state == PlaybackBackendState::Paused) ? PlaybackBackendState::Paused
                                                  : PlaybackBackendState::Playing;
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        safeStopForAudioOutputLoss(message);
        return false;
    }

    if (restartAudioSinkAfterDeviceChange(targetState, QStringLiteral("sink_error_recovery"))) {
        return true;
    }

    m_audioSafeStopCount.fetch_add(1, std::memory_order_relaxed);
    return false;
}

void OpenMptPlaybackBackend::teardownAudioSink()
{
    m_positionTimer.stop();
    m_endOfStreamPending.store(false);

    if (m_audioSink) {
        disconnect(m_audioSink.get(), nullptr, this, nullptr);
        m_audioSink->stop();
        m_audioSink.reset();
    }

    if (m_audioDevice) {
        m_audioDevice->close();
        m_audioDevice.reset();
    }
}

void OpenMptPlaybackBackend::setState(PlaybackBackendState newState, bool forceSignal)
{
    if (!forceSignal && m_state == newState) {
        return;
    }

    m_state = newState;
    emit stateChanged(m_state);
}

qint64 OpenMptPlaybackBackend::readAudioData(char *data, qint64 maxSize)
{
    QElapsedTimer renderTimer;
    renderTimer.start();
    auto finishRenderTiming = [this, &renderTimer]() {
        storeMax(&m_renderCallbackMaxDurationUs, renderTimer.nsecsElapsed() / 1000);
    };

    if (maxSize < TrackerPcmEngine::kOutputBytesPerFrame) {
        m_bufferStarvationCount.fetch_add(1, std::memory_order_relaxed);
        finishRenderTiming();
        return 0;
    }

    if (!m_pcmEngine.hasModule()) {
        m_bufferStarvationCount.fetch_add(1, std::memory_order_relaxed);
        finishRenderTiming();
        return 0;
    }

    const qint64 bytesRendered = m_pcmEngine.readInt16(data, maxSize);

    if (bytesRendered > 0) {
        maybeEmitPcmTap(data, bytesRendered);
        updatePosition(m_pcmEngine.positionMs());
        const bool firstFrames = !m_firstAudioFramesObserved.exchange(true, std::memory_order_relaxed);
        if (firstFrames) {
            const qint64 loadStartMs = m_loadStartWallClockMs.load(std::memory_order_relaxed);
            if (loadStartMs > 0) {
                m_loadToFirstAudioFramesMs.store(qMax<qint64>(
                                                     0,
                                                     QDateTime::currentMSecsSinceEpoch() - loadStartMs),
                                                 std::memory_order_relaxed);
            }
            traceDiagnostics("first_audio_frames");
        }
        finishRenderTiming();
        return bytesRendered;
    }

    if (!m_pcmEngine.isEndOfStream()) {
        m_bufferStarvationCount.fetch_add(1, std::memory_order_relaxed);
        m_decodeStarvationCount.fetch_add(1, std::memory_order_relaxed);
        finishRenderTiming();
        return 0;
    }

    updatePosition(m_durationMs);
    m_endOfStreamPending.store(true);
    finishRenderTiming();
    return 0;
}

qint64 OpenMptPlaybackBackend::audioBytesAvailable() const
{
    return qMax<qint64>(m_pcmEngine.availableOutputBytes(), 16384);
}

void OpenMptPlaybackBackend::maybeEmitPcmTap(const char *data, qint64 bytesRendered)
{
    if (!data || bytesRendered <= 0 || !m_pcmTapEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 previousMs = m_lastPcmTapWallClockMs.load(std::memory_order_relaxed);
    if (previousMs > 0 && nowMs - previousMs < kPcmTapIntervalMs) {
        return;
    }
    if (!m_lastPcmTapWallClockMs.compare_exchange_strong(previousMs,
                                                         nowMs,
                                                         std::memory_order_relaxed,
                                                         std::memory_order_relaxed)) {
        return;
    }

    const qint64 renderedFrames = bytesRendered / TrackerPcmEngine::kOutputBytesPerFrame;
    if (renderedFrames <= 0) {
        return;
    }

    const qint64 framesToCopy = qMin(renderedFrames, kPcmTapMaxFrames);
    const qint64 startFrame = renderedFrames - framesToCopy;
    const auto *samples = reinterpret_cast<const std::int16_t *>(data);

    QVector<float> monoSamples;
    monoSamples.reserve(static_cast<int>(framesToCopy));
    constexpr float kInt16Scale = 1.0f / 32768.0f;
    for (qint64 frame = startFrame; frame < renderedFrames; ++frame) {
        const qint64 base = frame * TrackerPcmEngine::kChannelCount;
        const float left = static_cast<float>(samples[base]) * kInt16Scale;
        const float right = static_cast<float>(samples[base + 1]) * kInt16Scale;
        monoSamples.push_back((left + right) * 0.5f);
    }

    if (!monoSamples.isEmpty()) {
        emit pcmFramesReady(std::move(monoSamples), TrackerPcmEngine::kSampleRate);
    }
}

void OpenMptPlaybackBackend::updatePosition(qint64 positionMs)
{
    const qint64 sanitizedPositionMs = qBound<qint64>(0, positionMs, m_durationMs);
    m_positionMs.store(sanitizedPositionMs);
}

void OpenMptPlaybackBackend::resetDiagnostics()
{
    m_firstAudioFramesObserved.store(false, std::memory_order_relaxed);
    m_audioSinkUnderruns.store(0, std::memory_order_relaxed);
    m_bufferStarvationCount.store(0, std::memory_order_relaxed);
    m_decodeErrors.store(0, std::memory_order_relaxed);
    m_seekCount.store(0, std::memory_order_relaxed);
    m_lastSeekLatencyUs.store(0, std::memory_order_relaxed);
    m_maxSeekLatencyUs.store(0, std::memory_order_relaxed);
    m_audioDeviceChangeCount.store(0, std::memory_order_relaxed);
    m_audioDeviceLossCount.store(0, std::memory_order_relaxed);
    m_audioSinkErrorCount.store(0, std::memory_order_relaxed);
    m_audioRecoveryAttemptCount.store(0, std::memory_order_relaxed);
    m_audioRecoverySuccessCount.store(0, std::memory_order_relaxed);
    m_audioSafeStopCount.store(0, std::memory_order_relaxed);
    m_audioDeviceReopenCount.store(0, std::memory_order_relaxed);
    m_decodeStarvationCount.store(0, std::memory_order_relaxed);
    m_dspReconfigurationCount.store(0, std::memory_order_relaxed);
    m_lastDspReconfigurationLatencyUs.store(0, std::memory_order_relaxed);
    m_maxDspReconfigurationLatencyUs.store(0, std::memory_order_relaxed);
    m_rateActivationCount.store(0, std::memory_order_relaxed);
    m_lastRateActivationLatencyUs.store(0, std::memory_order_relaxed);
    m_maxRateActivationLatencyUs.store(0, std::memory_order_relaxed);
    m_pitchActivationCount.store(0, std::memory_order_relaxed);
    m_lastPitchActivationLatencyUs.store(0, std::memory_order_relaxed);
    m_maxPitchActivationLatencyUs.store(0, std::memory_order_relaxed);
    m_reverseActivationCount.store(0, std::memory_order_relaxed);
    m_lastReverseActivationLatencyUs.store(0, std::memory_order_relaxed);
    m_maxReverseActivationLatencyUs.store(0, std::memory_order_relaxed);
    m_lastGaplessTransitionLatencyUs.store(0, std::memory_order_relaxed);
    m_maxGaplessTransitionLatencyUs.store(0, std::memory_order_relaxed);
    m_loadStartWallClockMs.store(0, std::memory_order_relaxed);
    m_loadToFirstAudioFramesMs.store(-1, std::memory_order_relaxed);
    m_renderCallbackMaxDurationUs.store(0, std::memory_order_relaxed);
    setAudioDeviceStatus({});
    {
        QMutexLocker locker(&m_dspDiagnosticsMutex);
        m_lastDspReconfigurationType.clear();
    }
    m_resumeAfterAudioDeviceRecovery = false;
    m_recoverySuspendedAfterDeviceRecovery = false;
}

void OpenMptPlaybackBackend::recordDspReconfiguration(const QString &type, qint64 latencyUs)
{
    const qint64 sanitizedLatencyUs = qMax<qint64>(0, latencyUs);
    m_dspReconfigurationCount.fetch_add(1, std::memory_order_relaxed);
    m_lastDspReconfigurationLatencyUs.store(sanitizedLatencyUs, std::memory_order_relaxed);
    storeMax(&m_maxDspReconfigurationLatencyUs, sanitizedLatencyUs);
    {
        QMutexLocker locker(&m_dspDiagnosticsMutex);
        m_lastDspReconfigurationType = type;
    }

    if (type == QStringLiteral("rate")) {
        m_rateActivationCount.fetch_add(1, std::memory_order_relaxed);
        m_lastRateActivationLatencyUs.store(sanitizedLatencyUs, std::memory_order_relaxed);
        storeMax(&m_maxRateActivationLatencyUs, sanitizedLatencyUs);
    } else if (type == QStringLiteral("pitch")) {
        m_pitchActivationCount.fetch_add(1, std::memory_order_relaxed);
        m_lastPitchActivationLatencyUs.store(sanitizedLatencyUs, std::memory_order_relaxed);
        storeMax(&m_maxPitchActivationLatencyUs, sanitizedLatencyUs);
    } else if (type == QStringLiteral("reverse")) {
        m_reverseActivationCount.fetch_add(1, std::memory_order_relaxed);
        m_lastReverseActivationLatencyUs.store(sanitizedLatencyUs, std::memory_order_relaxed);
        storeMax(&m_maxReverseActivationLatencyUs, sanitizedLatencyUs);
    }
}

void OpenMptPlaybackBackend::setAudioDeviceStatus(const QString &status)
{
    QMutexLocker locker(&m_audioDeviceStatusMutex);
    m_audioDeviceStatus = status;
}

QString OpenMptPlaybackBackend::audioDeviceStatus() const
{
    QMutexLocker locker(&m_audioDeviceStatusMutex);
    return m_audioDeviceStatus;
}

void OpenMptPlaybackBackend::traceDiagnostics(const char *event) const
{
    traceDiagnostics(event, {});
}

void OpenMptPlaybackBackend::traceDiagnostics(const char *event, const QString &detail) const
{
    if (!trackerDiagnosticsEnabled()) {
        return;
    }

    QVariantMap snapshot = diagnosticsExportSnapshot();
    QJsonObject payload = QJsonObject::fromVariantMap(snapshot);
    payload.insert(QStringLiteral("event"), QString::fromLatin1(event));
    payload.insert(QStringLiteral("tsMs"), QDateTime::currentMSecsSinceEpoch());
    if (!detail.isEmpty()) {
        payload.insert(QStringLiteral("detail"), detail);
    }

    qInfo().noquote() << "[TrackerDiag]"
                      << QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

bool OpenMptPlaybackBackend::trackerDiagnosticsEnabled()
{
    return qEnvironmentVariableIntValue("WAVEFLUX_TRACKER_DIAG") != 0;
}

} // namespace WaveFlux
