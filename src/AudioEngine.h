#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QVariantList>
#include <atomic>
#include <gst/gst.h>

/**
 * @brief AudioEngine - GStreamer-based audio playback engine
 * 
 * This class wraps GStreamer's playbin3 element to provide:
 * - Multi-format audio playback (MP3, FLAC, OGG, WAV, AAC)
 * - Gapless playback using the about-to-finish signal
 * - Seeking and position tracking
 * - Volume control
 */
class AudioEngine : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(bool reversePlayback READ reversePlayback WRITE setReversePlayback NOTIFY reversePlaybackChanged)
    Q_PROPERTY(QString audioQualityProfile READ audioQualityProfile WRITE setAudioQualityProfile NOTIFY audioQualityProfileChanged)
    Q_PROPERTY(int pitchSemitones READ pitchSemitones WRITE setPitchSemitones NOTIFY pitchSemitonesChanged)
    Q_PROPERTY(PlaybackState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QVariantList spectrumLevels READ spectrumLevels NOTIFY spectrumLevelsChanged)
    Q_PROPERTY(QVariantList equalizerBandFrequencies READ equalizerBandFrequencies CONSTANT)
    Q_PROPERTY(QVariantList equalizerBandGains READ equalizerBandGains NOTIFY equalizerBandGainsChanged)
    Q_PROPERTY(bool equalizerAvailable READ equalizerAvailable NOTIFY equalizerAvailableChanged)
    
public:
    enum PlaybackState {
        StoppedState,
        PlayingState,
        PausedState,
        ReadyState,
        EndedState,
        ErrorState
    };
    Q_ENUM(PlaybackState)
    
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;
    
    QString currentFile() const { return m_currentFile; }
    qint64 position() const;
    qint64 duration() const;
    double volume() const { return m_volume; }
    double playbackRate() const { return m_playbackRate; }
    bool reversePlayback() const { return m_reversePlayback; }
    QString audioQualityProfile() const { return m_audioQualityProfile; }
    int pitchSemitones() const { return m_pitchSemitones; }
    PlaybackState state() const { return m_state; }
    
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QVariantList spectrumLevels() const { return m_spectrumLevels; }
    QVariantList equalizerBandFrequencies() const { return m_equalizerBandFrequencies; }
    QVariantList equalizerBandGains() const { return m_equalizerBandGains; }
    bool equalizerAvailable() const { return m_equalizerAvailable; }
    quint64 currentTransitionId() const { return m_currentTransitionId; }
    quint64 lastAboutToFinishTransitionId() const { return m_lastAboutToFinishTransitionId; }
    quint64 lastEndOfStreamTransitionId() const { return m_lastEndOfStreamTransitionId; }
    
public slots:
    void play();
    void pause();
    void stop();
    void togglePlayPause();
    void seek(qint64 position);
    Q_INVOKABLE void seekWithSource(qint64 position, const QString &source);
    void setVolume(double volume);
    void setPlaybackRate(double rate);
    void setReversePlayback(bool enabled);
    void setAudioQualityProfile(const QString &profile);
    void setPitchSemitones(int semitones);
    void setSpectrumEnabled(bool enabled);
    void setEqualizerBandGain(int bandIndex, double gainDb);
    void setEqualizerBandGains(const QVariantList &gainsDb);
    void resetEqualizerBands();
    void loadFile(const QString &filePath);
    void loadFileWithTransition(const QString &filePath, quint64 transitionId);
    void loadUrl(const QUrl &url);
    void setNextFile(const QString &filePath);
    void setNextFileWithTransition(const QString &filePath, quint64 transitionId);
    
signals:
    void currentFileChanged(const QString &filePath);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(double volume);
    void playbackRateChanged(double rate);
    void reversePlaybackChanged(bool enabled);
    void audioQualityProfileChanged(const QString &profile);
    void pitchSemitonesChanged(int semitones);
    void stateChanged(PlaybackState state);
    void metadataChanged();
    void spectrumLevelsChanged();
    void equalizerBandGainsChanged();
    void equalizerAvailableChanged();
    void aboutToFinish();
    void endOfStream();
    void error(const QString &message);
    
