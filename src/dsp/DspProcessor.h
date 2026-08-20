#ifndef DSPPROCESSOR_H
#define DSPPROCESSOR_H

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <QString>
#include <QtGlobal>

namespace WaveFlux::Dsp {

struct BiquadState {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double z1[2] = {0.0, 0.0};
    double z2[2] = {0.0, 0.0};

    void reset() {
        z1[0] = z1[1] = 0.0;
        z2[0] = z2[1] = 0.0;
    }
};

class LowShelfFilter {
public:
    explicit LowShelfFilter(double frequencyHz = 100.0, int sampleRate = 48000);
    void setSampleRate(int sampleRate);
    void setBassMultiplier(double multiplier);
    void reset();
    void processInterleaved(float *samples, std::size_t frameCount, int channels);

private:
    void updateCoefficients();

    double m_frequencyHz = 100.0;
    int m_sampleRate = 48000;
    double m_multiplier = 1.0;
    BiquadState m_state;
};

class DelayEffect {
public:
    explicit DelayEffect(int sampleRate = 48000, double maxDelaySec = 1.0);
    void setSampleRate(int sampleRate);
    void setParameters(double delayMs, double feedback, double wetFraction);
    void reset();
    void processInterleaved(float *samples, std::size_t frameCount, int channels);

private:
    int m_sampleRate = 48000;
    double m_delayMs = 350.0;
    double m_feedback = 0.35;
    double m_wetFraction = 0.0;
    std::vector<float> m_buffer;
    std::size_t m_writeIndex = 0;
};

class ModulatedDelayEffect {
public:
    enum class Mode {
        Chorus,
        Flanger
    };

    explicit ModulatedDelayEffect(Mode mode = Mode::Chorus, int sampleRate = 48000);
    void setSampleRate(int sampleRate);
    void setWetFraction(double wetFraction);
    void setParameters(double baseDelayMs, double depthMs, double lfoRateHz, double wetFraction);
    void reset();
    void processInterleaved(float *samples, std::size_t frameCount, int channels);

private:
    Mode m_mode;
    int m_sampleRate = 48000;
    double m_wetFraction = 0.0;
    double m_baseDelayMs = 20.0;
    double m_depthMs = 3.0;
    double m_lfoRateHz = 1.0;
    double m_feedback = 0.2;
    double m_lfoPhase = 0.0;
    std::vector<float> m_buffer;
    std::size_t m_writeIndex = 0;
};

class SimpleReverb {
public:
    explicit SimpleReverb(int sampleRate = 48000);
    void setSampleRate(int sampleRate);
    void setWetFraction(double wetFraction);
    void setParameters(double roomSize, double damp, double wetFraction);
    void reset();
    void processInterleaved(float *samples, std::size_t frameCount, int channels);

private:
    struct CombFilter {
        std::vector<float> buffer;
        std::size_t index = 0;
        float feedback = 0.8f;
        float filterStore = 0.0f;
        float damp = 0.2f;

        void init(std::size_t size, float fb, float dmp) {
            buffer.assign(size, 0.0f);
            index = 0;
            feedback = fb;
            damp = dmp;
            filterStore = 0.0f;
        }

        float process(float input) {
            float output = buffer[index];
            filterStore = (output * (1.0f - damp)) + (filterStore * damp);
            buffer[index] = input + (filterStore * feedback);
            if (++index >= buffer.size()) index = 0;
            return output;
        }
    };

    struct AllPassFilter {
        std::vector<float> buffer;
        std::size_t index = 0;
        float feedback = 0.5f;

        void init(std::size_t size, float fb = 0.5f) {
            buffer.assign(size, 0.0f);
            index = 0;
            feedback = fb;
        }

        float process(float input) {
            float bufOut = buffer[index];
            float output = -input + bufOut;
            buffer[index] = input + (bufOut * feedback);
            if (++index >= buffer.size()) index = 0;
            return output;
        }
    };

