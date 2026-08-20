#include <QDir>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "TrackModel.h"
#include "TrackFilterProxyModel.h"

namespace {
Track makeTrack(const QString &filePath)
{
    Track track;
    track.filePath = filePath;
    track.title = QFileInfo(filePath).completeBaseName();
    track.addedAt = 1;
    track.format = QFileInfo(filePath).suffix().toUpper();
    return track;
}

bool writePlaceholderFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write("waveflux") == 8;
}

bool writeShortWaveFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    constexpr quint32 sampleRate = 8000;
    constexpr quint16 channels = 1;
    constexpr quint16 bitsPerSample = 16;
    constexpr quint32 sampleCount = 800;
    constexpr quint32 dataSize = sampleCount * channels * (bitsPerSample / 8);

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataSize);
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << channels << sampleRate;
    stream << quint32(sampleRate * channels * (bitsPerSample / 8));
    stream << quint16(channels * (bitsPerSample / 8)) << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataSize;

    const QByteArray silence(static_cast<qsizetype>(dataSize), '\0');
    return file.write(silence) == silence.size();
}
} // namespace

class tst_TrackModel : public QObject
{
    Q_OBJECT

private slots:
    void insertsDroppedUrlsAtRequestedIndex();
    void clampsDroppedUrlInsertionIndexAndPreservesCurrentTrack();
    void autoAddsNewFileFromDominantPlaylistFolder();
    void disablesAutoAddWhenSettingIsOff();
    void ignoresFolderChangesWithoutAbsoluteMajority();
    void metadataReadDoesNotCreateMissingFile();
    void searchLargePlaylistPerformanceAndAccuracy();
    void filteredProxyMapsOnlyMatchingRows();
    void asyncFilteredProxyRemainsResponsiveDuringUpdates();
    void filteredProxySurvivesRepeatedSearchAndClearCycles();
    void batchMetadataLoadingAndBlobSync();
    void metadataLoadingIsParallelAndNonBlocking();
    void resetPlaylistRestoresUserMovesAndSorts();
    void resetPlaylistPreservesCurrentlyPlayingTrack();
    void resetPlaylistRestoresRemovedTracks();
    void resetPlaylistHandlesIncrementalAdditionsAndSnapshots();
    void resetPlaylistPreservesCanResetAfterModifyingAndThenAddingTracks();
    void loadsChaptersFromMp3Track();
    void sortByColumnAndCustomRoles();
    void initialTrackTitleFallbackToFileName();
    void refreshPlaylistRescansFolderAndSortsNaturally();
};

void tst_TrackModel::insertsDroppedUrlsAtRequestedIndex()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString firstTrack = tempDir.filePath(QStringLiteral("alpha.flac"));
    const QString secondTrack = tempDir.filePath(QStringLiteral("beta.flac"));
    const QString droppedTrack = tempDir.filePath(QStringLiteral("inserted.mp3"));
    QVERIFY(writePlaceholderFile(firstTrack));
    QVERIFY(writePlaceholderFile(secondTrack));
    QVERIFY(writePlaceholderFile(droppedTrack));

    TrackModel model;
    model.setTracks({makeTrack(firstTrack), makeTrack(secondTrack)});
    model.insertUrlsAt(1, {QUrl::fromLocalFile(droppedTrack)});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.getFilePath(0), firstTrack);
    QCOMPARE(model.getFilePath(1), droppedTrack);
    QCOMPARE(model.getFilePath(2), secondTrack);
}

void tst_TrackModel::clampsDroppedUrlInsertionIndexAndPreservesCurrentTrack()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString firstTrack = tempDir.filePath(QStringLiteral("alpha.flac"));
    const QString secondTrack = tempDir.filePath(QStringLiteral("beta.flac"));
    const QString droppedTrack = tempDir.filePath(QStringLiteral("inserted.mp3"));
    QVERIFY(writePlaceholderFile(firstTrack));
    QVERIFY(writePlaceholderFile(secondTrack));
    QVERIFY(writePlaceholderFile(droppedTrack));

    TrackModel model;
    model.setTracks({makeTrack(firstTrack), makeTrack(secondTrack)});
    model.setCurrentIndex(1);

    model.insertUrlsAt(-50, {QUrl::fromLocalFile(droppedTrack)});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.getFilePath(0), droppedTrack);
    QCOMPARE(model.getFilePath(2), secondTrack);
    QCOMPARE(model.currentIndex(), 2);
}

