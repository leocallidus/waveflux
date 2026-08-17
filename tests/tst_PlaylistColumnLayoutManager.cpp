#include <QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "PlaylistColumnLayoutManager.h"
#include "AppSettingsManager.h"

class tst_PlaylistColumnLayoutManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void testCatalogCompleteness();
    void testDefaultLayouts();
    void testEffectiveVisibleColumnsNormal();
    void testEffectiveVisibleColumnsCompact();
    void testAllHiddenFallback();
    void testFormattingAll24Columns();
    void testPersistenceAndSanitization();
    void testCopyAndReset();
    void testUrlSchemeAllowed();
    void testColumnVisibilityAndReordering();
    void testWidthBucketAndCaching();
    void testIsColumnVisibleAndToggle();
};

void tst_PlaylistColumnLayoutManager::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("WaveFluxTest"));
    QCoreApplication::setApplicationName(QStringLiteral("WaveFluxTest"));
}

void tst_PlaylistColumnLayoutManager::cleanupTestCase()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void tst_PlaylistColumnLayoutManager::init()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void tst_PlaylistColumnLayoutManager::testCatalogCompleteness()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    const auto catalog = manager.catalog();
    QCOMPARE(catalog.size(), 24);

    const QStringList expectedIds = {
        QStringLiteral("playlistPosition"),
        QStringLiteral("trackSummary"),
        QStringLiteral("title"),
        QStringLiteral("artist"),
        QStringLiteral("album"),
        QStringLiteral("duration"),
        QStringLiteral("bitrate"),
        QStringLiteral("trackNumber"),
        QStringLiteral("year"),
        QStringLiteral("genre"),
        QStringLiteral("description"),
        QStringLiteral("composer"),
        QStringLiteral("originalArtist"),
        QStringLiteral("copyright"),
        QStringLiteral("url"),
        QStringLiteral("encoder"),
        QStringLiteral("format"),
        QStringLiteral("sampleRate"),
        QStringLiteral("bitDepth"),
        QStringLiteral("bpm"),
        QStringLiteral("channelCount"),
        QStringLiteral("fileName"),
        QStringLiteral("filePath"),
        QStringLiteral("dateAdded")
    };

    QSet<QString> seenIds;
    for (const auto &colVar : catalog) {
        const auto col = colVar.toMap();
        const QString id = col.value(QStringLiteral("id")).toString();
        QVERIFY(!id.isEmpty());
        QVERIFY(expectedIds.contains(id));
        QVERIFY(!seenIds.contains(id));
        seenIds.insert(id);

        const QString translationKey = col.value(QStringLiteral("translationKey")).toString();
        QVERIFY(!translationKey.isEmpty());
        QVERIFY(translationKey.startsWith(QStringLiteral("columns.")));

        const int defaultWidth = col.value(QStringLiteral("defaultWidth")).toInt();
        QVERIFY(defaultWidth >= 30);

        const int minWidth = col.value(QStringLiteral("minimumWidth")).toInt();
        QVERIFY(minWidth >= 20);

        const QString alignment = col.value(QStringLiteral("alignment")).toString();
        QVERIFY(alignment == QStringLiteral("left") || alignment == QStringLiteral("right") || alignment == QStringLiteral("center"));
    }

    QCOMPARE(seenIds.size(), 24);
}

void tst_PlaylistColumnLayoutManager::testDefaultLayouts()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    // Normal skin defaults
    const auto normalCols = manager.normalColumns();
    QCOMPARE(normalCols.size(), 24);

    QVERIFY(manager.hasVisibleColumns(QStringLiteral("normal")));
    QVERIFY(manager.hasVisibleColumns(QStringLiteral("compact")));

    // First 6 normal columns should be shown or auto
    const QStringList expectedNormalVisible = {
        QStringLiteral("playlistPosition"),
        QStringLiteral("title"),
        QStringLiteral("artist"),
        QStringLiteral("album"),
        QStringLiteral("duration"),
        QStringLiteral("bitrate")
    };

    for (int i = 0; i < expectedNormalVisible.size(); ++i) {
        const auto map = normalCols.at(i).toMap();
        QCOMPARE(map.value(QStringLiteral("id")).toString(), expectedNormalVisible.at(i));
        const QString vis = map.value(QStringLiteral("visibility")).toString();
        QVERIFY(vis == QStringLiteral("shown") || vis == QStringLiteral("automatic") || vis == QStringLiteral("auto"));
    }

    // Compact skin defaults: position and trackSummary
    const auto compactCols = manager.compactColumns();
    QCOMPARE(compactCols.size(), 24);

    const auto compactFirst = compactCols.at(0).toMap();
    QCOMPARE(compactFirst.value(QStringLiteral("id")).toString(), QStringLiteral("playlistPosition"));
    QCOMPARE(compactFirst.value(QStringLiteral("visibility")).toString(), QStringLiteral("shown"));

    const auto compactSecond = compactCols.at(1).toMap();
    QCOMPARE(compactSecond.value(QStringLiteral("id")).toString(), QStringLiteral("trackSummary"));
    QCOMPARE(compactSecond.value(QStringLiteral("visibility")).toString(), QStringLiteral("shown"));
}

