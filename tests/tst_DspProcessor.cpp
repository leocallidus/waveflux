#include <QtTest/QtTest>
#include <vector>
#include <cmath>
#include "dsp/DspProcessor.h"

class tst_DspProcessor : public QObject
{
    Q_OBJECT

private slots:
    void testLowShelfFilterNeutral();
    void testLowShelfFilterBoost();
    void testStereoWidthNeutral();
    void testStereoWidthWiden();
    void testVoiceSuppression();
    void testBalancePanning();
    void testPeakLimiter();
    void testDelayEffectMix();
    void testSimpleReverb();
    void testGainRampInstant();
    void testGainRampFade();
    void testFormatQualitySimulatorMono();
    void testFormatQualitySimulatorSampleRateDecimation();
    void testFormatQualitySimulatorBitrateQuantization();
    void testFormatQualitySimulatorEqualizer();
};

void tst_DspProcessor::testLowShelfFilterNeutral()
{
    WaveFlux::Dsp::LowShelfFilter filter(100.0, 48000);
    filter.setBassMultiplier(1.0); // Neutral

    std::vector<float> samples = {0.5f, -0.5f, 0.25f, -0.25f};
    std::vector<float> copy = samples;

    filter.processInterleaved(samples.data(), 2, 2);

    for (std::size_t i = 0; i < samples.size(); ++i) {
        QVERIFY(std::abs(samples[i] - copy[i]) < 1e-4f);
    }
}

void tst_DspProcessor::testLowShelfFilterBoost()
{
    WaveFlux::Dsp::LowShelfFilter filter(100.0, 48000);
    filter.setBassMultiplier(2.0); // Boost

    // Low frequency sine wave (~50 Hz at 48000 Hz)
    const int frames = 4800;
    std::vector<float> samples(frames * 2);
    for (int i = 0; i < frames; ++i) {
        float val = std::sin(2.0f * 3.14159265f * 50.0f * i / 48000.0f);
        samples[i * 2] = val;
        samples[i * 2 + 1] = val;
    }

    float initialPeak = 0.0f;
    for (float s : samples) initialPeak = std::max(initialPeak, std::abs(s));

    filter.processInterleaved(samples.data(), frames, 2);

    float filteredPeak = 0.0f;
    // Inspect after filter settles (second half)
    for (std::size_t i = frames; i < samples.size(); ++i) {
        filteredPeak = std::max(filteredPeak, std::abs(samples[i]));
    }

    QVERIFY(filteredPeak > initialPeak);
}

void tst_DspProcessor::testStereoWidthNeutral()
{
    std::vector<float> samples = {0.8f, -0.4f, 0.2f, 0.6f};
    std::vector<float> copy = samples;

    WaveFlux::Dsp::StereoProcessor::applyStereoWidth(samples.data(), 2, 1.0);

    for (std::size_t i = 0; i < samples.size(); ++i) {
        QVERIFY(std::abs(samples[i] - copy[i]) < 1e-5f);
    }
}

void tst_DspProcessor::testStereoWidthWiden()
{
    // L = 0.8, R = 0.2 -> Mid = 0.5, Side = 0.3
    // At width = 2.0 -> Side becomes 0.6 -> L = 1.1, R = -0.1
    std::vector<float> samples = {0.8f, 0.2f};
    WaveFlux::Dsp::StereoProcessor::applyStereoWidth(samples.data(), 1, 2.0);

    QVERIFY(std::abs(samples[0] - 1.1f) < 1e-4f);
    QVERIFY(std::abs(samples[1] - (-0.1f)) < 1e-4f);
}

void tst_DspProcessor::testVoiceSuppression()
{
    // Center panned vocal + side instrument
    // Center vocal: L=0.5, R=0.5. Side instrument: L=0.2, R=-0.2. Total: L=0.7, R=0.3
    std::vector<float> samples = {0.7f, 0.3f};
    WaveFlux::Dsp::StereoProcessor::applyVoiceSuppression(samples.data(), 1, true);

    // L - R = 0.4. L becomes +0.2, R becomes -0.2. Center 0.5 is canceled!
    QVERIFY(std::abs(samples[0] - 0.2f) < 1e-4f);
    QVERIFY(std::abs(samples[1] - (-0.2f)) < 1e-4f);
}

