#include "DspProcessor.h"

#include <cmath>
#include <cstring>
#include <numbers>

namespace WaveFlux::Dsp {

// --- LowShelfFilter ---

LowShelfFilter::LowShelfFilter(double frequencyHz, int sampleRate)
    : m_frequencyHz(frequencyHz)
    , m_sampleRate(sampleRate)
{
    updateCoefficients();
}

void LowShelfFilter::setSampleRate(int sampleRate)
{
    if (sampleRate > 0 && sampleRate != m_sampleRate) {
        m_sampleRate = sampleRate;
        updateCoefficients();
        reset();
    }
}

void LowShelfFilter::setBassMultiplier(double multiplier)
{
    if (std::abs(multiplier - m_multiplier) > 0.001) {
        m_multiplier = std::clamp(multiplier, 0.0, 2.0);
        updateCoefficients();
    }
}

void LowShelfFilter::reset()
{
    m_state.reset();
}

void LowShelfFilter::updateCoefficients()
{
    if (std::abs(m_multiplier - 1.0) < 0.001) {
        m_state.b0 = 1.0;
        m_state.b1 = 0.0;
        m_state.b2 = 0.0;
        m_state.a1 = 0.0;
        m_state.a2 = 0.0;
        return;
    }

    // Robert Bristow-Johnson low shelf formula
    // Gain in dB = 20 * log10(m_multiplier)
    // A = sqrt( 10^(dB/20) ) = sqrt(multiplier)
    const double A = std::sqrt(std::max(0.01, m_multiplier));
    const double w0 = 2.0 * std::numbers::pi * (m_frequencyHz / static_cast<double>(m_sampleRate));
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double S = 1.0; // slope
    const double alpha = 0.5 * sinw0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
    const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

    const double b0 =    A * ((A + 1.0) - (A - 1.0) * cosw0 + twoSqrtAAlpha);
    const double b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
    const double b2 =    A * ((A + 1.0) - (A - 1.0) * cosw0 - twoSqrtAAlpha);
    const double a0 =         (A + 1.0) + (A - 1.0) * cosw0 + twoSqrtAAlpha;
    const double a1 = -2.0 *   ((A - 1.0) + (A + 1.0) * cosw0);
    const double a2 =         (A + 1.0) + (A - 1.0) * cosw0 - twoSqrtAAlpha;

    const double invA0 = (std::abs(a0) > 1e-12) ? (1.0 / a0) : 1.0;
    m_state.b0 = b0 * invA0;
    m_state.b1 = b1 * invA0;
    m_state.b2 = b2 * invA0;
    m_state.a1 = a1 * invA0;
    m_state.a2 = a2 * invA0;
}

void LowShelfFilter::processInterleaved(float *samples, std::size_t frameCount, int channels)
{
    if (std::abs(m_multiplier - 1.0) < 0.001) {
        return; // bypass
    }

    const int chs = std::min(channels, 2);
    for (std::size_t f = 0; f < frameCount; ++f) {
        for (int c = 0; c < chs; ++c) {
            const double x = samples[f * channels + c];
            const double y = m_state.b0 * x + m_state.z1[c];
            m_state.z1[c] = m_state.b1 * x - m_state.a1 * y + m_state.z2[c];
            m_state.z2[c] = m_state.b2 * x - m_state.a2 * y;
            samples[f * channels + c] = static_cast<float>(y);
        }
    }
}

// --- DelayEffect ---

DelayEffect::DelayEffect(int sampleRate, double maxDelaySec)
    : m_sampleRate(sampleRate)
{
    m_buffer.assign(static_cast<std::size_t>(sampleRate * maxDelaySec * 2), 0.0f);
}

void DelayEffect::setSampleRate(int sampleRate)
{
    if (sampleRate > 0 && sampleRate != m_sampleRate) {
        m_sampleRate = sampleRate;
        m_buffer.assign(static_cast<std::size_t>(sampleRate * 2.0), 0.0f);
        m_writeIndex = 0;
    }
}

void DelayEffect::setParameters(double delayMs, double feedback, double wetFraction)
{
    m_delayMs = std::clamp(delayMs, 10.0, 2000.0);
    m_feedback = std::clamp(feedback, 0.0, 0.65);
    m_wetFraction = std::clamp(wetFraction, 0.0, 1.0);
}

void DelayEffect::reset()
{
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
    m_writeIndex = 0;
}

void DelayEffect::processInterleaved(float *samples, std::size_t frameCount, int channels)
{
    if (m_wetFraction <= 0.001 || channels < 2) {
        return;
    }

    const double delayFrames = m_delayMs * 0.001 * static_cast<double>(m_sampleRate);
    const std::size_t totalFrames = m_buffer.size() / 2;
    if (totalFrames == 0) return;

    for (std::size_t f = 0; f < frameCount; ++f) {
        double readPos = static_cast<double>(m_writeIndex) - delayFrames;
        while (readPos < 0.0) readPos += static_cast<double>(totalFrames);
        while (readPos >= static_cast<double>(totalFrames)) readPos -= static_cast<double>(totalFrames);

        const std::size_t i0 = static_cast<std::size_t>(readPos);
        const std::size_t i1 = (i0 + 1) % totalFrames;
        const float frac = static_cast<float>(readPos - static_cast<double>(i0));

        for (int c = 0; c < 2; ++c) {
            const float in = samples[f * channels + c];
            const float d0 = m_buffer[i0 * 2 + c];
            const float d1 = m_buffer[i1 * 2 + c];
            const float delayed = d0 + frac * (d1 - d0);

            m_buffer[m_writeIndex * 2 + c] = in + static_cast<float>(m_feedback) * delayed;
            samples[f * channels + c] = in * static_cast<float>(1.0 - m_wetFraction * 0.5) + delayed * static_cast<float>(m_wetFraction * 0.5);
        }

        if (++m_writeIndex >= totalFrames) {
            m_writeIndex = 0;
        }
    }
}

// --- ModulatedDelayEffect (Chorus / Flanger) ---

ModulatedDelayEffect::ModulatedDelayEffect(Mode mode, int sampleRate)
    : m_mode(mode)
    , m_sampleRate(sampleRate)
{
    if (m_mode == Mode::Chorus) {
        m_baseDelayMs = 20.0;
        m_depthMs = 3.5;
        m_lfoRateHz = 1.2;
        m_feedback = 0.25;
    } else {
        m_baseDelayMs = 2.5;
        m_depthMs = 1.5;
        m_lfoRateHz = 0.4;
        m_feedback = 0.45;
    }
    m_buffer.assign(static_cast<std::size_t>(sampleRate * 0.2 * 2), 0.0f);
}

void ModulatedDelayEffect::setSampleRate(int sampleRate)
{
    if (sampleRate > 0 && sampleRate != m_sampleRate) {
        m_sampleRate = sampleRate;
        m_buffer.assign(static_cast<std::size_t>(sampleRate * 0.2 * 2), 0.0f);
        m_writeIndex = 0;
    }
}

void ModulatedDelayEffect::setWetFraction(double wetFraction)
{
    m_wetFraction = std::clamp(wetFraction, 0.0, 1.0);
}

void ModulatedDelayEffect::setParameters(double baseDelayMs, double depthMs, double lfoRateHz, double wetFraction)
{
    m_baseDelayMs = baseDelayMs;
    m_depthMs = depthMs;
    m_lfoRateHz = lfoRateHz;
    m_wetFraction = std::clamp(wetFraction, 0.0, 1.0);
}

void ModulatedDelayEffect::reset()
{
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
    m_writeIndex = 0;
    m_lfoPhase = 0.0;
}

void ModulatedDelayEffect::processInterleaved(float *samples, std::size_t frameCount, int channels)
{
    if (m_wetFraction <= 0.001 || channels < 2) {
        return;
    }

    const std::size_t totalFrames = m_buffer.size() / 2;
    if (totalFrames == 0) return;

    const double lfoInc = 2.0 * std::numbers::pi * m_lfoRateHz / static_cast<double>(m_sampleRate);

    for (std::size_t f = 0; f < frameCount; ++f) {
        m_lfoPhase += lfoInc;
        if (m_lfoPhase > 2.0 * std::numbers::pi) {
            m_lfoPhase -= 2.0 * std::numbers::pi;
        }

        const double modDelayMs = m_baseDelayMs + m_depthMs * std::sin(m_lfoPhase);
        const double modDelayFrames = modDelayMs * 0.001 * m_sampleRate;
        const double readPos = static_cast<double>(m_writeIndex) - modDelayFrames;
        double wrappedPos = readPos;
        while (wrappedPos < 0.0) wrappedPos += static_cast<double>(totalFrames);
        while (wrappedPos >= static_cast<double>(totalFrames)) wrappedPos -= static_cast<double>(totalFrames);

        const std::size_t i0 = static_cast<std::size_t>(wrappedPos);
        const std::size_t i1 = (i0 + 1) % totalFrames;
        const float frac = static_cast<float>(wrappedPos - static_cast<double>(i0));

        for (int c = 0; c < 2; ++c) {
            const float in = samples[f * channels + c];
            const float d0 = m_buffer[i0 * 2 + c];
            const float d1 = m_buffer[i1 * 2 + c];
            const float delayed = d0 + frac * (d1 - d0);

            m_buffer[m_writeIndex * 2 + c] = in + static_cast<float>(m_feedback) * delayed;
            samples[f * channels + c] = in * static_cast<float>(1.0 - m_wetFraction * 0.5) + delayed * static_cast<float>(m_wetFraction * 0.5);
        }

        if (++m_writeIndex >= totalFrames) {
            m_writeIndex = 0;
        }
    }
}

// --- SimpleReverb ---

SimpleReverb::SimpleReverb(int sampleRate)
    : m_sampleRate(sampleRate)
{
    setSampleRate(sampleRate);
}

void SimpleReverb::setSampleRate(int sampleRate)
{
    m_sampleRate = (sampleRate > 0) ? sampleRate : 48000;
    const float scale = static_cast<float>(m_sampleRate) / 44100.0f;
    m_combs[0].init(static_cast<std::size_t>(1116 * scale), 0.84f, 0.2f);
    m_combs[1].init(static_cast<std::size_t>(1188 * scale), 0.84f, 0.2f);
    m_combs[2].init(static_cast<std::size_t>(1277 * scale), 0.84f, 0.2f);
    m_combs[3].init(static_cast<std::size_t>(1356 * scale), 0.84f, 0.2f);
    m_allpasses[0].init(static_cast<std::size_t>(556 * scale), 0.5f);
    m_allpasses[1].init(static_cast<std::size_t>(441 * scale), 0.5f);
}

void SimpleReverb::setWetFraction(double wetFraction)
{
    m_wetFraction = std::clamp(wetFraction, 0.0, 1.0);
}

void SimpleReverb::setParameters(double roomSize, double damp, double wetFraction)
{
    (void)roomSize;
    (void)damp;
    m_wetFraction = std::clamp(wetFraction, 0.0, 1.0);
}

void SimpleReverb::reset()
{
    setSampleRate(m_sampleRate);
}

void SimpleReverb::processInterleaved(float *samples, std::size_t frameCount, int channels)
{
    if (m_wetFraction <= 0.001 || channels < 2) {
        return;
    }

    const float dryGain = static_cast<float>(1.0 - m_wetFraction * 0.3);
    const float wetGain = static_cast<float>(m_wetFraction * 0.4);

    for (std::size_t f = 0; f < frameCount; ++f) {
        const float inL = samples[f * channels + 0];
        const float inR = samples[f * channels + 1];
        const float monoIn = (inL + inR) * 0.5f;

        float combSum = 0.0f;
        for (int i = 0; i < 4; ++i) {
            combSum += m_combs[i].process(monoIn);
        }

        float revOut = combSum * 0.25f;
        revOut = m_allpasses[0].process(revOut);
        revOut = m_allpasses[1].process(revOut);

        samples[f * channels + 0] = inL * dryGain + revOut * wetGain;
        samples[f * channels + 1] = inR * dryGain + revOut * wetGain;
    }
}

// --- StereoProcessor ---

void StereoProcessor::applyStereoWidth(float *samples, std::size_t frameCount, double widthMultiplier)
{
    if (std::abs(widthMultiplier - 1.0) < 0.001) {
        return;
    }

    const float width = static_cast<float>(std::clamp(widthMultiplier, 1.0, 5.0));
    for (std::size_t f = 0; f < frameCount; ++f) {
        const float left = samples[f * 2 + 0];
        const float right = samples[f * 2 + 1];
        const float mid = 0.5f * (left + right);
        const float side = 0.5f * (left - right);
        const float sideExpanded = side * width;
        samples[f * 2 + 0] = mid + sideExpanded;
        samples[f * 2 + 1] = mid - sideExpanded;
    }
}

void StereoProcessor::applyBalance(float *samples, std::size_t frameCount, double balance)
{
    if (std::abs(balance) < 0.001) {
        return;
    }

    const float bal = static_cast<float>(std::clamp(balance, -1.0, 1.0));
    const float leftGain = std::min(1.0f, 1.0f - bal);
    const float rightGain = std::min(1.0f, 1.0f + bal);

    for (std::size_t f = 0; f < frameCount; ++f) {
        samples[f * 2 + 0] *= leftGain;
        samples[f * 2 + 1] *= rightGain;
    }
}

void StereoProcessor::applyVoiceSuppression(float *samples, std::size_t frameCount, bool enabled)
{
    if (!enabled) {
        return;
    }

    for (std::size_t f = 0; f < frameCount; ++f) {
        const float left = samples[f * 2 + 0];
        const float right = samples[f * 2 + 1];
        const float diff = 0.5f * (left - right);
        samples[f * 2 + 0] = diff;
        samples[f * 2 + 1] = -diff;
    }
}

void StereoProcessor::applyPeakLimiter(float *samples, std::size_t frameCount, int channels, float threshold)
{
    const float knee = 0.75f * std::clamp(threshold, 0.5f, 1.0f);
    const float margin = 1.0f - knee;
    for (std::size_t f = 0; f < frameCount; ++f) {
        for (int c = 0; c < channels; ++c) {
            float v = samples[f * channels + c];
            const float absV = std::abs(v);
            if (absV > knee) {
                const float excess = (absV - knee) / margin;
                const float compressed = knee + margin * std::tanh(excess);
                v = (v >= 0.0f) ? compressed : -compressed;
            }
            samples[f * channels + c] = std::clamp(v, -1.0f, 1.0f);
        }
    }
}

// --- SilenceDetector ---

SilenceDetector::SilenceDetector(int sampleRate)
    : m_sampleRate(sampleRate)
{
    setThresholdDbfs(m_thresholdDbfs);
}

void SilenceDetector::setSampleRate(int sampleRate)
{
    if (sampleRate > 0) {
        m_sampleRate = sampleRate;
    }
}

void SilenceDetector::setThresholdDbfs(double thresholdDbfs)
{
    m_thresholdDbfs = std::clamp(thresholdDbfs, -90.0, -20.0);
    // Linear amplitude: 10^(dBFS / 20)
    m_linearThreshold = std::pow(10.0, m_thresholdDbfs / 20.0);
}

void SilenceDetector::setMinimumDurationMs(int minDurationMs)
{
    m_minDurationMs = std::clamp(minDurationMs, 50, 5000);
}

void SilenceDetector::setTrimEdges(bool trimEdges)
{
    m_trimEdges = trimEdges;
}

void SilenceDetector::reset()
{
    m_consecutiveSilenceMs = 0;
    m_isQualifiedSilence = false;
}

bool SilenceDetector::processFrameChunk(const float *samples, std::size_t frameCount, int channels)
{
    if (frameCount == 0 || channels <= 0) {
        return false;
    }

    double sumSq = 0.0;
    const std::size_t totalSamples = frameCount * channels;
    for (std::size_t i = 0; i < totalSamples; ++i) {
        const double s = samples[i];
        sumSq += s * s;
    }

    const double rms = std::sqrt(sumSq / static_cast<double>(totalSamples));
    const bool isSilentChunk = (rms < m_linearThreshold);

    const double chunkMs = (static_cast<double>(frameCount) / static_cast<double>(m_sampleRate)) * 1000.0;
    if (isSilentChunk) {
        m_consecutiveSilenceMs += static_cast<qint64>(chunkMs);
        if (m_consecutiveSilenceMs >= m_minDurationMs) {
            m_isQualifiedSilence = true;
        }
    } else {
        m_consecutiveSilenceMs = 0;
        m_isQualifiedSilence = false;
    }

    return isSilentChunk;
}

std::vector<SilenceDetector::SilenceInterval> SilenceDetector::detectSilence(const float *samples, std::size_t totalFrames, int channels)
{
    std::vector<SilenceInterval> intervals;
    if (!samples || totalFrames == 0 || channels <= 0) {
        return intervals;
    }

    const std::size_t chunkSize = static_cast<std::size_t>(m_sampleRate * 0.01); // 10ms chunks
    const std::size_t minSilenceFrames = static_cast<std::size_t>((static_cast<double>(m_minDurationMs) / 1000.0) * m_sampleRate);

    std::size_t currentSilenceStart = 0;
    bool inSilence = false;

    for (std::size_t frame = 0; frame < totalFrames; frame += chunkSize) {
        const std::size_t curChunk = std::min(chunkSize, totalFrames - frame);
        double sumSq = 0.0;
        const std::size_t sampleOffset = frame * channels;
        const std::size_t totalSamplesInChunk = curChunk * channels;
        for (std::size_t i = 0; i < totalSamplesInChunk; ++i) {
            const double s = samples[sampleOffset + i];
            sumSq += s * s;
        }
        const double rms = std::sqrt(sumSq / static_cast<double>(totalSamplesInChunk));
        const bool isSilent = (rms < m_linearThreshold);

        if (isSilent) {
            if (!inSilence) {
                inSilence = true;
                currentSilenceStart = frame;
            }
        } else {
            if (inSilence) {
                const std::size_t silenceDurationFrames = frame - currentSilenceStart;
                const bool isEdge = (currentSilenceStart == 0);
                if (silenceDurationFrames >= minSilenceFrames || (isEdge && m_trimEdges)) {
                    intervals.push_back({currentSilenceStart, frame});
                }
                inSilence = false;
            }
        }
    }

    if (inSilence) {
        const std::size_t silenceDurationFrames = totalFrames - currentSilenceStart;
        const bool isEdge = true;
        if (silenceDurationFrames >= minSilenceFrames || (isEdge && m_trimEdges)) {
            intervals.push_back({currentSilenceStart, totalFrames});
        }
    }

    return intervals;
}

// --- GainRamp ---

GainRamp::GainRamp(int sampleRate)
    : m_sampleRate(sampleRate)
{
}

void GainRamp::setSampleRate(int sampleRate)
{
    if (sampleRate > 0) {
        m_sampleRate = sampleRate;
    }
}

void GainRamp::setGainInstant(double gain)
{
    m_currentGain = std::clamp(gain, 0.0, 2.0);
    m_targetGain = m_currentGain;
    m_step = 0.0;
    m_framesRemaining = 0;
}

void GainRamp::rampTo(double targetGain, int durationMs)
{
    m_targetGain = std::clamp(targetGain, 0.0, 2.0);
    if (durationMs <= 0) {
        setGainInstant(m_targetGain);
        return;
    }

    const std::size_t totalFrames = static_cast<std::size_t>((static_cast<double>(durationMs) / 1000.0) * m_sampleRate);
    if (totalFrames == 0) {
        setGainInstant(m_targetGain);
        return;
    }

    m_framesRemaining = totalFrames;
    m_step = (m_targetGain - m_currentGain) / static_cast<double>(totalFrames);
}

void GainRamp::processInterleaved(float *samples, std::size_t frameCount, int channels)
{
    if (!samples || frameCount == 0 || channels <= 0) {
        return;
    }

    if (m_framesRemaining == 0 && std::abs(m_currentGain - 1.0) < 0.0001) {
        return; // bypass unity gain
    }

    for (std::size_t f = 0; f < frameCount; ++f) {
        if (m_framesRemaining > 0) {
            m_currentGain += m_step;
            --m_framesRemaining;
            if (m_framesRemaining == 0) {
                m_currentGain = m_targetGain;
                m_step = 0.0;
            }
        }
        const float g = static_cast<float>(m_currentGain);
        for (int c = 0; c < channels; ++c) {
            samples[f * channels + c] *= g;
        }
    }
}

// --- FormatQualitySimulator ---

namespace {

void updateLowpassCoeffs(BiquadState &state, double fc, double Q, int sampleRate)
{
    if (fc >= sampleRate * 0.48) {
        state.b0 = 1.0; state.b1 = 0.0; state.b2 = 0.0;
        state.a1 = 0.0; state.a2 = 0.0;
        return;
    }
    const double w0 = 2.0 * std::numbers::pi * std::clamp(fc, 20.0, sampleRate * 0.48) / static_cast<double>(sampleRate);
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * Q);

