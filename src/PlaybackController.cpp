#include "PlaybackController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QUuid>
#include <QVariantMap>
#include <QtGlobal>
#include "DiagnosticsFlags.h"
#include <utility>

namespace {
bool seekDiagEnabled()
{
    return DiagnosticsFlags::detailedDiagnosticsEnabled();
}

QString transitionTracePayload(const QString &component,
                               const char *event,
                               quint64 transitionId,
                               const QVariantMap &extra)
{
    QVariantMap payload = extra;
    payload.insert(QStringLiteral("component"), component);
    payload.insert(QStringLiteral("event"), QString::fromLatin1(event));
    payload.insert(QStringLiteral("transitionId"), static_cast<qulonglong>(transitionId));
    payload.insert(QStringLiteral("tsMs"), QDateTime::currentMSecsSinceEpoch());
    return QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(payload)).toJson(QJsonDocument::Compact));
}
} // namespace

PlaybackController::PlaybackController(TrackModel *trackModel,
                                       AudioEngine *audioEngine,
                                       QObject *parent)
    : QObject(parent)
    , m_trackModel(trackModel)
    , m_audioEngine(audioEngine)
{
    m_trackLoadTimer.setSingleShot(true);
    m_trackLoadTimer.setInterval(m_trackLoadTimeoutMs);
    connect(&m_trackLoadTimer, &QTimer::timeout, this, [this]() {
        onLoadFailure(QStringLiteral("Track loading timed out"));
    });

    m_playbackStatsDebounceTimer.setSingleShot(true);
    m_playbackStatsDebounceTimer.setInterval(1200);
    connect(&m_playbackStatsDebounceTimer, &QTimer::timeout, this, [this]() {
        flushPlaybackStats(false, SessionEndReason::TrackChange, false);
    });

    m_playbackStatsForcedTimer.setSingleShot(false);
    m_playbackStatsForcedTimer.setInterval(7000);
    connect(&m_playbackStatsForcedTimer, &QTimer::timeout, this, [this]() {
        flushPlaybackStats(false, SessionEndReason::TrackChange, false);
    });
    m_playbackStatsForcedTimer.start();

    m_playbackStatsSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (m_trackModel) {
        m_activeTrackIndex = m_trackModel->currentIndex();

        connect(m_trackModel, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
            captureShuffleSnapshot();
        });
        connect(m_trackModel, &QAbstractItemModel::modelReset, this, [this]() {
            pruneQueue();
            restoreShuffleSnapshot();
            if (m_gaplessQueuedIndex >= m_trackModel->rowCount()) {
                clearGaplessTransitionState();
            }
            if (m_trackModel->rowCount() <= 0) {
                setActiveTrackIndex(-1);
                setPendingTrackIndex(-1);
                setTransitionState(TransitionIdle);
            } else {
                if (m_audioEngine) {
                    const int activeByPath = findTrackIndexByPath(m_audioEngine->currentFile());
                    if (activeByPath >= 0) {
                        setActiveTrackIndex(activeByPath);
                    } else if (!m_audioEngine->currentFile().isEmpty()) {
                        setActiveTrackIndex(-1);
                    }
                }
                if (m_pendingTrackIndex >= m_trackModel->rowCount()) {
                    setPendingTrackIndex(-1);
                }
            }
            emit navigationStateChanged();
        });
        connect(m_trackModel, &QAbstractItemModel::rowsAboutToBeMoved, this,
                [this](const QModelIndex &, int, int, const QModelIndex &, int) {
            captureShuffleSnapshot();
        });
        connect(m_trackModel, &QAbstractItemModel::rowsMoved, this,
                [this](const QModelIndex &, int, int, const QModelIndex &, int) {
            restoreShuffleSnapshot();
            if (m_audioEngine) {
                const int activeByPath = findTrackIndexByPath(m_audioEngine->currentFile());
                if (activeByPath >= 0) {
                    setActiveTrackIndex(activeByPath);
                } else if (!m_audioEngine->currentFile().isEmpty()) {
                    setActiveTrackIndex(-1);
                }
            }
            emit navigationStateChanged();
        });

        connect(m_trackModel, &TrackModel::currentIndexChanged, this, [this](int) {
            if (m_shuffleEnabled) {
                syncShuffleToCurrentTrack();
            }
            emit navigationStateChanged();
        });

        connect(m_trackModel, &TrackModel::trackSelected, this, [this](const QString &filePath) {
            startTrackLoadWatch();
            onTrackSelectionRequested(filePath);
        });

        connect(m_trackModel, &TrackModel::countChanged, this, [this]() {
            pruneQueue();
            if (m_shuffleEnabled) {
                regenerateShuffleOrder(effectiveCurrentIndex());
            } else {
                m_shuffleOrder.clear();
                m_shufflePosition = -1;
            }

            if (m_gaplessQueuedIndex >= m_trackModel->rowCount()) {
                clearGaplessTransitionState();
            }
            if (m_trackModel->rowCount() <= 0) {
                setActiveTrackIndex(-1);
                setPendingTrackIndex(-1);
                setTransitionState(TransitionIdle);
            } else {
                if (m_audioEngine) {
                    const int activeByPath = findTrackIndexByPath(m_audioEngine->currentFile());
                    if (activeByPath >= 0) {
                        setActiveTrackIndex(activeByPath);
                    } else if (!m_audioEngine->currentFile().isEmpty()) {
                        setActiveTrackIndex(-1);
                    }
                }
                if (m_pendingTrackIndex >= m_trackModel->rowCount()) {
                    setPendingTrackIndex(-1);
                }
            }
            emit navigationStateChanged();
        });
    }

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::aboutToFinish, this, [this]() {
            onAudioAboutToFinish();
        });

        connect(m_audioEngine, &AudioEngine::endOfStream, this, [this]() {
            onAudioEndOfStream();
        });

        connect(m_audioEngine, &AudioEngine::stateChanged, this,
                [this](AudioEngine::PlaybackState state) {
            onPlaybackStateChanged(state);

            if (!m_trackLoading) {
                return;
            }

            if (state == AudioEngine::PausedState ||
                state == AudioEngine::PlayingState) {
                onLoadSuccess();
            } else if (state == AudioEngine::ReadyState) {
                // Keep waiting when cue start seek is pending: in READY state
                // many backends still reject seek and report duration=0.
                if (m_pendingCueSeekMs <= 0) {
                    onLoadSuccess();
                } else if (seekDiagEnabled()) {
                    qInfo().noquote()
                        << "[SeekDiag][Cue] load success deferred at READY"
                        << "pendingCueSeekMs=" << m_pendingCueSeekMs
                        << "currentFile=" << (m_audioEngine ? m_audioEngine->currentFile() : QString());
                }
            } else if (state == AudioEngine::StoppedState) {
                clearTrackLoadWatch();
            }
        });

        connect(m_audioEngine, &AudioEngine::error, this, [this](const QString &message) {
            onPlaybackError(message);
            if (m_trackLoading) {
                onLoadFailure(message);
            } else {
                setLastError(message);
                emit errorRaised(message);
            }
        });

        connect(m_audioEngine, &AudioEngine::positionChanged, this, [this](qint64 positionMs) {
            if (m_trackLoading && m_audioEngine && !m_audioEngine->currentFile().isEmpty() && positionMs >= 0) {
                onLoadSuccess();
            }
            onAudioPositionChanged(positionMs);
        });

        // Listen for file changes to handle gapless transitions
        connect(m_audioEngine, &AudioEngine::currentFileChanged, this, [this](const QString &filePath) {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_activeSession.active && m_activeSession.filePath != filePath) {
                finalizeActiveSession(SessionEndReason::TrackChange, nowMs);
            }
            if (!filePath.isEmpty() &&
                (!m_activeSession.active || m_activeSession.filePath != filePath)) {
                beginPlaybackSession(filePath, nowMs);
            }
            onCurrentFileChanged(filePath);
        });
    }

    if (m_trackModel && m_audioEngine) {
        const int activeByPath = findTrackIndexByPath(m_audioEngine->currentFile());
        if (activeByPath >= 0) {
            m_activeTrackIndex = activeByPath;
        }
    }

    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(),
                &QCoreApplication::aboutToQuit,
                this,
                &PlaybackController::forceFlushPlaybackStats);
    }
}

PlaybackController::~PlaybackController()
{
    forceFlushPlaybackStats();
}

bool PlaybackController::canGoNext() const
{
    if (!m_trackModel) {
        return false;
    }

    const int count = m_trackModel->rowCount();
    const int current = effectiveCurrentIndex();
    if (count <= 0 || current < 0) {
        return false;
    }

    if (m_repeatMode == RepeatOne) {
        return true;
    }

    if (peekNextQueuedIndex() >= 0) {
        return true;
    }

    if (m_shuffleEnabled) {
        if (count == 1) {
            return m_repeatMode == RepeatAll;
        }
        const int pos = currentShufflePosition();
        return (pos >= 0 && pos + 1 < count) || m_repeatMode == RepeatAll;
    }

    return current + 1 < count || m_repeatMode == RepeatAll;
}

bool PlaybackController::canGoPrevious() const
{
    if (!m_trackModel) {
        return false;
    }

    const int count = m_trackModel->rowCount();
    const int current = effectiveCurrentIndex();
    if (count <= 0 || current < 0) {
        return false;
    }

    if (m_shuffleEnabled) {
        return count > 1 || m_repeatMode == RepeatAll;
    }

    return current > 0 || m_repeatMode == RepeatAll;
}

void PlaybackController::forceFlushPlaybackStats()
{
    m_playbackStatsDebounceTimer.stop();
    m_playbackStatsForcedTimer.stop();
    flushPlaybackStats(true, SessionEndReason::Shutdown, true);
}

void PlaybackController::onPlaybackStateChanged(AudioEngine::PlaybackState state)
{
    if (!m_activeSession.active) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (state == AudioEngine::PlayingState) {
        if (!m_activeSession.currentlyPlaying) {
            m_activeSession.currentlyPlaying = true;
            m_activeSession.hasPlayed = true;
            m_activeSession.lastPlayResumeAtMs = nowMs;
        }
        return;
    }

    updateActiveSessionListenAccumulation(nowMs);

    if (state == AudioEngine::StoppedState) {
        finalizeActiveSession(SessionEndReason::Stop, nowMs);
    }
}