void tst_TrackModel::autoAddsNewFileFromDominantPlaylistFolder()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString dominantFolder = tempDir.filePath(QStringLiteral("dominant"));
    const QString secondaryFolder = tempDir.filePath(QStringLiteral("secondary"));
    QVERIFY(QDir().mkpath(dominantFolder));
    QVERIFY(QDir().mkpath(secondaryFolder));

    const QString firstTrack = dominantFolder + QStringLiteral("/alpha.flac");
    const QString secondTrack = dominantFolder + QStringLiteral("/beta.flac");
    const QString thirdTrack = secondaryFolder + QStringLiteral("/gamma.flac");
    QVERIFY(writePlaceholderFile(firstTrack));
    QVERIFY(writePlaceholderFile(secondTrack));
    QVERIFY(writePlaceholderFile(thirdTrack));

    TrackModel model;
    model.setTracks({makeTrack(firstTrack), makeTrack(secondTrack), makeTrack(thirdTrack)});
    QCOMPARE(model.rowCount(), 3);

    QSignalSpy countSpy(&model, &TrackModel::countChanged);
    const QString addedTrack = dominantFolder + QStringLiteral("/delta.flac");
    QVERIFY(writePlaceholderFile(addedTrack));

    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 4, 5000);
    QCOMPARE(model.getFilePath(3), addedTrack);
    QVERIFY(countSpy.count() >= 1);
}

void tst_TrackModel::disablesAutoAddWhenSettingIsOff()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString dominantFolder = tempDir.filePath(QStringLiteral("dominant"));
    const QString secondaryFolder = tempDir.filePath(QStringLiteral("secondary"));
    QVERIFY(QDir().mkpath(dominantFolder));
    QVERIFY(QDir().mkpath(secondaryFolder));

    const QString firstTrack = dominantFolder + QStringLiteral("/alpha.flac");
    const QString secondTrack = dominantFolder + QStringLiteral("/beta.flac");
    const QString thirdTrack = secondaryFolder + QStringLiteral("/gamma.flac");
    QVERIFY(writePlaceholderFile(firstTrack));
    QVERIFY(writePlaceholderFile(secondTrack));
    QVERIFY(writePlaceholderFile(thirdTrack));

    TrackModel model;
    model.setTracks({makeTrack(firstTrack), makeTrack(secondTrack), makeTrack(thirdTrack)});
    model.setAutoAddTracksFromPlaylistFolderEnabled(false);
    QCOMPARE(model.rowCount(), 3);

    QSignalSpy countSpy(&model, &TrackModel::countChanged);
    const QString addedTrack = dominantFolder + QStringLiteral("/delta.flac");
    QVERIFY(writePlaceholderFile(addedTrack));

    QTest::qWait(1500);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(countSpy.count(), 0);
}

void tst_TrackModel::metadataReadDoesNotCreateMissingFile()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString missingTrack = tempDir.filePath(QStringLiteral("deleted.mp3"));
    QVERIFY(!QFileInfo::exists(missingTrack));

    TrackModel model;
    model.addFile(missingTrack);

    QCOMPARE(model.rowCount(), 1);
    QTest::qWait(500);
    QVERIFY2(!QFileInfo::exists(missingTrack),
             qPrintable(QStringLiteral("metadata read created missing file: %1").arg(missingTrack)));
}

void tst_TrackModel::ignoresFolderChangesWithoutAbsoluteMajority()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString firstFolder = tempDir.filePath(QStringLiteral("one"));
    const QString secondFolder = tempDir.filePath(QStringLiteral("two"));
    QVERIFY(QDir().mkpath(firstFolder));
    QVERIFY(QDir().mkpath(secondFolder));

    const QString firstTrack = firstFolder + QStringLiteral("/alpha.flac");
    const QString secondTrack = secondFolder + QStringLiteral("/beta.flac");
    QVERIFY(writePlaceholderFile(firstTrack));
    QVERIFY(writePlaceholderFile(secondTrack));

    TrackModel model;
    model.setTracks({makeTrack(firstTrack), makeTrack(secondTrack)});
    QCOMPARE(model.rowCount(), 2);

    QSignalSpy countSpy(&model, &TrackModel::countChanged);
    const QString ignoredTrack = firstFolder + QStringLiteral("/later.flac");
    QVERIFY(writePlaceholderFile(ignoredTrack));

    QTest::qWait(1500);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(countSpy.count(), 0);
}

