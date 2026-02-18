#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>
#include <QVariantList>
#include <QVector>
#include "AudioEngine.h"
#include "TrackModel.h"

class PlaybackController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(RepeatMode repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled WRITE setShuffleEnabled NOTIFY shuffleEnabledChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY navigationStateChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY navigationStateChanged)
    Q_PROPERTY(int trackLoadTimeoutMs READ trackLoadTimeoutMs WRITE setTrackLoadTimeoutMs NOTIFY trackLoadTimeoutMsChanged)
    Q_PROPERTY(int maxConsecutiveErrors READ maxConsecutiveErrors WRITE setMaxConsecutiveErrors NOTIFY maxConsecutiveErrorsChanged)
    Q_PROPERTY(int consecutiveErrors READ consecutiveErrors NOTIFY consecutiveErrorsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY queueChanged)
    Q_PROPERTY(int queueRevision READ queueRevision NOTIFY queueChanged)
    Q_PROPERTY(QVariantList queueItems READ queueItems NOTIFY queueChanged)
    Q_PROPERTY(bool deterministicShuffleEnabled READ deterministicShuffleEnabled WRITE setDeterministicShuffleEnabled NOTIFY deterministicShuffleEnabledChanged)
    Q_PROPERTY(quint32 shuffleSeed READ shuffleSeed WRITE setShuffleSeed NOTIFY shuffleSeedChanged)
    Q_PROPERTY(bool repeatableShuffle READ repeatableShuffle WRITE setRepeatableShuffle NOTIFY repeatableShuffleChanged)
    Q_PROPERTY(int restartThresholdMs READ restartThresholdMs WRITE setRestartThresholdMs NOTIFY restartThresholdMsChanged)
    Q_PROPERTY(int activeTrackIndex READ activeTrackIndex NOTIFY activeTrackIndexChanged)
    Q_PROPERTY(int pendingTrackIndex READ pendingTrackIndex NOTIFY pendingTrackIndexChanged)
    Q_PROPERTY(TransitionState transitionState READ transitionState NOTIFY transitionStateChanged)
    Q_PROPERTY(bool detailedDiagnosticsEnabled READ detailedDiagnosticsEnabled WRITE setDetailedDiagnosticsEnabled NOTIFY detailedDiagnosticsEnabledChanged)

public:
    enum RepeatMode {
        RepeatOff,
        RepeatAll,
        RepeatOne
    };
    Q_ENUM(RepeatMode)

    enum TransitionState {
        TransitionIdle,
        TransitionPreparingGapless,
        TransitionPendingCommit,
        TransitionCommitted,
        TransitionFallbackEos
    };
    Q_ENUM(TransitionState)

    explicit PlaybackController(TrackModel *trackModel,
                                AudioEngine *audioEngine,
                                QObject *parent = nullptr);
    ~PlaybackController() override;

    RepeatMode repeatMode() const { return m_repeatMode; }
    bool shuffleEnabled() const { return m_shuffleEnabled; }
    bool canGoNext() const;
    bool canGoPrevious() const;
    int trackLoadTimeoutMs() const { return m_trackLoadTimeoutMs; }
    int maxConsecutiveErrors() const { return m_maxConsecutiveErrors; }
    int consecutiveErrors() const { return m_consecutiveErrors; }
    QString lastError() const { return m_lastError; }
    int queueCount() const { return m_playQueue.size(); }
    int queueRevision() const { return m_queueRevision; }
    QVariantList queueItems() const;
    bool deterministicShuffleEnabled() const { return m_deterministicShuffleEnabled; }
    quint32 shuffleSeed() const { return m_shuffleSeed; }
    bool repeatableShuffle() const { return m_repeatableShuffle; }
    int restartThresholdMs() const { return m_restartThresholdMs; }
    int activeTrackIndex() const { return m_activeTrackIndex; }
    int pendingTrackIndex() const { return m_pendingTrackIndex; }
    TransitionState transitionState() const { return m_transitionState; }
    bool detailedDiagnosticsEnabled() const;

