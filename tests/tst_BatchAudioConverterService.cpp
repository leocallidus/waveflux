#include <QtTest>

#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <gst/gst.h>
#include <taglib/tag.h>

#include "BatchAudioConverterService.h"
#include "TagLibPath.h"

namespace {
void writeSilentWavFile(const QString &path,
                        int sampleRate = 44100,
                        int channels = 1,
                        int durationMs = 250)
{
    const int bytesPerSample = 2;
    const int sampleCount = qMax(1, (sampleRate * durationMs) / 1000);
    const int dataBytes = sampleCount * channels * bytesPerSample;

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataBytes);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << quint16(channels);
    stream << quint32(sampleRate);
    stream << quint32(sampleRate * channels * bytesPerSample);
    stream << quint16(channels * bytesPerSample);
    stream << quint16(16);
    stream.writeRawData("data", 4);
    stream << quint32(dataBytes);

    QByteArray samples(dataBytes, '\0');
    QVERIFY(stream.writeRawData(samples.constData(), samples.size()) == samples.size());
    file.close();
}

bool hasFactory(const char *name)
{
    GstElementFactory *factory = gst_element_factory_find(name);
    if (!factory) {
        return false;
    }
    gst_object_unref(factory);
    return true;
}

bool writeBasicTags(const QString &path,
                    const QString &title,
                    const QString &artist,
                    const QString &album,
                    uint trackNumber = 0)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, false);
    if (file.isNull() || !file.tag()) {
        return false;
    }

    const QByteArray titleUtf8 = title.toUtf8();
    const QByteArray artistUtf8 = artist.toUtf8();
    const QByteArray albumUtf8 = album.toUtf8();
    file.tag()->setTitle(TagLib::String(titleUtf8.constData(), TagLib::String::UTF8));
    file.tag()->setArtist(TagLib::String(artistUtf8.constData(), TagLib::String::UTF8));
    file.tag()->setAlbum(TagLib::String(albumUtf8.constData(), TagLib::String::UTF8));
    file.tag()->setTrack(trackNumber);
    return file.save();
}
} // namespace

class BatchAudioConverterServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exposesStableDefaults();
    void exposesSequentialParallelismDecision();
    void normalizesSlashPrefixedWindowsOutputDirectory();
    void materializesPendingItemsFromSourceFiles();
    void exposesJobMetadataAndStableItemIds();
    void supportsItemLookupAndMutationById();
    void exportsAndAppliesReusableSettings();
    void replacesSourcesFromVariantListWithValidationAndOrigins();
    void appendsSupportedFilesFromFolderRecursively();
    void mixesPlaylistPickerAndFolderIntakeWithoutLosingOrigins();
    void reordersAndRemovesItemsBeforeStart();
    void retriesFailedItemsWithoutDuplicatingQueueEntries();
    void retriesSkippedConflictItemsAfterOutputDirectoryChange();
    void retriesPermissionFailureAfterOutputDirectoryRecovery();
    void recordsAttemptHistoryWithSettingsChanges();
    void exportsAndRestoresDraftStateSafely();
    void exportsAndRestoresLargeDraftQueue();
    void exportsFinishedJobHistoryReports();
    void removesPendingTailItemWhileRunning();
    void updatesRuntimeFieldsAndCounters();
    void previewsOutputFilesBeforeStart();
    void supportsArtistTitleNamingPolicyWithFallback();
    void supportsAlbumTrackTitleNamingPolicyWithFallback();
    void appliesExplicitConflictPoliciesInPreview();
    void runsSequentialBatchConversion();
    void v1SimpleBatchFlowStillWorksUnchanged();
    void emitsPlaylistResultsForSucceededItemsInOrder();
    void defersPlaylistResultsUntilExplicitAction();
    void suppressesPlaylistResultsWhenDisabled();
    void rejectsBatchStartOnFatalValidationFailure();
    void preservesSummaryAfterPartialFailure();
    void aggregatesBatchProgressAcrossMixedItemStates();
    void continuesAfterPerItemFailure();
    void cancelsDuringSecondItemAndKeepsFirstResult();
    void cancelStopsCurrentAndRemainingItems();
    void cancelsLongQueueAndMarksPendingTail();
};

void BatchAudioConverterServiceTest::initTestCase()
{
    int argc = 0;
    char **argv = nullptr;
    gst_init(&argc, &argv);
}

void BatchAudioConverterServiceTest::exposesStableDefaults()
{
    BatchAudioConverterService service;

    QCOMPARE(service.items().size(), 0);
    QCOMPARE(service.currentItem(), QVariantMap());
    QCOMPARE(service.outputDirectory(), QString());
    QCOMPARE(service.namingPolicy(), QStringLiteral("basename"));
    QCOMPARE(service.format(), QStringLiteral("mp3"));
    QCOMPARE(service.bitrate(), 320);
    QCOMPARE(service.sampleRate(), 44100);
    QCOMPARE(service.channelMode(), QStringLiteral("stereo"));
    QCOMPARE(service.playbackRate(), 1.0);
    QCOMPARE(service.pitchSemitones(), 0);
    QCOMPARE(service.addResultsToPlaylist(), true);
    QCOMPARE(service.totalCount(), 0);
    QCOMPARE(service.currentIndex(), -1);
    QCOMPARE(service.pendingCount(), 0);
    QCOMPARE(service.runningCount(), 0);
    QCOMPARE(service.succeededCount(), 0);
    QCOMPARE(service.failedCount(), 0);
    QCOMPARE(service.canceledCount(), 0);
    QCOMPARE(service.skippedCount(), 0);

    const QVariantMap settings = service.settings();
    QCOMPARE(settings.value(QStringLiteral("namingPolicy")).toString(), QStringLiteral("basename"));
    QCOMPARE(settings.value(QStringLiteral("format")).toString(), QStringLiteral("mp3"));
    QCOMPARE(settings.value(QStringLiteral("conflictPolicy")).toString(), QStringLiteral("auto-rename"));
    QCOMPARE(settings.value(QStringLiteral("retryPolicy")).toString(), QStringLiteral("manual"));
    QCOMPARE(settings.value(QStringLiteral("playlistAddMode")).toString(), QStringLiteral("immediate"));
    QCOMPARE(settings.value(QStringLiteral("bitrate")).toInt(), 320);
    QCOMPARE(settings.value(QStringLiteral("sampleRate")).toInt(), 44100);
    QCOMPARE(settings.value(QStringLiteral("channelMode")).toString(), QStringLiteral("stereo"));
    QCOMPARE(settings.value(QStringLiteral("playbackRate")).toDouble(), 1.0);
    QCOMPARE(settings.value(QStringLiteral("pitchSemitones")).toInt(), 0);
    QCOMPARE(settings.value(QStringLiteral("addResultsToPlaylist")).toBool(), true);
    QCOMPARE(service.jobMetadata(), QVariantMap());
}

void BatchAudioConverterServiceTest::exposesSequentialParallelismDecision()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString trackerPath = tempDir.filePath(QStringLiteral("demo.mod"));
    const QString wavPath = tempDir.filePath(QStringLiteral("demo.wav"));
    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly));
    trackerFile.close();
    writeSilentWavFile(wavPath);

    BatchAudioConverterService service;
    service.setPlaybackRate(1.25);
    service.setSourceFiles(QStringList{trackerPath, wavPath});

    const QVariantMap diagnostics = service.runtimeDiagnostics();
    QCOMPARE(diagnostics.value(QStringLiteral("executionMode")).toString(),
             QStringLiteral("sequential-single-worker"));
    QCOMPARE(diagnostics.value(QStringLiteral("maxConcurrentJobsConfigured")).toInt(), 1);
    QCOMPARE(diagnostics.value(QStringLiteral("trackerItemCount")).toInt(), 1);
    QCOMPARE(diagnostics.value(QStringLiteral("dspAdjustedItemCount")).toInt(), 2);

    const QVariantMap decision = service.parallelismDecision();
    QCOMPARE(decision.value(QStringLiteral("decisionKey")).toString(),
             QStringLiteral("sequential-default-safe"));
    QCOMPARE(decision.value(QStringLiteral("recommendedMaxConcurrentJobs")).toInt(), 1);
    QCOMPARE(decision.value(QStringLiteral("futureCapabilityDrivenCap")).toInt(), 1);
    QCOMPARE(decision.value(QStringLiteral("multiWorkerEnabled")).toBool(), false);
    const QVariantList blockedReasons = decision.value(QStringLiteral("blockedReasons")).toList();
    QVERIFY(blockedReasons.contains(QStringLiteral("tracker modules stay sequential")));
    QVERIFY(blockedReasons.contains(QStringLiteral("heavy DSP profiles stay sequential")));
    QCOMPARE(decision.value(QStringLiteral("semantics")).toMap()
                 .value(QStringLiteral("reportOrdering")).toString(),
             QStringLiteral("queue-order"));
}