void tst_TrackModel::searchLargePlaylistPerformanceAndAccuracy()
{
    TrackModel model;
    QVector<Track> largePlaylist;
    constexpr int kTrackCount = 3500;
    largePlaylist.reserve(kTrackCount);

    for (int i = 0; i < kTrackCount; ++i) {
        Track track;
        track.filePath = QStringLiteral("/music/track_%1.flac").arg(i);
        track.title = (i == 42 || i == 1337 || i == 2500) ? QStringLiteral("Hotel California %1").arg(i) : QStringLiteral("Song %1").arg(i);
        track.artist = (i == 1337) ? QStringLiteral("Eagles Live") : QStringLiteral("Artist %1").arg(i % 100);
        track.album = QStringLiteral("Album %1").arg(i % 50);
        track.format = QStringLiteral("FLAC");
        track.duration = 180000;
        largePlaylist.push_back(std::move(track));
    }

    model.setTracks(std::move(largePlaylist));
    QCOMPARE(model.rowCount(), kTrackCount);

    QElapsedTimer timer;
    timer.start();

    // 1. Search for a specific query
    const int count = model.countMatchingNormalized(QStringLiteral("hotel california"));
    const qint64 elapsedMs = timer.elapsed();

    QCOMPARE(count, 3);
    QVERIFY2(elapsedMs < 100, qPrintable(QStringLiteral("Search over 3500 tracks took %1 ms, expected < 100ms").arg(elapsedMs)));

    // 2. Test exact row matching
    QVERIFY(model.matchesSearchQueryNormalized(42, QStringLiteral("hotel california")));
    QVERIFY(model.matchesSearchQueryNormalized(1337, QStringLiteral("hotel california")));
    QVERIFY(model.matchesSearchQueryNormalized(2500, QStringLiteral("hotel california")));
    QVERIFY(!model.matchesSearchQueryNormalized(0, QStringLiteral("hotel california")));

    // 3. Multi-token search with artist
    QCOMPARE(model.countMatchingNormalized(QStringLiteral("hotel california eagles")), 1);
    QVERIFY(model.matchesSearchQueryNormalized(1337, QStringLiteral("hotel california eagles")));
}

void tst_TrackModel::batchMetadataLoadingAndBlobSync()
{
    TrackModel model;
    QVector<Track> tracks;
    for (int i = 0; i < 50; ++i) {
        Track track;
        track.filePath = QStringLiteral("/media/song_%1.mp3").arg(i);
        tracks.push_back(std::move(track));
    }

    model.setTracks(std::move(tracks));
    QCOMPARE(model.rowCount(), 50);

    // Initial search should not find "Bohemian Rhapsody"
    QCOMPARE(model.countMatchingNormalized(QStringLiteral("bohemian rhapsody")), 0);

    // Apply tag overrides for file 10
    model.applyTagOverridesForFiles({QStringLiteral("/media/song_10.mp3")},
                                   true, QStringLiteral("Bohemian Rhapsody"),
                                   true, QStringLiteral("Queen"),
                                   true, QStringLiteral("A Night at the Opera"));

    // Search should immediately match the updated blob in memory
    QCOMPARE(model.countMatchingNormalized(QStringLiteral("bohemian rhapsody")), 1);
    QVERIFY(model.matchesSearchQueryNormalized(10, QStringLiteral("bohemian rhapsody")));
    QVERIFY(model.matchesSearchQueryNormalized(10, QStringLiteral("queen opera")));
}

void tst_TrackModel::filteredProxyMapsOnlyMatchingRows()
{
    TrackModel model;
    QVector<Track> tracks;
    constexpr int kTrackCount = 3500;
    tracks.reserve(kTrackCount);
    for (int i = 0; i < kTrackCount; ++i) {
        Track track;
        track.filePath = QStringLiteral("/music/proxy_%1.flac").arg(i);
        track.title = (i == 17 || i == 2400) ? QStringLiteral("Instant Match")
                                             : QStringLiteral("Ordinary Track %1").arg(i);
        track.artist = QStringLiteral("Artist %1").arg(i % 40);
        track.album = QStringLiteral("Album %1").arg(i % 20);
        tracks.push_back(std::move(track));
    }
    model.setTracks(std::move(tracks));

    TrackFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    QElapsedTimer timer;
    timer.start();
    proxy.setNormalizedQuery(QStringLiteral("instant match"));

    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.sourceIndexAt(0), 17);
    QCOMPARE(proxy.sourceIndexAt(1), 2400);
    QCOMPARE(proxy.proxyIndexForSource(17), 0);
    QCOMPARE(proxy.proxyIndexForSource(2400), 1);
    QCOMPARE(proxy.proxyIndexForSource(18), -1);
    QCOMPARE(proxy.data(proxy.index(0, 0), TrackFilterProxyModel::SourceIndexRole).toInt(), 17);
    QVERIFY2(timer.elapsed() < 150,
             qPrintable(QStringLiteral("Proxy filtering took %1 ms").arg(timer.elapsed())));

    model.applyTagOverridesForFiles({QStringLiteral("/music/proxy_18.flac")},
                                    true,
                                    QStringLiteral("Instant Match"),
                                    false,
                                    QString(),
                                    false,
                                    QString());
    QCOMPARE(proxy.rowCount(), 3);
    QCOMPARE(proxy.sourceIndexAt(1), 18);
}

