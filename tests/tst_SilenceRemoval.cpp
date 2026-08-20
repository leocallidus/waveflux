#include <QtTest/QtTest>
#include <vector>
#include <cmath>
#include "dsp/DspProcessor.h"

class tst_SilenceRemoval : public QObject
{
    Q_OBJECT

private slots:
    void testPureSilenceDetection();
    void testAudibleSignalNoSilence();
    void testSilenceIntervalsWithHysteresis();
};

void tst_SilenceRemoval::testPureSilenceDetection()
{
    WaveFlux::Dsp::SilenceDetector detector(48000);
    detector.setThresholdDbfs(-60.0);
    detector.setMinimumSilenceDurationMs(100);
    detector.setTrimEdges(true);

    // 1 second of pure silence (all zeros)
    const std::size_t totalFrames = 48000;
    std::vector<float> silentBlock(totalFrames * 2, 0.0f);

    auto intervals = detector.detectSilence(silentBlock.data(), totalFrames, 2);

    QVERIFY(!intervals.empty());
    QCOMPARE(intervals.size(), static_cast<std::size_t>(1));
    QCOMPARE(intervals[0].startFrame, static_cast<std::size_t>(0));
    QCOMPARE(intervals[0].endFrame, totalFrames);
}

void tst_SilenceRemoval::testAudibleSignalNoSilence()
{
    WaveFlux::Dsp::SilenceDetector detector(48000);
    detector.setThresholdDbfs(-60.0);
    detector.setMinimumSilenceDurationMs(100);
    detector.setTrimEdges(false);

    // 1 second of loud sine wave (0 dBFS peak)
    const std::size_t totalFrames = 48000;
    std::vector<float> loudBlock(totalFrames * 2);
    for (std::size_t i = 0; i < totalFrames; ++i) {
        float val = std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
        loudBlock[i * 2] = val;
        loudBlock[i * 2 + 1] = val;
    }

    auto intervals = detector.detectSilence(loudBlock.data(), totalFrames, 2);
    QVERIFY(intervals.empty());
}

void tst_SilenceRemoval::testSilenceIntervalsWithHysteresis()
{
    WaveFlux::Dsp::SilenceDetector detector(48000);
    detector.setThresholdDbfs(-50.0);
    detector.setMinimumSilenceDurationMs(200); // 200ms = 9600 frames
    detector.setTrimEdges(false);

    // Layout: 0.5s loud (24000 frames), 0.5s quiet (24000 frames), 0.5s loud (24000 frames)
    const std::size_t partFrames = 24000;
    std::vector<float> buffer(partFrames * 3 * 2, 0.0f);

    // Part 1: loud
    for (std::size_t i = 0; i < partFrames; ++i) {
        float val = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
        buffer[i * 2] = val;
        buffer[i * 2 + 1] = val;
    }
    // Part 2: quiet (zeros)
    // (already zeros)

    // Part 3: loud
    for (std::size_t i = 0; i < partFrames; ++i) {
        std::size_t frame = partFrames * 2 + i;
        float val = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
        buffer[frame * 2] = val;
        buffer[frame * 2 + 1] = val;
    }

    auto intervals = detector.detectSilence(buffer.data(), partFrames * 3, 2);

    QCOMPARE(intervals.size(), static_cast<std::size_t>(1));
    QVERIFY(intervals[0].startFrame >= partFrames);
    QVERIFY(intervals[0].endFrame <= partFrames * 2 + 500); // small tolerance around block boundaries
}

QTEST_MAIN(tst_SilenceRemoval)
#include "tst_SilenceRemoval.moc"
