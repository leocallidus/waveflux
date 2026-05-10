#ifndef OPENMPTPLAYBACKBACKEND_H
#define OPENMPTPLAYBACKBACKEND_H

#include <QMutex>
#include <QMediaDevices>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <QtMultimedia/qtaudio.h>

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <vector>

#include "playback/IPlaybackBackend.h"
#include "playback/TrackerPcmEngine.h"

QT_BEGIN_NAMESPACE
class QAudioSink;
QT_END_NAMESPACE

namespace WaveFlux {

class OpenMptRenderDevice;

struct OpenMptPlaybackDiagnostics {
    quint64 audioSinkUnderruns = 0;
    quint64 bufferStarvationCount = 0;
    quint64 decodeErrors = 0;
    quint64 seekCount = 0;
    qint64 lastSeekLatencyUs = 0;
    qint64 maxSeekLatencyUs = 0;
    quint64 preloadedNextCount = 0;
    quint64 promotedPreloadedNextCount = 0;
    qint64 lastPreloadLatencyUs = 0;
    qint64 maxPreloadLatencyUs = 0;
    quint64 audioDeviceChangeCount = 0;
    quint64 audioDeviceLossCount = 0;
    quint64 audioSinkErrorCount = 0;
    quint64 audioRecoveryAttemptCount = 0;
    quint64 audioRecoverySuccessCount = 0;
    quint64 audioSafeStopCount = 0;
    quint64 audioDeviceReopenCount = 0;
    QString lastAudioDeviceStatus;
    quint64 decodeStarvationCount = 0;
    quint64 dspReconfigurationCount = 0;
    qint64 lastDspReconfigurationLatencyUs = 0;
    qint64 maxDspReconfigurationLatencyUs = 0;
    QString lastDspReconfigurationType;
    quint64 rateActivationCount = 0;
    qint64 lastRateActivationLatencyUs = 0;
    qint64 maxRateActivationLatencyUs = 0;
    quint64 pitchActivationCount = 0;
    qint64 lastPitchActivationLatencyUs = 0;
    qint64 maxPitchActivationLatencyUs = 0;
    quint64 reverseActivationCount = 0;
    qint64 lastReverseActivationLatencyUs = 0;
    qint64 maxReverseActivationLatencyUs = 0;
    qint64 lastGaplessTransitionLatencyUs = 0;
    qint64 maxGaplessTransitionLatencyUs = 0;
    qint64 loadToFirstAudioFramesMs = -1;
    qint64 renderCallbackMaxDurationUs = 0;
    TrackerPcmEngineSnapshot graphSnapshot;
};

class OpenMptPlaybackBackend final : public IPlaybackBackend
{
    Q_OBJECT

public:
    explicit OpenMptPlaybackBackend(QObject *parent = nullptr);
    ~OpenMptPlaybackBackend() override;

    void load(const QString &source) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 positionMs) override;
    qint64 position() const override;
    qint64 duration() const override;
    PlaybackBackendState state() const override;
    PlaybackMetadata metadata() const override;
    PlaybackBackendCapabilities capabilities() const override;
    OpenMptPlaybackDiagnostics diagnosticsSnapshot() const;
    QVariantMap diagnosticsExportSnapshot() const;
    bool preloadNext(const QString &source);
    bool hasPreloadedNext() const;
    void clearPreloadedNext();
    void setVolume(double volume) override;
    void setPlaybackRate(double playbackRate);
    void setPitchSemitones(int pitchSemitones);
    void setReversePlayback(bool reversePlayback);
    void setPcmTapEnabled(bool enabled);
    void setEqualizerBands(const std::vector<double> &frequenciesHz,
                           const std::vector<double> &gainsDb);

signals:
    void pcmFramesReady(QVector<float> monoSamples, int sampleRate);

private slots:
    void emitPositionIfChanged();
    void handleAudioSinkStateChanged(QAudio::State state);
    void handleAudioOutputsChanged();
    void handleDefaultAudioOutputChanged(const QAudioDevice &device);

private:
    friend class OpenMptRenderDevice;
    struct LoadedTrackerModule;