private:
    void performSeek(qint64 positionMs);
    void applyPlaybackRateToPipeline();
    void setState(PlaybackState state);
    void setupPipeline();
    void teardownPipeline();
    static QString normalizeAudioQualityProfile(const QString &profile);
    void applyAudioQualityProfileToPipeline();
    void applyDeferredSeekIfNeeded();
    void updatePosition();
    void handleBusMessage(GstMessage *message);
    void drainBusMessages();
    void handleSpectrumMessage(const GstStructure *structure);
    void syncEqualizerBandFrequenciesFromElement();
    void applyEqualizerBandSettings();
    void queueEqualizerApply(bool allowRamp, bool immediate = false);
    void applyEqualizerBandValues(const QVariantList &gainsDb);
    void startEqualizerRamp(const QVariantList &targetGainsDb);
    void processEqualizerRampStep();
    void applyEqualizerBandGain(int bandIndex);
    void applyPendingReverseStartIfNeeded();
    qint64 safeReverseStartPositionMs(qint64 durationMs) const;
    double effectivePlaybackRate() const;
    void enableGaplessBypass(const QString &reason,
                             quint64 transitionId,
                             const QVariantMap &extra = {});
    void handleAboutToFinishOnMainThread(quint64 callbackSerial);
    void extractMetadata();
    void resetSpectrumLevels();
    quint64 resolveTransitionId(quint64 requestedTransitionId);
    qint64 stabilizeDurationValue(qint64 rawDurationMs, qint64 nowMs) const;
    void scheduleDeferredDurationRefresh(quint64 transitionId,
                                         const QString &expectedFilePath,
                                         int remainingAttempts);
    
    static gboolean busCallback(GstBus *bus, GstMessage *message, gpointer userData);
    static void onAboutToFinish(GstElement *playbin, gpointer userData);
    static void onDeepElementAdded(GstBin *bin,
                                   GstBin *subBin,
                                   GstElement *element,
                                   gpointer userData);
    
    GstElement *m_pipeline = nullptr;
    GstElement *m_pitchElement = nullptr;
    GstElement *m_spectrumElement = nullptr;
    GstElement *m_equalizerElement = nullptr;
    GstElement *m_resampleElement = nullptr;
    GstElement *m_replayGainElement = nullptr;
    GstElement *m_dynamicRangeElement = nullptr;
    GstElement *m_limiterElement = nullptr;
    GstElement *m_outputConvertElement = nullptr;
    GstBus *m_bus = nullptr;
    guint m_busWatchId = 0;
    
    QString m_currentFile;
    QString m_nextFile;
    quint64 m_transitionIdCounter = 0;
    quint64 m_currentTransitionId = 0;
    quint64 m_nextFileTransitionId = 0;
    quint64 m_gaplessPendingTransitionId = 0;
    quint64 m_lastAboutToFinishTransitionId = 0;
    quint64 m_lastEndOfStreamTransitionId = 0;
    // During gapless transitions we pre-queue the next URI in about-to-finish, but we only
    // update currentFile once the new stream actually starts to avoid UI/state desync.
    QString m_gaplessPendingFile;
    qint64 m_gaplessPendingSinceWallClockMs = 0;
    // Some backends report a continuous timeline across gapless switches. Keep a base offset
    // so the exposed position() stays relative to the current track start.
    qint64 m_trackTimelineOffsetMs = 0;
    double m_volume = 1.0;
    double m_playbackRate = 1.0;
    bool m_reversePlayback = false;
    QString m_audioQualityProfile = QStringLiteral("standard");
    int m_pitchSemitones = 0;
    PlaybackState m_state = StoppedState;
    
    QString m_title;
    QString m_artist;
    QString m_album;
    
    QTimer *m_positionTimer = nullptr;
    QTimer m_busPollTimer;
    QTimer m_gaplessEosDeferralTimer;
    QTimer m_seekCoalesceTimer;
    qint64 m_coalescedSeekPositionMs = -1;
    qint64 m_deferredSeekPositionMs = -1;
    qint64 m_lastSeekWallClockMs = 0;
    qint64 m_lastSeekTargetMs = -1;
    QString m_lastSeekSource;
    QString m_pendingSeekSource;
    qint64 m_lastTrackSwitchWallClockMs = 0;
    qint64 m_lastEmittedPositionMs = -1;
    qint64 m_recentGaplessStreamStartMs = 0;
    qint64 m_gaplessProgressWatchStartedMs = 0;
    qint64 m_gaplessProgressWatchStartPositionMs = 0;
    bool m_gaplessProgressRecoveryApplied = false;
    bool m_gaplessProgressReloadIssued = false;
    int m_gaplessRecalibrationResyncAttempts = 0;
    bool m_gaplessBypassMode = false;
    QString m_gaplessBypassReason;
    qint64 m_metadataFallbackDurationMs = 0;
    mutable qint64 m_lastStableDurationMs = 0;
    mutable qint64 m_lastStableDurationUpdateMs = 0;
    bool m_pendingRateApplication = false;
    bool m_pendingReverseStart = false;
    bool m_isLoading = false;
    std::atomic<quint64> m_callbackSerial {0};
    bool m_spectrumEnabled = false;
    int m_spectrumDisplayBandCount = 15;
    int m_spectrumAnalysisBandCount = 96;
    QVariantList m_spectrumLevels;
    int m_equalizerBandCount = 10;
    QVariantList m_equalizerBandFrequencies;
    QVariantList m_equalizerBandGains;
    QVariantList m_equalizerAppliedBandGains;
    QVariantList m_equalizerRampStartGains;
    QVariantList m_equalizerRampTargetGains;
    bool m_equalizerAvailable = false;
    bool m_equalizerPendingApplyAllowRamp = false;
    int m_equalizerRampCurrentStep = 0;
    int m_equalizerRampTotalSteps = 0;
    QTimer m_equalizerApplyTimer;
    QTimer m_equalizerRampTimer;

    static constexpr int kSeekCoalesceIntervalMs = 35;
    // Stability-first mode: rely on EOS/manual fallback transitions instead of
    // playbin about-to-finish URI handoff, which is unstable on some backends.
    static constexpr bool kForceReliableEosTransitions = true;
    static constexpr qint64 kSeekSafeEndMarginMs = 120;
    static constexpr qint64 kSeekEosGuardWindowMs = 600;
    static constexpr qint64 kTrackSwitchEosGuardWindowMs = 2200;
    static constexpr qint64 kGaplessEosDeferralWindowMs = 1000;
    static constexpr qint64 kGaplessTrailingEosGuardWindowMs = 2200;
    static constexpr qint64 kGaplessTrailingEosMaxPositionMs = 2200;
    static constexpr qint64 kGaplessTrailingEosMinRemainingMs = 400;
    static constexpr qint64 kGaplessTimelineRecalibrationWindowMs = 2500;
    static constexpr qint64 kGaplessTimelineRecalibrationThresholdMs = 8000;
    static constexpr qint64 kGaplessRecalibrationResyncThresholdMs = 12000;
    static constexpr qint64 kGaplessRecalibrationResyncMaxAgeMs = 1800;
    static constexpr int kGaplessRecalibrationResyncMaxAttempts = 1;
    static constexpr qint64 kDurationStabilizationWindowMs = 3500;
    static constexpr qint64 kDurationJumpAbsoluteThresholdMs = 20000;
    static constexpr qint64 kDurationMaxReasonableMs = 12LL * 60LL * 60LL * 1000LL;
    static constexpr qint64 kGaplessProgressWatchDelayMs = 1400;
    static constexpr qint64 kGaplessProgressWatchMinAdvanceMs = 220;
    static constexpr qint64 kReverseStartMarginMs = 120;
    static constexpr qint64 kReverseEnableSeekNudgeMs = 45;
    static constexpr int kEqualizerApplyCoalesceMs = 20;
    static constexpr int kEqualizerRampDurationMs = 32;
    static constexpr int kEqualizerRampStepMs = 8;
    static constexpr double kEqualizerRampThresholdDb = 5.0;
};

#endif // AUDIOENGINE_H