void tst_DspProcessor::testBalancePanning()
{
    // Pan fully Left (-1.0)
    std::vector<float> samplesLeft = {0.5f, 0.5f};
    WaveFlux::Dsp::StereoProcessor::applyBalance(samplesLeft.data(), 1, -1.0);
    QVERIFY(std::abs(samplesLeft[0] - 0.5f) < 1e-4f);
    QVERIFY(std::abs(samplesLeft[1] - 0.0f) < 1e-4f);

    // Pan fully Right (+1.0)
    std::vector<float> samplesRight = {0.5f, 0.5f};
    WaveFlux::Dsp::StereoProcessor::applyBalance(samplesRight.data(), 1, 1.0);
    QVERIFY(std::abs(samplesRight[0] - 0.0f) < 1e-4f);
    QVERIFY(std::abs(samplesRight[1] - 0.5f) < 1e-4f);

    // Neutral (0.0)
    std::vector<float> samplesCenter = {0.5f, 0.5f};
    WaveFlux::Dsp::StereoProcessor::applyBalance(samplesCenter.data(), 1, 0.0);
    QVERIFY(std::abs(samplesCenter[0] - 0.5f) < 1e-4f);
    QVERIFY(std::abs(samplesCenter[1] - 0.5f) < 1e-4f);
}

void tst_DspProcessor::testPeakLimiter()
{
    std::vector<float> samples = {1.5f, -2.0f, 0.8f, -0.9f};
    WaveFlux::Dsp::StereoProcessor::applyPeakLimiter(samples.data(), 2, 1.0f);

    for (float s : samples) {
        QVERIFY(s >= -1.0f && s <= 1.0f);
    }
}

void tst_DspProcessor::testDelayEffectMix()
{
    WaveFlux::Dsp::DelayEffect delay(48000);
    delay.setParameters(10.0, 0.5, 0.0); // 0% wet mix (bypassed)

    std::vector<float> samples = {0.5f, -0.5f, 0.25f, -0.25f};
    std::vector<float> copy = samples;

    delay.processInterleaved(samples.data(), 2, 2);

    for (std::size_t i = 0; i < samples.size(); ++i) {
        QVERIFY(std::abs(samples[i] - copy[i]) < 1e-5f);
    }
}

void tst_DspProcessor::testGainRampInstant()
{
    WaveFlux::Dsp::GainRamp ramp(48000);
    ramp.setGainInstant(0.5);
    QCOMPARE(ramp.currentGain(), 0.5);
    QVERIFY(!ramp.isRamping());

    std::vector<float> samples = {1.0f, -1.0f, 0.5f, -0.5f};
    ramp.processInterleaved(samples.data(), 2, 2);

    QVERIFY(std::abs(samples[0] - 0.5f) < 1e-4f);
    QVERIFY(std::abs(samples[1] - (-0.5f)) < 1e-4f);
    QVERIFY(std::abs(samples[2] - 0.25f) < 1e-4f);
    QVERIFY(std::abs(samples[3] - (-0.25f)) < 1e-4f);
}

void tst_DspProcessor::testGainRampFade()
{
    WaveFlux::Dsp::GainRamp ramp(48000);
    ramp.setGainInstant(1.0);
    // Ramp to 0.0 over 10ms = 480 frames
    ramp.rampTo(0.0, 10);
    QVERIFY(ramp.isRamping());

    std::vector<float> samples(480 * 2, 1.0f);
    ramp.processInterleaved(samples.data(), 480, 2);

    QVERIFY(!ramp.isRamping());
    QVERIFY(std::abs(ramp.currentGain() - 0.0) < 1e-4);
    // First sample should be slightly less than 1.0, and last sample near 0.0
    QVERIFY(samples[0] < 1.0f);
    QVERIFY(std::abs(samples[samples.size() - 1]) < 0.01f);
}