void BatchAudioConverterServiceTest::normalizesSlashPrefixedWindowsOutputDirectory()
{
#if defined(Q_OS_WIN)
    BatchAudioConverterService service;

    service.setOutputDirectory(QStringLiteral("/C:/Users/leo/Desktop"));

    QCOMPARE(service.outputDirectory(), QStringLiteral("C:/Users/leo/Desktop"));
#else
    QSKIP("Windows drive-letter path normalization only applies on Windows.");
#endif
}

void BatchAudioConverterServiceTest::materializesPendingItemsFromSourceFiles()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("Track01.FLAC"));
    const QString secondPath = tempDir.filePath(QStringLiteral("Demo Track.mod"));
    QFile firstFile(firstPath);
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    firstFile.close();
    QFile secondFile(secondPath);
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    secondFile.close();

    BatchAudioConverterService service;
    QSignalSpy itemsSpy(&service, &BatchAudioConverterService::itemsChanged);

    QVariantList sources;
    sources.push_back(firstPath);
    sources.push_back(QUrl::fromLocalFile(secondPath).toString());
    service.setSourceFilesFromVariantList(sources);

    QCOMPARE(itemsSpy.count(), 1);
    QCOMPARE(service.totalCount(), 2);
    QCOMPARE(service.pendingCount(), 2);
    QCOMPARE(service.currentIndex(), -1);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);

    const QVariantMap first = items.at(0).toMap();
    QVERIFY(!first.value(QStringLiteral("itemId")).toString().isEmpty());
    QCOMPARE(first.value(QStringLiteral("sourceFile")).toString(), firstPath);
    QCOMPARE(first.value(QStringLiteral("sourceDisplayName")).toString(), QStringLiteral("Track01.FLAC"));
    QCOMPARE(first.value(QStringLiteral("sourceFormat")).toString(), QStringLiteral("flac"));
    QCOMPARE(first.value(QStringLiteral("sourceDurationMs")).toLongLong(), 0);
    QCOMPARE(first.value(QStringLiteral("sourceOriginType")).toString(), QStringLiteral("unknown"));
    QCOMPARE(first.value(QStringLiteral("retryCount")).toInt(), 0);
    QCOMPARE(first.value(QStringLiteral("terminalResult")).toString(), QStringLiteral("none"));
    QCOMPARE(first.value(QStringLiteral("failureType")).toString(), QStringLiteral("none"));
    QCOMPARE(first.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(first.value(QStringLiteral("progress")).toDouble(), 0.0);
    QVERIFY(first.value(QStringLiteral("createdAtMs")).toLongLong() > 0);
    QVERIFY(first.value(QStringLiteral("updatedAtMs")).toLongLong() > 0);
    QVERIFY(first.value(QStringLiteral("sourceIdentity")).toMap().contains(QStringLiteral("itemId")));
    QVERIFY(first.value(QStringLiteral("queueMetadata")).toMap().contains(QStringLiteral("conflictResolution")));
    QVERIFY(first.value(QStringLiteral("runtimeState")).toMap().contains(QStringLiteral("effectiveSettingsSnapshot")));
    QVERIFY(first.value(QStringLiteral("finalResultState")).toMap().contains(QStringLiteral("failureType")));

    const QVariantMap second = items.at(1).toMap();
    QVERIFY(!second.value(QStringLiteral("itemId")).toString().isEmpty());
    QVERIFY(second.value(QStringLiteral("itemId")).toString() != first.value(QStringLiteral("itemId")).toString());
    QCOMPARE(second.value(QStringLiteral("sourceFile")).toString(), secondPath);
    QCOMPARE(second.value(QStringLiteral("sourceDisplayName")).toString(), QStringLiteral("Demo Track.mod"));
    QCOMPARE(second.value(QStringLiteral("sourceFormat")).toString(), QStringLiteral("mod"));
    QCOMPARE(second.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
}

void BatchAudioConverterServiceTest::exposesJobMetadataAndStableItemIds()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);

    BatchAudioConverterService service;
    service.setSourceFiles(QStringList{firstPath, secondPath});

    const QVariantMap job = service.jobMetadata();
    QVERIFY(!job.value(QStringLiteral("jobId")).toString().isEmpty());
    QVERIFY(job.value(QStringLiteral("createdAtMs")).toLongLong() > 0);
    QCOMPARE(job.value(QStringLiteral("startedAtMs")).toLongLong(), 0);
    QCOMPARE(job.value(QStringLiteral("finishedAtMs")).toLongLong(), 0);
    QCOMPARE(job.value(QStringLiteral("isRunning")).toBool(), false);

    const QString firstId = service.itemIdAt(0);
    const QString secondId = service.itemIdAt(1);
    QVERIFY(!firstId.isEmpty());
    QVERIFY(!secondId.isEmpty());
    QVERIFY(firstId != secondId);
    QCOMPARE(service.indexOfItemId(firstId), 0);
    QCOMPARE(service.indexOfItemId(secondId), 1);
}

void BatchAudioConverterServiceTest::supportsItemLookupAndMutationById()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);

    BatchAudioConverterService service;
    service.setSourceFiles(QStringList{firstPath, secondPath});

    const QString secondId = service.itemIdAt(1);
    QVERIFY(!secondId.isEmpty());

    QVERIFY(service.setItemStateById(secondId, BatchAudioConverterService::Running));
    QVERIFY(service.setItemProgressById(secondId, 0.4));

    const QVariantMap item = service.itemById(secondId);
    QCOMPARE(item.value(QStringLiteral("sourceFile")).toString(), secondPath);
    QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("running"));
    QCOMPARE(item.value(QStringLiteral("progress")).toDouble(), 0.4);
    QCOMPARE(item.value(QStringLiteral("itemActionability")).toString(), QStringLiteral("running"));
    QVERIFY(item.value(QStringLiteral("updatedAtMs")).toLongLong()
            >= item.value(QStringLiteral("createdAtMs")).toLongLong());

    QVERIFY(!service.setItemStateById(QStringLiteral("missing-id"), BatchAudioConverterService::Failed));
    QVERIFY(!service.setItemProgressById(QStringLiteral("missing-id"), 0.2));
    QCOMPARE(service.itemById(QStringLiteral("missing-id")), QVariantMap());
}

void BatchAudioConverterServiceTest::exportsAndAppliesReusableSettings()
{
    BatchAudioConverterService service;
    service.setOutputDirectory(QStringLiteral("/tmp/batch-target"));
    service.setNamingPolicy(QStringLiteral("artist-title"));
    service.setFormat(QStringLiteral("webm"));
    service.setConflictPolicy(QStringLiteral("overwrite-if-allowed"));
    service.setRetryPolicy(QStringLiteral("retry-failed-only"));
    service.setPlaylistAddMode(QStringLiteral("deferred"));
    service.setBitrate(192);
    service.setSampleRate(48000);
    service.setChannelMode(QStringLiteral("mono"));
    service.setPlaybackRate(1.25);
    service.setPitchSemitones(-3);

    const QVariantMap preset = service.exportPresetSettings();
    QVERIFY(!preset.contains(QStringLiteral("retryPolicy")));
    QCOMPARE(preset.value(QStringLiteral("conflictPolicy")).toString(), QStringLiteral("overwrite-if-allowed"));
    QCOMPARE(preset.value(QStringLiteral("playlistAddMode")).toString(), QStringLiteral("deferred"));
    QCOMPARE(preset.value(QStringLiteral("outputDirectory")).toString(), QStringLiteral("/tmp/batch-target"));
    QCOMPARE(preset.value(QStringLiteral("format")).toString(), QStringLiteral("webm"));

    BatchAudioConverterService restored;
    QVERIFY(restored.applySettingsMap(service.settings()));
    QCOMPARE(restored.settings(), service.settings());

    restored.setRetryPolicy(QStringLiteral("manual"));
    QVERIFY(restored.applySettingsMap(preset));
    QCOMPARE(restored.outputDirectory(), QStringLiteral("/tmp/batch-target"));
    QCOMPARE(restored.namingPolicy(), QStringLiteral("artist-title"));
    QCOMPARE(restored.format(), QStringLiteral("webm"));
    QCOMPARE(restored.conflictPolicy(), QStringLiteral("overwrite-if-allowed"));
    QCOMPARE(restored.playlistAddMode(), QStringLiteral("deferred"));
    QCOMPARE(restored.retryPolicy(), QStringLiteral("manual"));
    QCOMPARE(restored.addResultsToPlaylist(), true);
}