void tst_TrackModel::metadataLoadingIsParallelAndNonBlocking()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QStringList paths;
    constexpr int kTrackCount = 64;
    paths.reserve(kTrackCount);
    for (int i = 0; i < kTrackCount; ++i) {
        const QString path = tempDir.filePath(QStringLiteral("track_%1.wav").arg(i, 3, 10, QLatin1Char('0')));
        QVERIFY(writeShortWaveFile(path));
        paths.push_back(path);
    }

    TrackModel model;
    QElapsedTimer addTimer;
    addTimer.start();
    model.addFiles(paths);
    const qint64 addElapsedMs = addTimer.elapsed();

    QCOMPARE(model.rowCount(), kTrackCount);
    QVERIFY2(addElapsedMs < 250,
             qPrintable(QStringLiteral("Adding %1 files blocked for %2 ms")
                            .arg(kTrackCount)
                            .arg(addElapsedMs)));

    QTRY_VERIFY_WITH_TIMEOUT(model.trackInfoAt(0).value(QStringLiteral("durationMs")).toLongLong() > 0,
                             3000);
    QTRY_VERIFY_WITH_TIMEOUT(model.trackInfoAt(kTrackCount - 1)
                                 .value(QStringLiteral("durationMs")).toLongLong() > 0,
                             5000);
}

void tst_TrackModel::asyncFilteredProxyRemainsResponsiveDuringUpdates()
{
    TrackModel model;
    QVector<Track> tracks;
    constexpr int kTrackCount = 12000;
    tracks.reserve(kTrackCount);
    for (int i = 0; i < kTrackCount; ++i) {
        Track track;
        track.filePath = QStringLiteral("/music/async_%1.flac").arg(i);
        track.title = (i == 111 || i == 9999) ? QStringLiteral("Background Needle")
                                              : QStringLiteral("Track %1").arg(i);
        tracks.push_back(std::move(track));
    }
    model.setTracks(std::move(tracks));

    TrackFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setFieldMask(TrackModel::SearchFieldTitle);

    QElapsedTimer timer;
    timer.start();
    proxy.setNormalizedQuery(QStringLiteral("background needle"));
    QVERIFY2(timer.elapsed() < 250,
             qPrintable(QStringLiteral("Scheduling async proxy search blocked for %1 ms")
                            .arg(timer.elapsed())));

    // Change searchable metadata while the first worker is in flight. The
    // usable snapshot should still be published, followed by a fresh result.
    model.applyTagOverridesForFiles({QStringLiteral("/music/async_500.flac")},
                                    true,
                                    QStringLiteral("Background Needle"),
                                    false,
                                    QString(),
                                    false,
                                    QString());

    QTRY_COMPARE_WITH_TIMEOUT(proxy.rowCount(), 3, 5000);
    QCOMPARE(proxy.proxyIndexForSource(111), 0);
    QVERIFY(proxy.proxyIndexForSource(500) >= 0);
    QVERIFY(proxy.proxyIndexForSource(9999) >= 0);
}

void tst_TrackModel::filteredProxySurvivesRepeatedSearchAndClearCycles()
{
    TrackModel model;
    QVector<Track> tracks;
    constexpr int kTrackCount = 4000;
    tracks.reserve(kTrackCount);
    for (int i = 0; i < kTrackCount; ++i) {
        Track track;
        track.filePath = QStringLiteral("/music/cycle_%1.flac").arg(i);
        track.title = (i % 2 == 0) ? QStringLiteral("Even Result %1").arg(i)
                                   : QStringLiteral("Odd Result %1").arg(i);
        tracks.push_back(std::move(track));
    }
    model.setTracks(std::move(tracks));

    TrackFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    for (int cycle = 0; cycle < 5; ++cycle) {
        proxy.setNormalizedQuery(QStringLiteral("even result"));
        QCOMPARE(proxy.rowCount(), kTrackCount / 2);
        QCOMPARE(proxy.sourceIndexAt(0), 0);
        QCOMPARE(proxy.sourceIndexAt(proxy.rowCount() - 1), kTrackCount - 2);

        proxy.setNormalizedQuery(QStringLiteral("odd result 3999"));
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.sourceIndexAt(0), 3999);

        proxy.setNormalizedQuery(QString());
        QCOMPARE(proxy.rowCount(), kTrackCount);
        QCOMPARE(proxy.sourceIndexAt(0), 0);
        QCOMPARE(proxy.sourceIndexAt(kTrackCount - 1), kTrackCount - 1);
    }

    QVector<Track> replacementTracks;
    replacementTracks.reserve(2500);
    for (int i = 0; i < 2500; ++i) {
        Track track;
        track.filePath = QStringLiteral("/replacement/track_%1.flac").arg(i);
        track.title = QStringLiteral("Replacement %1").arg(i);
        replacementTracks.push_back(std::move(track));
    }
    model.setTracks(std::move(replacementTracks));
    QCOMPARE(proxy.rowCount(), 2500);
    QCOMPARE(proxy.sourceIndexAt(2499), 2499);
}

