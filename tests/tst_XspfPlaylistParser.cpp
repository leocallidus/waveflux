#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest/QtTest>

#include "XspfPlaylistParser.h"

namespace {
bool writeTextFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    if (file.write(content) != content.size()) {
        return false;
    }

    return true;
}
} // namespace

class XspfPlaylistParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesHttpHttpsStreamsWithMetadata();
    void resolvesRelativePathsAgainstPlaylistDirectory();
    void resolvesFileUriToLocalPath();
    void skipsTrackWithoutLocation();
    void reportsFatalErrorForMalformedXml();
};

void XspfPlaylistParserTest::parsesHttpHttpsStreamsWithMetadata()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString xspfPath = tempDir.filePath(QStringLiteral("streams.xspf"));
    const QByteArray xspfContent =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<playlist xmlns=\"http://xspf.org/ns/0/\" xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\" version=\"1\">\n"
        "  <trackList>\n"
        "    <track>\n"
        "      <location>https://example.com/live.mp3</location>\n"
        "      <title>Live HTTPS</title>\n"
        "      <creator>Station A</creator>\n"
        "      <album>Online Radio</album>\n"
        "      <duration>12345</duration>\n"
        "      <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n"
        "        <vlc:id>0</vlc:id>\n"
        "      </extension>\n"
        "    </track>\n"
        "    <track>\n"
        "      <location>http://example.net/stream</location>\n"
        "      <title>Live HTTP</title>\n"
        "      <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n"
        "        <vlc:id>1</vlc:id>\n"
        "      </extension>\n"
        "    </track>\n"
        "  </trackList>\n"
        "</playlist>\n";
    QVERIFY2(writeTextFile(xspfPath, xspfContent), "failed to write xspf");

    QVector<XspfTrackEntry> entries;
    QStringList warnings;
    XspfParseError error;

    QVERIFY2(XspfPlaylistParser::parseFile(xspfPath, &entries, &warnings, &error), qPrintable(error.message));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    QCOMPARE(entries.size(), 2);

    QCOMPARE(entries.at(0).source, QStringLiteral("https://example.com/live.mp3"));
    QCOMPARE(entries.at(0).title, QStringLiteral("Live HTTPS"));
    QCOMPARE(entries.at(0).creator, QStringLiteral("Station A"));
    QCOMPARE(entries.at(0).album, QStringLiteral("Online Radio"));
    QCOMPARE(entries.at(0).durationMs, static_cast<qint64>(12345));

    QCOMPARE(entries.at(1).source, QStringLiteral("http://example.net/stream"));
    QCOMPARE(entries.at(1).title, QStringLiteral("Live HTTP"));
    QCOMPARE(entries.at(1).creator, QString());
    QCOMPARE(entries.at(1).album, QString());
    QCOMPARE(entries.at(1).durationMs, static_cast<qint64>(-1));
}

void XspfPlaylistParserTest::resolvesRelativePathsAgainstPlaylistDirectory()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString playlistDirPath = tempDir.filePath(QStringLiteral("playlists"));
    QVERIFY(QDir().mkpath(playlistDirPath));

    const QString xspfPath = QDir(playlistDirPath).filePath(QStringLiteral("relative_paths.xspf"));
    const QByteArray xspfContent =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<playlist xmlns=\"http://xspf.org/ns/0/\" version=\"1\">\n"
        "  <trackList>\n"
        "    <track>\n"
        "      <location>../music/album/song.flac</location>\n"
        "    </track>\n"
        "    <track>\n"
        "      <location>./folder/../voice.ogg</location>\n"
        "    </track>\n"
        "  </trackList>\n"
        "</playlist>\n";
    QVERIFY2(writeTextFile(xspfPath, xspfContent), "failed to write xspf");

    QVector<XspfTrackEntry> entries;
    QStringList warnings;
    XspfParseError error;
    QVERIFY2(XspfPlaylistParser::parseFile(xspfPath, &entries, &warnings, &error), qPrintable(error.message));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    QCOMPARE(entries.size(), 2);

    const QDir playlistDir(playlistDirPath);
    const QString expectedFirst = QDir::cleanPath(playlistDir.filePath(QStringLiteral("../music/album/song.flac")));
    const QString expectedSecond = QDir::cleanPath(playlistDir.filePath(QStringLiteral("./folder/../voice.ogg")));

    QCOMPARE(entries.at(0).source, expectedFirst);
    QCOMPARE(entries.at(1).source, expectedSecond);
}