void BatchAudioConverterServiceTest::replacesSourcesFromVariantListWithValidationAndOrigins()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString validPath = tempDir.filePath(QStringLiteral("valid.wav"));
    const QString unsupportedPath = tempDir.filePath(QStringLiteral("notes.txt"));
    const QString missingPath = tempDir.filePath(QStringLiteral("missing.wav"));
    writeSilentWavFile(validPath);
    QFile unsupportedFile(unsupportedPath);
    QVERIFY(unsupportedFile.open(QIODevice::WriteOnly));
    unsupportedFile.close();

    BatchAudioConverterService service;
    const QVariantMap summary = service.replaceSourceFilesFromVariantList(
        QVariantList{
            validPath,
            QUrl::fromLocalFile(validPath).toString(),
            unsupportedPath,
            QStringLiteral("https://example.com/remote.flac"),
            missingPath
        },
        QStringLiteral("file-picker"));

    QCOMPARE(summary.value(QStringLiteral("acceptedCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("duplicateCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("unsupportedCount")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("missingCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("queueCount")).toInt(), 5);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 5);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("file-picker"));

    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("validation"));

    QCOMPARE(items.at(2).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("unsupported-format"));

    QCOMPARE(items.at(3).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(items.at(3).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("validation"));

    QCOMPARE(items.at(4).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QCOMPARE(items.at(4).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("source-missing"));
}

void BatchAudioConverterServiceTest::appendsSupportedFilesFromFolderRecursively()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString rootFolder = tempDir.filePath(QStringLiteral("import"));
    const QString subFolder = tempDir.filePath(QStringLiteral("import/sub"));
    QVERIFY(QDir().mkpath(subFolder));

    const QString existingPath = tempDir.filePath(QStringLiteral("existing.wav"));
    writeSilentWavFile(existingPath);

    const QString duplicateInFolder = rootFolder + QStringLiteral("/existing.wav");
    const QString nestedValid = subFolder + QStringLiteral("/nested.wav");
    const QString hiddenValid = rootFolder + QStringLiteral("/.hidden.wav");
    const QString unsupported = subFolder + QStringLiteral("/notes.txt");
    writeSilentWavFile(duplicateInFolder);
    writeSilentWavFile(nestedValid);
    writeSilentWavFile(hiddenValid);
    QFile unsupportedFile(unsupported);
    QVERIFY(unsupportedFile.open(QIODevice::WriteOnly));
    unsupportedFile.close();

    BatchAudioConverterService service;
    const QVariantMap replaceSummary = service.replaceSourceFilesFromVariantList(
        QVariantList{existingPath},
        QStringLiteral("playlist-selection"));
    QCOMPARE(replaceSummary.value(QStringLiteral("acceptedCount")).toInt(), 1);

    const QVariantMap appendSummary = service.appendSourceFolder(rootFolder);
    QCOMPARE(appendSummary.value(QStringLiteral("acceptedCount")).toInt(), 2);
    QCOMPARE(appendSummary.value(QStringLiteral("duplicateCount")).toInt(), 0);
    QCOMPARE(appendSummary.value(QStringLiteral("unsupportedCount")).toInt(), 1);
    QCOMPARE(appendSummary.value(QStringLiteral("hiddenSkippedCount")).toInt(), 1);
    QCOMPARE(appendSummary.value(QStringLiteral("recursive")).toBool(), true);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("playlist-selection"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("folder-import"));
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("folder-import"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
}

void BatchAudioConverterServiceTest::mixesPlaylistPickerAndFolderIntakeWithoutLosingOrigins()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString playlistPath = tempDir.filePath(QStringLiteral("playlist.wav"));
    const QString pickerPath = tempDir.filePath(QStringLiteral("picker.wav"));
    writeSilentWavFile(playlistPath);
    writeSilentWavFile(pickerPath);

    const QString folder = tempDir.filePath(QStringLiteral("folder"));
    const QString nestedFolder = QDir(folder).filePath(QStringLiteral("nested"));
    QVERIFY(QDir().mkpath(nestedFolder));
    const QString folderPath = QDir(nestedFolder).filePath(QStringLiteral("folder-track.wav"));
    const QString hiddenPath = QDir(folder).filePath(QStringLiteral(".hidden.wav"));
    const QString unsupportedPath = QDir(folder).filePath(QStringLiteral("notes.txt"));
    writeSilentWavFile(folderPath);
    writeSilentWavFile(hiddenPath);
    QFile unsupportedFile(unsupportedPath);
    QVERIFY(unsupportedFile.open(QIODevice::WriteOnly));
    unsupportedFile.close();

    BatchAudioConverterService service;
    const QVariantMap playlistSummary = service.replaceSourceFilesFromVariantList(
        QVariantList{playlistPath},
        QStringLiteral("playlist-selection"));
    QCOMPARE(playlistSummary.value(QStringLiteral("acceptedCount")).toInt(), 1);

    const QVariantMap pickerSummary = service.appendSourceFilesFromVariantList(
        QVariantList{QUrl::fromLocalFile(pickerPath).toString()},
        QStringLiteral("file-picker"));
    QCOMPARE(pickerSummary.value(QStringLiteral("acceptedCount")).toInt(), 1);

    const QVariantMap folderSummary = service.appendSourceFolder(folder);
    QCOMPARE(folderSummary.value(QStringLiteral("acceptedCount")).toInt(), 1);
    QCOMPARE(folderSummary.value(QStringLiteral("unsupportedCount")).toInt(), 1);
    QCOMPARE(folderSummary.value(QStringLiteral("hiddenSkippedCount")).toInt(), 1);
    QCOMPARE(folderSummary.value(QStringLiteral("queueCount")).toInt(), 3);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("sourceFile")).toString(), playlistPath);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("playlist-selection"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("sourceFile")).toString(), pickerPath);
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("file-picker"));
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("sourceFile")).toString(), folderPath);
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("folder-import"));
}

