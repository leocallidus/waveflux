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

QTEST_GUILESS_MAIN(PlaylistProfilesManagerTest)

#include "tst_PlaylistProfilesManager.moc"