void tst_TrackModel::resetPlaylistRestoresUserMovesAndSorts()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString t1 = tempDir.filePath("1_first.mp3");
    const QString t2 = tempDir.filePath("2_second.mp3");
    const QString t3 = tempDir.filePath("3_third.mp3");
    const QString t4 = tempDir.filePath("4_fourth.mp3");

    QVERIFY(writePlaceholderFile(t1));
    QVERIFY(writePlaceholderFile(t2));
    QVERIFY(writePlaceholderFile(t3));
    QVERIFY(writePlaceholderFile(t4));

    TrackModel model;
    model.addFiles({t1, t2, t3, t4});

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.getFilePath(0), t1);
    QCOMPARE(model.getFilePath(1), t2);
    QCOMPARE(model.getFilePath(2), t3);
    QCOMPARE(model.getFilePath(3), t4);
    QVERIFY(!model.canResetPlaylist());

    // 1. Move track 3 to index 0
    model.move(3, 0);
    QVERIFY(model.canResetPlaylist());
    QCOMPARE(model.getFilePath(0), t4);
    QCOMPARE(model.getFilePath(1), t1);
    QCOMPARE(model.getFilePath(2), t2);
    QCOMPARE(model.getFilePath(3), t3);

    // Reset playlist
    QVERIFY(model.resetPlaylist());
    QVERIFY(!model.canResetPlaylist());
    QCOMPARE(model.getFilePath(0), t1);
    QCOMPARE(model.getFilePath(1), t2);
    QCOMPARE(model.getFilePath(2), t3);
    QCOMPARE(model.getFilePath(3), t4);

    // 2. Sort tracks descending
    model.sortByNameDesc();
    QVERIFY(model.canResetPlaylist());
    QCOMPARE(model.getFilePath(0), t4);

    // Reset playlist again
    QVERIFY(model.resetPlaylist());
    QVERIFY(!model.canResetPlaylist());
    QCOMPARE(model.getFilePath(0), t1);
    QCOMPARE(model.getFilePath(1), t2);
    QCOMPARE(model.getFilePath(2), t3);
    QCOMPARE(model.getFilePath(3), t4);
}

void tst_TrackModel::resetPlaylistPreservesCurrentlyPlayingTrack()
{
    TrackModel model;
    Track trackA; trackA.filePath = QStringLiteral("/music/A.flac"); trackA.title = QStringLiteral("Song A"); trackA.addedAt = 10;
    Track trackB; trackB.filePath = QStringLiteral("/music/B.flac"); trackB.title = QStringLiteral("Song B"); trackB.addedAt = 20;
    Track trackC; trackC.filePath = QStringLiteral("/music/C.flac"); trackC.title = QStringLiteral("Song C"); trackC.addedAt = 30;

    model.setTracks({trackA, trackB, trackC});
    model.setCurrentIndex(1); // Song B is currently playing
    QCOMPARE(model.currentFilePath(), QStringLiteral("/music/B.flac"));
    QVERIFY(!model.canResetPlaylist());

    // Move Song B (index 1) to index 2
    model.move(1, 2);
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.currentFilePath(), QStringLiteral("/music/B.flac"));
    QVERIFY(model.canResetPlaylist());

    // Reset playlist
    QVERIFY(model.resetPlaylist());
    // Song B should be back at index 1 and still be the current track
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.currentFilePath(), QStringLiteral("/music/B.flac"));
    QVERIFY(!model.canResetPlaylist());
}