    const double b0 = (1.0 - cosw0) * 0.5;
    const double b1 = 1.0 - cosw0;
    const double b2 = (1.0 - cosw0) * 0.5;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha;

    const double invA0 = (std::abs(a0) > 1e-12) ? (1.0 / a0) : 1.0;
    state.b0 = b0 * invA0;
    state.b1 = b1 * invA0;
    state.b2 = b2 * invA0;
    state.a1 = a1 * invA0;
    state.a2 = a2 * invA0;
}

void updatePeakingCoeffs(BiquadState &state, double f0, double gainDb, double Q, int sampleRate)
{
    if (std::abs(gainDb) < 0.05 || f0 >= sampleRate * 0.48) {
        state.b0 = 1.0; state.b1 = 0.0; state.b2 = 0.0;
        state.a1 = 0.0; state.a2 = 0.0;
        return;
    }
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * std::min(f0, sampleRate * 0.48) / static_cast<double>(sampleRate);
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * Q);

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * cosw0;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha / A;

    const double invA0 = (std::abs(a0) > 1e-12) ? (1.0 / a0) : 1.0;
    state.b0 = b0 * invA0;
    state.b1 = b1 * invA0;
    state.b2 = b2 * invA0;
    state.a1 = a1 * invA0;
    state.a2 = a2 * invA0;
}

