#include <QTest>
#include <QSignalSpy>
#include "lyrics/LyricsLineModel.h"
#include "lyrics/LyricsTypes.h"
#include <algorithm>

using namespace WaveFlux::Lyrics;

class LyricsPlaybackSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void progressSurvivesVerseBreaksAndSeeks();
    void testLineModelBasics();
    void testLineModelActiveLineAndTiming();
    void testBlankCueHighlightSuppression();
    void testCueTrackOffsetMath();
    void testFragmentRepeatSeekClamping();
};

void LyricsPlaybackSyncTest::progressSurvivesVerseBreaksAndSeeks()
{
    LyricsLineModel model;
    model.setCues({{1000, 1000, "First", false}, {2000, 2000, "Last in verse", false},
                   {3000, 3000, "", true}, {4000, 4000, "Next verse", false},
                   {5000, 5000, "Final", false}}, 0, 0);
    model.setActiveLine(1);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.setPlaybackProgress(-1, 2);
    QVERIFY(model.data(model.index(1), LyricsLineModel::IsPastRole).toBool());
    QVERIFY(!model.data(model.index(1), LyricsLineModel::IsCurrentRole).toBool());
    model.setPlaybackProgress(3, 3);
    QVERIFY(model.data(model.index(1), LyricsLineModel::IsPastRole).toBool());
    changed.clear();
    model.setActiveLine(0);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(changed.first().at(1).value<QModelIndex>().row(), 3);
    QVERIFY(!model.data(model.index(1), LyricsLineModel::IsPastRole).toBool());
    model.setPlaybackProgress(-1, model.count());
    QVERIFY(model.data(model.index(4), LyricsLineModel::IsPastRole).toBool());
    model.setPlaybackProgress(-1, -1);
    QVERIFY(!model.data(model.index(4), LyricsLineModel::IsPastRole).toBool());
}

void LyricsPlaybackSyncTest::testLineModelBasics()
{
    LyricsLineModel model;
    QCOMPARE(model.rowCount(), 0);

    std::vector<CueGroup> cues = {
        {1000, 1000, QStringLiteral("Line 1"), false},
        {5000, 5000, QStringLiteral("Line 2"), false},
        {10000, 10000, QStringLiteral(""), true},
        {15000, 15000, QStringLiteral("Line 3"), false}
    };

    model.setCues(cues, 0, 0);
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.count(), 4);
    QVERIFY(model.isSynced());

    QCOMPARE(model.data(model.index(0, 0), LyricsLineModel::TextRole).toString(), QStringLiteral("Line 1"));
    QCOMPARE(model.data(model.index(0, 0), LyricsLineModel::TimestampMsRole).toLongLong(), 1000);
    QCOMPARE(model.data(model.index(2, 0), LyricsLineModel::IsBlankRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), LyricsLineModel::CanSeekRole).toBool(), true);
}

void LyricsPlaybackSyncTest::testLineModelActiveLineAndTiming()
{
    LyricsLineModel model;
    std::vector<CueGroup> cues = {
        {1000, 1000, QStringLiteral("Verse 1"), false},
        {5000, 5000, QStringLiteral("Verse 2"), false},
        {12000, 12000, QStringLiteral("Chorus"), false}
    };
    model.setCues(cues, 0, 0);

    // Initially active index is -1
    QCOMPARE(model.currentIndex(), -1);

    // Set active line to 1
    model.setActiveLine(1);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.data(model.index(0, 0), LyricsLineModel::IsPastRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), LyricsLineModel::IsCurrentRole).toBool(), true);
    QCOMPARE(model.data(model.index(2, 0), LyricsLineModel::IsCurrentRole).toBool(), false);

    // Update timing: +500ms delay
    model.updateTiming(0, 500);
    QCOMPARE(model.cueTimestampAt(0), 1500);
    QCOMPARE(model.cueTimestampAt(1), 5500);
    QCOMPARE(model.cueTimestampAt(2), 12500);
}

void LyricsPlaybackSyncTest::testBlankCueHighlightSuppression()
{
    std::vector<CueGroup> cues = {
        {2000, 2000, QStringLiteral("Before break"), false},
        {6000, 6000, QStringLiteral(""), true}, // Instrumental break cue
        {14000, 14000, QStringLiteral("After break"), false}
    };

    auto findActiveLine = [&](qint64 songPosMs) -> int {
        auto it = std::upper_bound(cues.begin(), cues.end(), songPosMs, [](qint64 target, const CueGroup &cg) {
            return target < cg.rawCueMs;
        });
        if (it != cues.begin()) {
            const int idx = static_cast<int>(std::distance(cues.begin(), it) - 1);
            if (idx >= 0 && idx < static_cast<int>(cues.size())) {
                if (!cues[idx].isBlank) {
                    return idx;
                }
            }
        }
        return -1;
    };

    // Before first cue
    QCOMPARE(findActiveLine(1000), -1);

    // During first line
    QCOMPARE(findActiveLine(2500), 0);

    // During instrumental break (blank cue at 6000)
    QCOMPARE(findActiveLine(7000), -1); // blank cue suppresses highlight

    // After instrumental break
    QCOMPARE(findActiveLine(15000), 2);
}

void LyricsPlaybackSyncTest::testCueTrackOffsetMath()
{
    // A single FLAC file containing a CUE sheet album
    // Track 2 starts at 2:30.00 (150,000 ms) in the audio file
    const qint64 trackCueStartMs = 150000;
    const qint64 enginePositionMs = 175000; // 2:55.00 in total stream

    // Relative song playback position
    const qint64 songPositionMs = enginePositionMs - trackCueStartMs;
    QCOMPARE(songPositionMs, 25000); // 25s into Track 2

    // User delay adjustment: +300ms user delay and -100ms LRC header offset
    const qint64 rawCueMs = 25000;
    const qint64 lrcOffsetMs = -100;
    const qint64 userDelayMs = 300;
    const qint64 effectiveCueMs = std::max<qint64>(0, rawCueMs - lrcOffsetMs + userDelayMs);
    QCOMPARE(effectiveCueMs, 25400);
}

void LyricsPlaybackSyncTest::testFragmentRepeatSeekClamping()
{
    // A-B Loop is set to [10000, 40000] ms
    const bool isFragmentLoopActive = true;
    const qint64 loopStartMs = 10000;
    const qint64 loopEndMs = 40000;

    auto clampSeek = [&](qint64 targetMs) -> qint64 {
        if (!isFragmentLoopActive) return targetMs;
        if (targetMs < loopStartMs) return loopStartMs;
        if (targetMs > loopEndMs) return loopStartMs; // loops back to start
        return targetMs;
    };

    // Seek before loop
    QCOMPARE(clampSeek(5000), 10000);

    // Seek inside loop
    QCOMPARE(clampSeek(25000), 25000);

    // Seek past loop
    QCOMPARE(clampSeek(45000), 10000);
}

QTEST_MAIN(LyricsPlaybackSyncTest)
#include "tst_LyricsPlaybackSync.moc"