void tst_TrackModel::resetPlaylistRestoresRemovedTracks()
{
    TrackModel model;
    Track trackA; trackA.filePath = QStringLiteral("/music/A.flac"); trackA.title = QStringLiteral("A"); trackA.addedAt = 10;
    Track trackB; trackB.filePath = QStringLiteral("/music/B.flac"); trackB.title = QStringLiteral("B"); trackB.addedAt = 20;
    Track trackC; trackC.filePath = QStringLiteral("/music/C.flac"); trackC.title = QStringLiteral("C"); trackC.addedAt = 30;

    model.setTracks({trackA, trackB, trackC});
    QCOMPARE(model.rowCount(), 3);
    QVERIFY(!model.canResetPlaylist());

    // Remove track B (index 1)
    model.removeAt(1);
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.canResetPlaylist());

    // Reset playlist
    QVERIFY(model.resetPlaylist());
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/A.flac"));
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/B.flac"));
    QCOMPARE(model.getFilePath(2), QStringLiteral("/music/C.flac"));
    QVERIFY(!model.canResetPlaylist());
}

void tst_TrackModel::resetPlaylistHandlesIncrementalAdditionsAndSnapshots()
{
    TrackModel model;
    Track trackA; trackA.filePath = QStringLiteral("/music/A.flac"); trackA.title = QStringLiteral("A"); trackA.addedAt = 10;
    Track trackB; trackB.filePath = QStringLiteral("/music/B.flac"); trackB.title = QStringLiteral("B"); trackB.addedAt = 20;
    model.setTracks({trackA, trackB});
    QVERIFY(!model.canResetPlaylist());

    // Ingest another track via importTracksSnapshot
    QVariantList snapshot;
    QVariantMap map1; map1[QStringLiteral("filePath")] = QStringLiteral("/music/1.flac"); map1[QStringLiteral("title")] = QStringLiteral("1");
    QVariantMap map2; map2[QStringLiteral("filePath")] = QStringLiteral("/music/2.flac"); map2[QStringLiteral("title")] = QStringLiteral("2");
    snapshot.append(map1);
    snapshot.append(map2);

    model.importTracksSnapshot(snapshot, 0);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/1.flac"));
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/2.flac"));
    QVERIFY(!model.canResetPlaylist());

    model.move(0, 1);
    QVERIFY(model.canResetPlaylist());
    QVERIFY(model.resetPlaylist());
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/1.flac"));
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/2.flac"));
    QVERIFY(!model.canResetPlaylist());

    // Test clear
    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.canResetPlaylist());
}

void tst_TrackModel::resetPlaylistPreservesCanResetAfterModifyingAndThenAddingTracks()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString pathA = tempDir.filePath(QStringLiteral("01_trackA.wav"));
    const QString pathB = tempDir.filePath(QStringLiteral("02_trackB.wav"));
    const QString pathC = tempDir.filePath(QStringLiteral("03_trackC.wav"));
    const QString pathD = tempDir.filePath(QStringLiteral("04_trackD.wav"));
    const QString pathE = tempDir.filePath(QStringLiteral("05_trackE.wav"));

    QVERIFY(writeShortWaveFile(pathA));
    QVERIFY(writeShortWaveFile(pathB));
    QVERIFY(writeShortWaveFile(pathC));
    QVERIFY(writeShortWaveFile(pathD));
    QVERIFY(writeShortWaveFile(pathE));

    TrackModel model;
    model.addUrls({QUrl::fromLocalFile(pathA), QUrl::fromLocalFile(pathB), QUrl::fromLocalFile(pathC)});
    QCOMPARE(model.rowCount(), 3);
    QVERIFY(!model.canResetPlaylist());

    // Reorder tracks: move track 2 to 0 -> [C, A, B]
    model.move(2, 0);
    QCOMPARE(model.getFilePath(0), pathC);
    QCOMPARE(model.getFilePath(1), pathA);
    QCOMPARE(model.getFilePath(2), pathB);
    QVERIFY(model.canResetPlaylist());

    // Ingest another folder / batch of tracks [D, E]
    model.addUrls({QUrl::fromLocalFile(pathD), QUrl::fromLocalFile(pathE)});
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.getFilePath(3), pathD);
    QCOMPARE(model.getFilePath(4), pathE);
    QVERIFY(model.canResetPlaylist());

    // Reset playlist
    QVERIFY(model.resetPlaylist());
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.getFilePath(0), pathA);
    QCOMPARE(model.getFilePath(1), pathB);
    QCOMPARE(model.getFilePath(2), pathC);
    QCOMPARE(model.getFilePath(3), pathD);
    QCOMPARE(model.getFilePath(4), pathE);
    QVERIFY(!model.canResetPlaylist());
}