void PlaybackController::onAudioPositionChanged(qint64 positionMs)
{
    if (positionMs < 0) {
        return;
    }

    if (m_activeSession.active && positionMs > m_activeSession.maxObservedPositionMs) {
        m_activeSession.maxObservedPositionMs = positionMs;
    }

    if (!m_trackModel || !m_audioEngine) {
        return;
    }

    const int playbackIndex = effectiveCurrentIndex();
    if (playbackIndex < 0 || playbackIndex >= m_trackModel->rowCount()) {
        return;
    }

    const int syncedIndex = syncCueTrackIndexForPosition(positionMs, playbackIndex);
    if (syncedIndex < 0 || syncedIndex >= m_trackModel->rowCount()) {
        return;
    }
    if (seekDiagEnabled() && syncedIndex != playbackIndex) {
        qInfo().noquote()
            << "[SeekDiag][Cue] index synced on position"
            << "posMs=" << positionMs
            << "fromIndex=" << playbackIndex
            << "toIndex=" << syncedIndex
            << "file=" << m_trackModel->getFilePath(syncedIndex);
    }
    setActiveTrackIndex(syncedIndex);

    const bool reversePlayback = m_audioEngine->reversePlayback();
    const bool cueTrack = m_trackModel->isCueTrack(syncedIndex);
    if (!cueTrack) {
        if (m_lastCueBoundaryTrackIndex == syncedIndex) {
            m_lastCueBoundaryTrackIndex = -1;
        }
        return;
    }

    const qint64 cueStartMs = qMax<qint64>(0, m_trackModel->cueStartMs(syncedIndex));
    const qint64 cueEndMs = m_trackModel->cueEndMs(syncedIndex);
    if (!reversePlayback && cueEndMs <= 0) {
        if (m_lastCueBoundaryTrackIndex == syncedIndex) {
            m_lastCueBoundaryTrackIndex = -1;
        }
        return;
    }

    const qint64 triggerMs = reversePlayback
        ? cueStartMs + 90
        : qMax<qint64>(0, cueEndMs - 90);
    const bool boundaryReached = reversePlayback
        ? (positionMs <= triggerMs)
        : (positionMs >= triggerMs);
    if (!boundaryReached) {
        if (m_lastCueBoundaryTrackIndex == syncedIndex) {
            m_lastCueBoundaryTrackIndex = -1;
        }
        return;
    }

    if (m_lastCueBoundaryTrackIndex == syncedIndex) {
        return;
    }
    m_lastCueBoundaryTrackIndex = syncedIndex;

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] cue boundary reached"
            << "posMs=" << positionMs
            << "index=" << syncedIndex
            << "cueStartMs=" << cueStartMs
            << "cueEndMs=" << cueEndMs
            << "reverse=" << reversePlayback
            << "repeatMode=" << static_cast<int>(m_repeatMode);
    }

    if (m_repeatMode == RepeatOne) {
        qint64 loopStartMs = cueStartMs;
        if (reversePlayback) {
            loopStartMs = cueEndMs > cueStartMs ? cueEndMs : m_audioEngine->duration();
        }
        m_audioEngine->seekWithSource(loopStartMs,
                                      QStringLiteral("playback_controller.cue_repeat_one_boundary"));
        m_audioEngine->play();
        return;
    }

    const bool ignoreBoundaryInsideCueFile = reversePlayback
        ? hasEarlierCueSegmentInSameFile(syncedIndex)
        : hasLaterCueSegmentInSameFile(syncedIndex);
    if (ignoreBoundaryInsideCueFile) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] boundary ignored: adjacent segment in same file"
                << "index=" << syncedIndex
                << "file=" << m_trackModel->getFilePath(syncedIndex)
                << "reverse=" << reversePlayback
                << "posMs=" << positionMs;
        }
        return;
    }

    const bool canAdvanceOnBoundary = reversePlayback ? canGoPrevious() : canGoNext();
    if (!canAdvanceOnBoundary) {
        finalizeActiveSession(SessionEndReason::TrackEnded, QDateTime::currentMSecsSinceEpoch());
        m_audioEngine->stop();
        return;
    }

    handleTrackEnded();
}

int PlaybackController::findCueTrackIndexForPosition(const QString &filePath, qint64 positionMs) const
{
    if (!m_trackModel || filePath.isEmpty()) {
        return -1;
    }

    const QVector<Track> &tracks = m_trackModel->tracks();
    int containingIndex = -1;
    qint64 containingStartMs = -1;

    int precedingIndex = -1;
    qint64 precedingStartMs = -1;

    for (int i = 0; i < tracks.size(); ++i) {
        const Track &track = tracks.at(i);
        if (!track.cueSegment || track.filePath != filePath) {
            continue;
        }

        const qint64 cueStart = qMax<qint64>(0, track.cueStartMs);
        const qint64 cueEnd = track.cueEndMs;

        if (positionMs >= cueStart && (cueEnd <= cueStart || positionMs < cueEnd)) {
            if (cueStart >= containingStartMs) {
                containingStartMs = cueStart;
                containingIndex = i;
            }
            continue;
        }

        if (positionMs >= cueStart && cueStart >= precedingStartMs) {
            precedingStartMs = cueStart;
            precedingIndex = i;
        }
    }

    if (containingIndex >= 0) {
        return containingIndex;
    }
    if (precedingIndex >= 0) {
        return precedingIndex;
    }
    return -1;
}

bool PlaybackController::hasLaterCueSegmentInSameFile(int index) const
{
    if (!m_trackModel || index < 0 || index >= m_trackModel->rowCount()) {
        return false;
    }
    if (!m_trackModel->isCueTrack(index)) {
        return false;
    }

    const QVector<Track> &tracks = m_trackModel->tracks();
    if (index >= tracks.size()) {
        return false;
    }

    const Track &currentTrack = tracks.at(index);
    const QString filePath = currentTrack.filePath;
    if (filePath.isEmpty()) {
        return false;
    }

    const qint64 currentStartMs = qMax<qint64>(0, currentTrack.cueStartMs);
    for (const Track &track : tracks) {
        if (!track.cueSegment || track.filePath != filePath) {
            continue;
        }
        if (qMax<qint64>(0, track.cueStartMs) > currentStartMs) {
            return true;
        }
    }
    return false;
}

bool PlaybackController::hasEarlierCueSegmentInSameFile(int index) const
{
    if (!m_trackModel || index < 0 || index >= m_trackModel->rowCount()) {
        return false;
    }
    if (!m_trackModel->isCueTrack(index)) {
        return false;
    }

    const QVector<Track> &tracks = m_trackModel->tracks();
    if (index >= tracks.size()) {
        return false;
    }

    const Track &currentTrack = tracks.at(index);
    const QString filePath = currentTrack.filePath;
    if (filePath.isEmpty()) {
        return false;
    }

    const qint64 currentStartMs = qMax<qint64>(0, currentTrack.cueStartMs);
    for (const Track &track : tracks) {
        if (!track.cueSegment || track.filePath != filePath) {
            continue;
        }
        if (qMax<qint64>(0, track.cueStartMs) < currentStartMs) {
            return true;
        }
    }
    return false;
}

int PlaybackController::syncCueTrackIndexForPosition(qint64 positionMs, int currentIndex)
{
    if (!m_trackModel || !m_audioEngine
        || currentIndex < 0 || currentIndex >= m_trackModel->rowCount()) {
        return currentIndex;
    }
    if (!m_trackModel->isCueTrack(currentIndex)) {
        return currentIndex;
    }

    const QString cueFilePath = m_trackModel->getFilePath(currentIndex);
    if (cueFilePath.isEmpty()) {
        return currentIndex;
    }

    // While loading a new track or waiting for deferred cue-start seek,
    // keep cue-segment resolution stable to avoid index oscillation.
    if (m_trackLoading) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] sync skipped: track loading"
                << "posMs=" << positionMs
                << "index=" << currentIndex;
        }
        return currentIndex;
    }
    if (m_pendingCueSeekMs > 0
        && !m_pendingCueSeekFilePath.isEmpty()
        && m_pendingCueSeekFilePath == cueFilePath) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] sync skipped: pending cue seek"
                << "posMs=" << positionMs
                << "index=" << currentIndex
                << "file=" << cueFilePath
                << "pendingSeekMs=" << m_pendingCueSeekMs;
        }
        return currentIndex;
    }

    const QString activeFilePath = m_audioEngine->currentFile();
    if (!activeFilePath.isEmpty() && activeFilePath != cueFilePath) {
        return currentIndex;
    }

    const int resolvedIndex = findCueTrackIndexForPosition(cueFilePath, positionMs);
    if (resolvedIndex < 0) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] sync unresolved for position"
                << "posMs=" << positionMs
                << "index=" << currentIndex
                << "file=" << cueFilePath;
        }
        return currentIndex;
    }
    if (resolvedIndex == currentIndex) {
        return currentIndex;
    }

    m_lastCueBoundaryTrackIndex = -1;
    return resolvedIndex;
}

void PlaybackController::onTrackSelectionRequested(const QString &filePath)
{
    if (!m_audioEngine) {
        return;
    }

    int targetIndex = -1;
    if (m_trackModel) {
        targetIndex = m_trackModel->currentIndex();
        if (targetIndex < 0 || m_trackModel->getFilePath(targetIndex) != filePath) {
            targetIndex = findTrackIndexByPath(filePath);
        }
    }
    if (targetIndex >= 0) {
        setPendingTrackIndex(targetIndex);
    }
    setTransitionState(TransitionPendingCommit);

    clearGaplessTransitionState();
    scheduleCueSeekForCurrentTrack(filePath);

    quint64 transitionId = 0;
    if (!filePath.isEmpty()
        && m_forcedTrackLoadTransitionId > 0
        && m_forcedTrackLoadPath == filePath) {
        transitionId = m_forcedTrackLoadTransitionId;
    }
    m_forcedTrackLoadPath.clear();
    m_forcedTrackLoadTransitionId = 0;

    if (transitionId == 0) {
        transitionId = nextTrackTransitionId();
    }
    m_activeTrackTransitionId = qMax(m_activeTrackTransitionId, transitionId);
    traceTransitionEvent("track_selection_load", transitionId, {
        {QStringLiteral("filePath"), filePath},
        {QStringLiteral("targetIndex"), targetIndex}
    });

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] track selection requested"
            << "file=" << filePath
            << "transitionId=" << transitionId;
    }

    m_audioEngine->loadFileWithTransition(filePath, transitionId);

    if (m_activeSession.active && !filePath.isEmpty() && filePath != m_activeSession.filePath) {
        finalizeActiveSession(SessionEndReason::UserSkip, QDateTime::currentMSecsSinceEpoch());
    }
}