void tst_DspProcessor::testSimpleReverb()
{
    WaveFlux::Dsp::SimpleReverb reverb(48000);
    reverb.setParameters(0.8, 0.5, 0.0); // 0% wet mix

    std::vector<float> samples = {0.5f, -0.5f, 0.25f, -0.25f};
    std::vector<float> copy = samples;

    reverb.processInterleaved(samples.data(), 2, 2);

    for (std::size_t i = 0; i < samples.size(); ++i) {
        QVERIFY(std::abs(samples[i] - copy[i]) < 1e-5f);
    }
}

void tst_DspProcessor::testFormatQualitySimulatorMono()
{
    WaveFlux::Dsp::FormatQualitySimulator sim(48000);
    sim.setChannelMode(QStringLiteral("mono"));

    std::vector<float> samples = {0.8f, 0.2f, -0.4f, 0.6f};
    sim.processInterleaved(samples.data(), 2, 2);

    // L & R should both equal 0.5f and 0.1f (before filtering/limiting)
    QVERIFY(std::abs(samples[0] - samples[1]) < 1e-4f);
    QVERIFY(std::abs(samples[2] - samples[3]) < 1e-4f);
}

void tst_DspProcessor::testFormatQualitySimulatorSampleRateDecimation()
{
    WaveFlux::Dsp::FormatQualitySimulator sim(48000);
    sim.setFormat(QStringLiteral("wav"), false);
    sim.setTargetSampleRate(8000); // 8kHz downsampling from 48kHz (step = 6)
    sim.setChannelMode(QStringLiteral("stereo"));

    // High frequency test signal (~16 kHz at 48000 Hz, above 8kHz Nyquist of 4kHz)
    const int frames = 4800;
    std::vector<float> samples(frames * 2);
    for (int i = 0; i < frames; ++i) {
        float val = std::sin(2.0f * 3.14159265f * 16000.0f * i / 48000.0f);
        samples[i * 2] = val;
        samples[i * 2 + 1] = val;
    }

    sim.processInterleaved(samples.data(), frames, 2);

    // High frequency 16kHz sine wave should be heavily attenuated by the lowpass filter
    float attenuatedPeak = 0.0f;
    for (int i = frames / 2; i < frames; ++i) {
        attenuatedPeak = std::max(attenuatedPeak, std::abs(samples[i * 2]));
    }
    QVERIFY(attenuatedPeak < 0.25f);
}

void tst_DspProcessor::testFormatQualitySimulatorBitrateQuantization()
{
    WaveFlux::Dsp::FormatQualitySimulator sim(48000);
    sim.setFormat(QStringLiteral("mp3"), true);
    sim.setTargetBitrate(32); // 32 kbps low bitrate
    sim.setChannelMode(QStringLiteral("stereo"));

    std::vector<float> samples(1000 * 2);
    for (int i = 0; i < 1000; ++i) {
        float val = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
        samples[i * 2] = val;
        samples[i * 2 + 1] = val;
    }

    sim.processInterleaved(samples.data(), 1000, 2);

    // Samples should be quantized and bounded
    for (float s : samples) {
        QVERIFY(std::abs(s) <= 1.0f);
    }
}

void tst_DspProcessor::testFormatQualitySimulatorEqualizer()
{
    WaveFlux::Dsp::FormatQualitySimulator sim(48000);
    sim.setFormat(QStringLiteral("flac"), false);
    sim.setTargetSampleRate(48000);
    sim.setApplyEqualizer(true);

    std::vector<double> gains = {6.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}; // Boost 31 Hz band by +6 dB
    sim.setEqualizerBandGains(gains);

    // 31 Hz low frequency test
    const int frames = 4800;
    std::vector<float> samples(frames * 2);
    for (int i = 0; i < frames; ++i) {
        float val = 0.3f * std::sin(2.0f * 3.14159265f * 31.0f * i / 48000.0f);
        samples[i * 2] = val;
        samples[i * 2 + 1] = val;
    }

    sim.processInterleaved(samples.data(), frames, 2);

    float peak = 0.0f;
    for (int i = frames / 2; i < frames; ++i) {
        peak = std::max(peak, std::abs(samples[i * 2]));
    }
    // +6 dB boost should result in peak > 0.35f
    QVERIFY(peak > 0.35f);
}

QTEST_MAIN(tst_DspProcessor)
#include "tst_DspProcessor.moc"

