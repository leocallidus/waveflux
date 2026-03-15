#include <QtTest>

#include <gst/gst.h>

#define private public
#define protected public
#include "AudioEngine.h"
#include "PlaybackController.h"
#include "TrackModel.h"
#undef protected
#undef private

namespace {
Track makeTrack(const QString &path, const QString &title)
{
    Track track;
    track.filePath = path;
    track.title = title;
    track.duration = 180000;
    return track;
}

Track makeCueTrack(const QString &path,
                   const QString &title,
                   qint64 cueStartMs,
                   qint64 cueEndMs)
{
    Track track = makeTrack(path, title);
    track.cueSegment = true;
    track.cueStartMs = cueStartMs;
    track.cueEndMs = cueEndMs;
    return track;
}

class FakeTrackModel final : public TrackModel
{
public:
    using TrackModel::TrackModel;

    void seed(const QVector<Track> &tracks, int selectedIndex)
    {
        m_tracks = tracks;
        m_currentIndex = selectedIndex;
    }
};

class FakeAudioEngine final : public AudioEngine
{
public:
    using AudioEngine::AudioEngine;

    void setCurrentFileForTest(const QString &filePath, quint64 transitionId)
    {
        m_currentFile = filePath;
        m_currentTransitionId = transitionId;
    }

    void emitCurrentFileForTest(const QString &filePath, quint64 transitionId)
    {
        setCurrentFileForTest(filePath, transitionId);
        emit currentFileChanged(filePath);
    }

    void emitAboutToFinishForTest(quint64 sourceTransitionId)
    {
        m_lastAboutToFinishTransitionId = sourceTransitionId;
        emit aboutToFinish();
    }

    void emitEndOfStreamForTest(quint64 eosTransitionId)
    {
        m_lastEndOfStreamTransitionId = eosTransitionId;
        emit endOfStream();
    }
};
} // namespace