void BatchAudioConverterServiceTest::reordersAndRemovesItemsBeforeStart()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("01-first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("02-second.wav"));
    const QString thirdPath = tempDir.filePath(QStringLiteral("03-third.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);
    writeSilentWavFile(thirdPath);

    BatchAudioConverterService service;
    service.setSourceFiles(QStringList{firstPath, secondPath, thirdPath});

    const QString secondId = service.itemIdAt(1);
    const QString thirdId = service.itemIdAt(2);
    QVERIFY(service.canMoveItemDown(secondId));
    QVERIFY(service.moveItemDown(secondId));
    QCOMPARE(service.items().at(2).toMap().value(QStringLiteral("sourceFile")).toString(), secondPath);

    QVERIFY(service.canMoveItemUp(thirdId));
    QVERIFY(service.moveItemUp(thirdId));
    QCOMPARE(service.items().at(0).toMap().value(QStringLiteral("sourceFile")).toString(), thirdPath);

    QVERIFY(service.canRemoveItem(secondId));
    QVERIFY(service.removeItemById(secondId));
    QCOMPARE(service.totalCount(), 2);
    QCOMPARE(service.items().at(0).toMap().value(QStringLiteral("sourceFile")).toString(), thirdPath);
    QCOMPARE(service.items().at(1).toMap().value(QStringLiteral("sourceFile")).toString(), firstPath);
}

void BatchAudioConverterServiceTest::retriesFailedItemsWithoutDuplicatingQueueEntries()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);

    BatchAudioConverterService service;
    service.setSourceFiles(QStringList{firstPath, secondPath});

    const QString failedId = service.itemIdAt(1);
    QVERIFY(service.setItemErrorText(1, QStringLiteral("Decoder failed")));
    QVERIFY(service.setItemState(1, BatchAudioConverterService::Failed));
    QCOMPARE(service.failedCount(), 1);

    QVERIFY(service.canRetryItem(failedId));
    QVERIFY(service.retryItemById(failedId));
    QCOMPARE(service.totalCount(), 2);
    QCOMPARE(service.failedCount(), 0);
    QCOMPARE(service.pendingCount(), 2);

    const QVariantMap retriedItem = service.itemById(failedId);
    QCOMPARE(retriedItem.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(retriedItem.value(QStringLiteral("retryCount")).toInt(), 1);
    const QVariantList attempts = retriedItem.value(QStringLiteral("reportMetadata")).toMap()
                                      .value(QStringLiteral("attempts")).toList();
    QCOMPARE(attempts.size(), 1);
    QCOMPARE(attempts.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QCOMPARE(attempts.at(0).toMap().value(QStringLiteral("attemptNumber")).toInt(), 1);
    QCOMPARE(attempts.at(0).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("internal-pipeline"));

    QVERIFY(service.setItemErrorText(1, QStringLiteral("Still failing")));
    QVERIFY(service.setItemState(1, BatchAudioConverterService::Failed));
    QCOMPARE(service.retryFailedItems(), 1);
    QCOMPARE(service.itemById(failedId).value(QStringLiteral("retryCount")).toInt(), 2);
}

void BatchAudioConverterServiceTest::retriesSkippedConflictItemsAfterOutputDirectoryChange()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString blockedOutputDir = tempDir.filePath(QStringLiteral("blocked"));
    const QString recoveredOutputDir = tempDir.filePath(QStringLiteral("recovered"));
    QVERIFY(QDir().mkpath(blockedOutputDir));
    QVERIFY(QDir().mkpath(recoveredOutputDir));

    const QString sourcePath = tempDir.filePath(QStringLiteral("track.wav"));
    writeSilentWavFile(sourcePath);

    QFile existingOutput(QDir(blockedOutputDir).filePath(QStringLiteral("track (converted).wav")));
    QVERIFY(existingOutput.open(QIODevice::WriteOnly));
    existingOutput.close();

    BatchAudioConverterService service;
    service.setOutputDirectory(blockedOutputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setConflictPolicy(QStringLiteral("skip-on-conflict"));
    service.setSourceFiles(QStringList{sourcePath});

    const QString itemId = service.itemIdAt(0);
    const QVariantMap blockedItem = service.itemById(itemId);
    QCOMPARE(blockedItem.value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(blockedItem.value(QStringLiteral("failureType")).toString(), QStringLiteral("output-conflict"));
    QVERIFY(service.canRetryItem(itemId));

    service.setOutputDirectory(recoveredOutputDir);
    QCOMPARE(service.retrySkippedItems(), 1);

    const QVariantMap retriedItem = service.itemById(itemId);
    QCOMPARE(retriedItem.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(retriedItem.value(QStringLiteral("retryCount")).toInt(), 1);
    QCOMPARE(retriedItem.value(QStringLiteral("outputFile")).toString(),
             QDir(recoveredOutputDir).filePath(QStringLiteral("track (converted).wav")));
}

void BatchAudioConverterServiceTest::retriesPermissionFailureAfterOutputDirectoryRecovery()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString blockedOutputDir = tempDir.filePath(QStringLiteral("blocked"));
    const QString recoveredOutputDir = tempDir.filePath(QStringLiteral("recovered"));
    QVERIFY(QDir().mkpath(blockedOutputDir));
    QVERIFY(QDir().mkpath(recoveredOutputDir));

    const QString sourcePath = tempDir.filePath(QStringLiteral("track.wav"));
    writeSilentWavFile(sourcePath);

    BatchAudioConverterService service;
    service.setOutputDirectory(blockedOutputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{sourcePath});

    const QString itemId = service.itemIdAt(0);
    QVERIFY(service.setItemErrorText(0, QStringLiteral("Permission denied: cannot write to the output directory.")));
    QVERIFY(service.setItemState(0, BatchAudioConverterService::Failed));
    QCOMPARE(service.itemById(itemId).value(QStringLiteral("failureType")).toString(),
             QStringLiteral("permission-denied"));
    QVERIFY(service.canRetryItem(itemId));

    service.setOutputDirectory(recoveredOutputDir);
    QVERIFY(service.retryItemById(itemId));

    const QVariantMap retriedItem = service.itemById(itemId);
    QCOMPARE(retriedItem.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(retriedItem.value(QStringLiteral("retryCount")).toInt(), 1);
    QCOMPARE(retriedItem.value(QStringLiteral("outputFile")).toString(),
             QDir(recoveredOutputDir).filePath(QStringLiteral("track (converted).wav")));
    const QVariantList attempts = retriedItem.value(QStringLiteral("reportMetadata")).toMap()
                                      .value(QStringLiteral("attempts")).toList();
    QCOMPARE(attempts.size(), 1);
    QCOMPARE(attempts.constFirst().toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("permission-denied"));
}

void BatchAudioConverterServiceTest::recordsAttemptHistoryWithSettingsChanges()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstOutputDir = tempDir.filePath(QStringLiteral("out-a"));
    const QString secondOutputDir = tempDir.filePath(QStringLiteral("out-b"));
    QVERIFY(QDir().mkpath(firstOutputDir));
    QVERIFY(QDir().mkpath(secondOutputDir));

    const QString sourcePath = tempDir.filePath(QStringLiteral("track.wav"));
    writeSilentWavFile(sourcePath);

    BatchAudioConverterService service;
    service.setOutputDirectory(firstOutputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{sourcePath});

    const QString itemId = service.itemIdAt(0);
    QVERIFY(service.setItemErrorText(0, QStringLiteral("Output file already exists: %1")
                                         .arg(QDir(firstOutputDir).filePath(QStringLiteral("track (converted).wav")))));
    QVERIFY(service.setItemState(0, BatchAudioConverterService::Failed));
    QVERIFY(service.retryItemById(itemId));

    service.setOutputDirectory(secondOutputDir);
    QVERIFY(service.setItemErrorText(0, QStringLiteral("Output file already exists: %1")
                                         .arg(QDir(secondOutputDir).filePath(QStringLiteral("track (converted).wav")))));
    QVERIFY(service.setItemState(0, BatchAudioConverterService::Failed));
    QVERIFY(service.retryItemById(itemId));

    const QVariantList attempts = service.itemById(itemId)
                                      .value(QStringLiteral("reportMetadata")).toMap()
                                      .value(QStringLiteral("attempts")).toList();
    QCOMPARE(attempts.size(), 2);
    QCOMPARE(attempts.at(0).toMap().value(QStringLiteral("attemptNumber")).toInt(), 1);
    QCOMPARE(attempts.at(0).toMap().value(QStringLiteral("settingsChangedFromPreviousAttempt")).toBool(),
             false);
    QCOMPARE(attempts.at(0).toMap().value(QStringLiteral("effectiveSettingsSnapshot")).toMap()
                 .value(QStringLiteral("outputDirectory")).toString(),
             firstOutputDir);
    QCOMPARE(attempts.at(1).toMap().value(QStringLiteral("attemptNumber")).toInt(), 2);
    QCOMPARE(attempts.at(1).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("output-conflict"));
    QCOMPARE(attempts.at(1).toMap().value(QStringLiteral("settingsChangedFromPreviousAttempt")).toBool(),
             true);
    QCOMPARE(attempts.at(1).toMap().value(QStringLiteral("changedSettingKeys")).toList(),
             QVariantList{QStringLiteral("outputDirectory")});
    QCOMPARE(attempts.at(1).toMap().value(QStringLiteral("effectiveSettingsSnapshot")).toMap()
                 .value(QStringLiteral("outputDirectory")).toString(),
             secondOutputDir);
}

void BatchAudioConverterServiceTest::exportsAndRestoresDraftStateSafely()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);

    BatchAudioConverterService service;
    service.setOutputDirectory(tempDir.filePath(QStringLiteral("out")));
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{firstPath, secondPath});
    QVERIFY(service.setItemState(0, BatchAudioConverterService::Running));
    QVERIFY(service.setItemProgress(0, 0.5));
    QVERIFY(service.setItemStatusText(0, QStringLiteral("Halfway there")));
    QVERIFY(service.setItemErrorText(1, QStringLiteral("Output file already exists: %1")
                                         .arg(tempDir.filePath(QStringLiteral("existing.wav")))));
    QVERIFY(service.setItemState(1, BatchAudioConverterService::Failed));

    const QVariantMap draft = service.exportDraftState();
    QCOMPARE(draft.value(QStringLiteral("schema")).toString(),
             QStringLiteral("waveflux.batch-audio-converter.draft.v1"));
    QCOMPARE(draft.value(QStringLiteral("items")).toList().size(), 2);

    BatchAudioConverterService restored;
    QVERIFY(restored.restoreDraftState(draft));
    QCOMPARE(restored.outputDirectory(), service.outputDirectory());
    QCOMPARE(restored.format(), QStringLiteral("wav"));
    QCOMPARE(restored.itemIdAt(0), service.itemIdAt(0));
    QCOMPARE(restored.items().at(0).toMap().value(QStringLiteral("state")).toString(),
             QStringLiteral("pending"));
    QCOMPARE(restored.items().at(0).toMap().value(QStringLiteral("progress")).toDouble(), 0.0);
    QCOMPARE(restored.items().at(1).toMap().value(QStringLiteral("state")).toString(),
             QStringLiteral("failed"));
    QCOMPARE(restored.items().at(1).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("output-conflict"));
}