void PlaybackController::onPlaybackError(const QString & /*message*/)
{
    if (!m_activeSession.active) {
        return;
    }

    finalizeActiveSession(SessionEndReason::Error, QDateTime::currentMSecsSinceEpoch());
}

void PlaybackController::beginPlaybackSession(const QString &filePath, qint64 nowMs)
{
    if (filePath.isEmpty()) {
        return;
    }
    if (m_activeSession.active && m_activeSession.filePath == filePath) {
        return;
    }

    m_activeSession = {};
    m_activeSession.active = true;
    m_activeSession.filePath = filePath;
    m_activeSession.startedAtMs = nowMs;
    m_activeSession.durationMs = resolveDurationForFile(filePath);

    if (m_audioEngine && m_audioEngine->currentFile() == filePath) {
        const qint64 position = qMax<qint64>(0, m_audioEngine->position());
        m_activeSession.maxObservedPositionMs = position;
        if (m_audioEngine->state() == AudioEngine::PlayingState) {
            m_activeSession.currentlyPlaying = true;
            m_activeSession.hasPlayed = true;
            m_activeSession.lastPlayResumeAtMs = nowMs;
        }
    }
}

void PlaybackController::updateActiveSessionListenAccumulation(qint64 nowMs)
{
    if (!m_activeSession.active || !m_activeSession.currentlyPlaying) {
        return;
    }

    const qint64 deltaMs = qMax<qint64>(0, nowMs - m_activeSession.lastPlayResumeAtMs);
    m_activeSession.accumulatedListenMs += deltaMs;
    m_activeSession.currentlyPlaying = false;
    m_activeSession.lastPlayResumeAtMs = 0;
}

void PlaybackController::finalizeActiveSession(SessionEndReason reason, qint64 nowMs)
{
    if (!m_activeSession.active) {
        return;
    }

    if (m_audioEngine && m_audioEngine->currentFile() == m_activeSession.filePath) {
        const qint64 currentPositionMs = qMax<qint64>(0, m_audioEngine->position());
        if (currentPositionMs > m_activeSession.maxObservedPositionMs) {
            m_activeSession.maxObservedPositionMs = currentPositionMs;
        }
    }

    updateActiveSessionListenAccumulation(nowMs);

    const qint64 durationMs = m_activeSession.durationMs > 0
        ? m_activeSession.durationMs
        : resolveDurationForFile(m_activeSession.filePath);
    const qint64 listenMs = qMax<qint64>(0, m_activeSession.accumulatedListenMs);
    const qint64 observedPositionMs = qMax<qint64>(0, m_activeSession.maxObservedPositionMs);

    double completionRatio = 0.0;
    if (durationMs > 0) {
        completionRatio = qBound(0.0,
                                 static_cast<double>(observedPositionMs) /
                                     static_cast<double>(durationMs),
                                 1.0);
    }

    const bool completedByProgress =
        durationMs > 0 &&
        (completionRatio >= 0.985 || observedPositionMs + 1500 >= durationMs);
    const bool wasCompleted = (reason == SessionEndReason::TrackEnded) || completedByProgress;
    const bool wasSkipped = !wasCompleted &&
        (reason == SessionEndReason::UserSkip ||
         reason == SessionEndReason::Stop ||
         reason == SessionEndReason::Error ||
         reason == SessionEndReason::Shutdown ||
         reason == SessionEndReason::TrackChange);
    const bool meaningfulPlayback =
        m_activeSession.hasPlayed || listenMs >= 750 || observedPositionMs >= 1000;

    if (meaningfulPlayback) {
        TrackPlaybackEvent event;
        event.filePath = m_activeSession.filePath;
        event.startedAtMs = qMax<qint64>(0, m_activeSession.startedAtMs);
        event.endedAtMs = qMax(event.startedAtMs, nowMs);
        event.listenMs = listenMs;
        event.completionRatio = completionRatio;
        event.source = sourceForSessionEndReason(reason);
        event.wasSkipped = wasSkipped;
        event.wasCompleted = wasCompleted;
        event.sessionId = m_playbackStatsSessionId;
        m_pendingPlaybackEvents.push_back(std::move(event));
        schedulePlaybackStatsFlush();
    }

    m_activeSession = {};
}

qint64 PlaybackController::resolveDurationForFile(const QString &filePath) const
{
    if (!m_trackModel || filePath.isEmpty()) {
        return 0;
    }

    const QVector<Track> &tracks = m_trackModel->tracks();
    for (const Track &track : tracks) {
        if (track.filePath == filePath) {
            return qMax<qint64>(0, track.duration);
        }
    }
    return 0;
}

QString PlaybackController::sourceForSessionEndReason(SessionEndReason reason) const
{
    switch (reason) {
    case SessionEndReason::TrackEnded:
        return QStringLiteral("eos");
    case SessionEndReason::UserSkip:
        return QStringLiteral("user_skip");
    case SessionEndReason::Stop:
        return QStringLiteral("stop");
    case SessionEndReason::Error:
        return QStringLiteral("error");
    case SessionEndReason::Shutdown:
        return QStringLiteral("shutdown");
    case SessionEndReason::TrackChange:
        return QStringLiteral("track_change");
    }
    return QStringLiteral("unknown");
}

void PlaybackController::schedulePlaybackStatsFlush()
{
    if (m_pendingPlaybackEvents.isEmpty()) {
        return;
    }
    m_playbackStatsDebounceTimer.start();
}

void PlaybackController::flushPlaybackStats(bool includeActiveSession,
                                            SessionEndReason activeReason,
                                            bool blocking)
{
    if (includeActiveSession && m_activeSession.active) {
        finalizeActiveSession(activeReason, QDateTime::currentMSecsSinceEpoch());
    }

    if (!m_trackModel || m_pendingPlaybackEvents.isEmpty()) {
        return;
    }

    QVector<TrackPlaybackEvent> batch = std::move(m_pendingPlaybackEvents);
    m_pendingPlaybackEvents.clear();
    m_trackModel->recordPlaybackEvents(batch, blocking);
}

void PlaybackController::setRepeatMode(RepeatMode mode)
{
    if (m_repeatMode == mode) {
        return;
    }

    m_repeatMode = mode;
    emit repeatModeChanged();
    emit navigationStateChanged();
}

void PlaybackController::setShuffleEnabled(bool enabled)
{
    if (m_shuffleEnabled == enabled) {
        return;
    }

    m_shuffleEnabled = enabled;
    if (m_shuffleEnabled) {
        regenerateShuffleOrder(effectiveCurrentIndex());
    } else {
        m_shuffleOrder.clear();
        m_shufflePosition = -1;
    }

    emit shuffleEnabledChanged();
    emit navigationStateChanged();
}

void PlaybackController::toggleRepeatMode()
{
    switch (m_repeatMode) {
    case RepeatOff:
        setRepeatMode(RepeatAll);
        break;
    case RepeatAll:
        setRepeatMode(RepeatOne);
        break;
    case RepeatOne:
        setRepeatMode(RepeatOff);
        break;
    }
}

void PlaybackController::toggleShuffle()
{
    setShuffleEnabled(!m_shuffleEnabled);
}

void PlaybackController::setRestartThresholdMs(int thresholdMs)
{
    const int clamped = qMax(0, thresholdMs);
    if (m_restartThresholdMs == clamped) {
        return;
    }
    m_restartThresholdMs = clamped;
    emit restartThresholdMsChanged();
}

bool PlaybackController::detailedDiagnosticsEnabled() const
{
    return DiagnosticsFlags::detailedDiagnosticsEnabled();
}

void PlaybackController::setDetailedDiagnosticsEnabled(bool enabled)
{
    if (DiagnosticsFlags::detailedDiagnosticsEnabled() == enabled) {
        return;
    }
    DiagnosticsFlags::setDetailedDiagnosticsEnabled(enabled);
    traceTransitionEvent("detailed_diagnostics_toggled", m_activeTrackTransitionId, {
        {QStringLiteral("enabled"), enabled}
    });
    emit detailedDiagnosticsEnabledChanged();
}

void PlaybackController::seekRelative(qint64 deltaMs)
{
    if (!m_audioEngine || !m_trackModel) {
        return;
    }

    const qint64 durationMs = m_audioEngine->duration();
    if (durationMs <= 0) {
        return;
    }

    const qint64 currentMs = qMax<qint64>(0, m_audioEngine->position());
    const int currentIndex = effectiveCurrentIndex();
    const bool isCue = (currentIndex >= 0 && currentIndex < m_trackModel->rowCount())
                           ? m_trackModel->isCueTrack(currentIndex)
                           : false;

    if (!isCue) {
        // Regular track: clamp within [0, duration]
        const qint64 target = qBound<qint64>(0, currentMs + deltaMs, durationMs);
        m_audioEngine->seekWithSource(target,
                                      QStringLiteral("playback_controller.seek_relative_regular"));
        return;
    }

    // Cue track: clamp within the current cue segment boundaries
    const qint64 cueStart = qMax<qint64>(0, m_trackModel->cueStartMs(currentIndex));
    const qint64 cueEnd = m_trackModel->cueEndMs(currentIndex);
    const qint64 effectiveEnd = (cueEnd > cueStart) ? cueEnd : durationMs;
    const qint64 rawTarget = currentMs + deltaMs;

    if (rawTarget < cueStart) {
        // Seeking past the beginning of the cue segment →  go to previous track
        previousTrack();
        return;
    }

    if (rawTarget >= effectiveEnd) {
        // Seeking past the end of the cue segment → go to next track
        nextTrack();
        return;
    }

    m_audioEngine->seekWithSource(rawTarget,
                                  QStringLiteral("playback_controller.seek_relative_cue_segment"));
}

quint64 PlaybackController::nextTrackTransitionId()
{
    m_transitionIdCounter = qMax(m_transitionIdCounter, m_activeTrackTransitionId);
    ++m_transitionIdCounter;
    traceTransitionEvent("transition_id_generated", m_transitionIdCounter);
    return m_transitionIdCounter;
}

void PlaybackController::onAudioAboutToFinish()
{
    if (!m_audioEngine) {
        return;
    }

    const quint64 sourceTransitionId = m_audioEngine->lastAboutToFinishTransitionId();
    traceTransitionEvent("about_to_finish_received", sourceTransitionId);
    if (sourceTransitionId > 0
        && m_activeTrackTransitionId > 0
        && sourceTransitionId < m_activeTrackTransitionId) {
        traceTransitionEvent("about_to_finish_ignored_stale", sourceTransitionId);
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] ignoring stale about-to-finish"
                << "sourceTransitionId=" << sourceTransitionId
                << "activeTransitionId=" << m_activeTrackTransitionId;
        }
        return;
    }

    prepareGaplessTransitionForSource(sourceTransitionId);
}

