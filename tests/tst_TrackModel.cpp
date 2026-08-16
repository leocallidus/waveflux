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

QTEST_MAIN(tst_TrackModel)

#include "tst_TrackModel.moc"