void BatchAudioConverterServiceTest::exportsAndRestoresLargeDraftQueue()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const int itemCount = 500;
    QStringList sources;
    sources.reserve(itemCount);
    for (int i = 0; i < itemCount; ++i) {
        const QString path = tempDir.filePath(QStringLiteral("large-%1.wav").arg(i, 3, 10, QLatin1Char('0')));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        sources.push_back(path);
    }

    BatchAudioConverterService service;
    service.setOutputDirectory(tempDir.filePath(QStringLiteral("out")));
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(sources);

    QCOMPARE(service.totalCount(), itemCount);
    QVERIFY(service.moveItemDown(service.itemIdAt(0)));
    QVERIFY(service.setItemErrorText(10, QStringLiteral("Source file is missing.")));
    QVERIFY(service.setItemState(10, BatchAudioConverterService::Failed));

    const QVariantMap draft = service.exportDraftState();
    QCOMPARE(draft.value(QStringLiteral("items")).toList().size(), itemCount);

    BatchAudioConverterService restored;
    QVERIFY(restored.restoreDraftState(draft));
    QCOMPARE(restored.totalCount(), itemCount);
    QCOMPARE(restored.outputDirectory(), service.outputDirectory());
    QCOMPARE(restored.items().at(0).toMap().value(QStringLiteral("sourceFile")).toString(), sources.at(1));
    QCOMPARE(restored.items().at(1).toMap().value(QStringLiteral("sourceFile")).toString(), sources.at(0));
    QCOMPARE(restored.items().at(10).toMap().value(QStringLiteral("state")).toString(),
             QStringLiteral("failed"));
    QCOMPARE(restored.items().at(10).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("source-missing"));
}

void BatchAudioConverterServiceTest::exportsFinishedJobHistoryReports()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    QVariantMap report;
    report.insert(QStringLiteral("schema"), QStringLiteral("waveflux.batch-audio-converter.report.v1"));
    report.insert(QStringLiteral("jobMetadata"),
                  QVariantMap{{QStringLiteral("jobId"), QStringLiteral("history-job")},
                              {QStringLiteral("createdAtMs"), static_cast<qint64>(1)},
                              {QStringLiteral("startedAtMs"), static_cast<qint64>(2)},
                              {QStringLiteral("finishedAtMs"), static_cast<qint64>(3)},
                              {QStringLiteral("isRunning"), false}});
    report.insert(QStringLiteral("settings"),
                  QVariantMap{{QStringLiteral("format"), QStringLiteral("wav")}});
    report.insert(QStringLiteral("finalSummary"),
                  QVariantMap{{QStringLiteral("succeededCount"), 1},
                              {QStringLiteral("failedCount"), 0},
                              {QStringLiteral("canceledCount"), 0},
                              {QStringLiteral("skippedCount"), 0}});
    report.insert(QStringLiteral("items"),
                  QVariantList{QVariantMap{
                      {QStringLiteral("sourceFile"), QStringLiteral("/tmp/source.wav")},
                      {QStringLiteral("outputFile"), QStringLiteral("/tmp/output.wav")},
                      {QStringLiteral("finalState"), QStringLiteral("succeeded")},
                      {QStringLiteral("failureType"), QStringLiteral("none")},
                      {QStringLiteral("errorReason"), QString()},
                      {QStringLiteral("retryCount"), 0},
                      {QStringLiteral("attempts"), QVariantList{QVariantMap{
                           {QStringLiteral("startedAtMs"), static_cast<qint64>(2)},
                           {QStringLiteral("finishedAtMs"), static_cast<qint64>(3)},
                           {QStringLiteral("terminalResult"), QStringLiteral("succeeded")}
                       }}}
                  }});

    BatchAudioConverterService service;
    QVERIFY(service.replaceFinishedJobHistory(QVariantList{report}));
    QCOMPARE(service.finishedJobHistory().size(), 1);

    const QString jsonPath = tempDir.filePath(QStringLiteral("report.json"));
    const QString txtPath = tempDir.filePath(QStringLiteral("report.txt"));
    const QString csvPath = tempDir.filePath(QStringLiteral("report.csv"));
    QVERIFY(service.exportHistoryReportToFile(QStringLiteral("history-job"), jsonPath, QStringLiteral("json")));
    QVERIFY(service.exportHistoryReportToFile(QStringLiteral("history-job"), txtPath, QStringLiteral("txt")));
    QVERIFY(service.exportHistoryReportToFile(QStringLiteral("history-job"), csvPath, QStringLiteral("csv")));

    QFile jsonFile(jsonPath);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    QVERIFY(jsonFile.readAll().contains("\"history-job\""));

    QFile txtFile(txtPath);
    QVERIFY(txtFile.open(QIODevice::ReadOnly));
    QVERIFY(txtFile.readAll().contains("WaveFlux Batch Audio Converter Report"));

    QFile csvFile(csvPath);
    QVERIFY(csvFile.open(QIODevice::ReadOnly));
    QVERIFY(csvFile.readAll().contains("\"sourceFile\""));
}

void BatchAudioConverterServiceTest::removesPendingTailItemWhileRunning()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));
    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    const QString thirdPath = tempDir.filePath(QStringLiteral("third.wav"));
    writeSilentWavFile(firstPath, 44100, 2, 60000);
    writeSilentWavFile(secondPath, 44100, 2, 500);
    writeSilentWavFile(thirdPath, 44100, 2, 500);

    BatchAudioConverterService service;
    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setPlaybackRate(0.25);
    service.setSourceFiles(QStringList{firstPath, secondPath, thirdPath});

    const QString pendingTailId = service.itemIdAt(2);
    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(service.currentIndex(), 0, 3000);
    QVERIFY(!service.canRemoveItem(service.itemIdAt(0)));
    QVERIFY(service.canRemoveItem(pendingTailId));
    QVERIFY(service.removeItemById(pendingTailId));
    QCOMPARE(service.totalCount(), 2);
    QCOMPARE(service.pendingCount(), 1);
    QVERIFY(!service.canMoveItemDown(service.itemIdAt(1)));
    service.cancelBatch();
    QTRY_COMPARE_WITH_TIMEOUT(service.isRunning(), false, 15000);
}

void BatchAudioConverterServiceTest::updatesRuntimeFieldsAndCounters()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("alpha.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("beta.wav"));
    QFile firstFile(firstPath);
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    firstFile.close();
    QFile secondFile(secondPath);
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    secondFile.close();

    BatchAudioConverterService service;
    service.setSourceFiles(QStringList{firstPath, secondPath});

    QVERIFY(service.setItemSourceMetadata(0, QStringLiteral("Alpha"), QStringLiteral("WAV"), 1234));
    QVERIFY(service.setItemOutputFile(0, tempDir.filePath(QStringLiteral("alpha (converted).mp3"))));
    QVERIFY(service.setItemState(0, BatchAudioConverterService::Running));
    QVERIFY(service.setItemProgress(0, 0.25));
    QVERIFY(service.setItemStatusText(0, QStringLiteral("Converting alpha")));

    QCOMPARE(service.currentIndex(), 0);
    QCOMPARE(service.runningCount(), 1);
    QCOMPARE(service.pendingCount(), 1);

    const QVariantMap runningItem = service.currentItem();
    QCOMPARE(runningItem.value(QStringLiteral("sourceDisplayName")).toString(), QStringLiteral("Alpha"));
    QCOMPARE(runningItem.value(QStringLiteral("sourceFormat")).toString(), QStringLiteral("wav"));
    QCOMPARE(runningItem.value(QStringLiteral("sourceDurationMs")).toLongLong(), 1234);
    QCOMPARE(runningItem.value(QStringLiteral("progress")).toDouble(), 0.25);
    QCOMPARE(runningItem.value(QStringLiteral("statusText")).toString(), QStringLiteral("Converting alpha"));
    QCOMPARE(runningItem.value(QStringLiteral("itemActionability")).toString(), QStringLiteral("running"));

    QVERIFY(service.setItemResultFile(0, tempDir.filePath(QStringLiteral("alpha (converted).mp3"))));
    QVERIFY(service.setItemState(0, BatchAudioConverterService::Succeeded));
    QCOMPARE(service.succeededCount(), 1);
    QCOMPARE(service.runningCount(), 0);
    QCOMPARE(service.pendingCount(), 1);

    QVERIFY(service.setItemErrorText(1, QStringLiteral("Unsupported source.")));
    QVERIFY(service.setItemState(1, BatchAudioConverterService::Skipped));
    QCOMPARE(service.skippedCount(), 1);
    QCOMPARE(service.totalCount(), 2);

    const QVariantList items = service.items();
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("progress")).toDouble(), 1.0);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("resultFile")).toString(),
             tempDir.filePath(QStringLiteral("alpha (converted).mp3")));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("terminalResult")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("failureType")).toString(), QStringLiteral("validation"));

    QVERIFY(!service.setItemState(9, BatchAudioConverterService::Failed));
    QVERIFY(!service.setItemProgress(-1, 0.5));
}