inline void processBiquad(BiquadState &state, float *samples, std::size_t frameCount, int channels)
{
    if (std::abs(state.b0 - 1.0) < 1e-6 && std::abs(state.b1) < 1e-6 && std::abs(state.b2) < 1e-6 &&
        std::abs(state.a1) < 1e-6 && std::abs(state.a2) < 1e-6) {
        return;
    }

    const int chCount = std::min(channels, 2);
    for (std::size_t i = 0; i < frameCount; ++i) {
        for (int ch = 0; ch < chCount; ++ch) {
            const double in = static_cast<double>(samples[i * channels + ch]);
            const double out = state.b0 * in + state.z1[ch];
            state.z1[ch] = state.b1 * in - state.a1 * out + state.z2[ch];
            state.z2[ch] = state.b2 * in - state.a2 * out;
            samples[i * channels + ch] = static_cast<float>(out);
        }
    }
}

const double kEqCenterFrequencies[10] = {31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};

} // namespace

FormatQualitySimulator::FormatQualitySimulator(int sinkSampleRate)
    : m_sinkSampleRate(sinkSampleRate > 0 ? sinkSampleRate : 48000)
    , m_format(QStringLiteral("mp3"))
    , m_isLossy(true)
    , m_targetBitrate(320)
    , m_targetSampleRate(44100)
    , m_channelMode(QStringLiteral("stereo"))
{
    updateFilters();
}

