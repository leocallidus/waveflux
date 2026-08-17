#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QVariantMap>

#include "PlaylistProfilesManager.h"

namespace {
QString playlistStoragePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("playlist_profiles.json"));
}

void removeAppData()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataPath.isEmpty()) {
        QDir(appDataPath).removeRecursively();
    }
}

QVariantList sampleSnapshot()
{
    QVariantMap track;
    track.insert(QStringLiteral("filePath"), QStringLiteral("/tmp/test.flac"));
    track.insert(QStringLiteral("title"), QStringLiteral("Test Track"));
    return {track};
}
} // namespace

class PlaylistProfilesManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void resetForFullApplicationReset_removesStorageAndClearsLoadedProfiles();
    void updatePlaylist_updatesTracksOrderAndPersistsCorrectly();
};

void PlaylistProfilesManagerTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("WaveFluxTest"));
    QCoreApplication::setApplicationName(QStringLiteral("WaveFluxPlaylistProfilesManagerTest"));
}

void PlaylistProfilesManagerTest::init()
{
    removeAppData();
}

void PlaylistProfilesManagerTest::cleanup()
{
    removeAppData();
}

void PlaylistProfilesManagerTest::resetForFullApplicationReset_removesStorageAndClearsLoadedProfiles()
{
    PlaylistProfilesManager manager;
    QSignalSpy playlistsChangedSpy(&manager, &PlaylistProfilesManager::playlistsChanged);

    const int playlistId = manager.savePlaylist(QStringLiteral("Saved Playlist"), sampleSnapshot(), 0);
    QVERIFY(playlistId > 0);
    QCOMPARE(manager.listPlaylists().size(), 1);
    QVERIFY(QFile::exists(playlistStoragePath()));

    QVERIFY(manager.resetForFullApplicationReset());
    QCOMPARE(manager.listPlaylists().size(), 0);
    QVERIFY(!QFile::exists(playlistStoragePath()));
    QVERIFY(playlistsChangedSpy.count() >= 2);

    PlaylistProfilesManager reloadedManager;
    QCOMPARE(reloadedManager.listPlaylists().size(), 0);
}

void PlaylistProfilesManagerTest::updatePlaylist_updatesTracksOrderAndPersistsCorrectly()
{
    PlaylistProfilesManager manager;

    QVariantMap trackA;
    trackA.insert(QStringLiteral("filePath"), QStringLiteral("/music/trackA.flac"));
    trackA.insert(QStringLiteral("title"), QStringLiteral("Track A"));

    QVariantMap trackB;
    trackB.insert(QStringLiteral("filePath"), QStringLiteral("/music/trackB.flac"));
    trackB.insert(QStringLiteral("title"), QStringLiteral("Track B"));

    QVariantMap trackC;
    trackC.insert(QStringLiteral("filePath"), QStringLiteral("/music/trackC.flac"));
    trackC.insert(QStringLiteral("title"), QStringLiteral("Track C"));

    const QVariantList initialTracks = {trackA, trackB, trackC};
    const int playlistId = manager.savePlaylist(QStringLiteral("My Playlist"), initialTracks, 0);
    QVERIFY(playlistId > 0);

    // Update with swapped tracks: [trackC, trackA, trackB]
    const QVariantList updatedTracks = {trackC, trackA, trackB};
    QVERIFY(manager.updatePlaylist(playlistId, QStringLiteral("My Renamed Playlist"), updatedTracks, 2));

    // Reload playlist
    const QVariantMap payload = manager.loadPlaylist(playlistId);
    QCOMPARE(payload.value(QStringLiteral("name")).toString(), QStringLiteral("My Renamed Playlist"));
    const QVariantList reloadedTracks = payload.value(QStringLiteral("tracks")).toList();
    QCOMPARE(reloadedTracks.size(), 3);
    QCOMPARE(reloadedTracks.at(0).toMap().value(QStringLiteral("filePath")).toString(), QStringLiteral("/music/trackC.flac"));
    QCOMPARE(reloadedTracks.at(1).toMap().value(QStringLiteral("filePath")).toString(), QStringLiteral("/music/trackA.flac"));
    QCOMPARE(reloadedTracks.at(2).toMap().value(QStringLiteral("filePath")).toString(), QStringLiteral("/music/trackB.flac"));
    QCOMPARE(payload.value(QStringLiteral("currentIndex")).toInt(), 2);

    // Verify persistence across fresh manager instance
    PlaylistProfilesManager freshManager;
    const QVariantMap freshPayload = freshManager.loadPlaylist(playlistId);
    const QVariantList freshTracks = freshPayload.value(QStringLiteral("tracks")).toList();
    QCOMPARE(freshTracks.size(), 3);
    QCOMPARE(freshTracks.at(0).toMap().value(QStringLiteral("filePath")).toString(), QStringLiteral("/music/trackC.flac"));
    QCOMPARE(freshTracks.at(1).toMap().value(QStringLiteral("filePath")).toString(), QStringLiteral("/music/trackA.flac"));
    QCOMPARE(freshTracks.at(2).toMap().value(QStringLiteral("filePath")).toString(), QStringLiteral("/music/trackB.flac"));
}

QTEST_GUILESS_MAIN(PlaylistProfilesManagerTest)

#include "tst_PlaylistProfilesManager.moc"