    int m_sampleRate = 48000;
    double m_wetFraction = 0.0;
    CombFilter m_combs[4];
    AllPassFilter m_allpasses[2];
};

class StereoProcessor {
public:
    static void applyStereoWidth(float *samples, std::size_t frameCount, double widthMultiplier);
    static void applyBalance(float *samples, std::size_t frameCount, double balance);
    static void applyVoiceSuppression(float *samples, std::size_t frameCount, bool enabled);
    static void applyPeakLimiter(float *samples, std::size_t frameCount, int channels, float threshold = 0.99f);
};

class SilenceDetector {
public:
    struct SilenceInterval {
        std::size_t startFrame = 0;
        std::size_t endFrame = 0;
    };

    explicit SilenceDetector(int sampleRate = 48000);
    void setSampleRate(int sampleRate);
    void setThresholdDbfs(double thresholdDbfs);
    void setMinimumDurationMs(int minDurationMs);
    void setMinimumSilenceDurationMs(int minDurationMs) { setMinimumDurationMs(minDurationMs); }
    void setTrimEdges(bool trimEdges);
    void reset();

    // Process a chunk of interleaved samples and return whether current block is silence
    bool processFrameChunk(const float *samples, std::size_t frameCount, int channels);
    std::vector<SilenceInterval> detectSilence(const float *samples, std::size_t totalFrames, int channels);

    qint64 currentSilenceDurationMs() const { return m_consecutiveSilenceMs; }
    bool isQualifiedSilence() const { return m_isQualifiedSilence; }

private:
    int m_sampleRate = 48000;
    double m_thresholdDbfs = -60.0;
    int m_minDurationMs = 500;
    bool m_trimEdges = true;
    qint64 m_consecutiveSilenceMs = 0;
    bool m_isQualifiedSilence = false;
    double m_linearThreshold = 0.001;
};

class GainRamp {
public:
    explicit GainRamp(int sampleRate = 48000);
    void setSampleRate(int sampleRate);
    void setGainInstant(double gain);
    void rampTo(double targetGain, int durationMs);
    bool isRamping() const { return m_framesRemaining > 0; }
    double currentGain() const { return m_currentGain; }
    void processInterleaved(float *samples, std::size_t frameCount, int channels);

private:
    int m_sampleRate = 48000;
    double m_currentGain = 1.0;
    double m_targetGain = 1.0;
    double m_step = 0.0;
    std::size_t m_framesRemaining = 0;
};

class FormatQualitySimulator {
public:
    explicit FormatQualitySimulator(int sinkSampleRate = 48000);
    void setSinkSampleRate(int sampleRate);
    void setFormat(const QString &format, bool isLossy);
    void setTargetBitrate(int bitrateKbps);
    void setTargetSampleRate(int targetSampleRateHz);
    void setChannelMode(const QString &channelMode);
    void setApplyEqualizer(bool enabled);
    void setEqualizerBandGains(const std::vector<double> &gainsDb);
    void reset();
    void processInterleaved(float *samples, std::size_t frameCount, int channels);

    int targetBitrate() const { return m_targetBitrate; }
    int targetSampleRate() const { return m_targetSampleRate; }
    QString channelMode() const { return m_channelMode; }
    bool isLossy() const { return m_isLossy; }

private:
    void updateFilters();

    int m_sinkSampleRate = 48000;
    QString m_format;
    bool m_isLossy = true;
    int m_targetBitrate = 320;
    int m_targetSampleRate = 44100;
    QString m_channelMode;
    bool m_applyEqualizer = false;
    std::vector<double> m_equalizerBandGains;

    BiquadState m_lowpass1;
    BiquadState m_lowpass2;
    BiquadState m_eqBands[10];
    int m_decimationPhase = 0;
    float m_holdSample[2] = {0.0f, 0.0f};
};

} // namespace WaveFlux::Dsp

#endif // DSPPROCESSOR_H
