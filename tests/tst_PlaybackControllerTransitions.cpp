#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <gst/gst.h>

#define private public
#define protected public
#include "AudioEngine.h"
#include "PlaybackController.h"
#include "SessionManager.h"
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

void appendFixedString(QByteArray *buffer, QByteArray value, int width)
{
    value.truncate(width);
    buffer->append(value);
    if (value.size() < width) {
        buffer->append(QByteArray(width - value.size(), '\0'));
    }
}

void appendBigEndianWord(QByteArray *buffer, quint16 value)
{
    buffer->append(static_cast<char>((value >> 8) & 0xff));
    buffer->append(static_cast<char>(value & 0xff));
}

QByteArray createMinimalModFile()
{
    QByteArray data;
    appendFixedString(&data, "WaveFlux Seek Demo", 20);

    for (int sampleIndex = 0; sampleIndex < 31; ++sampleIndex) {
        appendFixedString(&data, sampleIndex == 0 ? "Pulse" : QByteArray(), 22);
        appendBigEndianWord(&data, sampleIndex == 0 ? 2 : 0);
        data.append('\0');
        data.append(static_cast<char>(sampleIndex == 0 ? 64 : 0));
        appendBigEndianWord(&data, 0);
        appendBigEndianWord(&data, sampleIndex == 0 ? 1 : 0);
    }

    data.append('\1');
    data.append(static_cast<char>(127));
    data.append(QByteArray(128, '\0'));
    data.append("M.K.", 4);

    QByteArray pattern(64 * 4 * 4, '\0');
    pattern[0] = static_cast<char>(0x01);
    pattern[1] = static_cast<char>(0xac);
    pattern[2] = static_cast<char>(0x10);
    pattern[3] = static_cast<char>(0x00);
    data.append(pattern);
    data.append(QByteArray::fromHex("0040c000"));
    return data;
}

QString writeModuleFixture(QTemporaryDir *dir, const QString &fileName = QStringLiteral("seek-demo.mod"))
{
    const QString filePath = dir->filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }

    file.write(createMinimalModFile());
    file.close();
    return filePath;
}
} // namespace