void PlaybackController::onAudioEndOfStream()
{
    const quint64 eosTransitionId = m_audioEngine
        ? m_audioEngine->lastEndOfStreamTransitionId()
        : 0;
    traceTransitionEvent("eos_received", eosTransitionId);
    handleTrackEndedInternal(eosTransitionId, true);
}

void PlaybackController::requestPlayIndex(int index, const QString &reason)
{
    if (!m_trackModel || !m_audioEngine) {
        return;
    }

    if (index < 0 || index >= m_trackModel->rowCount()) {
        return;
    }

    const QString targetPath = m_trackModel->getFilePath(index);
    if (targetPath.isEmpty()) {
        return;
    }

    traceTransitionEvent("request_play_index", 0, {
        {QStringLiteral("requestedIndex"), index},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("targetPath"), targetPath}
    });

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] play index requested"
            << "index=" << index
            << "reason=" << reason
            << "targetPath=" << targetPath
            << "currentIndex=" << m_trackModel->currentIndex()
            << "currentFile=" << m_audioEngine->currentFile();
    }

    const int currentIndex = m_trackModel->currentIndex();
    if (index == currentIndex) {
        if (m_audioEngine->currentFile() != targetPath) {
            const quint64 transitionId = nextTrackTransitionId();
            m_activeTrackTransitionId = transitionId;
            traceTransitionEvent("request_play_reload_current_selection", transitionId, {
                {QStringLiteral("index"), index},
                {QStringLiteral("targetPath"), targetPath}
            });
            clearGaplessTransitionState();
            setPendingTrackIndex(index);
            setTransitionState(TransitionPendingCommit);
            scheduleCueSeekForCurrentTrack(targetPath);
            startTrackLoadWatch();
            m_audioEngine->loadFileWithTransition(targetPath, transitionId);
            return;
        }

        traceTransitionEvent("request_play_resume_current", m_activeTrackTransitionId, {
            {QStringLiteral("index"), index}
        });
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
        m_audioEngine->play();
        return;
    }

    traceTransitionEvent("request_play_select_and_load", 0, {
        {QStringLiteral("index"), index},
        {QStringLiteral("targetPath"), targetPath}
    });
    clearGaplessTransitionState();
    setPendingTrackIndex(index);
    setTransitionState(TransitionPendingCommit);
    m_trackModel->setCurrentIndex(index);
}

void PlaybackController::nextTrack()
{
    if (!m_trackModel || !m_audioEngine) {
        return;
    }

    const int count = m_trackModel->rowCount();
    const int current = effectiveCurrentIndex();
    if (count <= 0 || current < 0) {
        traceTransitionEvent("navigate_next_no_target", m_activeTrackTransitionId);
        return;
    }

    int nextIndex = takeNextQueuedIndex();
    if (nextIndex < 0) {
        if (!m_shuffleEnabled) {
            if (current + 1 < count) {
                nextIndex = current + 1;
            } else if (m_repeatMode == RepeatAll) {
                nextIndex = 0;
            }
        } else {
            if (!isShuffleStateValid()) {
                regenerateShuffleOrder(current);
            } else {
                syncShuffleToCurrentTrack();
            }

            int currentPos = m_shufflePosition;
            if (currentPos < 0) {
                currentPos = currentShufflePosition();
                if (currentPos >= 0) {
                    m_shufflePosition = currentPos;
                }
            }

            if (currentPos >= 0) {
                int nextPos = currentPos + 1;
                if (nextPos >= m_shuffleOrder.size()) {
                    if (m_repeatMode == RepeatAll) {
                        regenerateShuffleOrder();
                        nextPos = 0;
                    } else {
                        nextPos = -1;
                    }
                }

                if (nextPos >= 0) {
                    m_shufflePosition = nextPos;
                    nextIndex = m_shuffleOrder.value(nextPos, -1);
                }
            }
        }
    }

    if (nextIndex < 0) {
        traceTransitionEvent("navigate_next_no_target", m_activeTrackTransitionId);
        return;
    }

    if (nextIndex == current) {
        navigateToNextTrackInternal(SessionEndReason::UserSkip);
        return;
    }

    traceTransitionEvent("next_track_direct_request", m_activeTrackTransitionId, {
        {QStringLiteral("nextIndex"), nextIndex},
        {QStringLiteral("reason"), QStringLiteral("user_skip")}
    });
    finalizeActiveSession(SessionEndReason::UserSkip, QDateTime::currentMSecsSinceEpoch());
    requestPlayIndex(nextIndex, QStringLiteral("playback_controller.next_track"));
}

void PlaybackController::previousTrack()
{
    if (!m_trackModel || !m_audioEngine) {
        return;
    }

    // Restart current track if playback position exceeds threshold.
    // For cue tracks, restart means seeking to the cue segment start.
    const int currentIndex = effectiveCurrentIndex();
    if (currentIndex >= 0 && currentIndex < m_trackModel->rowCount()) {
        const qint64 positionMs = qMax<qint64>(0, m_audioEngine->position());
        const bool isCue = m_trackModel->isCueTrack(currentIndex);
        const qint64 trackOriginMs = isCue
            ? qMax<qint64>(0, m_trackModel->cueStartMs(currentIndex))
            : 0;
        const qint64 elapsedInTrackMs = positionMs - trackOriginMs;

        if (elapsedInTrackMs > m_restartThresholdMs) {
            // Restart the current track / cue segment from its beginning
            m_audioEngine->seekWithSource(trackOriginMs,
                                          QStringLiteral("playback_controller.previous_restart"));
            m_audioEngine->play();
            return;
        }
    }

    navigateToPreviousTrackInternal(SessionEndReason::UserSkip);
}

void PlaybackController::clearGaplessTransitionState()
{
    if (m_gaplessQueuedIndex >= 0 || m_gaplessQueuedTransitionId > 0 || m_gaplessTransitionPending) {
        traceTransitionEvent("gapless_state_cleared", m_gaplessQueuedTransitionId, {
            {QStringLiteral("previousGaplessIndex"), m_gaplessQueuedIndex},
            {QStringLiteral("pending"), m_gaplessTransitionPending}
        });
    }

    const int previousGaplessIndex = m_gaplessQueuedIndex;

    m_gaplessQueuedIndex = -1;
    m_gaplessQueuedTransitionId = 0;
    m_gaplessTransitionPending = false;
    m_gaplessTrailingEosGuardUntilMs = 0;
    m_lastPreparedGaplessSourceTransitionId = 0;

    if (m_transitionState == TransitionPreparingGapless
        && m_pendingTrackIndex == previousGaplessIndex) {
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
    }
}

void PlaybackController::navigateToNextTrackInternal(SessionEndReason reason)
{
    if (!m_trackModel) {
        return;
    }

    const int nextIndex = calculateNextIndex(true);
    if (nextIndex < 0) {
        traceTransitionEvent("navigate_next_no_target", m_activeTrackTransitionId);
        return;
    }

    traceTransitionEvent("navigate_next_selected", m_activeTrackTransitionId, {
        {QStringLiteral("nextIndex"), nextIndex},
        {QStringLiteral("reason"), sourceForSessionEndReason(reason)}
    });

    finalizeActiveSession(reason, QDateTime::currentMSecsSinceEpoch());

    // Cancel any pending gapless transition
    clearGaplessTransitionState();
    setPendingTrackIndex(nextIndex);
    setTransitionState(TransitionPendingCommit);

    // Navigation commands use the playing track as the anchor, not the table
    // selection. If selection already points at the target row, setCurrentIndex()
    // becomes a no-op and would skip the actual load; explicitly request the
    // load in that case.
    const QString targetPath = m_trackModel->getFilePath(nextIndex);
    if (m_trackModel->currentIndex() == nextIndex
        && !targetPath.isEmpty()
        && (!m_audioEngine || m_audioEngine->currentFile() != targetPath)) {
        onTrackSelectionRequested(targetPath);
        return;
    }

    m_trackModel->setCurrentIndex(nextIndex);
}

void PlaybackController::navigateToPreviousTrackInternal(SessionEndReason reason)
{
    if (!m_trackModel) {
        return;
    }

    const int prevIndex = calculatePreviousIndex();
    if (prevIndex < 0) {
        traceTransitionEvent("navigate_previous_no_target", m_activeTrackTransitionId);
        return;
    }

    traceTransitionEvent("navigate_previous_selected", m_activeTrackTransitionId, {
        {QStringLiteral("prevIndex"), prevIndex},
        {QStringLiteral("reason"), sourceForSessionEndReason(reason)}
    });

    finalizeActiveSession(reason, QDateTime::currentMSecsSinceEpoch());

    // Cancel any pending gapless transition
    clearGaplessTransitionState();
    setPendingTrackIndex(prevIndex);
    setTransitionState(TransitionPendingCommit);

    const QString targetPath = m_trackModel->getFilePath(prevIndex);
    if (m_trackModel->currentIndex() == prevIndex
        && !targetPath.isEmpty()
        && (!m_audioEngine || m_audioEngine->currentFile() != targetPath)) {
        onTrackSelectionRequested(targetPath);
        return;
    }

    m_trackModel->setCurrentIndex(prevIndex);
}

void PlaybackController::handleTrackEnded()
{
    handleTrackEndedInternal(0, false);
}