    QString resolveLocalTrackerPath(const QString &source, QString *errorMessage) const;
    std::unique_ptr<LoadedTrackerModule> loadTrackerModuleForSource(const QString &source,
                                                                    QString *errorMessage);
    void adoptLoadedModule(std::unique_ptr<LoadedTrackerModule> loaded);
    bool preloadedNextMatches(const QString &source) const;
    void fail(const QString &message, bool clearLoadedState = true);
    void clearLoadedModule();
    bool startAudioSink(bool startSuspended);
    void teardownAudioSink();
    void safeStopForAudioOutputLoss(const QString &message);
    bool restartAudioSinkAfterDeviceChange(PlaybackBackendState targetState, const QString &reason);
    bool recoverFromAudioSinkError(const QString &message);
    void setState(PlaybackBackendState newState, bool forceSignal = false);
    qint64 readAudioData(char *data, qint64 maxSize);
    qint64 audioBytesAvailable() const;
    void maybeEmitPcmTap(const char *data, qint64 bytesRendered);
    void updatePosition(qint64 positionMs);
    void resetDiagnostics();
    void recordDspReconfiguration(const QString &type, qint64 latencyUs);
    void setAudioDeviceStatus(const QString &status);
    QString audioDeviceStatus() const;
    void traceDiagnostics(const char *event) const;
    void traceDiagnostics(const char *event, const QString &detail) const;
    static bool trackerDiagnosticsEnabled();

    std::unique_ptr<OpenMptRenderDevice> m_audioDevice;
    std::unique_ptr<QAudioSink> m_audioSink;
    QMediaDevices m_mediaDevices;
    TrackerPcmEngine m_pcmEngine;
    QTimer m_positionTimer;
    PlaybackMetadata m_metadata;
    std::atomic<qint64> m_positionMs {0};
    qint64 m_durationMs = 0;
    qint64 m_lastEmittedPositionMs = 0;
    PlaybackBackendState m_state = PlaybackBackendState::Stopped;
    double m_volume = 1.0;
    std::atomic_bool m_endOfStreamPending {false};
    std::atomic_bool m_firstAudioFramesObserved {false};
    std::atomic<quint64> m_audioSinkUnderruns {0};
    std::atomic<quint64> m_bufferStarvationCount {0};
    std::atomic<quint64> m_decodeErrors {0};
    std::atomic<quint64> m_seekCount {0};
    std::atomic<qint64> m_lastSeekLatencyUs {0};
    std::atomic<qint64> m_maxSeekLatencyUs {0};
    std::atomic<quint64> m_preloadedNextCount {0};
    std::atomic<quint64> m_promotedPreloadedNextCount {0};
    std::atomic<qint64> m_lastPreloadLatencyUs {0};
    std::atomic<qint64> m_maxPreloadLatencyUs {0};
    std::atomic<quint64> m_audioDeviceChangeCount {0};
    std::atomic<quint64> m_audioDeviceLossCount {0};
    std::atomic<quint64> m_audioSinkErrorCount {0};
    std::atomic<quint64> m_audioRecoveryAttemptCount {0};
    std::atomic<quint64> m_audioRecoverySuccessCount {0};
    std::atomic<quint64> m_audioSafeStopCount {0};
    std::atomic<quint64> m_audioDeviceReopenCount {0};
    std::atomic<quint64> m_decodeStarvationCount {0};
    std::atomic<quint64> m_dspReconfigurationCount {0};
    std::atomic<qint64> m_lastDspReconfigurationLatencyUs {0};
    std::atomic<qint64> m_maxDspReconfigurationLatencyUs {0};
    std::atomic<quint64> m_rateActivationCount {0};
    std::atomic<qint64> m_lastRateActivationLatencyUs {0};
    std::atomic<qint64> m_maxRateActivationLatencyUs {0};
    std::atomic<quint64> m_pitchActivationCount {0};
    std::atomic<qint64> m_lastPitchActivationLatencyUs {0};
    std::atomic<qint64> m_maxPitchActivationLatencyUs {0};
    std::atomic<quint64> m_reverseActivationCount {0};
    std::atomic<qint64> m_lastReverseActivationLatencyUs {0};
    std::atomic<qint64> m_maxReverseActivationLatencyUs {0};
    std::atomic<qint64> m_lastGaplessTransitionLatencyUs {0};
    std::atomic<qint64> m_maxGaplessTransitionLatencyUs {0};
    std::atomic<qint64> m_loadStartWallClockMs {0};
    std::atomic<qint64> m_loadToFirstAudioFramesMs {-1};
    std::atomic<qint64> m_renderCallbackMaxDurationUs {0};
    std::atomic_bool m_pcmTapEnabled {false};
    std::atomic<qint64> m_lastPcmTapWallClockMs {0};
    std::unique_ptr<LoadedTrackerModule> m_preloadedNext;
    mutable QMutex m_audioDeviceStatusMutex;
    QString m_audioDeviceStatus;
    mutable QMutex m_dspDiagnosticsMutex;
    QString m_lastDspReconfigurationType;
    bool m_resumeAfterAudioDeviceRecovery = false;
    bool m_recoverySuspendedAfterDeviceRecovery = false;
};

} // namespace WaveFlux

#endif // OPENMPTPLAYBACKBACKEND_H