void tst_PlaylistColumnLayoutManager::testEffectiveVisibleColumnsNormal()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    // Wide viewport: all 6 columns visible
    const auto wideCols = manager.effectiveVisibleColumns(QStringLiteral("normal"), 1400.0);
    QVERIFY(wideCols.size() >= 5);

    // Narrow viewport: drops low priority columns (e.g. bitrate, album)
    const auto narrowCols = manager.effectiveVisibleColumns(QStringLiteral("normal"), 350.0);
    QVERIFY(narrowCols.size() < wideCols.size());
    QVERIFY(narrowCols.size() >= 2);

    // Check width allocation: total width should approximately fill available
    double totalAllocated = 0.0;
    for (const auto &colVar : wideCols) {
        totalAllocated += colVar.toMap().value(QStringLiteral("width")).toDouble();
    }
    QVERIFY(totalAllocated <= 1400.0 + 1.0);
}

void tst_PlaylistColumnLayoutManager::testEffectiveVisibleColumnsCompact()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    const auto cols = manager.effectiveVisibleColumns(QStringLiteral("compact"), 300.0);
    QCOMPARE(cols.size(), 2);
    QCOMPARE(cols.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("playlistPosition"));
    QCOMPARE(cols.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("trackSummary"));
}

void tst_PlaylistColumnLayoutManager::testAllHiddenFallback()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    // Hide all normal columns
    const auto catalog = manager.catalog();
    for (const auto &c : catalog) {
        manager.setColumnVisibility(QStringLiteral("normal"), c.toMap().value(QStringLiteral("id")).toString(), QStringLiteral("hidden"));
    }

    QVERIFY(!manager.hasVisibleColumns(QStringLiteral("normal")));

    // When all hidden, effectiveVisibleColumns should return fallback trackSummary column
    const auto effective = manager.effectiveVisibleColumns(QStringLiteral("normal"), 800.0);
    QCOMPARE(effective.size(), 1);
    QCOMPARE(effective.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("trackSummary"));
}