void XspfPlaylistParserTest::resolvesFileUriToLocalPath()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString musicDirPath = tempDir.filePath(QStringLiteral("music"));
    QVERIFY(QDir().mkpath(musicDirPath));
    const QString localTrackPath = QDir(musicDirPath).filePath(QStringLiteral("My Track.flac"));

    QFile localTrack(localTrackPath);
    QVERIFY(localTrack.open(QIODevice::WriteOnly));
    QVERIFY(localTrack.write("dummy") > 0);
    localTrack.close();

    const QString xspfPath = tempDir.filePath(QStringLiteral("file_uri.xspf"));
    const QString fileUri = QUrl::fromLocalFile(localTrackPath).toString();
    const QByteArray xspfContent =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<playlist xmlns=\"http://xspf.org/ns/0/\" version=\"1\">\n"
        "  <trackList>\n"
        "    <track>\n"
        "      <location>" + fileUri.toUtf8() + "</location>\n"
        "    </track>\n"
        "  </trackList>\n"
        "</playlist>\n";
    QVERIFY2(writeTextFile(xspfPath, xspfContent), "failed to write xspf");

    QVector<XspfTrackEntry> entries;
    QStringList warnings;
    XspfParseError error;
    QVERIFY2(XspfPlaylistParser::parseFile(xspfPath, &entries, &warnings, &error), qPrintable(error.message));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).source, QDir::cleanPath(localTrackPath));
}

void XspfPlaylistParserTest::skipsTrackWithoutLocation()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString xspfPath = tempDir.filePath(QStringLiteral("missing_location.xspf"));
    const QByteArray xspfContent =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<playlist xmlns=\"http://xspf.org/ns/0/\" version=\"1\">\n"
        "  <trackList>\n"
        "    <track>\n"
        "      <title>No Location</title>\n"
        "    </track>\n"
        "    <track>\n"
        "      <location>https://example.org/radio</location>\n"
        "      <title>Valid Stream</title>\n"
        "    </track>\n"
        "  </trackList>\n"
        "</playlist>\n";
    QVERIFY2(writeTextFile(xspfPath, xspfContent), "failed to write xspf");

    QVector<XspfTrackEntry> entries;
    QStringList warnings;
    XspfParseError error;
    QVERIFY2(XspfPlaylistParser::parseFile(xspfPath, &entries, &warnings, &error), qPrintable(error.message));

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).source, QStringLiteral("https://example.org/radio"));
    QCOMPARE(entries.at(0).title, QStringLiteral("Valid Stream"));

    QVERIFY(!warnings.isEmpty());
    const QString allWarnings = warnings.join(QLatin1Char('\n'));
    QVERIFY(allWarnings.contains(QStringLiteral("location is missing"), Qt::CaseInsensitive));
}

void XspfPlaylistParserTest::reportsFatalErrorForMalformedXml()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString xspfPath = tempDir.filePath(QStringLiteral("broken_xml.xspf"));
    const QByteArray xspfContent =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<playlist xmlns=\"http://xspf.org/ns/0/\" version=\"1\">\n"
        "  <trackList>\n"
        "    <track>\n"
        "      <location>https://example.com/live</location>\n"
        "  </trackList>\n"
        "</playlist>\n";
    QVERIFY2(writeTextFile(xspfPath, xspfContent), "failed to write xspf");

    QVector<XspfTrackEntry> entries;
    QStringList warnings;
    XspfParseError error;
    QVERIFY(!XspfPlaylistParser::parseFile(xspfPath, &entries, &warnings, &error));
    QVERIFY(entries.isEmpty());
    QVERIFY(!error.message.trimmed().isEmpty());
    QVERIFY(error.line > 0);
    QVERIFY(error.column >= 0);
}

QTEST_MAIN(XspfPlaylistParserTest)
#include "tst_XspfPlaylistParser.moc"
