#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "TrackModel.h"

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

QTEST_MAIN(tst_TrackModel)

#include "tst_TrackModel.moc"