void FormatQualitySimulator::setSinkSampleRate(int sampleRate)
{
    if (sampleRate > 0 && sampleRate != m_sinkSampleRate) {
        m_sinkSampleRate = sampleRate;
        updateFilters();
        reset();
    }
}

void FormatQualitySimulator::setFormat(const QString &format, bool isLossy)
{
    if (m_format != format || m_isLossy != isLossy) {
        m_format = format;
        m_isLossy = isLossy;
        updateFilters();
    }
}

void FormatQualitySimulator::setTargetBitrate(int bitrateKbps)
{
    const int val = std::max(16, bitrateKbps);
    if (m_targetBitrate != val) {
        m_targetBitrate = val;
        updateFilters();
    }
}

void FormatQualitySimulator::setTargetSampleRate(int targetSampleRateHz)
{
    const int val = std::max(4000, targetSampleRateHz);
    if (m_targetSampleRate != val) {
        m_targetSampleRate = val;
        updateFilters();
    }
}

void FormatQualitySimulator::setChannelMode(const QString &channelMode)
{
    if (m_channelMode != channelMode) {
        m_channelMode = channelMode;
        updateFilters();
    }
}

void FormatQualitySimulator::setApplyEqualizer(bool enabled)
{
    if (m_applyEqualizer != enabled) {
        m_applyEqualizer = enabled;
        updateFilters();
    }
}

