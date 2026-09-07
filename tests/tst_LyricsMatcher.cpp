#include <QTest>
#include "lyrics/LyricsMatcher.h"

using namespace WaveFlux::Lyrics;

class LyricsMatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void testNormalize();
    void testStripEditions();
    void testLevenshteinDistance();
    void testStringSimilarity();
    void testEligibilityAndTolerance();
    void testScoreAndRanking();
};

void LyricsMatcherTest::testNormalize()
{
    QCOMPARE(LyricsMatcher::normalize(QStringLiteral("  Café  del   MAR! ")), QStringLiteral("cafe del mar"));
    QCOMPARE(LyricsMatcher::normalize(QStringLiteral("Rock & Roll")), QStringLiteral("rock roll"));
    QCOMPARE(LyricsMatcher::normalize(QStringLiteral("Кино — Группа крови")), QStringLiteral("кино группа крови"));
    QVERIFY(LyricsMatcher::normalize(QStringLiteral("夜曲")) != LyricsMatcher::normalize(QStringLiteral("晴天")));
}

void LyricsMatcherTest::testStripEditions()
{
    QCOMPARE(LyricsMatcher::stripEditions(QStringLiteral("Master of Puppets (Remastered 2017)")), QStringLiteral("Master of Puppets"));
    QCOMPARE(LyricsMatcher::stripEditions(QStringLiteral("In the End [Live at Wembley]")), QStringLiteral("In the End"));
    QCOMPARE(LyricsMatcher::stripEditions(QStringLiteral("Track (feat. Drake)")), QStringLiteral("Track"));
    QCOMPARE(LyricsMatcher::stripEditions(QStringLiteral("Song (Deluxe Edition)")), QStringLiteral("Song"));
    QCOMPARE(LyricsMatcher::stripEditions(QStringLiteral("Clean Track Name")), QStringLiteral("Clean Track Name"));
    QCOMPARE(LyricsMatcher::stripEditions(QStringLiteral("Song - 2011 Remaster")), QStringLiteral("Song"));
}

void LyricsMatcherTest::testLevenshteinDistance()
{
    QCOMPARE(LyricsMatcher::levenshteinDistance(QStringLiteral("kitten"), QStringLiteral("sitting")), 3);
    QCOMPARE(LyricsMatcher::levenshteinDistance(QStringLiteral("hello"), QStringLiteral("hello")), 0);
    QCOMPARE(LyricsMatcher::levenshteinDistance(QStringLiteral(""), QStringLiteral("abc")), 3);
}

void LyricsMatcherTest::testStringSimilarity()
{
    QCOMPARE(LyricsMatcher::stringSimilarity(QStringLiteral("hello"), QStringLiteral("hello")), 1.0);
    QVERIFY(LyricsMatcher::stringSimilarity(QStringLiteral("hello"), QStringLiteral("world")) < 0.3);
    QVERIFY(LyricsMatcher::stringSimilarity(QStringLiteral("hello"), QStringLiteral("helo")) > 0.7);
}

void LyricsMatcherTest::testEligibilityAndTolerance()
{
    TrackSnapshot track;
    track.title = QStringLiteral("Comfortably Numb");
    track.artist = QStringLiteral("Pink Floyd");
    track.durationMs = 382000; // 6:22

    // Synced candidate within 2s tolerance: eligible
    LyricsCandidate candSynced;
    candSynced.title = QStringLiteral("Comfortably Numb");
    candSynced.artist = QStringLiteral("Pink Floyd");
    candSynced.hasSynced = true;
    candSynced.durationMs = 383500; // 1.5s difference <= 2.0s
    QVERIFY(LyricsMatcher::isEligible(candSynced, track));

    // Synced candidate beyond 2s tolerance: ineligible
    candSynced.durationMs = 385000; // 3.0s difference > 2.0s
    QVERIFY(!LyricsMatcher::isEligible(candSynced, track));

    // Plain candidate within 5s tolerance: eligible
    LyricsCandidate candPlain;
    candPlain.title = QStringLiteral("Comfortably Numb");
    candPlain.artist = QStringLiteral("Pink Floyd");
    candPlain.hasSynced = false;
    candPlain.durationMs = 386000; // 4.0s difference <= 5.0s
    QVERIFY(LyricsMatcher::isEligible(candPlain, track));

    // Plain candidate beyond 5s tolerance: ineligible
    candPlain.durationMs = 388000; // 6.0s difference > 5.0s
    QVERIFY(!LyricsMatcher::isEligible(candPlain, track));

    // Zero candidate duration: bypasses strict duration gate
    candPlain.durationMs = 0;
    QVERIFY(LyricsMatcher::isEligible(candPlain, track));
}

void LyricsMatcherTest::testScoreAndRanking()
{
    TrackSnapshot track;
    track.title = QStringLiteral("Bohemian Rhapsody");
    track.artist = QStringLiteral("Queen");
    track.album = QStringLiteral("A Night at the Opera");
    track.durationMs = 354000;

    LyricsCandidate candExact;
    candExact.id = QStringLiteral("exact");
    candExact.title = QStringLiteral("Bohemian Rhapsody");
    candExact.artist = QStringLiteral("Queen");
    candExact.album = QStringLiteral("A Night at the Opera");
    candExact.durationMs = 354000;
    candExact.hasSynced = true;

    LyricsCandidate candPartial;
    candPartial.id = QStringLiteral("partial");
    candPartial.title = QStringLiteral("Bohemian Rhapsody");
    candPartial.artist = QStringLiteral("Queen");
    candPartial.album = QStringLiteral("Greatest Hits");
    candPartial.durationMs = 353000;
    candPartial.hasSynced = false;

    LyricsCandidate candWeak;
    candWeak.id = QStringLiteral("weak");
    candWeak.title = QStringLiteral("Bohemian Rhapsody (Live)");
    candWeak.artist = QStringLiteral("Queen Cover");
    candWeak.durationMs = 352000;
    candWeak.hasSynced = false;

    std::vector<LyricsCandidate> candidates = {candWeak, candExact, candPartial};
    const auto ranked = LyricsMatcher::rankCandidates(candidates, track);

    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked[0].id, QStringLiteral("exact"));
    QCOMPARE(ranked[1].id, QStringLiteral("partial"));
    QCOMPARE(ranked[2].id, QStringLiteral("weak"));
}

QTEST_MAIN(LyricsMatcherTest)
#include "tst_LyricsMatcher.moc"