class tst_PlaybackControllerScenarios : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("WAVEFLUX_SKIP_SOURCE_VALIDATION", "1");
        gst_init(nullptr, nullptr);
    }

    void runtimeDetailedDiagnosticsFlagCanBeToggled()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        const bool initial = controller.detailedDiagnosticsEnabled();
        QSignalSpy spy(&controller, &PlaybackController::detailedDiagnosticsEnabledChanged);

        controller.setDetailedDiagnosticsEnabled(!initial);
        QCOMPARE(controller.detailedDiagnosticsEnabled(), !initial);
        QCOMPARE(spy.count(), 1);

        controller.setDetailedDiagnosticsEnabled(initial);
        QCOMPARE(controller.detailedDiagnosticsEnabled(), initial);
        QCOMPARE(spy.count(), 2);
    }

    void aboutToFinishThenStreamStartCommitsGapless()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-gapless-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-gapless-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-gapless-c.flac"), QStringLiteral("C"))
                        },
                        0);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(0), 100);
        controller.onCurrentFileChanged(trackModel.getFilePath(0));
        QCOMPARE(controller.activeTrackIndex(), 0);

        audioEngine.emitAboutToFinishForTest(100);
        QVERIFY(controller.m_gaplessTransitionPending);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPreparingGapless);

        const quint64 queuedTransitionId = controller.m_gaplessQueuedTransitionId;
        QVERIFY(queuedTransitionId > 100);

        audioEngine.emitCurrentFileForTest(trackModel.getFilePath(1), queuedTransitionId);

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.activeTrackIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionCommitted);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
    }

    void aboutToFinishThenEosFallbackLoadsPreparedTrack()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-fallback-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-fallback-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-fallback-c.flac"), QStringLiteral("C"))
                        },
                        0);

        QObject::disconnect(&trackModel, &TrackModel::trackSelected, &controller, nullptr);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(0), 200);
        controller.onCurrentFileChanged(trackModel.getFilePath(0));

        audioEngine.emitAboutToFinishForTest(200);
        QVERIFY(controller.m_gaplessTransitionPending);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        const quint64 queuedTransitionId = controller.m_gaplessQueuedTransitionId;
        QVERIFY(queuedTransitionId > 200);

        audioEngine.emitEndOfStreamForTest(200);

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionFallbackEos);
        QCOMPARE(controller.m_forcedTrackLoadPath, trackModel.getFilePath(1));
        QCOMPARE(controller.m_forcedTrackLoadTransitionId, queuedTransitionId);
    }

    void staleAndDuplicateEosIgnored()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-eos-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-eos-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-eos-c.flac"), QStringLiteral("C"))
                        },
                        0);

        controller.m_activeTrackTransitionId = 300;
        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 301;
        controller.m_gaplessTransitionPending = true;

        controller.handleTrackEndedInternal(299, true);
        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        QCOMPARE(controller.m_lastHandledEosTransitionId, 0ULL);

        controller.m_lastHandledEosTransitionId = 300;
        controller.handleTrackEndedInternal(300, true);
        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 301ULL);
    }

    void repeatOneReloadsTrackWithFreshTransitionEachCycle()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-repeat-one.flac"),
                                      QStringLiteral("Repeat One"))
                        },
                        0);

        controller.setRepeatMode(PlaybackController::RepeatOne);

        const QString trackPath = trackModel.getFilePath(0);
        audioEngine.setCurrentFileForTest(trackPath, 1000);
        controller.onCurrentFileChanged(trackPath);

        controller.handleTrackEndedInternal(1000, true);
        const quint64 firstRepeatTransitionId = audioEngine.currentTransitionId();
        QVERIFY(firstRepeatTransitionId > 1000);
        QCOMPARE(audioEngine.currentFile(), trackPath);

        controller.handleTrackEndedInternal(firstRepeatTransitionId, true);
        const quint64 secondRepeatTransitionId = audioEngine.currentTransitionId();
        QVERIFY(secondRepeatTransitionId > firstRepeatTransitionId);
        QCOMPARE(audioEngine.currentFile(), trackPath);
    }

    void nextDuringPendingTransitionUsesPlayingAnchor()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-pending-next-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-pending-next-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-pending-next-c.flac"), QStringLiteral("C"))
                        },
                        2);

        QObject::disconnect(&trackModel, &TrackModel::trackSelected, &controller, nullptr);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(0), 400);
        controller.onCurrentFileChanged(trackModel.getFilePath(0));

        controller.m_pendingTrackIndex = 2;
        controller.m_transitionState = PlaybackController::TransitionPendingCommit;

        controller.nextTrack();

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPendingCommit);
    }

    void previousDuringPendingTransitionUsesPlayingAnchor()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-pending-prev-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-pending-prev-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-pending-prev-c.flac"), QStringLiteral("C"))
                        },
                        2);

        QObject::disconnect(&trackModel, &TrackModel::trackSelected, &controller, nullptr);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(1), 500);
        controller.onCurrentFileChanged(trackModel.getFilePath(1));

        controller.m_pendingTrackIndex = 2;
        controller.m_transitionState = PlaybackController::TransitionPendingCommit;

        controller.previousTrack();

        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(controller.pendingTrackIndex(), 0);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPendingCommit);
    }

    void queueShuffleRepeatAtPlaylistEnd()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-queue-shuffle-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-queue-shuffle-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-queue-shuffle-c.flac"), QStringLiteral("C"))
                        },
                        2);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(2), 600);
        controller.onCurrentFileChanged(trackModel.getFilePath(2));

        controller.setShuffleEnabled(true);
        controller.setRepeatMode(PlaybackController::RepeatAll);
        controller.m_shuffleOrder = {0, 1, 2};
        controller.m_shufflePosition = 2;

        controller.addToQueue(1);
        QCOMPARE(controller.queueCount(), 1);

        const int fromQueue = controller.calculateNextIndex(true);
        QCOMPARE(fromQueue, 1);
        QCOMPARE(controller.queueCount(), 0);

        const int repeatShuffleNext = controller.calculateNextIndex(true);
        QVERIFY(repeatShuffleNext >= 0);
        QVERIFY(repeatShuffleNext < trackModel.rowCount());
    }

    void moveAndRemoveCurrentTrackWhilePlaying()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-move-remove-a.flac"), QStringLiteral("A")),
                            makeTrack(QStringLiteral("/tmp/waveflux-move-remove-b.flac"), QStringLiteral("B")),
                            makeTrack(QStringLiteral("/tmp/waveflux-move-remove-c.flac"), QStringLiteral("C"))
                        },
                        0);

        const QString activePath = trackModel.getFilePath(1);
        audioEngine.setCurrentFileForTest(activePath, 700);
        controller.onCurrentFileChanged(activePath);
        QCOMPARE(controller.activeTrackIndex(), 1);

        trackModel.move(1, 2);
        QCOMPARE(controller.activeTrackIndex(), 2);

        trackModel.removeAt(2);
        QCOMPARE(controller.activeTrackIndex(), -1);
    }

    void cueBoundaryForwardTransitionsToNextTrack()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-cue-forward-anchor.flac"), QStringLiteral("Anchor")),
                            makeCueTrack(QStringLiteral("/tmp/waveflux-cue-forward.flac"),
                                         QStringLiteral("Cue"),
                                         0,
                                         5000),
                            makeTrack(QStringLiteral("/tmp/waveflux-cue-forward-next.flac"), QStringLiteral("Next"))
                        },
                        1);

        QObject::disconnect(&trackModel, &TrackModel::trackSelected, &controller, nullptr);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(1), 800);
        audioEngine.m_reversePlayback = false;
        controller.onCurrentFileChanged(trackModel.getFilePath(1));

        controller.onAudioPositionChanged(4950);

        QCOMPARE(trackModel.currentIndex(), 2);
        QCOMPARE(controller.pendingTrackIndex(), 2);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPendingCommit);
    }

    void cueBoundaryReverseTransitionsToPreviousTrack()
    {
        FakeAudioEngine audioEngine;
        FakeTrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.seed({
                            makeTrack(QStringLiteral("/tmp/waveflux-cue-reverse-prev.flac"), QStringLiteral("Prev")),
                            makeCueTrack(QStringLiteral("/tmp/waveflux-cue-reverse.flac"),
                                         QStringLiteral("Cue"),
                                         0,
                                         5000),
                            makeTrack(QStringLiteral("/tmp/waveflux-cue-reverse-next.flac"), QStringLiteral("Next"))
                        },
                        1);

        QObject::disconnect(&trackModel, &TrackModel::trackSelected, &controller, nullptr);

        audioEngine.setCurrentFileForTest(trackModel.getFilePath(1), 900);
        audioEngine.m_reversePlayback = true;
        controller.onCurrentFileChanged(trackModel.getFilePath(1));

        controller.onAudioPositionChanged(50);

        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(controller.pendingTrackIndex(), 0);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPendingCommit);
    }
};

QTEST_GUILESS_MAIN(tst_PlaybackControllerScenarios)
#include "tst_PlaybackControllerScenarios.moc"