void FormatQualitySimulator::setEqualizerBandGains(const std::vector<double> &gainsDb)
{
    m_equalizerBandGains = gainsDb;
    updateFilters();
}

void FormatQualitySimulator::reset()
{
    m_lowpass1.reset();
    m_lowpass2.reset();
    for (int i = 0; i < 10; ++i) {
        m_eqBands[i].reset();
    }
    m_decimationPhase = 0;
    m_holdSample[0] = m_holdSample[1] = 0.0f;
}

void FormatQualitySimulator::updateFilters()
{
    // 1. Calculate effective lowpass cutoff
    double cutoff = static_cast<double>(m_sinkSampleRate) * 0.48;

    // Sample rate limit (Nyquist)
    if (m_targetSampleRate > 0) {
        cutoff = std::min(cutoff, static_cast<double>(m_targetSampleRate) * 0.45);
    }

    // Lossy bitrate frequency cutoff
    if (m_isLossy) {
        const int effectiveKbps = (m_channelMode == QStringLiteral("mono") || m_channelMode == QStringLiteral("1"))
            ? (m_targetBitrate * 2)
            : m_targetBitrate;
        double bitrateCutoff = 21000.0;
        if (effectiveKbps <= 32) {
            bitrateCutoff = 6500.0;
        } else if (effectiveKbps <= 48) {
            bitrateCutoff = 8500.0;
        } else if (effectiveKbps <= 64) {
            bitrateCutoff = 11000.0;
        } else if (effectiveKbps <= 96) {
            bitrateCutoff = 13500.0;
        } else if (effectiveKbps <= 128) {
            bitrateCutoff = 15500.0;
        } else if (effectiveKbps <= 160) {
            bitrateCutoff = 17000.0;
        } else if (effectiveKbps <= 192) {
            bitrateCutoff = 18500.0;
        } else if (effectiveKbps <= 256) {
            bitrateCutoff = 19500.0;
        } else {
            bitrateCutoff = 21000.0;
        }
        cutoff = std::min(cutoff, bitrateCutoff);
    }

    // 4th order Butterworth Lowpass (Q1 = 0.5412, Q2 = 1.3065)
    updateLowpassCoeffs(m_lowpass1, cutoff, 0.54119610, m_sinkSampleRate);
    updateLowpassCoeffs(m_lowpass2, cutoff, 1.3065630, m_sinkSampleRate);

    // 2. 10-band Equalizer coefficients
    for (int i = 0; i < 10; ++i) {
        double gain = 0.0;
        if (m_applyEqualizer && i < static_cast<int>(m_equalizerBandGains.size())) {
            gain = m_equalizerBandGains[i];
        }
        updatePeakingCoeffs(m_eqBands[i], kEqCenterFrequencies[i], gain, 1.414, m_sinkSampleRate);
    }
}

