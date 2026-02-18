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

QVariantMap makeCollectionRow(const QString &path, const QString &title)
{
    QVariantMap row;
    row.insert(QStringLiteral("filePath"), path);
    row.insert(QStringLiteral("title"), title);
    row.insert(QStringLiteral("durationMs"), 180000);
    return row;
}

void seedPlaylist(TrackModel *trackModel, int currentIndex = 0)
{
    QVERIFY(trackModel != nullptr);
    trackModel->m_tracks = {
        makeTrack(QStringLiteral("/tmp/waveflux-transition-a.flac"), QStringLiteral("A")),
        makeTrack(QStringLiteral("/tmp/waveflux-transition-b.flac"), QStringLiteral("B")),
        makeTrack(QStringLiteral("/tmp/waveflux-transition-c.flac"), QStringLiteral("C"))
    };
    trackModel->m_currentIndex = currentIndex;
}
} // namespace

class tst_PlaybackControllerTransitions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        gst_init(nullptr, nullptr);
    }

    void staleAboutToFinishIgnored()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        controller.m_activeTrackTransitionId = 10;
        audioEngine.m_lastAboutToFinishTransitionId = 9;

        controller.onAudioAboutToFinish();

        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
    }

    void duplicateAboutToFinishIgnored()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        controller.m_activeTrackTransitionId = 10;
        audioEngine.m_lastAboutToFinishTransitionId = 10;

        controller.onAudioAboutToFinish();

        QVERIFY(controller.m_gaplessTransitionPending);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPreparingGapless);
        const quint64 firstQueuedTransitionId = controller.m_gaplessQueuedTransitionId;
        QVERIFY(firstQueuedTransitionId > 10);
        QCOMPARE(audioEngine.m_nextFile, trackModel.getFilePath(1));
        QCOMPARE(audioEngine.m_nextFileTransitionId, firstQueuedTransitionId);

        // Same source transition id should be treated as duplicate.
        controller.onAudioAboutToFinish();
        QCOMPARE(controller.m_gaplessQueuedTransitionId, firstQueuedTransitionId);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
    }

    void staleEosIgnored()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        controller.m_activeTrackTransitionId = 20;
        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 21;
        controller.m_gaplessTransitionPending = true;

        controller.handleTrackEndedInternal(19, true);

        QCOMPARE(trackModel.m_currentIndex, 0);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 21ULL);
        QCOMPARE(controller.m_lastHandledEosTransitionId, 0ULL);
        QCOMPARE(controller.m_forcedTrackLoadTransitionId, 0ULL);
    }

    void duplicateEosIgnored()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        controller.m_activeTrackTransitionId = 20;
        controller.m_lastHandledEosTransitionId = 20;
        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 21;
        controller.m_gaplessTransitionPending = true;

        controller.handleTrackEndedInternal(20, true);

        QCOMPARE(trackModel.m_currentIndex, 0);
        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 21ULL);
        QCOMPARE(controller.m_forcedTrackLoadTransitionId, 0ULL);
    }

    void staleCurrentFileChangeIgnoredAndMatchingConfirmed()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        controller.m_activeTrackTransitionId = 50;
        controller.m_activeTrackIndex = 0;
        controller.m_pendingTrackIndex = 1;
        controller.m_transitionState = PlaybackController::TransitionPreparingGapless;
        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 50;
        controller.m_gaplessTransitionPending = true;

        audioEngine.m_currentTransitionId = 49;
        controller.onCurrentFileChanged(trackModel.getFilePath(1));
        QCOMPARE(trackModel.m_currentIndex, 0);
        QCOMPARE(controller.activeTrackIndex(), 0);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPreparingGapless);
        QCOMPARE(controller.m_gaplessTransitionPending, true);

        audioEngine.m_currentTransitionId = 50;
        controller.onCurrentFileChanged(trackModel.getFilePath(1));
        QCOMPARE(trackModel.m_currentIndex, 1);
        QCOMPARE(controller.activeTrackIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionCommitted);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
    }

    void gaplessFallbackPreservesQueuedTransitionId()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        // Isolate fallback behavior from trackSelected side-effects.
        QObject::disconnect(&trackModel, nullptr, &controller, nullptr);

        controller.m_activeTrackTransitionId = 42;
        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 43;
        controller.m_gaplessTransitionPending = true;

        controller.handleTrackEndedInternal(42, true);

        QCOMPARE(trackModel.m_currentIndex, 1);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionFallbackEos);
        QCOMPARE(controller.m_forcedTrackLoadPath, trackModel.getFilePath(1));
        QCOMPARE(controller.m_forcedTrackLoadTransitionId, 43ULL);
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_lastHandledEosTransitionId, 42ULL);
    }

    void nextTrackUsesPlayingAnchorWhenSelectionDiffers()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 2);

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        controller.m_activeTrackIndex = 0;

        QVERIFY(controller.canGoNext());
        QCOMPARE(controller.calculateNextIndex(false), 1);
    }

    void activeTrackFollowsMoveByCurrentFile()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 2);

        const QString activePath = trackModel.getFilePath(1);
        audioEngine.m_currentFile = activePath;
        controller.onCurrentFileChanged(activePath);
        QCOMPARE(controller.activeTrackIndex(), 1);

        trackModel.move(1, 2);
        QCOMPARE(controller.activeTrackIndex(), 2);
    }

    void activeTrackClearsWhenCurrentFileRemovedFromPlaylist()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        const QString activePath = trackModel.getFilePath(1);
        audioEngine.m_currentFile = activePath;
        controller.onCurrentFileChanged(activePath);
        QCOMPARE(controller.activeTrackIndex(), 1);

        trackModel.removeAt(1);
        QCOMPARE(controller.activeTrackIndex(), -1);
    }

    void activeTrackFollowsSortAndCollectionResets()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 2);

        const QString pathA = trackModel.getFilePath(0);
        const QString pathB = trackModel.getFilePath(1);
        const QString pathC = trackModel.getFilePath(2);
        const QString activePath = pathA;
        audioEngine.m_currentFile = activePath;
        controller.onCurrentFileChanged(activePath);
        QCOMPARE(controller.activeTrackIndex(), 0);

        trackModel.sortByNameDesc();
        QCOMPARE(controller.activeTrackIndex(), 2);

        QVariantList collectionRows;
        collectionRows.push_back(makeCollectionRow(pathC, QStringLiteral("C")));
        collectionRows.push_back(makeCollectionRow(pathA, QStringLiteral("A")));
        trackModel.applySmartCollectionRows(collectionRows);
        QCOMPARE(controller.activeTrackIndex(), 1);

        QVariantList collectionRowsWithoutActive;
        collectionRowsWithoutActive.push_back(makeCollectionRow(pathC, QStringLiteral("C")));
        collectionRowsWithoutActive.push_back(makeCollectionRow(pathB, QStringLiteral("B")));
        trackModel.applySmartCollectionRows(collectionRowsWithoutActive);
        QCOMPARE(controller.activeTrackIndex(), -1);
    }

    void queueOperationsUsePlayingTrackInsteadOfSelection()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        const QString activePath = trackModel.getFilePath(1);
        audioEngine.m_currentFile = activePath;
        controller.onCurrentFileChanged(activePath);
        QCOMPARE(controller.activeTrackIndex(), 1);

        controller.addToQueue(0);
        QCOMPARE(controller.queueCount(), 1);

        controller.addToQueue(1);
        QCOMPARE(controller.queueCount(), 1);

        controller.playNextInQueue(1);
        QCOMPARE(controller.queueCount(), 1);

        const QVariantList queueItems = controller.queueItems();
        QCOMPARE(queueItems.size(), 1);
        QCOMPARE(queueItems.first().toMap().value(QStringLiteral("filePath")).toString(),
                 trackModel.getFilePath(0));
    }
};

QTEST_GUILESS_MAIN(tst_PlaybackControllerTransitions)
#include "tst_PlaybackControllerTransitions.moc"