public slots:
    void setRepeatMode(RepeatMode mode);
    void setShuffleEnabled(bool enabled);
    void toggleRepeatMode();
    void toggleShuffle();
    void nextTrack();
    void previousTrack();
    void handleTrackEnded();
    void prepareGaplessTransition();
    void setTrackLoadTimeoutMs(int timeoutMs);
    void setMaxConsecutiveErrors(int maxErrors);
    void setDeterministicShuffleEnabled(bool enabled);
    void setShuffleSeed(quint32 seed);
    void setRepeatableShuffle(bool enabled);
    void setRestartThresholdMs(int thresholdMs);
    void setDetailedDiagnosticsEnabled(bool enabled);
    Q_INVOKABLE void seekRelative(qint64 deltaMs);
    Q_INVOKABLE void requestPlayIndex(int index, const QString &reason = QString());
    Q_INVOKABLE void addToQueue(int index);
    Q_INVOKABLE void playNextInQueue(int index);
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void removeQueueAt(int queueIndex);
    Q_INVOKABLE void moveQueueItem(int from, int to);
    Q_INVOKABLE bool isQueued(const QString &filePath) const;
    Q_INVOKABLE int queuedPosition(const QString &filePath) const;
    Q_INVOKABLE void forceFlushPlaybackStats();

signals:
    void repeatModeChanged();
    void shuffleEnabledChanged();
    void navigationStateChanged();
    void trackLoadTimeoutMsChanged();
    void maxConsecutiveErrorsChanged();
    void consecutiveErrorsChanged();
    void lastErrorChanged();
    void errorRaised(const QString &message);
    void queueChanged();
    void deterministicShuffleEnabledChanged();
    void shuffleSeedChanged();
    void repeatableShuffleChanged();
    void restartThresholdMsChanged();
    void activeTrackIndexChanged();
    void pendingTrackIndexChanged();
    void transitionStateChanged();
    void detailedDiagnosticsEnabledChanged();