void BatchAudioConverterServiceTest::previewsOutputFilesBeforeStart()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstDir = tempDir.filePath(QStringLiteral("disc-a"));
    const QString secondDir = tempDir.filePath(QStringLiteral("disc-b"));
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(firstDir));
    QVERIFY(QDir().mkpath(secondDir));
    QVERIFY(QDir().mkpath(outputDir));

    const QString firstPath = QDir(firstDir).filePath(QStringLiteral("Same Name.wav"));
    const QString secondPath = QDir(secondDir).filePath(QStringLiteral("Same Name.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);

    QFile existingOutput(QDir(outputDir).filePath(QStringLiteral("Same Name (converted).webm")));
    QVERIFY(existingOutput.open(QIODevice::WriteOnly));
    existingOutput.close();

    BatchAudioConverterService service;
    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("webm"));
    service.setSourceFiles(QStringList{firstPath, secondPath});

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("outputFile")).toString(),
             QDir(outputDir).filePath(QStringLiteral("Same Name (converted 1).webm")));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("outputFile")).toString(),
             QDir(outputDir).filePath(QStringLiteral("Same Name (converted 2).webm")));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("conflictResolution")).toMap()
                 .value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("auto-renamed"));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("previewDiagnostics")).toMap()
                 .value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("auto-renamed"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("conflictResolution")).toMap()
                 .value(QStringLiteral("hadConflict")).toBool(),
             true);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
}

void BatchAudioConverterServiceTest::supportsArtistTitleNamingPolicyWithFallback()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString taggedPath = tempDir.filePath(QStringLiteral("tagged.wav"));
    const QString fallbackPath = tempDir.filePath(QStringLiteral("fallback.wav"));
    writeSilentWavFile(taggedPath);
    writeSilentWavFile(fallbackPath);
    QVERIFY(writeBasicTags(taggedPath,
                           QStringLiteral("Signal/Noise"),
                           QStringLiteral("Artist:Name"),
                           QStringLiteral("Demo Album")));

    BatchAudioConverterService service;
    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setNamingPolicy(QStringLiteral("artist-title"));
    service.setSourceFiles(QStringList{taggedPath, fallbackPath});

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("outputFile")).toString(),
             QDir(outputDir).filePath(QStringLiteral("Artist_Name - Signal_Noise (converted).wav")));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("outputFile")).toString(),
             QDir(outputDir).filePath(QStringLiteral("fallback (converted).wav")));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("previewDiagnostics")).toMap()
                 .value(QStringLiteral("appliedNamingPolicy")).toString(),
             QStringLiteral("artist-title"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("previewDiagnostics")).toMap()
                 .value(QStringLiteral("usedFallback")).toBool(),
             true);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
}

void BatchAudioConverterServiceTest::supportsAlbumTrackTitleNamingPolicyWithFallback()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString taggedPath = tempDir.filePath(QStringLiteral("album-tagged.wav"));
    const QString fallbackPath = tempDir.filePath(QStringLiteral("album-fallback.wav"));
    writeSilentWavFile(taggedPath);
    writeSilentWavFile(fallbackPath);
    QVERIFY(writeBasicTags(taggedPath,
                           QStringLiteral("Track:One"),
                           QStringLiteral("Artist"),
                           QStringLiteral("Album/Name"),
                           3));

    BatchAudioConverterService service;
    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setNamingPolicy(QStringLiteral("album-track-title"));
    service.setSourceFiles(QStringList{taggedPath, fallbackPath});

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("outputFile")).toString(),
             QDir(outputDir).filePath(QStringLiteral("Album_Name - 03 - Track_One (converted).wav")));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("previewDiagnostics")).toMap()
                 .value(QStringLiteral("appliedNamingPolicy")).toString(),
             QStringLiteral("album-track-title"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("previewDiagnostics")).toMap()
                 .value(QStringLiteral("usedFallback")).toBool(),
             true);
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("previewDiagnostics")).toMap()
                 .value(QStringLiteral("missingMetadataFields")).toList().size(),
             3);
}

void BatchAudioConverterServiceTest::appliesExplicitConflictPoliciesInPreview()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));
    const QString firstDir = tempDir.filePath(QStringLiteral("a"));
    const QString secondDir = tempDir.filePath(QStringLiteral("b"));
    QVERIFY(QDir().mkpath(firstDir));
    QVERIFY(QDir().mkpath(secondDir));
    const QString firstPath = QDir(firstDir).filePath(QStringLiteral("same.wav"));
    const QString secondPath = QDir(secondDir).filePath(QStringLiteral("same.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);

    QFile existingOutput(QDir(outputDir).filePath(QStringLiteral("same (converted).wav")));
    QVERIFY(existingOutput.open(QIODevice::WriteOnly));
    existingOutput.close();

    BatchAudioConverterService overwriteService;
    overwriteService.setOutputDirectory(outputDir);
    overwriteService.setFormat(QStringLiteral("wav"));
    overwriteService.setConflictPolicy(QStringLiteral("overwrite-if-allowed"));
    overwriteService.setSourceFiles(QStringList{firstPath});
    const QVariantMap overwriteItem = overwriteService.items().constFirst().toMap();
    QCOMPARE(overwriteItem.value(QStringLiteral("outputFile")).toString(),
             existingOutput.fileName());
    QCOMPARE(overwriteItem.value(QStringLiteral("conflictResolution")).toMap()
                 .value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("overwrite-existing"));
    QCOMPARE(overwriteItem.value(QStringLiteral("conflictResolution")).toMap()
                 .value(QStringLiteral("willOverwriteExisting")).toBool(),
             true);

    BatchAudioConverterService skipService;
    skipService.setOutputDirectory(outputDir);
    skipService.setFormat(QStringLiteral("wav"));
    skipService.setConflictPolicy(QStringLiteral("skip-on-conflict"));
    skipService.setSourceFiles(QStringList{firstPath});
    const QVariantMap skipItem = skipService.items().constFirst().toMap();
    QCOMPARE(skipItem.value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(skipItem.value(QStringLiteral("conflictResolution")).toMap()
                 .value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("skip-existing-conflict"));

    BatchAudioConverterService failService;
    failService.setOutputDirectory(outputDir);
    failService.setFormat(QStringLiteral("wav"));
    failService.setConflictPolicy(QStringLiteral("fail-on-conflict"));
    failService.setSourceFiles(QStringList{firstPath});
    const QVariantMap failItem = failService.items().constFirst().toMap();
    QCOMPARE(failItem.value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QCOMPARE(failItem.value(QStringLiteral("failureType")).toString(), QStringLiteral("output-conflict"));

    BatchAudioConverterService queueConflictService;
    queueConflictService.setOutputDirectory(outputDir);
    queueConflictService.setFormat(QStringLiteral("wav"));
    queueConflictService.setConflictPolicy(QStringLiteral("overwrite-if-allowed"));
    queueConflictService.setSourceFiles(QStringList{firstPath, secondPath});
    const QVariantList queueItems = queueConflictService.items();
    QCOMPARE(queueItems.size(), 2);
    QCOMPARE(queueItems.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QCOMPARE(queueItems.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QCOMPARE(queueItems.at(1).toMap().value(QStringLiteral("conflictResolution")).toMap()
                 .value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("overwrite-blocked-queue-conflict"));
}

void BatchAudioConverterServiceTest::runsSequentialBatchConversion()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath, 44100, 2, 400);
    writeSilentWavFile(secondPath, 44100, 2, 600);

    BatchAudioConverterService service;
    QSignalSpy startedSpy(&service, &BatchAudioConverterService::batchStarted);
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);
    QSignalSpy canceledSpy(&service, &BatchAudioConverterService::batchCanceled);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{firstPath, secondPath});

    QVERIFY(service.startBatch());
    QCOMPARE(startedSpy.count(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);
    QCOMPARE(canceledSpy.count(), 0);
    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.succeededCount(), 2);
    QCOMPARE(service.failedCount(), 0);
    QCOMPARE(service.canceledCount(), 0);
    QCOMPARE(service.skippedCount(), 0);
    QCOMPARE(service.batchProgress(), 1.0);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    for (const QVariant &itemValue : items) {
        const QVariantMap item = itemValue.toMap();
        QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
        QVERIFY(QFileInfo::exists(item.value(QStringLiteral("resultFile")).toString()));
        QVERIFY(item.value(QStringLiteral("outputFile")).toString().startsWith(outputDir));
        const QVariantList attempts = item.value(QStringLiteral("reportMetadata")).toMap()
                                          .value(QStringLiteral("attempts")).toList();
        QCOMPARE(attempts.size(), 1);
        QCOMPARE(attempts.constFirst().toMap().value(QStringLiteral("terminalResult")).toString(),
                 QStringLiteral("succeeded"));
    }

    const QVariantMap diagnostics = service.runtimeDiagnostics();
    QCOMPARE(diagnostics.value(QStringLiteral("executionMode")).toString(),
             QStringLiteral("sequential-single-worker"));
    QCOMPARE(diagnostics.value(QStringLiteral("maxConcurrentJobsObserved")).toInt(), 1);
    QCOMPARE(diagnostics.value(QStringLiteral("measuredItemCount")).toInt(), 2);
    QVERIFY(diagnostics.value(QStringLiteral("resultBytesMeasured")).toLongLong() > 0);
    QCOMPARE(diagnostics.value(QStringLiteral("peakTempFileCountObserved")).toInt(), 1);

    const QVariantMap decision = service.parallelismDecision();
    QCOMPARE(decision.value(QStringLiteral("decisionKey")).toString(),
             QStringLiteral("sequential-default-safe"));
    QCOMPARE(decision.value(QStringLiteral("semantics")).toMap()
                 .value(QStringLiteral("playlistInsertOrdering")).toString(),
             QStringLiteral("queue-order-on-success"));

    const QVariantMap report = service.currentReport();
    QCOMPARE(report.value(QStringLiteral("parallelismDecision")).toMap()
                 .value(QStringLiteral("decisionKey")).toString(),
             QStringLiteral("sequential-default-safe"));
    QCOMPARE(report.value(QStringLiteral("runtimeDiagnostics")).toMap()
                 .value(QStringLiteral("measuredItemCount")).toInt(),
             2);
    const QVariantList reportItems = report.value(QStringLiteral("items")).toList();
    QCOMPARE(reportItems.size(), 2);
    QCOMPARE(reportItems.at(0).toMap().value(QStringLiteral("sourceFile")).toString(), firstPath);
    QCOMPARE(reportItems.at(1).toMap().value(QStringLiteral("sourceFile")).toString(), secondPath);
    QVERIFY(service.currentReportText(QStringLiteral("txt")).contains(QStringLiteral("Parallelism decision")));
}

void BatchAudioConverterServiceTest::v1SimpleBatchFlowStillWorksUnchanged()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString firstPath = tempDir.filePath(QStringLiteral("playlist-first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("playlist-second.wav"));
    writeSilentWavFile(firstPath, 44100, 2, 300);
    writeSilentWavFile(secondPath, 44100, 2, 300);

    BatchAudioConverterService service;
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);
    QSignalSpy playlistSpy(&service, &BatchAudioConverterService::playlistResultReady);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    const QVariantMap intakeSummary = service.replaceSourceFilesFromVariantList(
        QVariantList{firstPath, secondPath},
        QStringLiteral("playlist-selection"));
    QCOMPARE(intakeSummary.value(QStringLiteral("acceptedCount")).toInt(), 2);

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);

    QCOMPARE(service.hasFinished(), true);
    QCOMPARE(service.wasCanceled(), false);
    QCOMPARE(service.succeededCount(), 2);
    QCOMPARE(service.failedCount(), 0);
    QCOMPARE(service.skippedCount(), 0);
    QCOMPARE(playlistSpy.count(), 2);
    QCOMPARE(playlistSpy.at(0).at(0).toString(),
             QDir(outputDir).filePath(QStringLiteral("playlist-first (converted).wav")));
    QCOMPARE(playlistSpy.at(1).at(0).toString(),
             QDir(outputDir).filePath(QStringLiteral("playlist-second (converted).wav")));
    QCOMPARE(service.items().at(0).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("playlist-selection"));
    QCOMPARE(service.items().at(1).toMap().value(QStringLiteral("sourceOriginType")).toString(),
             QStringLiteral("playlist-selection"));
    QCOMPARE(service.settings().value(QStringLiteral("playlistAddMode")).toString(),
             QStringLiteral("immediate"));
}