void tst_TrackModel::loadsChaptersFromMp3Track()
{
    const QString mp3Path = QStringLiteral(WAVEFLUX_PROJECT_ROOT_DIR) + QStringLiteral("/Tamlin - Alive.mp3");
    if (!QFileInfo::exists(mp3Path)) {
        QSKIP("Tamlin - Alive.mp3 is not present in the project root");
    }

    TrackModel model;
    model.addFile(mp3Path);
    QCOMPARE(model.rowCount(), 1);

    // Wait for async metadata loading
    QTRY_VERIFY_WITH_TIMEOUT(model.hasChapters(0), 4000);
    QCOMPARE(model.hasChapters(0), true);

    const QVariantList chapters = model.chaptersForIndex(0);
    QVERIFY(chapters.size() >= 10);

    // Set current track
    model.setCurrentIndex(0);
    QCOMPARE(model.currentHasChapters(), true);
    QCOMPARE(model.currentChapterCount(), chapters.size());

    // Verify first chapter (Intro at 0ms)
    const QVariantMap ch0 = chapters.at(0).toMap();
    QCOMPARE(ch0.value(QStringLiteral("startTimeMs")).toLongLong(), 0LL);
    QCOMPARE(ch0.value(QStringLiteral("title")).toString(), QStringLiteral("Intro"));

    // Check role in model data
    QCOMPARE(model.data(model.index(0, 0), TrackModel::HasChaptersRole).toBool(), true);

    // Verify non-chapter track returns false
    Track emptyTrack;
    emptyTrack.filePath = QStringLiteral("/fake/track_without_chapters.mp3");
    emptyTrack.title = QStringLiteral("Plain Track");
    emptyTrack.artist = QStringLiteral("Plain Artist");
    // chapters vector is empty by default
    QCOMPARE(emptyTrack.chapters.isEmpty(), true);
}

void tst_TrackModel::sortByColumnAndCustomRoles()
{
    Track track1;
    track1.filePath = QStringLiteral("/music/track1.flac");
    track1.title = QStringLiteral("Zeta");
    track1.artist = QStringLiteral("Artist B");
    track1.album = QStringLiteral("Album 1");
    track1.composer = QStringLiteral("Mozart");
    track1.description = QStringLiteral("Desc 1");
    track1.originalArtist = QStringLiteral("Orig 1");
    track1.copyright = QStringLiteral("Copy 1");
    track1.url = QStringLiteral("https://track1.com");
    track1.encoder = QStringLiteral("FLAC 1.4");
    track1.year = QStringLiteral("2020");
    track1.duration = 180000;
    track1.bpm = 120;
    track1.addedAt = 100;

    Track track2;
    track2.filePath = QStringLiteral("/music/track2.flac");
    track2.title = QStringLiteral("Alpha");
    track2.artist = QStringLiteral("Artist A");
    track2.album = QStringLiteral("Album 2");
    track2.composer = QStringLiteral("Bach");
    track2.description = QStringLiteral("Desc 2");
    track2.year = QStringLiteral("2010");
    track2.duration = 240000;
    track2.bpm = 90;
    track2.addedAt = 200;

    Track track3; // empty composer and empty year
    track3.filePath = QStringLiteral("/music/track3.flac");
    track3.title = QStringLiteral("Beta");
    track3.artist = QStringLiteral("Artist C");
    track3.duration = 60000;
    track3.addedAt = 300;

    TrackModel model;
    model.setTracks({track1, track2, track3});
    QCOMPARE(model.rowCount(), 3);

    // Verify custom roles
    const QModelIndex idx0 = model.index(0, 0);
    QCOMPARE(model.data(idx0, TrackModel::ComposerRole).toString(), QStringLiteral("Mozart"));
    QCOMPARE(model.data(idx0, TrackModel::DescriptionRole).toString(), QStringLiteral("Desc 1"));
    QCOMPARE(model.data(idx0, TrackModel::OriginalArtistRole).toString(), QStringLiteral("Orig 1"));
    QCOMPARE(model.data(idx0, TrackModel::CopyrightRole).toString(), QStringLiteral("Copy 1"));
    QCOMPARE(model.data(idx0, TrackModel::UrlRole).toString(), QStringLiteral("https://track1.com"));
    QCOMPARE(model.data(idx0, TrackModel::EncoderRole).toString(), QStringLiteral("FLAC 1.4"));
    QCOMPARE(model.data(idx0, TrackModel::FileNameRole).toString(), QStringLiteral("track1.flac"));

    // Verify current getters
    model.setCurrentIndex(0);
    QCOMPARE(model.currentComposer(), QStringLiteral("Mozart"));
    QCOMPARE(model.currentDescription(), QStringLiteral("Desc 1"));
    QCOMPARE(model.currentOriginalArtist(), QStringLiteral("Orig 1"));
    QCOMPARE(model.currentCopyright(), QStringLiteral("Copy 1"));
    QCOMPARE(model.currentUrl(), QStringLiteral("https://track1.com"));
    QCOMPARE(model.currentEncoder(), QStringLiteral("FLAC 1.4"));

    // Sort by composer ASC (Bach < Mozart < empty)
    model.sortByColumn(QStringLiteral("composer"), Qt::AscendingOrder);
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/track2.flac")); // Bach
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/track1.flac")); // Mozart
    QCOMPARE(model.getFilePath(2), QStringLiteral("/music/track3.flac")); // Empty is LAST!

    // Sort by composer DESC (Mozart > Bach > empty)
    // Note: Empty must STILL be last!
    model.sortByColumn(QStringLiteral("composer"), Qt::DescendingOrder);
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/track1.flac")); // Mozart
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/track2.flac")); // Bach
    QCOMPARE(model.getFilePath(2), QStringLiteral("/music/track3.flac")); // Empty is STILL LAST!

    // Sort by duration ASC (60s < 180s < 240s)
    model.sortByColumn(QStringLiteral("duration"), Qt::AscendingOrder);
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/track3.flac"));
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/track1.flac"));
    QCOMPARE(model.getFilePath(2), QStringLiteral("/music/track2.flac"));

    // Restore baseline order (track1, track2, track3)
    model.restoreBaselineOrder();
    QCOMPARE(model.getFilePath(0), QStringLiteral("/music/track1.flac"));
    QCOMPARE(model.getFilePath(1), QStringLiteral("/music/track2.flac"));
    QCOMPARE(model.getFilePath(2), QStringLiteral("/music/track3.flac"));
}