void PlaybackController::handleTrackEndedInternal(quint64 eosTransitionId, bool fromEosSignal)
{
    if (!m_trackModel || !m_audioEngine) {
        return;
    }

    traceTransitionEvent("handle_track_ended_enter", eosTransitionId, {
        {QStringLiteral("fromEosSignal"), fromEosSignal}
    });

    if (fromEosSignal && eosTransitionId > 0) {
        if (m_activeTrackTransitionId > 0 && eosTransitionId < m_activeTrackTransitionId) {
            traceTransitionEvent("handle_track_ended_ignored_stale_eos", eosTransitionId);
            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Cue] ignoring stale EOS"
                    << "eosTransitionId=" << eosTransitionId
                    << "activeTransitionId=" << m_activeTrackTransitionId;
            }
            return;
        }
        if (m_lastHandledEosTransitionId == eosTransitionId) {
            traceTransitionEvent("handle_track_ended_ignored_duplicate_eos", eosTransitionId);
            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Cue] ignoring duplicate EOS"
                    << "eosTransitionId=" << eosTransitionId;
            }
            return;
        }
        m_lastHandledEosTransitionId = eosTransitionId;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // Some backends can still emit EOS from the previous stream right after a
    // successful gapless switch. Guard only the short post-transition window,
    // then always clear stale markers to avoid swallowing real EOS later.
    if (m_gaplessQueuedIndex >= 0 && !m_gaplessTransitionPending) {
        const bool eosState = (m_audioEngine->state() == AudioEngine::EndedState);
        const bool guardActive =
            (m_gaplessTrailingEosGuardUntilMs > 0 && nowMs <= m_gaplessTrailingEosGuardUntilMs);
        const qint64 posMs = qMax<qint64>(0, m_audioEngine->position());
        const qint64 durMs = qMax<qint64>(0, m_audioEngine->duration());
        const bool nearStartOfNewTrack =
            posMs <= kGaplessTrailingEosMaxPositionMs
            && (durMs <= 0 || (posMs + kGaplessTrailingEosMinRemainingMs) < durMs);
        const bool likelyTrailingGaplessEos =
            eosState && guardActive && nearStartOfNewTrack;
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] gapless trailing EOS guard"
                << "queuedIndex=" << m_gaplessQueuedIndex
                << "queuedTransitionId=" << m_gaplessQueuedTransitionId
                << "eosState=" << eosState
                << "guardActive=" << guardActive
                << "posMs=" << posMs
                << "durMs=" << durMs
                << "nearStartOfNewTrack=" << nearStartOfNewTrack
                << "ignoreEos=" << likelyTrailingGaplessEos;
        }
        clearGaplessTransitionState();
        if (likelyTrailingGaplessEos) {
            traceTransitionEvent("handle_track_ended_ignored_trailing_gapless_eos",
                                 eosTransitionId,
                                 {
                                     {QStringLiteral("posMs"), posMs},
                                     {QStringLiteral("durMs"), durMs}
                                 });
            return;
        }
    }

    finalizeActiveSession(SessionEndReason::TrackEnded, nowMs);

    // RepeatOne: just restart the current track
    if (m_repeatMode == RepeatOne && peekNextQueuedIndex() < 0) {
        const int currentIndex = effectiveCurrentIndex();
        const QString currentPath = m_trackModel->getFilePath(currentIndex);
        traceTransitionEvent("handle_track_ended_repeat_one_restart", eosTransitionId, {
            {QStringLiteral("currentIndex"), currentIndex},
            {QStringLiteral("currentPath"), currentPath}
        });
        if (!currentPath.isEmpty()) {
            beginPlaybackSession(currentPath, nowMs);
        }
        clearGaplessTransitionState();
        if (currentIndex < 0 || currentPath.isEmpty()) {
            setPendingTrackIndex(-1);
            setTransitionState(TransitionIdle);
            return;
        }

        const quint64 repeatTransitionId = nextTrackTransitionId();
        m_activeTrackTransitionId = qMax(m_activeTrackTransitionId, repeatTransitionId);
        traceTransitionEvent("handle_track_ended_repeat_one_reload",
                             repeatTransitionId,
                             {
                                 {QStringLiteral("currentIndex"), currentIndex},
                                 {QStringLiteral("currentPath"), currentPath}
                             });

        setPendingTrackIndex(currentIndex);
        setTransitionState(TransitionPendingCommit);
        scheduleCueSeekForCurrentTrack(currentPath);
        startTrackLoadWatch();
        m_audioEngine->loadFileWithTransition(currentPath, repeatTransitionId);
        return;
    }

    // Gapless transition was prepared but not completed - need to load the track manually
    if (m_gaplessQueuedIndex >= 0 && m_gaplessQueuedIndex < m_trackModel->rowCount()) {
        const int targetIndex = m_gaplessQueuedIndex;
        const QString targetPath = m_trackModel->getFilePath(targetIndex);
        const quint64 targetTransitionId = m_gaplessQueuedTransitionId;
        traceTransitionEvent("handle_track_ended_gapless_fallback", targetTransitionId, {
            {QStringLiteral("targetIndex"), targetIndex},
            {QStringLiteral("targetPath"), targetPath}
        });
        setPendingTrackIndex(targetIndex);
        setTransitionState(TransitionFallbackEos);
        clearGaplessTransitionState();
        if (targetTransitionId > 0 && !targetPath.isEmpty()) {
            m_forcedTrackLoadPath = targetPath;
            m_forcedTrackLoadTransitionId = targetTransitionId;
        }
        if (!m_playQueue.isEmpty() && !targetPath.isEmpty() && m_playQueue.front() == targetPath) {
            m_playQueue.removeFirst();
            emitQueueChanged();
        }
        // Use setCurrentIndex to trigger track loading via trackSelected signal
        m_trackModel->setCurrentIndex(targetIndex);
        return;
    }

    // No gapless transition was prepared, try to advance to adjacent track.
    // In reverse mode, natural progression goes backwards in playlist order.
    // Use TrackEnded reason since the session was already finalized at the top.
    clearGaplessTransitionState();
    if (m_audioEngine->reversePlayback()) {
        if (!canGoPrevious()) {
            traceTransitionEvent("handle_track_ended_no_previous", eosTransitionId);
            setPendingTrackIndex(-1);
            setTransitionState(TransitionIdle);
            return;
        }
        traceTransitionEvent("handle_track_ended_navigate_previous", eosTransitionId);
        navigateToPreviousTrackInternal(SessionEndReason::TrackEnded);
    } else {
        if (!canGoNext()) {
            traceTransitionEvent("handle_track_ended_no_next", eosTransitionId);
            setPendingTrackIndex(-1);
            setTransitionState(TransitionIdle);
            return;
        }
        traceTransitionEvent("handle_track_ended_navigate_next", eosTransitionId);
        navigateToNextTrackInternal(SessionEndReason::TrackEnded);
    }
}

void PlaybackController::prepareGaplessTransition()
{
    const quint64 sourceTransitionId = m_audioEngine
        ? m_audioEngine->lastAboutToFinishTransitionId()
        : 0;
    prepareGaplessTransitionForSource(sourceTransitionId);
}

void PlaybackController::prepareGaplessTransitionForSource(quint64 sourceTransitionId)
{
    if (!m_trackModel || !m_audioEngine) {
        return;
    }

    traceTransitionEvent("prepare_gapless_requested", sourceTransitionId);

    if (sourceTransitionId > 0
        && m_activeTrackTransitionId > 0
        && sourceTransitionId < m_activeTrackTransitionId) {
        traceTransitionEvent("prepare_gapless_ignored_stale", sourceTransitionId);
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] gapless prepare ignored: stale source transition"
                << "sourceTransitionId=" << sourceTransitionId
                << "activeTransitionId=" << m_activeTrackTransitionId;
        }
        return;
    }

    if (m_audioEngine->reversePlayback()) {
        traceTransitionEvent("prepare_gapless_skipped_reverse", sourceTransitionId);
        clearGaplessTransitionState();
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
        return;
    }

    // RepeatOne is handled in handleTrackEnded, no gapless needed
    if (m_repeatMode == RepeatOne) {
        traceTransitionEvent("prepare_gapless_skipped_repeat_one", sourceTransitionId);
        clearGaplessTransitionState();
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
        return;
    }

    const int nextIndex = calculateNextIndex(false);
    if (nextIndex < 0) {
        traceTransitionEvent("prepare_gapless_no_next_track", sourceTransitionId);
        clearGaplessTransitionState();
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
        return;
    }

    if (m_gaplessTransitionPending
        && sourceTransitionId > 0
        && m_lastPreparedGaplessSourceTransitionId == sourceTransitionId) {
        traceTransitionEvent("prepare_gapless_ignored_duplicate_source", sourceTransitionId, {
            {QStringLiteral("queuedTransitionId"),
             static_cast<qulonglong>(m_gaplessQueuedTransitionId)}
        });
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] gapless prepare ignored: duplicate about-to-finish"
                << "sourceTransitionId=" << sourceTransitionId
                << "queuedTransitionId=" << m_gaplessQueuedTransitionId;
        }
        return;
    }

    const quint64 transitionId = nextTrackTransitionId();
    m_gaplessQueuedTransitionId = transitionId;
    m_gaplessQueuedIndex = nextIndex;
    m_gaplessTransitionPending = true;
    m_lastPreparedGaplessSourceTransitionId = sourceTransitionId;
    setPendingTrackIndex(nextIndex);
    setTransitionState(TransitionPreparingGapless);
    m_audioEngine->setNextFileWithTransition(m_trackModel->getFilePath(nextIndex), transitionId);
    traceTransitionEvent("prepare_gapless_set_next_file", transitionId, {
        {QStringLiteral("sourceTransitionId"),
         static_cast<qulonglong>(sourceTransitionId)},
        {QStringLiteral("nextIndex"), nextIndex},
        {QStringLiteral("nextFile"), m_trackModel->getFilePath(nextIndex)}
    });

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] gapless prepared"
            << "sourceTransitionId=" << sourceTransitionId
            << "queuedTransitionId=" << transitionId
            << "queuedIndex=" << m_gaplessQueuedIndex
            << "queuedFile=" << m_trackModel->getFilePath(nextIndex);
    }
}

void PlaybackController::setTrackLoadTimeoutMs(int timeoutMs)
{
    const int clamped = qMax(1000, timeoutMs);
    if (m_trackLoadTimeoutMs == clamped) {
        return;
    }

    m_trackLoadTimeoutMs = clamped;
    m_trackLoadTimer.setInterval(m_trackLoadTimeoutMs);
    emit trackLoadTimeoutMsChanged();
}

void PlaybackController::setMaxConsecutiveErrors(int maxErrors)
{
    const int clamped = qMax(1, maxErrors);
    if (m_maxConsecutiveErrors == clamped) {
        return;
    }

    m_maxConsecutiveErrors = clamped;
    emit maxConsecutiveErrorsChanged();
}

void PlaybackController::setDeterministicShuffleEnabled(bool enabled)
{
    if (m_deterministicShuffleEnabled == enabled) {
        return;
    }

    m_deterministicShuffleEnabled = enabled;
    m_shuffleGeneration = 0;
    if (m_shuffleEnabled) {
        regenerateShuffleOrder(effectiveCurrentIndex());
    }
    emit deterministicShuffleEnabledChanged();
    emit navigationStateChanged();
}