void BatchAudioConverterServiceTest::emitsPlaylistResultsForSucceededItemsInOrder()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString validFirst = tempDir.filePath(QStringLiteral("ok-first.wav"));
    const QString missingMiddle = tempDir.filePath(QStringLiteral("missing-middle.wav"));
    const QString validLast = tempDir.filePath(QStringLiteral("ok-last.wav"));
    writeSilentWavFile(validFirst, 44100, 2, 350);
    writeSilentWavFile(validLast, 44100, 2, 350);

    BatchAudioConverterService service;
    QSignalSpy playlistSpy(&service, &BatchAudioConverterService::playlistResultReady);
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{validFirst, missingMiddle, validLast});

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);
    QCOMPARE(playlistSpy.count(), 2);

    const QString firstOutput = playlistSpy.at(0).at(0).toString();
    const QString secondOutput = playlistSpy.at(1).at(0).toString();
    QCOMPARE(firstOutput, QDir(outputDir).filePath(QStringLiteral("ok-first (converted).wav")));
    QCOMPARE(secondOutput, QDir(outputDir).filePath(QStringLiteral("ok-last (converted).wav")));
}

void BatchAudioConverterServiceTest::defersPlaylistResultsUntilExplicitAction()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath, 44100, 2, 300);
    writeSilentWavFile(secondPath, 44100, 2, 300);

    BatchAudioConverterService service;
    QSignalSpy playlistSpy(&service, &BatchAudioConverterService::playlistResultReady);
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setPlaylistAddMode(QStringLiteral("deferred"));
    service.setSourceFiles(QStringList{firstPath, secondPath});

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);
    QCOMPARE(playlistSpy.count(), 0);
    QVERIFY(service.canAddSucceededResultsToPlaylist());
    QCOMPARE(service.addSucceededResultsToPlaylist(), 2);
    QCOMPARE(playlistSpy.count(), 2);
    QVERIFY(!service.canAddSucceededResultsToPlaylist());
    QCOMPARE(service.addSucceededResultsToPlaylist(), 0);
    QCOMPARE(service.settings().value(QStringLiteral("playlistAddMode")).toString(),
             QStringLiteral("deferred"));
}

void BatchAudioConverterServiceTest::suppressesPlaylistResultsWhenDisabled()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    writeSilentWavFile(firstPath, 44100, 2, 300);
    writeSilentWavFile(secondPath, 44100, 2, 300);

    BatchAudioConverterService service;
    QSignalSpy playlistSpy(&service, &BatchAudioConverterService::playlistResultReady);
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setAddResultsToPlaylist(false);
    service.setSourceFiles(QStringList{firstPath, secondPath});

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);
    QCOMPARE(service.succeededCount(), 2);
    QCOMPARE(playlistSpy.count(), 0);
}

void BatchAudioConverterServiceTest::rejectsBatchStartOnFatalValidationFailure()
{
    BatchAudioConverterService service;
    QSignalSpy startedSpy(&service, &BatchAudioConverterService::batchStarted);

    service.setSourceFiles(QStringList{QStringLiteral("https://example.com/not-local.mp3")});

    QVERIFY(!service.startBatch());
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.hasFinished(), false);
    QCOMPARE(service.wasCanceled(), false);
    QCOMPARE(service.finalSummary(), QVariantMap());
    QVERIFY(service.statusText().contains(QStringLiteral("no supported items"), Qt::CaseInsensitive));
    QVERIFY(service.lastError().contains(QStringLiteral("no supported items"), Qt::CaseInsensitive));

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("skipped"));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("failureType")).toString(),
             QStringLiteral("validation"));
    QVERIFY(!items.at(0).toMap().value(QStringLiteral("errorText")).toString().isEmpty());
}