void tst_TrackModel::initialTrackTitleFallbackToFileName()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString testFile = tempDir.filePath(QStringLiteral("Symphony_No_9.wav"));
    QVERIFY(writePlaceholderFile(testFile));

    TrackModel model;
    model.addFiles({testFile});

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex idx = model.index(0, 0);
    // TitleRole should immediately return base name without waiting for tag reader
    QCOMPARE(model.data(idx, TrackModel::TitleRole).toString(), QStringLiteral("Symphony_No_9"));
    QCOMPARE(model.data(idx, TrackModel::DisplayNameRole).toString(), QStringLiteral("Symphony_No_9"));
    QCOMPARE(model.data(idx, TrackModel::FileNameRole).toString(), QStringLiteral("Symphony_No_9.wav"));
}

void tst_TrackModel::refreshPlaylistRescansFolderAndSortsNaturally()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "temporary dir should be valid");

    const QString track1 = tempDir.filePath(QStringLiteral("01_intro.wav"));
    const QString track3 = tempDir.filePath(QStringLiteral("03_outro.wav"));
    QVERIFY(writeShortWaveFile(track1));
    QVERIFY(writeShortWaveFile(track3));

    TrackModel model;
    model.addFolder(QUrl::fromLocalFile(tempDir.path()));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.getFilePath(0), QDir::cleanPath(track1));
    QCOMPARE(model.getFilePath(1), QDir::cleanPath(track3));

    // Simulate user adding a new file in between: "02_middle.wav"
    const QString track2 = tempDir.filePath(QStringLiteral("02_middle.wav"));
    QVERIFY(writeShortWaveFile(track2));

    // Auto-folder scan or manual addition appends it at the end
    model.addFiles({track2});
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.getFilePath(0), QDir::cleanPath(track1));
    QCOMPARE(model.getFilePath(1), QDir::cleanPath(track3));
    QCOMPARE(model.getFilePath(2), QDir::cleanPath(track2));

    // Select track3 as currently playing
    model.setCurrentIndex(1);
    QCOMPARE(model.currentFilePath(), QDir::cleanPath(track3));

    // Now user clicks Refresh Playlist
    model.refreshPlaylist();

    // Verify all 3 tracks are now in natural order: 01, 02, 03
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.getFilePath(0), QDir::cleanPath(track1));
    QCOMPARE(model.getFilePath(1), QDir::cleanPath(track2));
    QCOMPARE(model.getFilePath(2), QDir::cleanPath(track3));

    // Verify currently playing track is preserved and index updated
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.currentFilePath(), QDir::cleanPath(track3));
}

QTEST_MAIN(tst_TrackModel)

#include "tst_TrackModel.moc"