void PlaybackController::setShuffleSeed(quint32 seed)
{
    if (m_shuffleSeed == seed) {
        return;
    }

    m_shuffleSeed = seed;
    m_shuffleGeneration = 0;
    if (m_shuffleEnabled) {
        regenerateShuffleOrder(effectiveCurrentIndex());
    }
    emit shuffleSeedChanged();
    emit navigationStateChanged();
}

void PlaybackController::setRepeatableShuffle(bool enabled)
{
    if (m_repeatableShuffle == enabled) {
        return;
    }

    m_repeatableShuffle = enabled;
    m_shuffleGeneration = 0;
    if (m_shuffleEnabled) {
        regenerateShuffleOrder(effectiveCurrentIndex());
    }
    emit repeatableShuffleChanged();
    emit navigationStateChanged();
}

void PlaybackController::addToQueue(int index)
{
    if (!m_trackModel || index < 0 || index >= m_trackModel->rowCount()) {
        return;
    }

    if (index == activePlaybackIndex()) {
        return;
    }

    const QString filePath = m_trackModel->getFilePath(index);
    if (filePath.isEmpty() || m_playQueue.contains(filePath)) {
        return;
    }

    m_playQueue.append(filePath);
    emitQueueChanged();
}

void PlaybackController::playNextInQueue(int index)
{
    if (!m_trackModel || index < 0 || index >= m_trackModel->rowCount()) {
        return;
    }

    if (index == activePlaybackIndex()) {
        return;
    }

    const QString filePath = m_trackModel->getFilePath(index);
    if (filePath.isEmpty()) {
        return;
    }

    m_playQueue.removeAll(filePath);
    m_playQueue.prepend(filePath);
    emitQueueChanged();
}

void PlaybackController::clearQueue()
{
    if (m_playQueue.isEmpty()) {
        return;
    }

    m_playQueue.clear();
    emitQueueChanged();
}

void PlaybackController::removeQueueAt(int queueIndex)
{
    if (queueIndex < 0 || queueIndex >= m_playQueue.size()) {
        return;
    }

    m_playQueue.removeAt(queueIndex);
    emitQueueChanged();
}

void PlaybackController::moveQueueItem(int from, int to)
{
    if (from < 0 || from >= m_playQueue.size() || to < 0 || to >= m_playQueue.size()) {
        return;
    }
    if (from == to) {
        return;
    }

    m_playQueue.move(from, to);
    emitQueueChanged();
}

bool PlaybackController::isQueued(const QString &filePath) const
{
    return !filePath.isEmpty() && m_playQueue.contains(filePath);
}

int PlaybackController::queuedPosition(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        return -1;
    }
    return m_playQueue.indexOf(filePath);
}

QVariantList PlaybackController::queueItems() const
{
    QVariantList items;
    if (!m_trackModel || m_playQueue.isEmpty()) {
        return items;
    }

    const QVector<Track> &tracks = m_trackModel->tracks();
    items.reserve(m_playQueue.size());

    for (const QString &path : m_playQueue) {
        const int trackIndex = findTrackIndexByPath(path);
        if (trackIndex < 0 || trackIndex >= tracks.size()) {
            continue;
        }

        const Track &track = tracks.at(trackIndex);

        QVariantMap item;
        item.insert(QStringLiteral("filePath"), path);
        item.insert(QStringLiteral("trackIndex"), trackIndex);
        item.insert(QStringLiteral("displayName"), track.displayName());
        item.insert(QStringLiteral("artist"), track.artist);
        item.insert(QStringLiteral("duration"), track.duration);
        items.push_back(item);
    }

    return items;
}

bool PlaybackController::isShuffleStateValid() const
{
    if (!m_trackModel) {
        return false;
    }

    const int count = m_trackModel->rowCount();
    if (count <= 0) {
        return m_shuffleOrder.isEmpty();
    }
    if (m_shuffleOrder.size() != count) {
        return false;
    }

    QVector<bool> seen(count, false);
    for (int value : m_shuffleOrder) {
        if (value < 0 || value >= count || seen[value]) {
            return false;
        }
        seen[value] = true;
    }

    return true;
}

void PlaybackController::regenerateShuffleOrder(int startIndex)
{
    m_shuffleOrder.clear();
    m_shufflePosition = -1;

    if (!m_trackModel) {
        return;
    }

    const int count = m_trackModel->rowCount();
    if (count <= 0) {
        return;
    }

    QVector<int> indices;
    indices.reserve(count);
    for (int i = 0; i < count; ++i) {
        indices.append(i);
    }

    if (startIndex >= 0 && startIndex < count) {
        indices.removeOne(startIndex);
    } else {
        startIndex = -1;
    }

    const bool deterministic = m_deterministicShuffleEnabled;
    QRandomGenerator deterministicGenerator(deterministic ? nextShuffleSeed() : 0u);
    for (int i = indices.size() - 1; i > 0; --i) {
        const int j = deterministic
            ? deterministicGenerator.bounded(i + 1)
            : QRandomGenerator::global()->bounded(i + 1);
        indices.swapItemsAt(i, j);
    }

    if (startIndex >= 0) {
        m_shuffleOrder.append(startIndex);
        m_shufflePosition = 0;
    }
    m_shuffleOrder += indices;

    if (m_shufflePosition < 0 && startIndex < 0 && !m_shuffleOrder.isEmpty()) {
        m_shufflePosition = currentShufflePosition();
    }
}

void PlaybackController::syncShuffleToCurrentTrack()
{
    if (!m_shuffleEnabled || !m_trackModel) {
        return;
    }

    const int current = effectiveCurrentIndex();
    if (!isShuffleStateValid()) {
        regenerateShuffleOrder(current);
        return;
    }

    m_shufflePosition = m_shuffleOrder.indexOf(current);
    if (m_shufflePosition < 0) {
        regenerateShuffleOrder(current);
    }
}

int PlaybackController::currentShufflePosition() const
{
    const int current = effectiveCurrentIndex();
    if (current < 0) {
        return -1;
    }
    return m_shuffleOrder.indexOf(current);
}

int PlaybackController::calculateNextIndex(bool advanceShuffleState)
{
    if (!m_trackModel) {
        return -1;
    }

    const int count = m_trackModel->rowCount();
    const int current = effectiveCurrentIndex();
    if (count <= 0 || current < 0) {
        return -1;
    }

    const int queuedIndex = advanceShuffleState ? takeNextQueuedIndex() : peekNextQueuedIndex();
    if (queuedIndex >= 0) {
        return queuedIndex;
    }

    if (m_repeatMode == RepeatOne) {
        return current;
    }

    if (!m_shuffleEnabled) {
        if (current + 1 < count) {
            return current + 1;
        }
        if (m_repeatMode == RepeatAll) {
            return 0;
        }
        return -1;
    }

    if (!isShuffleStateValid()) {
        regenerateShuffleOrder(current);
    } else {
        syncShuffleToCurrentTrack();
    }

    int currentPos = m_shufflePosition;
    if (currentPos < 0) {
        currentPos = currentShufflePosition();
        if (currentPos < 0) {
            return -1;
        }
        if (advanceShuffleState) {
            m_shufflePosition = currentPos;
        }
    }

    int nextPos = currentPos + 1;
    if (nextPos >= m_shuffleOrder.size()) {
        if (m_repeatMode != RepeatAll) {
            return -1;
        }
        regenerateShuffleOrder();
        nextPos = 0;
    }

    if (advanceShuffleState) {
        m_shufflePosition = nextPos;
    }
    return m_shuffleOrder.value(nextPos, -1);
}

int PlaybackController::calculatePreviousIndex()
{
    if (!m_trackModel) {
        return -1;
    }

    const int count = m_trackModel->rowCount();
    const int current = effectiveCurrentIndex();
    if (count <= 0 || current < 0) {
        return -1;
    }

    if (!m_shuffleEnabled) {
        if (current > 0) {
            return current - 1;
        }
        if (m_repeatMode == RepeatAll) {
            return count - 1;
        }
        return -1;
    }

    if (!isShuffleStateValid()) {
        regenerateShuffleOrder(current);
    } else {
        syncShuffleToCurrentTrack();
    }

    int pos = m_shufflePosition;
    if (pos < 0) {
        pos = currentShufflePosition();
        if (pos < 0) {
            return -1;
        }
    }

    pos -= 1;
    if (pos < 0) {
        if (m_repeatMode != RepeatAll) {
            return -1;
        }
        pos = m_shuffleOrder.size() - 1;
    }
    m_shufflePosition = pos;
    return m_shuffleOrder.value(pos, -1);
}

void PlaybackController::startTrackLoadWatch()
{
    m_trackLoading = true;
    m_trackLoadTimer.start();
}

void PlaybackController::clearTrackLoadWatch()
{
    m_trackLoading = false;
    m_trackLoadTimer.stop();
}

void PlaybackController::onLoadSuccess()
{
    clearTrackLoadWatch();
    applyPendingCueSeekIfReady(m_audioEngine ? m_audioEngine->currentFile() : QString());

    if (m_consecutiveErrors != 0) {
        setConsecutiveErrors(0);
    }
    if (!m_lastError.isEmpty()) {
        setLastError(QString());
    }
}

void PlaybackController::onLoadFailure(const QString &message)
{
    if (m_handlingFailure || !m_audioEngine) {
        return;
    }

    m_handlingFailure = true;
    clearTrackLoadWatch();
    setConsecutiveErrors(m_consecutiveErrors + 1);
    setLastError(message);

    qWarning() << "[PlaybackController] Load failure:" << message
               << "consecutive:" << m_consecutiveErrors
               << "max:" << m_maxConsecutiveErrors;

    emit errorRaised(message);

    if (m_consecutiveErrors >= m_maxConsecutiveErrors) {
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
        m_audioEngine->stop();
        emit errorRaised(QStringLiteral("Playback stopped after repeated load failures."));
    } else if (!canGoNext()) {
        setPendingTrackIndex(-1);
        setTransitionState(TransitionIdle);
        m_audioEngine->stop();
    } else {
        nextTrack();
    }

    m_handlingFailure = false;
}

