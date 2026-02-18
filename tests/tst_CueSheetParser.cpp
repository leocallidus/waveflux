#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "CueSheetParser.h"

class CueSheetParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesTracksAndBoundaries();
    void failsWhenCueHasNoPlayableTracks();
    void usesIndex00WhenIndex01IsMissing();
    void ignoresNonAudioTracksButUsesTheirBoundary();
    void parsesRemAlbumAndSongwriterFallbacks();
    void keepsBoundariesWithinSameSourceFile();
    void reportsLineForInvalidIndexTime();
};

void CueSheetParserTest::parsesTracksAndBoundaries()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString audioPath = tempDir.filePath(QStringLiteral("album.flac"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("dummy");
    audioFile.close();

    const QString cuePath = tempDir.filePath(QStringLiteral("album.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "PERFORMER \"Artist\"\n"
        "TITLE \"Album\"\n"
        "FILE \"album.flac\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    TITLE \"Intro\"\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 02 AUDIO\n"
        "    TITLE \"Main\"\n"
        "    INDEX 01 01:15:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY2(CueSheetParser::parseFile(cuePath, &segments, &error), qPrintable(error));
    QCOMPARE(segments.size(), 2);

    QCOMPARE(segments.at(0).sourceFilePath, audioPath);
    QCOMPARE(segments.at(0).title, QStringLiteral("Intro"));
    QCOMPARE(segments.at(0).performer, QStringLiteral("Artist"));
    QCOMPARE(segments.at(0).album, QStringLiteral("Album"));
    QCOMPARE(segments.at(0).startMs, 0);
    QCOMPARE(segments.at(0).endMs, 75000);

    QCOMPARE(segments.at(1).title, QStringLiteral("Main"));
    QCOMPARE(segments.at(1).startMs, 75000);
    QCOMPARE(segments.at(1).endMs, static_cast<qint64>(-1));
}

void CueSheetParserTest::failsWhenCueHasNoPlayableTracks()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString cuePath = tempDir.filePath(QStringLiteral("broken.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "PERFORMER \"Artist\"\n"
        "TITLE \"Album\"\n"
        "FILE \"missing.flac\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY(!CueSheetParser::parseFile(cuePath, &segments, &error));
    QVERIFY(!error.trimmed().isEmpty());
    QVERIFY(segments.isEmpty());
}

void CueSheetParserTest::usesIndex00WhenIndex01IsMissing()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString audioPath = tempDir.filePath(QStringLiteral("album.flac"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("dummy");
    audioFile.close();

    const QString cuePath = tempDir.filePath(QStringLiteral("index00.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "FILE \"album.flac\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    TITLE \"Hidden Intro\"\n"
        "    INDEX 00 00:00:00\n"
        "  TRACK 02 AUDIO\n"
        "    TITLE \"Main\"\n"
        "    INDEX 01 00:30:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY2(CueSheetParser::parseFile(cuePath, &segments, &error), qPrintable(error));
    QCOMPARE(segments.size(), 2);
    QCOMPARE(segments.at(0).title, QStringLiteral("Hidden Intro"));
    QCOMPARE(segments.at(0).startMs, static_cast<qint64>(0));
    QCOMPARE(segments.at(0).endMs, static_cast<qint64>(30000));
}

void CueSheetParserTest::ignoresNonAudioTracksButUsesTheirBoundary()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString audioPath = tempDir.filePath(QStringLiteral("disc.bin"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("dummy");
    audioFile.close();

    const QString cuePath = tempDir.filePath(QStringLiteral("mixed.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "FILE \"disc.bin\" BINARY\n"
        "  TRACK 01 AUDIO\n"
        "    TITLE \"Audio\"\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 02 MODE1/2352\n"
        "    INDEX 01 01:00:00\n"
        "  TRACK 03 AUDIO\n"
        "    TITLE \"Audio Two\"\n"
        "    INDEX 01 02:00:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY2(CueSheetParser::parseFile(cuePath, &segments, &error), qPrintable(error));
    QCOMPARE(segments.size(), 2);
    QCOMPARE(segments.at(0).trackNumber, 1);
    QCOMPARE(segments.at(0).endMs, static_cast<qint64>(60000));
    QCOMPARE(segments.at(1).trackNumber, 3);
    QCOMPARE(segments.at(1).startMs, static_cast<qint64>(120000));
}

void CueSheetParserTest::parsesRemAlbumAndSongwriterFallbacks()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString audioPath = tempDir.filePath(QStringLiteral("album.flac"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("dummy");
    audioFile.close();

    const QString cuePath = tempDir.filePath(QStringLiteral("fallbacks.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "REM ALBUM \"Rem Album\"\n"
        "SONGWRITER \"Composer\"\n"
        "FILE \"album.flac\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY2(CueSheetParser::parseFile(cuePath, &segments, &error), qPrintable(error));
    QCOMPARE(segments.size(), 1);
    QCOMPARE(segments.at(0).album, QStringLiteral("Rem Album"));
    QCOMPARE(segments.at(0).performer, QStringLiteral("Composer"));
    QCOMPARE(segments.at(0).title, QStringLiteral("Track 01"));
}

void CueSheetParserTest::keepsBoundariesWithinSameSourceFile()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString firstAudioPath = tempDir.filePath(QStringLiteral("disc1.flac"));
    QFile firstAudio(firstAudioPath);
    QVERIFY(firstAudio.open(QIODevice::WriteOnly));
    firstAudio.write("dummy");
    firstAudio.close();

    const QString secondAudioPath = tempDir.filePath(QStringLiteral("disc2.flac"));
    QFile secondAudio(secondAudioPath);
    QVERIFY(secondAudio.open(QIODevice::WriteOnly));
    secondAudio.write("dummy");
    secondAudio.close();

    const QString cuePath = tempDir.filePath(QStringLiteral("multi.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "FILE \"disc1.flac\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 02 AUDIO\n"
        "    INDEX 01 01:00:00\n"
        "FILE \"disc2.flac\" WAVE\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 04 AUDIO\n"
        "    INDEX 01 01:00:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY2(CueSheetParser::parseFile(cuePath, &segments, &error), qPrintable(error));
    QCOMPARE(segments.size(), 4);
    QCOMPARE(segments.at(0).sourceFilePath, firstAudioPath);
    QCOMPARE(segments.at(0).endMs, static_cast<qint64>(60000));
    QCOMPARE(segments.at(1).sourceFilePath, firstAudioPath);
    QCOMPARE(segments.at(1).endMs, static_cast<qint64>(-1));
    QCOMPARE(segments.at(2).sourceFilePath, secondAudioPath);
    QCOMPARE(segments.at(2).endMs, static_cast<qint64>(60000));
}

void CueSheetParserTest::reportsLineForInvalidIndexTime()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString audioPath = tempDir.filePath(QStringLiteral("album.flac"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("dummy");
    audioFile.close();

    const QString cuePath = tempDir.filePath(QStringLiteral("invalid-time.cue"));
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write(
        "FILE \"album.flac\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:61:00\n");
    cueFile.close();

    QVector<CueTrackSegment> segments;
    QString error;
    QVERIFY(!CueSheetParser::parseFile(cuePath, &segments, &error));
    QVERIFY(error.contains(QStringLiteral("line"), Qt::CaseInsensitive));
    QVERIFY(error.contains(QStringLiteral("3")));
    QVERIFY(segments.isEmpty());
}

QTEST_MAIN(CueSheetParserTest)
#include "tst_CueSheetParser.moc"