void FormatQualitySimulator::processInterleaved(float *samples, std::size_t frameCount, int channels)
{
    if (!samples || frameCount == 0 || channels <= 0) {
        return;
    }

    const bool isMono = (m_channelMode == QStringLiteral("mono") || m_channelMode == QStringLiteral("1"));

    // 1. Apply mono downmix if mono channel mode is chosen
    if (isMono && channels >= 2) {
        for (std::size_t i = 0; i < frameCount; ++i) {
            const float mono = (samples[i * channels] + samples[i * channels + 1]) * 0.5f;
            samples[i * channels] = mono;
            samples[i * channels + 1] = mono;
        }
    }

    // 2. Apply Equalizer if enabled
    if (m_applyEqualizer) {
        for (int b = 0; b < 10; ++b) {
            processBiquad(m_eqBands[b], samples, frameCount, channels);
        }
    }

    // 3. Apply Lowpass filter (sample rate + bitrate cutoff)
    processBiquad(m_lowpass1, samples, frameCount, channels);
    processBiquad(m_lowpass2, samples, frameCount, channels);

    // 4. Emulate discrete sample rate decimation if target rate < sink rate
    if (m_targetSampleRate > 0 && m_targetSampleRate < m_sinkSampleRate) {
        const int step = std::max(1, m_sinkSampleRate / m_targetSampleRate);
        if (step > 1) {
            for (std::size_t i = 0; i < frameCount; ++i) {
                if (m_decimationPhase % step == 0) {
                    m_holdSample[0] = samples[i * channels];
                    m_holdSample[1] = (channels > 1) ? samples[i * channels + 1] : m_holdSample[0];
                }
                samples[i * channels] = m_holdSample[0];
                if (channels > 1) {
                    samples[i * channels + 1] = m_holdSample[1];
                }
                ++m_decimationPhase;
            }
        }
    }

    // 5. Emulate lossy bitrate quantization if lossy format and bitrate < 320
    if (m_isLossy) {
        const int effKbps = isMono ? (m_targetBitrate * 2) : m_targetBitrate;
        if (effKbps < 320) {
            const double effectiveBits = 6.0 + 10.0 * (std::clamp(effKbps, 24, 320) - 24) / (320.0 - 24.0);
            const double steps = std::pow(2.0, effectiveBits);
            const double invSteps = 1.0 / steps;
            const std::size_t total = frameCount * channels;
            for (std::size_t i = 0; i < total; ++i) {
                samples[i] = static_cast<float>(std::round(samples[i] * steps) * invSteps);
            }
        }
    }
}

} // namespace WaveFlux::Dsp