private:
    enum class SessionEndReason {
        TrackEnded,
        UserSkip,
        Stop,
        Error,
        Shutdown,
        TrackChange
    };

    struct ActivePlaybackSession {
        QString filePath;
        qint64 startedAtMs = 0;
        qint64 lastPlayResumeAtMs = 0;
        qint64 accumulatedListenMs = 0;
        qint64 maxObservedPositionMs = 0;
        qint64 durationMs = 0;
        bool active = false;
        bool currentlyPlaying = false;
        bool hasPlayed = false;
    };

    void onPlaybackStateChanged(AudioEngine::PlaybackState state);
    void onAudioPositionChanged(qint64 positionMs);
    void onTrackSelectionRequested(const QString &filePath);
    void onPlaybackError(const QString &message);
    void beginPlaybackSession(const QString &filePath, qint64 nowMs);
    void updateActiveSessionListenAccumulation(qint64 nowMs);
    void finalizeActiveSession(SessionEndReason reason, qint64 nowMs);
    qint64 resolveDurationForFile(const QString &filePath) const;
    QString sourceForSessionEndReason(SessionEndReason reason) const;
    void schedulePlaybackStatsFlush();
    void flushPlaybackStats(bool includeActiveSession, SessionEndReason activeReason, bool blocking);
    quint64 nextTrackTransitionId();
    void onAudioAboutToFinish();
    void onAudioEndOfStream();
    void prepareGaplessTransitionForSource(quint64 sourceTransitionId);
    void handleTrackEndedInternal(quint64 eosTransitionId, bool fromEosSignal);

    bool isShuffleStateValid() const;
    void regenerateShuffleOrder(int startIndex = -1);
    void syncShuffleToCurrentTrack();
    int currentShufflePosition() const;
    int calculateNextIndex(bool advanceShuffleState);
    int calculatePreviousIndex();
    void navigateToNextTrackInternal(SessionEndReason reason);
    void navigateToPreviousTrackInternal(SessionEndReason reason);
    void startTrackLoadWatch();
    void clearTrackLoadWatch();
    void onLoadSuccess();
    void onLoadFailure(const QString &message);
    void setConsecutiveErrors(int value);
    void setLastError(const QString &message);
    void onCurrentFileChanged(const QString &filePath);
    void confirmGaplessTransition(int index, quint64 transitionId);
    void clearGaplessTransitionState();
    int activePlaybackIndex() const;
    int effectiveCurrentIndex() const;
    int findTrackIndexByPath(const QString &filePath) const;
    int peekNextQueuedIndex() const;
    int takeNextQueuedIndex();
    void removeCurrentTrackFromQueue();
    void pruneQueue();
    void emitQueueChanged();
    void captureShuffleSnapshot();
    void restoreShuffleSnapshot();
    quint32 nextShuffleSeed();
    void clearPendingCueSeek();
    void scheduleCueSeekForCurrentTrack(const QString &filePath);
    void applyPendingCueSeekIfReady(const QString &filePath);
    void setActiveTrackIndex(int index);
    void setPendingTrackIndex(int index);
    void setTransitionState(TransitionState state);
    int findCueTrackIndexForPosition(const QString &filePath, qint64 positionMs) const;
    bool hasLaterCueSegmentInSameFile(int index) const;
    bool hasEarlierCueSegmentInSameFile(int index) const;
    int syncCueTrackIndexForPosition(qint64 positionMs, int currentIndex);
    void traceTransitionEvent(const char *event,
                              quint64 transitionId,
                              const QVariantMap &extra = QVariantMap()) const;

    TrackModel *m_trackModel = nullptr;
    AudioEngine *m_audioEngine = nullptr;

    RepeatMode m_repeatMode = RepeatOff;
    bool m_shuffleEnabled = false;
    QVector<int> m_shuffleOrder;
    int m_shufflePosition = -1;
    quint64 m_transitionIdCounter = 0;
    quint64 m_activeTrackTransitionId = 0;
    int m_gaplessQueuedIndex = -1;
    quint64 m_gaplessQueuedTransitionId = 0;
    bool m_gaplessTransitionPending = false;
    qint64 m_gaplessTrailingEosGuardUntilMs = 0;
    quint64 m_lastPreparedGaplessSourceTransitionId = 0;
    quint64 m_lastHandledEosTransitionId = 0;
    QString m_forcedTrackLoadPath;
    quint64 m_forcedTrackLoadTransitionId = 0;

    QTimer m_trackLoadTimer;
    bool m_trackLoading = false;
    bool m_handlingFailure = false;

    int m_trackLoadTimeoutMs = 15000;
    int m_maxConsecutiveErrors = 3;
    int m_consecutiveErrors = 0;
    QString m_lastError;
    QStringList m_playQueue;
    int m_queueRevision = 0;
    QStringList m_shuffleSnapshotPaths;
    QString m_shuffleSnapshotCurrentPath;
    bool m_shuffleSnapshotValid = false;
    bool m_deterministicShuffleEnabled = false;
    quint32 m_shuffleSeed = 0xC4E5D2A1u;
    bool m_repeatableShuffle = true;
    int m_restartThresholdMs = 3000;
    int m_activeTrackIndex = -1;
    int m_pendingTrackIndex = -1;
    TransitionState m_transitionState = TransitionIdle;
    quint64 m_shuffleGeneration = 0;
    ActivePlaybackSession m_activeSession;
    QVector<TrackPlaybackEvent> m_pendingPlaybackEvents;
    QTimer m_playbackStatsDebounceTimer;
    QTimer m_playbackStatsForcedTimer;
    QString m_playbackStatsSessionId;
    QString m_pendingCueSeekFilePath;
    qint64 m_pendingCueSeekMs = -1;
    int m_lastCueBoundaryTrackIndex = -1;

    static constexpr qint64 kGaplessTrailingEosGuardWindowMs = 2200;
    static constexpr qint64 kGaplessTrailingEosMaxPositionMs = 2200;
    static constexpr qint64 kGaplessTrailingEosMinRemainingMs = 400;
};

#endif // PLAYBACKCONTROLLER_H