void tst_PlaylistColumnLayoutManager::testFormattingAll24Columns()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    QVariantMap extra;
    extra.insert(QStringLiteral("artist"), QStringLiteral("Test Artist"));
    extra.insert(QStringLiteral("title"), QStringLiteral("Test Track"));
    extra.insert(QStringLiteral("format"), QStringLiteral("FLAC"));

    QCOMPARE(manager.formatValue(QStringLiteral("playlistPosition"), 1, extra), QStringLiteral("1"));
    QCOMPARE(manager.formatValue(QStringLiteral("playlistPosition"), 12, extra), QStringLiteral("12"));

    QCOMPARE(manager.formatValue(QStringLiteral("trackSummary"), QStringLiteral("Custom Summary"), extra), QStringLiteral("Custom Summary"));
    QCOMPARE(manager.formatValue(QStringLiteral("trackSummary"), QString(), extra), QStringLiteral("Test Artist - Test Track"));

    QCOMPARE(manager.formatValue(QStringLiteral("title"), QStringLiteral("My Song"), extra), QStringLiteral("My Song"));
    QCOMPARE(manager.formatValue(QStringLiteral("artist"), QStringLiteral("My Artist"), extra), QStringLiteral("My Artist"));
    QCOMPARE(manager.formatValue(QStringLiteral("album"), QStringLiteral("Greatest Hits"), extra), QStringLiteral("Greatest Hits"));

    // Duration ms -> mm:ss / hh:mm:ss
    QCOMPARE(manager.formatValue(QStringLiteral("duration"), 65000, extra), QStringLiteral("1:05"));
    QCOMPARE(manager.formatValue(QStringLiteral("duration"), 3665000, extra), QStringLiteral("1:01:05"));

    // Bitrate
    QCOMPARE(manager.formatValue(QStringLiteral("bitrate"), 320, extra), QStringLiteral("320 kbps"));

    // Track number
    QCOMPARE(manager.formatValue(QStringLiteral("trackNumber"), QStringLiteral("5"), extra), QStringLiteral("5"));
    QCOMPARE(manager.formatValue(QStringLiteral("trackNumber"), QStringLiteral("03/12"), extra), QStringLiteral("03/12"));

    // Year
    QCOMPARE(manager.formatValue(QStringLiteral("year"), QStringLiteral("2024"), extra), QStringLiteral("2024"));

    // Genre
    QCOMPARE(manager.formatValue(QStringLiteral("genre"), QStringLiteral("Electronic"), extra), QStringLiteral("Electronic"));

    // Description, Composer, Original Artist, Copyright, Encoder
    QCOMPARE(manager.formatValue(QStringLiteral("description"), QStringLiteral("A nice track"), extra), QStringLiteral("A nice track"));
    QCOMPARE(manager.formatValue(QStringLiteral("composer"), QStringLiteral("Bach"), extra), QStringLiteral("Bach"));
    QCOMPARE(manager.formatValue(QStringLiteral("originalArtist"), QStringLiteral("Original Guy"), extra), QStringLiteral("Original Guy"));
    QCOMPARE(manager.formatValue(QStringLiteral("copyright"), QStringLiteral("(C) 2024"), extra), QStringLiteral("(C) 2024"));
    QCOMPARE(manager.formatValue(QStringLiteral("encoder"), QStringLiteral("Lavf"), extra), QStringLiteral("Lavf"));

    // URL (must be valid web URL)
    QCOMPARE(manager.formatValue(QStringLiteral("url"), QStringLiteral("https://example.com/audio.mp3"), extra), QStringLiteral("https://example.com/audio.mp3"));
    QCOMPARE(manager.formatValue(QStringLiteral("url"), QStringLiteral("/home/user/file.mp3"), extra), QStringLiteral("")); // local path ignored for url column

    // Format
    QCOMPARE(manager.formatValue(QStringLiteral("format"), QStringLiteral("FLAC"), extra), QStringLiteral("FLAC"));

    // Sample rate
    QCOMPARE(manager.formatValue(QStringLiteral("sampleRate"), 44100, extra), QStringLiteral("44.1 kHz"));
    QCOMPARE(manager.formatValue(QStringLiteral("sampleRate"), 96000, extra), QStringLiteral("96 kHz"));

    // Bit depth
    QCOMPARE(manager.formatValue(QStringLiteral("bitDepth"), 24, extra), QStringLiteral("24-bit"));

    // BPM
    QCOMPARE(manager.formatValue(QStringLiteral("bpm"), 128, extra), QStringLiteral("128 BPM"));

    // Channel count
    QCOMPARE(manager.formatValue(QStringLiteral("channelCount"), 1, extra), QStringLiteral("Mono"));
    QCOMPARE(manager.formatValue(QStringLiteral("channelCount"), 2, extra), QStringLiteral("Stereo"));
    QCOMPARE(manager.formatValue(QStringLiteral("channelCount"), 6, extra), QStringLiteral("5.1"));

    // File name and path
    QCOMPARE(manager.formatValue(QStringLiteral("fileName"), QStringLiteral("track01.flac"), extra), QStringLiteral("track01.flac"));
    QCOMPARE(manager.formatValue(QStringLiteral("filePath"), QStringLiteral("/music/track01.flac"), extra), QStringLiteral("/music/track01.flac"));

    // Date added
    const qint64 testTimeMs = 1700000000000LL;
    const QString dateStr = manager.formatValue(QStringLiteral("dateAdded"), testTimeMs, extra);
    QVERIFY(!dateStr.isEmpty());
}

void tst_PlaylistColumnLayoutManager::testPersistenceAndSanitization()
{
    {
        AppSettingsManager settingsManager;
        PlaylistColumnLayoutManager manager(&settingsManager);

        manager.setColumnVisibility(QStringLiteral("normal"), QStringLiteral("composer"), QStringLiteral("shown"));
        manager.moveColumn(QStringLiteral("normal"), 0, 3);
        manager.setCompactHeaderMode(QStringLiteral("alwaysShown"));
    }

    // New instance should read back the persisted settings
    {
        AppSettingsManager settingsManager;
        PlaylistColumnLayoutManager manager(&settingsManager);

        QCOMPARE(manager.compactHeaderMode(), QStringLiteral("alwaysShown"));

        const auto normalCols = manager.normalColumns();
        bool foundComposer = false;
        for (const auto &c : normalCols) {
            const auto map = c.toMap();
            if (map.value(QStringLiteral("id")).toString() == QStringLiteral("composer")) {
                foundComposer = true;
                QCOMPARE(map.value(QStringLiteral("visibility")).toString(), QStringLiteral("shown"));
            }
        }
        QVERIFY(foundComposer);
    }
}