class tst_PlaybackControllerTransitions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("WAVEFLUX_SKIP_SOURCE_VALIDATION", "1");
        qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
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
        QCOMPARE(audioEngine.m_nextFile, QString());
        QCOMPARE(audioEngine.m_nextFileTransitionId, 0ULL);

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

    void clearingPlaylistUnloadsAudioEngineAndResetsPlaybackState()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 0);

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_title = QStringLiteral("A");
        audioEngine.m_artist = QStringLiteral("Artist");
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_metadataFallbackDurationMs = 180000;
        audioEngine.m_lastStableDurationMs = 180000;
        audioEngine.m_state = AudioEngine::PlayingState;
        controller.m_activeTrackIndex = 0;
        controller.m_pendingTrackIndex = 1;
        controller.m_transitionState = PlaybackController::TransitionPendingCommit;

        trackModel.clear();

        QCOMPARE(trackModel.currentIndex(), -1);
        QCOMPARE(controller.activeTrackIndex(), -1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(), PlaybackController::TransitionIdle);
        QCOMPARE(audioEngine.currentFile(), QString());
        QCOMPARE(audioEngine.title(), QString());
        QCOMPARE(audioEngine.artist(), QString());
        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
        QCOMPARE(audioEngine.duration(), 0);
        QCOMPARE(audioEngine.state(), AudioEngine::StoppedState);
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

    void openMptEosIgnoresStaleGaplessStateAndAdvancesNormally()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-tracker-a.mod"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-tracker-b.mod"), QStringLiteral("B")),
            makeTrack(QStringLiteral("/tmp/waveflux-tracker-c.mod"), QStringLiteral("C"))
        };
        trackModel.m_currentIndex = 0;

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_currentTransitionId = 10;
        controller.m_activeTrackIndex = 0;
        controller.m_activeTrackTransitionId = 10;

        controller.m_gaplessQueuedIndex = 2;
        controller.m_gaplessQueuedTransitionId = 99;
        controller.m_gaplessTransitionPending = true;
        controller.m_transitionState = PlaybackController::TransitionPreparingGapless;

        controller.handleTrackEndedInternal(10, true);

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QCOMPARE(audioEngine.currentFile(), trackModel.getFilePath(1));
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
        QCOMPARE(controller.m_forcedTrackLoadTransitionId, 0ULL);
        QCOMPARE(controller.activeTrackIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionCommitted);
    }

    void openMptAboutToFinishPreparesTrackerNearGapless()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-openmpt-gapless-a.mod"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-openmpt-gapless-b.mod"), QStringLiteral("B"))
        };
        trackModel.m_currentIndex = 0;

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_lastAboutToFinishTransitionId = 10;
        controller.m_activeTrackTransitionId = 10;

        controller.onAudioAboutToFinish();

        QCOMPARE(controller.m_gaplessQueuedIndex, 1);
        QVERIFY(controller.m_gaplessQueuedTransitionId > 10);
        QCOMPARE(controller.m_gaplessTransitionPending, true);
        QCOMPARE(audioEngine.m_nextFile, trackModel.getFilePath(1));
        QCOMPARE(audioEngine.m_nextFileTransitionId, controller.m_gaplessQueuedTransitionId);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPreparingGapless);
    }

    void openMptRepeatAllWrapsTrackerPlaylistWithoutGaplessState()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-gapless-repeat-a.mod"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-gapless-repeat-b.mod"), QStringLiteral("B")),
            makeTrack(QStringLiteral("/tmp/waveflux-gapless-repeat-c.mod"), QStringLiteral("C"))
        };
        trackModel.m_currentIndex = 2;

        controller.setRepeatMode(PlaybackController::RepeatAll);
        audioEngine.m_currentFile = trackModel.getFilePath(2);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_currentTransitionId = 70;
        controller.m_activeTrackIndex = 2;
        controller.m_activeTrackTransitionId = 70;

        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 91;
        controller.m_gaplessTransitionPending = true;
        controller.m_transitionState = PlaybackController::TransitionPreparingGapless;

        controller.handleTrackEndedInternal(70, true);

        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QCOMPARE(audioEngine.currentFile(), trackModel.getFilePath(0));
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
        QCOMPARE(controller.activeTrackIndex(), 0);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionCommitted);
    }

    void openMptQueueTakesPriorityOverRepeatAllAtTrackerEos()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-gapless-queue-a.mod"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-gapless-queue-b.mod"), QStringLiteral("B")),
            makeTrack(QStringLiteral("/tmp/waveflux-gapless-queue-c.mod"), QStringLiteral("C"))
        };
        trackModel.m_currentIndex = 0;

        controller.setRepeatMode(PlaybackController::RepeatAll);
        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_currentTransitionId = 80;
        controller.m_activeTrackIndex = 0;
        controller.m_activeTrackTransitionId = 80;

        controller.addToQueue(2);
        QCOMPARE(controller.queueCount(), 1);

        controller.handleTrackEndedInternal(80, true);

        QCOMPARE(trackModel.currentIndex(), 2);
        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QCOMPARE(audioEngine.currentFile(), trackModel.getFilePath(2));
        QCOMPARE(controller.queueCount(), 0);
        QCOMPARE(controller.activeTrackIndex(), 2);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionCommitted);
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
    }

    void mixedBackendCurrentFileChangeDoesNotInheritGaplessState()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-a.flac"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-b.mod"), QStringLiteral("B")),
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-c.flac"), QStringLiteral("C"))
        };
        trackModel.m_currentIndex = 0;

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::GStreamer;
        audioEngine.m_currentTransitionId = 100;
        controller.m_activeTrackIndex = 0;
        controller.m_activeTrackTransitionId = 100;

        controller.m_gaplessQueuedIndex = 2;
        controller.m_gaplessQueuedTransitionId = 101;
        controller.m_gaplessTransitionPending = true;
        controller.m_transitionState = PlaybackController::TransitionPreparingGapless;
        controller.m_pendingTrackIndex = 2;

        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_currentTransitionId = 102;
        controller.onCurrentFileChanged(trackModel.getFilePath(1));

        QCOMPARE(controller.activeTrackIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(), PlaybackController::TransitionCommitted);
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
    }

    void openMptEosToRegularTrackClearsGaplessStateAndSelectsNextRow()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-openmpt-a.mod"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-openmpt-b.flac"), QStringLiteral("B"))
        };
        trackModel.m_currentIndex = 0;

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_currentTransitionId = 110;
        controller.m_activeTrackIndex = 0;
        controller.m_activeTrackTransitionId = 110;

        controller.m_gaplessQueuedIndex = 1;
        controller.m_gaplessQueuedTransitionId = 111;
        controller.m_gaplessTransitionPending = true;
        controller.m_transitionState = PlaybackController::TransitionPreparingGapless;

        controller.handleTrackEndedInternal(110, true);

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.activeTrackIndex(), 0);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPendingCommit);
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
    }

    void regularEosToOpenMptTrackClearsStaleGaplessStateOnCommit()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);

        trackModel.m_tracks = {
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-regular-a.flac"), QStringLiteral("A")),
            makeTrack(QStringLiteral("/tmp/waveflux-mixed-regular-b.mod"), QStringLiteral("B"))
        };
        trackModel.m_currentIndex = 0;

        audioEngine.m_currentFile = trackModel.getFilePath(0);
        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::GStreamer;
        audioEngine.m_currentTransitionId = 120;
        controller.m_activeTrackIndex = 0;
        controller.m_activeTrackTransitionId = 120;

        controller.handleTrackEndedInternal(120, true);

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.activeTrackIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionCommitted);

        controller.m_gaplessQueuedIndex = 0;
        controller.m_gaplessQueuedTransitionId = 121;
        controller.m_gaplessTransitionPending = true;
        controller.m_transitionState = PlaybackController::TransitionPreparingGapless;

        audioEngine.m_currentBackendKind = WaveFlux::PlaybackBackendKind::OpenMpt;
        audioEngine.m_currentTransitionId = 122;
        controller.onCurrentFileChanged(trackModel.getFilePath(1));

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.activeTrackIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), -1);
        QCOMPARE(controller.transitionState(), PlaybackController::TransitionCommitted);
        QCOMPARE(controller.m_gaplessQueuedIndex, -1);
        QCOMPARE(controller.m_gaplessQueuedTransitionId, 0ULL);
        QCOMPARE(controller.m_gaplessTransitionPending, false);
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

    void nextTrackLoadsTargetWhenSelectionAlreadyPointsAtNextRow()
    {
        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        seedPlaylist(&trackModel, 1);

        const QString playingPath = trackModel.getFilePath(0);
        const QString nextPath = trackModel.getFilePath(1);
        audioEngine.m_currentFile = playingPath;
        controller.onCurrentFileChanged(playingPath);

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.activeTrackIndex(), 0);

        controller.nextTrack();

        QCOMPARE(trackModel.currentIndex(), 1);
        QCOMPARE(controller.pendingTrackIndex(), 1);
        QCOMPARE(controller.transitionState(),
                 PlaybackController::TransitionPendingCommit);
        QCOMPARE(audioEngine.currentFile(), nextPath);
        QVERIFY(audioEngine.currentTransitionId() > 0);
    }

    void trackerSeekRelativeAndPreviousRestartUseOpenMptBackend()
    {
        if (QMediaDevices::defaultAudioOutput().isNull()) {
            QSKIP("Default audio output device is not available in this environment.");
        }

        qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
        struct RestorePipelineLoadEnv {
            ~RestorePipelineLoadEnv()
            {
                qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
            }
        } restorePipelineLoadEnv;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString modulePath = writeModuleFixture(&dir);
        QVERIFY(!modulePath.isEmpty());

        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        controller.setRestartThresholdMs(1000);

        trackModel.m_tracks = {makeTrack(modulePath, QStringLiteral("Tracker"))};
        trackModel.m_currentIndex = -1;
        trackModel.setCurrentIndex(0);
        controller.m_activeTrackIndex = 0;

        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.state() == AudioEngine::PlayingState, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.position() > 0, 3000);

        audioEngine.seekWithSource(1000, QStringLiteral("test.manual_progress_seek"));
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.position() >= 900, 3000);

        controller.seekRelative(500);
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.position() >= 1300, 3000);

        audioEngine.seekWithSource(2500, QStringLiteral("test.restart_anchor_seek"));
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.position() >= 2300, 3000);

        controller.previousTrack();
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.position() <= 800, 3000);
        QCOMPARE(audioEngine.state(), AudioEngine::PlayingState);

        qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
    }

    void trackerSessionRestoreLoadsOpenMptAndRestoresPosition()
    {
        if (QMediaDevices::defaultAudioOutput().isNull()) {
            QSKIP("Default audio output device is not available in this environment.");
        }

        QStandardPaths::setTestModeEnabled(true);
        qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
        struct RestorePipelineLoadEnv {
            ~RestorePipelineLoadEnv()
            {
                qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
            }
        } restorePipelineLoadEnv;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString modulePath = writeModuleFixture(&dir, QStringLiteral("restore-demo.mod"));
        QVERIFY(!modulePath.isEmpty());

        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        SessionManager sessionManager;
        sessionManager.initialize(&trackModel, &audioEngine, &controller);

        QFile::remove(sessionManager.sessionFilePath());
        QFile sessionFile(sessionManager.sessionFilePath());
        QVERIFY(QDir().mkpath(QFileInfo(sessionFile).absolutePath()));
        QVERIFY(sessionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));

        QJsonObject track;
        track.insert(QStringLiteral("path"), modulePath);
        track.insert(QStringLiteral("title"), QStringLiteral("Restore Tracker"));
        track.insert(QStringLiteral("duration"), 5000.0);

        QJsonObject root;
        root.insert(QStringLiteral("version"), 1);
        root.insert(QStringLiteral("playlist"), QJsonArray({track}));
        root.insert(QStringLiteral("currentIndex"), 0);
        root.insert(QStringLiteral("positionMs"), 1000.0);
        root.insert(QStringLiteral("wasPlaying"), true);
        root.insert(QStringLiteral("volume"), 1.0);
        root.insert(QStringLiteral("playbackRate"), 1.0);
        root.insert(QStringLiteral("pitchSemitones"), 0);
        root.insert(QStringLiteral("repeatMode"), 0);
        root.insert(QStringLiteral("shuffleEnabled"), false);
        sessionFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        sessionFile.close();

        sessionManager.restoreSession();

        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QCOMPARE(audioEngine.currentFile(), modulePath);
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.state() == AudioEngine::PlayingState, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.position() >= 900, 4000);
        QCOMPARE(audioEngine.m_lastSeekSource, QStringLiteral("session.restore_playback_position"));
        QVERIFY(audioEngine.m_lastSeekTargetMs >= 900);

        QFile::remove(sessionManager.sessionFilePath());
        qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
    }

    void trackerSessionRestoreNearEofResetsToStart()
    {
        if (QMediaDevices::defaultAudioOutput().isNull()) {
            QSKIP("Default audio output device is not available in this environment.");
        }

        QStandardPaths::setTestModeEnabled(true);
        qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
        struct RestorePipelineLoadEnv {
            ~RestorePipelineLoadEnv()
            {
                qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
            }
        } restorePipelineLoadEnv;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString modulePath = writeModuleFixture(&dir, QStringLiteral("restore-near-eof.mod"));
        QVERIFY(!modulePath.isEmpty());

        AudioEngine audioEngine;
        TrackModel trackModel;
        PlaybackController controller(&trackModel, &audioEngine);
        SessionManager sessionManager;
        sessionManager.initialize(&trackModel, &audioEngine, &controller);

        QFile::remove(sessionManager.sessionFilePath());
        QFile sessionFile(sessionManager.sessionFilePath());
        QVERIFY(QDir().mkpath(QFileInfo(sessionFile).absolutePath()));
        QVERIFY(sessionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));

        QJsonObject track;
        track.insert(QStringLiteral("path"), modulePath);
        track.insert(QStringLiteral("title"), QStringLiteral("Restore Tracker Near EOF"));
        track.insert(QStringLiteral("duration"), 7680.0);

        QJsonObject root;
        root.insert(QStringLiteral("version"), 1);
        root.insert(QStringLiteral("playlist"), QJsonArray({track}));
        root.insert(QStringLiteral("currentIndex"), 0);
        root.insert(QStringLiteral("positionMs"), 7600.0);
        root.insert(QStringLiteral("wasPlaying"), true);
        root.insert(QStringLiteral("volume"), 1.0);
        root.insert(QStringLiteral("playbackRate"), 1.0);
        root.insert(QStringLiteral("pitchSemitones"), 0);
        root.insert(QStringLiteral("repeatMode"), 0);
        root.insert(QStringLiteral("shuffleEnabled"), false);
        sessionFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        sessionFile.close();

        sessionManager.restoreSession();

        QCOMPARE(trackModel.currentIndex(), 0);
        QCOMPARE(audioEngine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QCOMPARE(audioEngine.currentFile(), modulePath);
        QTRY_VERIFY_WITH_TIMEOUT(audioEngine.state() == AudioEngine::PlayingState, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(audioEngine.m_lastSeekSource,
                                  QStringLiteral("session.restore_playback_position"),
                                  4000);
        QCOMPARE(audioEngine.m_lastSeekTargetMs, 0);
        QVERIFY(audioEngine.position() < 2500);

        QFile::remove(sessionManager.sessionFilePath());
        qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");
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