int PlaybackController::activePlaybackIndex() const
{
    if (!m_trackModel) {
        return -1;
    }

    const int count = m_trackModel->rowCount();
    if (count <= 0) {
        return -1;
    }

    if (m_audioEngine) {
        const QString currentFile = m_audioEngine->currentFile();
        if (!currentFile.isEmpty()) {
            const int byPath = findTrackIndexByPath(currentFile);
            if (byPath >= 0) {
                if (m_trackModel->isCueTrack(byPath)) {
                    const qint64 currentPosMs = qMax<qint64>(0, m_audioEngine->position());
                    const int cueByPos = findCueTrackIndexForPosition(currentFile, currentPosMs);
                    if (cueByPos >= 0) {
                        return cueByPos;
                    }
                    if (m_activeTrackIndex >= 0
                        && m_activeTrackIndex < count
                        && m_trackModel->isCueTrack(m_activeTrackIndex)
                        && m_trackModel->getFilePath(m_activeTrackIndex) == currentFile) {
                        return m_activeTrackIndex;
                    }
                }
                return byPath;
            }
            return -1;
        }
    }

    if (m_activeTrackIndex >= 0 && m_activeTrackIndex < count) {
        return m_activeTrackIndex;
    }

    return -1;
}

int PlaybackController::effectiveCurrentIndex() const
{
    const int activeIndex = activePlaybackIndex();
    if (activeIndex >= 0) {
        return activeIndex;
    }
    if (!m_trackModel) {
        return -1;
    }
    const int selectedIndex = m_trackModel->currentIndex();
    if (selectedIndex < 0 || selectedIndex >= m_trackModel->rowCount()) {
        return -1;
    }
    return selectedIndex;
}

int PlaybackController::findTrackIndexByPath(const QString &filePath) const
{
    if (!m_trackModel || filePath.isEmpty()) {
        return -1;
    }

    const int count = m_trackModel->rowCount();
    for (int i = 0; i < count; ++i) {
        if (m_trackModel->getFilePath(i) == filePath) {
            return i;
        }
    }
    return -1;
}

int PlaybackController::peekNextQueuedIndex() const
{
    for (const QString &queuedPath : m_playQueue) {
        const int index = findTrackIndexByPath(queuedPath);
        if (index >= 0) {
            return index;
        }
    }
    return -1;
}

int PlaybackController::takeNextQueuedIndex()
{
    bool changed = false;

    while (!m_playQueue.isEmpty()) {
        const QString queuedPath = m_playQueue.front();
        m_playQueue.removeFirst();
        changed = true;

        const int index = findTrackIndexByPath(queuedPath);
        if (index >= 0) {
            emitQueueChanged();
            return index;
        }
    }

    if (changed) {
        emitQueueChanged();
    }
    return -1;
}

void PlaybackController::removeCurrentTrackFromQueue()
{
    if (!m_trackModel || m_playQueue.isEmpty()) {
        return;
    }

    const int currentIndex = activePlaybackIndex();
    if (currentIndex < 0 || currentIndex >= m_trackModel->rowCount()) {
        return;
    }

    const QString currentPath = m_trackModel->getFilePath(currentIndex);
    if (currentPath.isEmpty()) {
        return;
    }

    const int removed = m_playQueue.removeAll(currentPath);
    if (removed <= 0) {
        return;
    }
    emitQueueChanged();
}

void PlaybackController::pruneQueue()
{
    if (m_playQueue.isEmpty()) {
        return;
    }

    if (!m_trackModel) {
        m_playQueue.clear();
        emitQueueChanged();
        return;
    }

    QSet<QString> playlistPaths;
    const int count = m_trackModel->rowCount();
    playlistPaths.reserve(count);
    for (int i = 0; i < count; ++i) {
        const QString path = m_trackModel->getFilePath(i);
        if (!path.isEmpty()) {
            playlistPaths.insert(path);
        }
    }

    bool changed = false;
    for (int i = m_playQueue.size() - 1; i >= 0; --i) {
        if (!playlistPaths.contains(m_playQueue.at(i))) {
            m_playQueue.removeAt(i);
            changed = true;
        }
    }

    if (changed) {
        emitQueueChanged();
    }
}

void PlaybackController::emitQueueChanged()
{
    ++m_queueRevision;
    emit queueChanged();
    emit navigationStateChanged();
}

void PlaybackController::captureShuffleSnapshot()
{
    m_shuffleSnapshotPaths.clear();
    m_shuffleSnapshotCurrentPath.clear();
    m_shuffleSnapshotValid = false;

    if (!m_trackModel || !m_shuffleEnabled || m_shuffleOrder.isEmpty()) {
        return;
    }

    const int currentIndex = effectiveCurrentIndex();
    m_shuffleSnapshotCurrentPath = m_trackModel->getFilePath(currentIndex);
    m_shuffleSnapshotPaths.reserve(m_shuffleOrder.size());
    for (int index : m_shuffleOrder) {
        const QString path = m_trackModel->getFilePath(index);
        if (!path.isEmpty()) {
            m_shuffleSnapshotPaths.push_back(path);
        }
    }

    m_shuffleSnapshotValid = !m_shuffleSnapshotPaths.isEmpty();
}

void PlaybackController::restoreShuffleSnapshot()
{
    if (!m_trackModel || !m_shuffleEnabled) {
        m_shuffleSnapshotPaths.clear();
        m_shuffleSnapshotCurrentPath.clear();
        m_shuffleSnapshotValid = false;
        return;
    }

    const int count = m_trackModel->rowCount();
    if (count <= 0) {
        m_shuffleOrder.clear();
        m_shufflePosition = -1;
        m_shuffleSnapshotPaths.clear();
        m_shuffleSnapshotCurrentPath.clear();
        m_shuffleSnapshotValid = false;
        return;
    }

    if (!m_shuffleSnapshotValid) {
        regenerateShuffleOrder(effectiveCurrentIndex());
        return;
    }

    QVector<int> restoredOrder;
    restoredOrder.reserve(count);
    QSet<int> usedIndices;
    usedIndices.reserve(count);
    QSet<QString> usedPaths;
    usedPaths.reserve(m_shuffleSnapshotPaths.size());

    for (const QString &path : m_shuffleSnapshotPaths) {
        if (path.isEmpty() || usedPaths.contains(path)) {
            continue;
        }

        const int index = findTrackIndexByPath(path);
        if (index < 0 || usedIndices.contains(index)) {
            continue;
        }

        restoredOrder.push_back(index);
        usedIndices.insert(index);
        usedPaths.insert(path);
    }

    for (int i = 0; i < count; ++i) {
        if (!usedIndices.contains(i)) {
            restoredOrder.push_back(i);
        }
    }

    m_shuffleOrder = restoredOrder;

    int currentIndex = -1;
    if (!m_shuffleSnapshotCurrentPath.isEmpty()) {
        currentIndex = findTrackIndexByPath(m_shuffleSnapshotCurrentPath);
    }
    if (currentIndex < 0) {
        currentIndex = effectiveCurrentIndex();
    }

    m_shufflePosition = m_shuffleOrder.indexOf(currentIndex);
    if (m_shufflePosition < 0 && !m_shuffleOrder.isEmpty()) {
        m_shufflePosition = 0;
    }

    m_shuffleSnapshotPaths.clear();
    m_shuffleSnapshotCurrentPath.clear();
    m_shuffleSnapshotValid = false;
}

quint32 PlaybackController::nextShuffleSeed()
{
    quint32 seed = m_shuffleSeed;

    if (m_trackModel) {
        seed ^= static_cast<quint32>(m_trackModel->rowCount() * 0x9E3779B1u);
        const QString currentPath = m_trackModel->getFilePath(effectiveCurrentIndex());
        if (!currentPath.isEmpty()) {
            seed ^= static_cast<quint32>(qHash(currentPath));
        }
    }

    if (!m_repeatableShuffle) {
        seed ^= static_cast<quint32>((m_shuffleGeneration + 1) * 0x85EBCA6Bu);
        ++m_shuffleGeneration;
    }

    return seed;
}

void PlaybackController::setConsecutiveErrors(int value)
{
    const int clamped = qMax(0, value);
    if (m_consecutiveErrors == clamped) {
        return;
    }

    m_consecutiveErrors = clamped;
    emit consecutiveErrorsChanged();
}

void PlaybackController::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

void PlaybackController::setActiveTrackIndex(int index)
{
    int boundedIndex = index;
    if (m_trackModel) {
        if (boundedIndex < -1 || boundedIndex >= m_trackModel->rowCount()) {
            boundedIndex = -1;
        }
    }

    if (m_activeTrackIndex == boundedIndex) {
        return;
    }

    m_activeTrackIndex = boundedIndex;
    m_lastCueBoundaryTrackIndex = -1;
    emit activeTrackIndexChanged();
    emit navigationStateChanged();
}

void PlaybackController::setPendingTrackIndex(int index)
{
    int boundedIndex = index;
    if (m_trackModel) {
        if (boundedIndex < -1 || boundedIndex >= m_trackModel->rowCount()) {
            boundedIndex = -1;
        }
    }

    if (m_pendingTrackIndex == boundedIndex) {
        return;
    }

    m_pendingTrackIndex = boundedIndex;
    emit pendingTrackIndexChanged();
}

void PlaybackController::setTransitionState(TransitionState state)
{
    if (m_transitionState == state) {
        return;
    }

    m_transitionState = state;
    emit transitionStateChanged();
}

void PlaybackController::traceTransitionEvent(const char *event,
                                              quint64 transitionId,
                                              const QVariantMap &extra) const
{
    if (!DiagnosticsFlags::transitionTraceEnabled()) {
        return;
    }

    QVariantMap payload = extra;
    payload.insert(QStringLiteral("activeTransitionId"),
                   static_cast<qulonglong>(m_activeTrackTransitionId));
    payload.insert(QStringLiteral("activeTrackIndex"), m_activeTrackIndex);
    payload.insert(QStringLiteral("pendingTrackIndex"), m_pendingTrackIndex);
    payload.insert(QStringLiteral("selectedTrackIndex"),
                   m_trackModel ? m_trackModel->currentIndex() : -1);
    payload.insert(QStringLiteral("transitionState"),
                   static_cast<int>(m_transitionState));
    payload.insert(QStringLiteral("gaplessQueuedIndex"), m_gaplessQueuedIndex);
    payload.insert(QStringLiteral("gaplessQueuedTransitionId"),
                   static_cast<qulonglong>(m_gaplessQueuedTransitionId));
    payload.insert(QStringLiteral("gaplessPending"), m_gaplessTransitionPending);
    payload.insert(QStringLiteral("currentFile"),
                   m_audioEngine ? m_audioEngine->currentFile() : QString());

    qInfo().noquote() << "[TransitionTrace]"
                      << transitionTracePayload(QStringLiteral("PlaybackController"),
                                                event,
                                                transitionId,
                                                payload);
}