void tst_PlaylistColumnLayoutManager::testCopyAndReset()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    // Modify compact layout
    manager.setColumnVisibility(QStringLiteral("compact"), QStringLiteral("genre"), QStringLiteral("shown"));

    // Copy normal to compact
    manager.copySkinLayout(QStringLiteral("normal"), QStringLiteral("compact"));
    QCOMPARE(manager.compactColumns(), manager.normalColumns());

    // Reset compact
    manager.resetSkin(QStringLiteral("compact"));
    QVERIFY(manager.compactColumns() != manager.normalColumns());

    // Reset all
    manager.resetAllSkins();
    QCOMPARE(manager.compactHeaderMode(), QStringLiteral("automatic"));
}

void tst_PlaylistColumnLayoutManager::testUrlSchemeAllowed()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    QVERIFY(manager.isUrlSchemeAllowed(QStringLiteral("https://example.com/stream")));
    QVERIFY(manager.isUrlSchemeAllowed(QStringLiteral("http://example.com/stream")));
    QVERIFY(!manager.isUrlSchemeAllowed(QStringLiteral("ftp://example.com/file")));
    QVERIFY(!manager.isUrlSchemeAllowed(QStringLiteral("file:///etc/passwd")));
    QVERIFY(!manager.isUrlSchemeAllowed(QStringLiteral("javascript:alert(1)")));
    QVERIFY(!manager.isUrlSchemeAllowed(QStringLiteral("")));
}

void tst_PlaylistColumnLayoutManager::testColumnVisibilityAndReordering()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    QSignalSpy normalSpy(&manager, &PlaylistColumnLayoutManager::normalColumnsChanged);
    QSignalSpy revSpy(&manager, &PlaylistColumnLayoutManager::layoutRevisionChanged);

    const int initialRev = manager.layoutRevision();
    manager.setColumnVisibility(QStringLiteral("normal"), QStringLiteral("bpm"), QStringLiteral("shown"));
    QVERIFY(normalSpy.count() >= 1);
    QVERIFY(revSpy.count() >= 1);
    QVERIFY(manager.layoutRevision() > initialRev);

    // Move column to top
    manager.moveColumn(QStringLiteral("normal"), 10, 0);
    const auto cols = manager.normalColumns();
    QCOMPARE(cols.size(), 24);
}

void tst_PlaylistColumnLayoutManager::testWidthBucketAndCaching()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    // Default normal layout has automatic columns: artist (480), bitrate (620), album (740)
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 1200.0), 740);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 740.0), 740);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 739.0), 620);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 620.0), 620);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 619.0), 480);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 480.0), 480);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 479.0), 0);
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 100.0), 0);

    // Check caching: repeated calls return consistent results
    const auto cols1 = manager.effectiveVisibleColumns(QStringLiteral("normal"), 740.0);
    const auto cols2 = manager.effectiveVisibleColumns(QStringLiteral("normal"), 740.0);
    QCOMPARE(cols1.size(), cols2.size());
    for (int i = 0; i < cols1.size(); ++i) {
        QCOMPARE(cols1.at(i).toMap().value(QStringLiteral("id")), cols2.at(i).toMap().value(QStringLiteral("id")));
    }

    // Changing column visibility should invalidate cache and update buckets
    manager.setColumnVisibility(QStringLiteral("normal"), QStringLiteral("genre"), QStringLiteral("automatic")); // genre has minWidth 550
    QCOMPARE(manager.widthBucket(QStringLiteral("normal"), 560.0), 550);
}

void tst_PlaylistColumnLayoutManager::testIsColumnVisibleAndToggle()
{
    AppSettingsManager settingsManager;
    PlaylistColumnLayoutManager manager(&settingsManager);

    // Initial state: playlistPosition is shown
    QVERIFY(manager.isColumnVisible(QStringLiteral("normal"), QStringLiteral("playlistPosition")));
    QVERIFY(!manager.isColumnVisible(QStringLiteral("normal"), QStringLiteral("composer")));

    // Toggle composer -> becomes shown
    manager.toggleColumnVisibility(QStringLiteral("normal"), QStringLiteral("composer"));
    QVERIFY(manager.isColumnVisible(QStringLiteral("normal"), QStringLiteral("composer")));

    // Toggle composer again -> becomes hidden
    manager.toggleColumnVisibility(QStringLiteral("normal"), QStringLiteral("composer"));
    QVERIFY(!manager.isColumnVisible(QStringLiteral("normal"), QStringLiteral("composer")));

    // Toggle playlistPosition (which is shown) -> becomes hidden
    manager.toggleColumnVisibility(QStringLiteral("normal"), QStringLiteral("playlistPosition"));
    QVERIFY(!manager.isColumnVisible(QStringLiteral("normal"), QStringLiteral("playlistPosition")));
}

QTEST_MAIN(tst_PlaylistColumnLayoutManager)
#include "tst_PlaylistColumnLayoutManager.moc"