void BatchAudioConverterServiceTest::preservesSummaryAfterPartialFailure()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString validFirst = tempDir.filePath(QStringLiteral("ok-first.wav"));
    const QString missingMiddle = tempDir.filePath(QStringLiteral("missing-middle.wav"));
    const QString validLast = tempDir.filePath(QStringLiteral("ok-last.wav"));
    writeSilentWavFile(validFirst, 44100, 2, 350);
    writeSilentWavFile(validLast, 44100, 2, 350);

    BatchAudioConverterService service;
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{validFirst, missingMiddle, validLast});

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);

    QCOMPARE(service.hasFinished(), true);
    QCOMPARE(service.wasCanceled(), false);
    QVERIFY(service.statusText().contains(QStringLiteral("issues"), Qt::CaseInsensitive));
    QVERIFY(!service.lastError().isEmpty());
    QVERIFY(service.lastError().contains(QStringLiteral("missing-middle"), Qt::CaseInsensitive)
            || service.lastError().contains(QStringLiteral("No such file"), Qt::CaseInsensitive));

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("totalCount")).toInt(), 3);
    QCOMPARE(summary.value(QStringLiteral("completedCount")).toInt(), 3);
    QCOMPARE(summary.value(QStringLiteral("succeededCount")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("failedCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("canceledCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("skippedCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("wasCanceled")).toBool(), false);
    QCOMPARE(summary.value(QStringLiteral("hasFailures")).toBool(), true);
    QCOMPARE(summary.value(QStringLiteral("hasSkips")).toBool(), false);
    QCOMPARE(summary.value(QStringLiteral("statusText")).toString(), service.statusText());
    QCOMPARE(summary.value(QStringLiteral("lastError")).toString(), service.lastError());
    QVERIFY(!summary.value(QStringLiteral("jobId")).toString().isEmpty());
    QVERIFY(summary.value(QStringLiteral("createdAtMs")).toLongLong() > 0);
    QVERIFY(summary.value(QStringLiteral("startedAtMs")).toLongLong() > 0);
    QVERIFY(summary.value(QStringLiteral("finishedAtMs")).toLongLong() > 0);
}

void BatchAudioConverterServiceTest::aggregatesBatchProgressAcrossMixedItemStates()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstPath = tempDir.filePath(QStringLiteral("first.wav"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.wav"));
    const QString thirdPath = tempDir.filePath(QStringLiteral("third.wav"));
    writeSilentWavFile(firstPath);
    writeSilentWavFile(secondPath);
    writeSilentWavFile(thirdPath);

    BatchAudioConverterService service;
    service.setSourceFiles(QStringList{firstPath, secondPath, thirdPath});

    QVERIFY(qAbs(service.batchProgress() - 0.0) < 0.000001);

    QVERIFY(service.setItemState(0, BatchAudioConverterService::Succeeded));
    QVERIFY(qAbs(service.batchProgress() - (1.0 / 3.0)) < 0.000001);

    QVERIFY(service.setItemState(1, BatchAudioConverterService::Running));
    QVERIFY(service.setItemProgress(1, 0.25));
    QVERIFY(qAbs(service.batchProgress() - (1.25 / 3.0)) < 0.000001);

    QVERIFY(service.setItemState(2, BatchAudioConverterService::Skipped));
    QVERIFY(qAbs(service.batchProgress() - (2.25 / 3.0)) < 0.000001);

    QVERIFY(service.setItemState(1, BatchAudioConverterService::Failed));
    QVERIFY(qAbs(service.batchProgress() - 1.0) < 0.000001);
}

void BatchAudioConverterServiceTest::continuesAfterPerItemFailure()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString validFirst = tempDir.filePath(QStringLiteral("ok-first.wav"));
    const QString missingMiddle = tempDir.filePath(QStringLiteral("missing-middle.wav"));
    const QString validLast = tempDir.filePath(QStringLiteral("ok-last.wav"));
    writeSilentWavFile(validFirst, 44100, 2, 350);
    writeSilentWavFile(validLast, 44100, 2, 350);

    BatchAudioConverterService service;
    QSignalSpy finishedSpy(&service, &BatchAudioConverterService::batchFinished);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setSourceFiles(QStringList{validFirst, missingMiddle, validLast});

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 15000);

    QCOMPARE(service.succeededCount(), 2);
    QCOMPARE(service.failedCount(), 1);
    QCOMPARE(service.canceledCount(), 0);
    QCOMPARE(service.skippedCount(), 0);
    QCOMPARE(service.batchProgress(), 1.0);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QVERIFY(!items.at(1).toMap().value(QStringLiteral("errorText")).toString().isEmpty());
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
}

void BatchAudioConverterServiceTest::cancelsDuringSecondItemAndKeepsFirstResult()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString firstSource = tempDir.filePath(QStringLiteral("first-short.wav"));
    const QString secondSource = tempDir.filePath(QStringLiteral("second-long.wav"));
    const QString queuedSource = tempDir.filePath(QStringLiteral("queued.wav"));
    writeSilentWavFile(firstSource, 44100, 2, 200);
    writeSilentWavFile(secondSource, 44100, 2, 60000);
    writeSilentWavFile(queuedSource, 44100, 2, 1000);

    BatchAudioConverterService service;
    QSignalSpy canceledSpy(&service, &BatchAudioConverterService::batchCanceled);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setPlaybackRate(0.25);
    service.setSourceFiles(QStringList{firstSource, secondSource, queuedSource});

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(service.succeededCount(), 1, 15000);
    QTRY_COMPARE_WITH_TIMEOUT(service.currentIndex(), 1, 3000);

    service.cancelBatch();
    QTRY_COMPARE_WITH_TIMEOUT(canceledSpy.count(), 1, 15000);

    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.wasCanceled(), true);
    QCOMPARE(service.succeededCount(), 1);
    QVERIFY(service.canceledCount() >= 2);
    QCOMPARE(service.pendingCount(), 0);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QVERIFY(QFileInfo::exists(items.at(0).toMap().value(QStringLiteral("resultFile")).toString()));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("canceled"));
    QCOMPARE(items.at(2).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("canceled"));
}

void BatchAudioConverterServiceTest::cancelStopsCurrentAndRemainingItems()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    const QString longSource = tempDir.filePath(QStringLiteral("long.wav"));
    const QString queuedSource = tempDir.filePath(QStringLiteral("queued.wav"));
    writeSilentWavFile(longSource, 44100, 2, 60000);
    writeSilentWavFile(queuedSource, 44100, 2, 1000);

    BatchAudioConverterService service;
    QSignalSpy startedSpy(&service, &BatchAudioConverterService::batchStarted);
    QSignalSpy canceledSpy(&service, &BatchAudioConverterService::batchCanceled);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setPlaybackRate(0.25);
    service.setSourceFiles(QStringList{longSource, queuedSource});

    QVERIFY(service.startBatch());
    QCOMPARE(startedSpy.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(service.currentIndex() >= 0, 3000);
    service.cancelBatch();
    QTRY_COMPARE_WITH_TIMEOUT(canceledSpy.count(), 1, 15000);

    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.hasFinished(), true);
    QCOMPARE(service.wasCanceled(), true);
    QVERIFY(service.canceledCount() >= 1);
    QCOMPARE(service.pendingCount(), 0);
    QVERIFY(service.statusText().contains(QStringLiteral("canceled"), Qt::CaseInsensitive));

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("totalCount")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("completedCount")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("wasCanceled")).toBool(), true);
    QCOMPARE(summary.value(QStringLiteral("canceledCount")).toInt(), service.canceledCount());

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    bool foundCanceled = false;
    for (const QVariant &itemValue : items) {
        const QString state = itemValue.toMap().value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("canceled")) {
            foundCanceled = true;
        }
    }
    QVERIFY(foundCanceled);
}

void BatchAudioConverterServiceTest::cancelsLongQueueAndMarksPendingTail()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");
    const QString outputDir = tempDir.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(outputDir));

    QStringList sources;
    const QString longSource = tempDir.filePath(QStringLiteral("long.wav"));
    writeSilentWavFile(longSource, 44100, 2, 60000);
    sources.push_back(longSource);

    const int pendingTailCount = 50;
    for (int i = 0; i < pendingTailCount; ++i) {
        const QString path = tempDir.filePath(QStringLiteral("queued-%1.wav").arg(i, 2, 10, QLatin1Char('0')));
        writeSilentWavFile(path, 44100, 2, 100);
        sources.push_back(path);
    }

    BatchAudioConverterService service;
    QSignalSpy canceledSpy(&service, &BatchAudioConverterService::batchCanceled);

    service.setOutputDirectory(outputDir);
    service.setFormat(QStringLiteral("wav"));
    service.setPlaybackRate(0.25);
    service.setSourceFiles(sources);

    QVERIFY(service.startBatch());
    QTRY_COMPARE_WITH_TIMEOUT(service.currentIndex(), 0, 3000);
    QCOMPARE(service.pendingCount(), pendingTailCount);

    service.cancelBatch();
    QTRY_COMPARE_WITH_TIMEOUT(canceledSpy.count(), 1, 15000);

    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.hasFinished(), true);
    QCOMPARE(service.wasCanceled(), true);
    QCOMPARE(service.pendingCount(), 0);
    QCOMPARE(service.canceledCount(), pendingTailCount + 1);
    QCOMPARE(service.finalSummary().value(QStringLiteral("totalCount")).toInt(), pendingTailCount + 1);
    QCOMPARE(service.finalSummary().value(QStringLiteral("canceledCount")).toInt(), pendingTailCount + 1);
}

QTEST_GUILESS_MAIN(BatchAudioConverterServiceTest)
#include "tst_BatchAudioConverterService.moc"
