#include <QTest>
#include <QTemporaryDir>
#include "lyrics/LyricsCache.h"

using namespace WaveFlux::Lyrics;

class LyricsCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitAndSchema();
    void testStoreAndGetDocument();
    void testQueryResultsAndNegativeCaching();
    void testTrackDelaysAndOverrides();
    void testImportedDocuments();
    void testEvictionAndPurge();
};

void LyricsCacheTest::testInitAndSchema()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_lyrics.db"));

    LyricsCache cache;
    QVERIFY(cache.init(dbPath));
    QVERIFY(QFile::exists(dbPath));
    cache.close();
}

void LyricsCacheTest::testStoreAndGetDocument()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_lyrics.db"));

    LyricsCache cache;
    QVERIFY(cache.init(dbPath));

    LyricsDocument doc;
    doc.id = QStringLiteral("doc-queen-01");
    doc.kind = LyricsKind::Synced;
    doc.provenance = LyricsProvenance::OnlineLrclib;
    doc.text = QStringLiteral("[00:01.00]Radio\n[00:02.00]Someone still loves you\n");
    doc.plainText = QStringLiteral("Radio\nSomeone still loves you\n");
    doc.lrcOffsetMs = 0;

    QVERIFY(cache.storeDocument(doc));

    auto retrieved = cache.getDocument(doc.id);
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->id, doc.id);
    QCOMPARE(retrieved->kind, LyricsKind::Synced);
    QCOMPARE(retrieved->text, doc.text);
    QCOMPARE(retrieved->cues.size(), 2);
    QCOMPARE(retrieved->cues[0].text, QStringLiteral("Radio"));

    // Non-existent document
    auto missing = cache.getDocument(QStringLiteral("non-existent"));
    QVERIFY(!missing.has_value());

    cache.close();
}

void LyricsCacheTest::testQueryResultsAndNegativeCaching()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_lyrics.db"));

    LyricsCache cache;
    QVERIFY(cache.init(dbPath));

    // Store referenced document first for foreign key integrity
    LyricsDocument doc;
    doc.id = QStringLiteral("doc-metallica-one");
    doc.kind = LyricsKind::Plain;
    doc.provenance = LyricsProvenance::OnlineLrclib;
    doc.text = QStringLiteral("I can't remember anything\n");
    QVERIFY(cache.storeDocument(doc));

    const QString positiveKey = QStringLiteral("query:artist=metallica&title=one");
    const QString negativeKey = QStringLiteral("query:artist=obscure&title=unknown");

    // Positive hit
    QVERIFY(cache.storeQueryResult(positiveKey, QStringLiteral("doc-metallica-one"), false));
    auto resPositive = cache.getQueryResult(positiveKey);
    QVERIFY(resPositive.has_value());
    QCOMPARE(resPositive->documentId, QStringLiteral("doc-metallica-one"));
    QCOMPARE(resPositive->isNegative, false);
    QCOMPARE(resPositive->expired, false);

    // Negative hit (documentId is null/empty)
    QVERIFY(cache.storeQueryResult(negativeKey, QString(), true));
    auto resNegative = cache.getQueryResult(negativeKey);
    QVERIFY(resNegative.has_value());
    QCOMPARE(resNegative->isNegative, true);
    QCOMPARE(resNegative->expired, false);

    cache.close();
}

void LyricsCacheTest::testTrackDelaysAndOverrides()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_lyrics.db"));

    LyricsCache cache;
    QVERIFY(cache.init(dbPath));

    const QString trackKey = QStringLiteral("hash:song-abc-123");

    // Initially no delay
    QVERIFY(!cache.getUserDelay(trackKey).has_value());

    // Store delay
    QVERIFY(cache.storeUserDelay(trackKey, -250));
    auto delay = cache.getUserDelay(trackKey);
    QVERIFY(delay.has_value());
    QCOMPARE(*delay, -250);

    // Update delay
    QVERIFY(cache.storeUserDelay(trackKey, 400));
    delay = cache.getUserDelay(trackKey);
    QVERIFY(delay.has_value());
    QCOMPARE(*delay, 400);

    // Store candidate document first for foreign key integrity
    LyricsDocument candDoc;
    candDoc.id = QStringLiteral("doc-custom-candidate");
    candDoc.kind = LyricsKind::Plain;
    candDoc.text = QStringLiteral("Custom text");
    QVERIFY(cache.storeDocument(candDoc));

    // Override
    QVERIFY(!cache.getTrackOverride(trackKey).has_value());
    QVERIFY(cache.storeTrackOverride(trackKey, QStringLiteral("doc-custom-candidate")));
    auto overrideDoc = cache.getTrackOverride(trackKey);
    QVERIFY(overrideDoc.has_value());
    QCOMPARE(*overrideDoc, QStringLiteral("doc-custom-candidate"));

    // Clear override
    QVERIFY(cache.clearTrackOverride(trackKey));
    QVERIFY(!cache.getTrackOverride(trackKey).has_value());

    cache.close();
}

void LyricsCacheTest::testImportedDocuments()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_lyrics.db"));

    LyricsCache cache;
    QVERIFY(cache.init(dbPath));

    const QString trackKey = QStringLiteral("file:///music/song.flac");
    LyricsDocument doc;
    doc.id = QStringLiteral("imported-001");
    doc.kind = LyricsKind::Synced;
    doc.provenance = LyricsProvenance::Imported;
    doc.text = QStringLiteral("[00:00.50]Imported line\n");

    QVERIFY(cache.storeImportedDocument(trackKey, doc));

    auto imported = cache.getImportedDocument(trackKey);
    QVERIFY(imported.has_value());
    QCOMPARE(imported->text, doc.text);

    cache.close();
}

void LyricsCacheTest::testEvictionAndPurge()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_lyrics.db"));

    LyricsCache cache;
    QVERIFY(cache.init(dbPath));

    LyricsDocument expDoc;
    expDoc.id = QStringLiteral("doc-exp");
    expDoc.kind = LyricsKind::Plain;
    expDoc.text = QStringLiteral("Exp text");
    QVERIFY(cache.storeDocument(expDoc));

    // Store a query result
    const QString expiredKey = QStringLiteral("query:expired");
    QVERIFY(cache.storeQueryResult(expiredKey, QStringLiteral("doc-exp"), false));

    // Purge expired queries
    cache.purgeExpired();

    cache.evictIfNeeded();

    cache.close();
}

QTEST_MAIN(LyricsCacheTest)
#include "tst_LyricsCache.moc"