void PlaybackController::onCurrentFileChanged(const QString &filePath)
{
    const quint64 fileTransitionId = m_audioEngine
        ? m_audioEngine->currentTransitionId()
        : 0;
    traceTransitionEvent("current_file_changed", fileTransitionId, {
        {QStringLiteral("filePath"), filePath}
    });

    if (fileTransitionId > 0 && fileTransitionId > m_transitionIdCounter) {
        m_transitionIdCounter = fileTransitionId;
    }

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] current file changed"
            << "file=" << filePath
            << "transitionId=" << fileTransitionId
            << "activeTransitionId=" << m_activeTrackTransitionId
            << "gaplessPending=" << m_gaplessTransitionPending
            << "gaplessIndex=" << m_gaplessQueuedIndex
            << "gaplessTransitionId=" << m_gaplessQueuedTransitionId
            << "pendingCueSeekFile=" << m_pendingCueSeekFilePath
            << "pendingCueSeekMs=" << m_pendingCueSeekMs;
    }

    if (fileTransitionId > 0
        && m_activeTrackTransitionId > 0
        && fileTransitionId < m_activeTrackTransitionId) {
        traceTransitionEvent("current_file_changed_ignored_stale", fileTransitionId, {
            {QStringLiteral("filePath"), filePath}
        });
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] current file change ignored: stale transition"
                << "file=" << filePath
                << "transitionId=" << fileTransitionId
                << "activeTransitionId=" << m_activeTrackTransitionId;
        }
        return;
    }

    if (fileTransitionId > 0) {
        m_activeTrackTransitionId = qMax(m_activeTrackTransitionId, fileTransitionId);
    }

    if (m_trackLoading && !filePath.isEmpty()) {
        onLoadSuccess();
    }

    applyPendingCueSeekIfReady(filePath);

    bool gaplessHandled = false;
    if (m_gaplessTransitionPending && m_gaplessQueuedIndex >= 0) {
        if (!m_trackModel) {
            return;
        }

        bool skipExpectedPathCheck = false;
        if (m_gaplessQueuedTransitionId > 0
            && fileTransitionId > 0
            && fileTransitionId != m_gaplessQueuedTransitionId) {
            traceTransitionEvent("current_file_changed_gapless_id_mismatch", fileTransitionId, {
                {QStringLiteral("expectedTransitionId"),
                 static_cast<qulonglong>(m_gaplessQueuedTransitionId)},
                {QStringLiteral("filePath"), filePath}
            });
            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Cue] gapless transition ignored: transition id mismatch"
                    << "expectedTransitionId=" << m_gaplessQueuedTransitionId
                    << "actualTransitionId=" << fileTransitionId
                    << "file=" << filePath;
            }
            if (fileTransitionId > m_gaplessQueuedTransitionId) {
                clearGaplessTransitionState();
                skipExpectedPathCheck = true;
            } else {
                return;
            }
        }

        if (!skipExpectedPathCheck && m_gaplessTransitionPending && m_gaplessQueuedIndex >= 0) {
            // Verify the file matches what we expected for gapless
            const QString expectedPath = m_trackModel->getFilePath(m_gaplessQueuedIndex);
            if (filePath == expectedPath) {
                traceTransitionEvent("current_file_changed_gapless_confirm", fileTransitionId, {
                    {QStringLiteral("expectedPath"), expectedPath}
                });
                confirmGaplessTransition(m_gaplessQueuedIndex, fileTransitionId);
                gaplessHandled = true;
            } else {
                traceTransitionEvent("current_file_changed_gapless_unexpected_file",
                                     fileTransitionId,
                                     {
                                         {QStringLiteral("expectedPath"), expectedPath},
                                         {QStringLiteral("actualPath"), filePath}
                                     });
                if (seekDiagEnabled()) {
                    qInfo().noquote()
                        << "[SeekDiag][Cue] gapless transition canceled: unexpected file"
                        << "expectedFile=" << expectedPath
                        << "actualFile=" << filePath;
                }
                clearGaplessTransitionState();
            }
        }
    }

    if (gaplessHandled || !m_trackModel) {
        return;
    }

    int resolvedIndex = -1;
    if (m_pendingTrackIndex >= 0
        && m_pendingTrackIndex < m_trackModel->rowCount()
        && m_trackModel->getFilePath(m_pendingTrackIndex) == filePath) {
        resolvedIndex = m_pendingTrackIndex;
    } else {
        resolvedIndex = findTrackIndexByPath(filePath);
    }

    if (resolvedIndex >= 0) {
        setActiveTrackIndex(resolvedIndex);
        traceTransitionEvent("current_file_changed_resolved", fileTransitionId, {
            {QStringLiteral("resolvedIndex"), resolvedIndex},
            {QStringLiteral("filePath"), filePath}
        });
        removeCurrentTrackFromQueue();
        if (m_shuffleEnabled) {
            syncShuffleToCurrentTrack();
        }
    } else if (!filePath.isEmpty()) {
        setActiveTrackIndex(-1);
        traceTransitionEvent("current_file_changed_unresolved", fileTransitionId, {
            {QStringLiteral("filePath"), filePath}
        });
    }

    if (m_pendingTrackIndex >= 0) {
        bool pendingMatches = false;
        if (m_pendingTrackIndex < m_trackModel->rowCount()) {
            pendingMatches = m_trackModel->getFilePath(m_pendingTrackIndex) == filePath;
        }
        if (pendingMatches || !filePath.isEmpty()) {
            setPendingTrackIndex(-1);
        }
    }

    if (!filePath.isEmpty()) {
        setTransitionState(TransitionCommitted);
        traceTransitionEvent("current_file_changed_committed", fileTransitionId, {
            {QStringLiteral("filePath"), filePath}
        });
    }
}

void PlaybackController::clearPendingCueSeek()
{
    m_pendingCueSeekFilePath.clear();
    m_pendingCueSeekMs = -1;
}

void PlaybackController::scheduleCueSeekForCurrentTrack(const QString &filePath)
{
    clearPendingCueSeek();
    if (!m_trackModel || filePath.isEmpty()) {
        return;
    }

    const int cueIndex = findTrackIndexByPath(filePath);
    if (cueIndex < 0 || cueIndex >= m_trackModel->rowCount()) {
        return;
    }
    if (!m_trackModel->isCueTrack(cueIndex)) {
        // Regular tracks in reverse mode are positioned by AudioEngine's
        // reverse-pending-start path using the target track duration.
        return;
    }

    const qint64 cueStartMs = qMax<qint64>(0, m_trackModel->cueStartMs(cueIndex));
    const qint64 cueEndMs = m_trackModel->cueEndMs(cueIndex);
    const bool reversePlayback = m_audioEngine && m_audioEngine->reversePlayback();

    qint64 cueSeekMs = cueStartMs;
    if (reversePlayback) {
        if (cueEndMs > cueStartMs) {
            cueSeekMs = cueEndMs;
        } else {
            // For last/unknown cue end in reverse mode, rely on AudioEngine
            // reverse start at track end to avoid carrying old-track duration.
            cueSeekMs = -1;
        }
    }

    if (cueSeekMs <= 0) {
        return;
    }

    m_pendingCueSeekFilePath = filePath;
    m_pendingCueSeekMs = cueSeekMs;
    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] scheduled pending cue seek"
            << "file=" << filePath
            << "index=" << cueIndex
            << "cueStartMs=" << cueStartMs
            << "cueEndMs=" << cueEndMs
            << "reverse=" << reversePlayback
            << "seekMs=" << cueSeekMs;
    }
}

void PlaybackController::applyPendingCueSeekIfReady(const QString &filePath)
{
    if (!m_audioEngine || m_pendingCueSeekMs <= 0 || filePath.isEmpty()) {
        return;
    }
    if (filePath != m_pendingCueSeekFilePath) {
        return;
    }

    const AudioEngine::PlaybackState engineState = m_audioEngine->state();
    if (engineState != AudioEngine::PlayingState &&
        engineState != AudioEngine::PausedState) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] pending cue seek deferred: engine state not seek-ready"
                << "file=" << filePath
                << "pendingSeekMs=" << m_pendingCueSeekMs
                << "engineState=" << static_cast<int>(engineState);
        }
        return;
    }

    const qint64 durationMs = m_audioEngine->duration();
    if (durationMs <= 0) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Cue] pending cue seek deferred: duration unresolved"
                << "file=" << filePath
                << "pendingSeekMs=" << m_pendingCueSeekMs
                << "engineState=" << static_cast<int>(engineState)
                << "durationMs=" << durationMs;
        }
        return;
    }

    const qint64 seekToMs = m_pendingCueSeekMs;
    clearPendingCueSeek();
    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] applying pending cue seek"
            << "file=" << filePath
            << "seekToMs=" << seekToMs
            << "durationMs=" << durationMs
            << "engineState=" << static_cast<int>(engineState);
    }
    m_audioEngine->seekWithSource(seekToMs,
                                  QStringLiteral("playback_controller.pending_cue_seek"));
}

void PlaybackController::confirmGaplessTransition(int index, quint64 transitionId)
{
    if (!m_trackModel || index < 0 || index >= m_trackModel->rowCount()) {
        return;
    }

    if (transitionId > 0) {
        m_activeTrackTransitionId = qMax(m_activeTrackTransitionId, transitionId);
        m_transitionIdCounter = qMax(m_transitionIdCounter, transitionId);
    }

    traceTransitionEvent("confirm_gapless_transition", transitionId, {
        {QStringLiteral("index"), index},
        {QStringLiteral("filePath"), m_trackModel->getFilePath(index)}
    });

    // Update TrackModel index silently (file already playing)
    m_trackModel->setCurrentIndexSilently(index);
    setActiveTrackIndex(index);
    setPendingTrackIndex(-1);
    setTransitionState(TransitionCommitted);

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Cue] gapless transition confirmed"
            << "index=" << index
            << "transitionId=" << transitionId
            << "file=" << m_trackModel->getFilePath(index);
    }

    removeCurrentTrackFromQueue();
    if (m_shuffleEnabled) {
        syncShuffleToCurrentTrack();
    }

    // Keep queued index briefly to identify trailing EOS from the old stream.
    m_gaplessTransitionPending = false;
    m_gaplessTrailingEosGuardUntilMs =
        QDateTime::currentMSecsSinceEpoch() + kGaplessTrailingEosGuardWindowMs;

    // Clear any errors on successful transition
    if (m_consecutiveErrors != 0) {
        setConsecutiveErrors(0);
    }
    if (!m_lastError.isEmpty()) {
        setLastError(QString());
    }

    emit navigationStateChanged();
}
