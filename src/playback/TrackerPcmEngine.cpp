#include "playback/TrackerPcmEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include <libopenmpt/libopenmpt.hpp>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEqualizerQ = 1.1;
constexpr float kLimiterThreshold = 1.0f;

qint64 secondsToMs(double seconds)
{
    return std::max<qint64>(0, qRound64(seconds * 1000.0));
}

std::int16_t floatToInt16(float sample)
{
    const float clamped = std::clamp(sample, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lrintf(clamped * 32767.0f));
}

} // namespace

namespace WaveFlux {

TrackerPcmEngine::TrackerPcmEngine(int sampleRate)
    : m_sampleRate(sampleRate)
    , m_ringCapacityFrames(static_cast<std::size_t>(sampleRate) * 2u)
    , m_ring(m_ringCapacityFrames * kChannelCount, 0.0f)
{
}

TrackerPcmEngine::~TrackerPcmEngine()
{
    stopDecoderThread();
}

void TrackerPcmEngine::load(std::unique_ptr<openmpt::module> module,
                            std::unique_ptr<std::istream> stream,
                            std::unique_ptr<std::ostream> logStream,
                            qint64 durationMs)
{
    stopDecoderThread();

    {
        std::lock_guard<std::mutex> decodeLock(m_decodeMutex);
        m_module = std::move(module);
        m_stream = std::move(stream);
        m_logStream = std::move(logStream);
        m_durationMs = std::max<qint64>(0, durationMs);
        m_decodePositionMs = m_transportStage.reversePlayback ? m_durationMs : 0;
        m_reverseCursorMs = m_decodePositionMs;
        resetDspGraphStateLocked();
        ++m_generation;
    }

    {
        std::lock_guard<std::mutex> ringLock(m_ringMutex);
        clearRingLocked();
        m_clockBaseMs = m_transportStage.reversePlayback ? m_durationMs : 0;
        m_lastFlushGeneration = m_targetGraphGeneration;
    }

    ensureDecoderThread();
    m_decodeCondition.notify_all();
}

void TrackerPcmEngine::clear()
{
    stopDecoderThread();

    {
        std::lock_guard<std::mutex> decodeLock(m_decodeMutex);
        m_module.reset();
        m_stream.reset();
        m_logStream.reset();
        m_durationMs = 0;
        m_decodePositionMs = 0;
        m_reverseCursorMs = 0;
        resetDspGraphStateLocked();
        ++m_generation;
    }

    {
        std::lock_guard<std::mutex> ringLock(m_ringMutex);
        clearRingLocked();
        m_clockBaseMs = 0;
        m_lastFlushGeneration = m_targetGraphGeneration;
    }
}

bool TrackerPcmEngine::hasModule() const
{
    std::lock_guard<std::mutex> lock(m_decodeMutex);
    return m_module != nullptr;
}

qint64 TrackerPcmEngine::seek(qint64 positionMs)
{
    qint64 appliedPositionMs = 0;
    {
        std::lock_guard<std::mutex> decodeLock(m_decodeMutex);
        if (!m_module) {
            return 0;
        }

        appliedPositionMs = sanitizePosition(positionMs);
        try {
            appliedPositionMs = secondsToMs(
                m_module->set_position_seconds(appliedPositionMs / 1000.0));
        } catch (...) {
            appliedPositionMs = sanitizePosition(appliedPositionMs);
        }
        m_decodePositionMs = appliedPositionMs;
        m_reverseCursorMs = appliedPositionMs;
        resetDspGraphStateLocked();
        ++m_generation;
    }

    {
        std::lock_guard<std::mutex> ringLock(m_ringMutex);
        clearRingLocked();
        m_clockBaseMs = appliedPositionMs;
        m_lastFlushGeneration = m_targetGraphGeneration;
    }

    m_decodeCondition.notify_all();
    return appliedPositionMs;
}

qint64 TrackerPcmEngine::resetToStart()
{
    return seek(0);
}

void TrackerPcmEngine::setPlaybackRate(double playbackRate)
{
    Q_UNUSED(playbackRate);
}

void TrackerPcmEngine::setPitchSemitones(int pitchSemitones)
{
    Q_UNUSED(pitchSemitones);
}

void TrackerPcmEngine::setReversePlayback(bool reversePlayback)
{
    Q_UNUSED(reversePlayback);
}

void TrackerPcmEngine::setEqualizerBands(const std::vector<double> &frequenciesHz,
                                         const std::vector<double> &gainsDb)
{
    {
        std::lock_guard<std::mutex> decodeLock(m_decodeMutex);
        updateEqualizerTargetsLocked(frequenciesHz, gainsDb);
    }

    m_decodeCondition.notify_all();
}

qint64 TrackerPcmEngine::positionMs() const
{
    std::lock_guard<std::mutex> lock(m_ringMutex);
    return positionFromClockLocked();
}

qint64 TrackerPcmEngine::durationMs() const
{
    std::lock_guard<std::mutex> lock(m_decodeMutex);
    return m_durationMs;
}

qint64 TrackerPcmEngine::readInt16(char *data, qint64 maxSize)
{
    if (!data || maxSize < kOutputBytesPerFrame) {
        return 0;
    }

    const std::size_t framesRequested =
        static_cast<std::size_t>(maxSize / kOutputBytesPerFrame);
    std::size_t framesRead = 0;

    {
        std::lock_guard<std::mutex> lock(m_ringMutex);
        const std::size_t framesToRead = std::min(framesRequested, m_bufferedFrames);
        auto *out = reinterpret_cast<std::int16_t *>(data);

        for (std::size_t frame = 0; frame < framesToRead; ++frame) {
            const std::size_t ringFrame = (m_readFrame + frame) % m_ringCapacityFrames;
            const std::size_t ringSample = ringFrame * kChannelCount;
            out[frame * kChannelCount] = floatToInt16(m_ring[ringSample]);
            out[frame * kChannelCount + 1] = floatToInt16(m_ring[ringSample + 1]);
        }

        m_readFrame = (m_readFrame + framesToRead) % m_ringCapacityFrames;
        m_bufferedFrames -= framesToRead;
        m_consumedFrames += framesToRead;
        framesRead = framesToRead;
    }

    if (framesRead > 0) {
        m_decodeCondition.notify_all();
    }

    return static_cast<qint64>(framesRead) * kOutputBytesPerFrame;
}

qint64 TrackerPcmEngine::availableOutputBytes() const
{
    std::lock_guard<std::mutex> lock(m_ringMutex);
    return static_cast<qint64>(m_bufferedFrames) * kOutputBytesPerFrame;
}

bool TrackerPcmEngine::isEndOfStream() const
{
    std::lock_guard<std::mutex> lock(m_ringMutex);
    return m_endOfStream && m_bufferedFrames == 0;
}

bool TrackerPcmEngine::supportsTimeStretch() const
{
    return false;
}

bool TrackerPcmEngine::supportsPitchShift() const
{
    return false;
}

bool TrackerPcmEngine::supportsReversePlayback() const
{
    return false;
}

TrackerPcmEngineSnapshot TrackerPcmEngine::snapshot() const
{
    TrackerPcmEngineSnapshot result;

    {
        std::lock_guard<std::mutex> decodeLock(m_decodeMutex);
        result.decodePositionMs = m_decodePositionMs;
        result.activeGraphGeneration = m_activeGraphGeneration;
        result.targetGraphGeneration = m_targetGraphGeneration;
        result.playbackRate = m_transportStage.playbackRate;
        result.pitchSemitones = m_transportStage.pitchSemitones;
        result.reversePlayback = m_transportStage.reversePlayback;
        result.reverseRatePitchStageConfigured =
            m_transportStage.reversePlayback
            || !qFuzzyCompare(m_transportStage.playbackRate, 1.0)
            || m_transportStage.pitchSemitones != 0;
        result.equalizerStageActive = !m_equalizerBands.empty();
        result.equalizerBandCount = static_cast<int>(m_equalizerBands.size());
        result.pendingEqualizerBandCount =
            static_cast<int>(std::min(m_pendingEqualizerFrequenciesHz.size(),
                                      m_pendingEqualizerGainsDb.size()));
        result.equalizerRampBlocksRemaining = m_equalizerRampBlocksRemaining;
        result.limiterStageActive = true;
        result.limiterEngaged = m_limiterEngaged;
    }

    {
        std::lock_guard<std::mutex> ringLock(m_ringMutex);
        result.positionMs = positionFromClockLocked();
        result.submittedFrames = m_submittedFrames;
        result.consumedFrames = m_consumedFrames;
        result.bufferedFrames = m_bufferedFrames;
        result.lastFlushGeneration = m_lastFlushGeneration;
        result.endOfStream = m_endOfStream && m_bufferedFrames == 0;
    }

    return result;
}

void TrackerPcmEngine::ensureDecoderThread()
{
    if (m_decoderRunning.load(std::memory_order_acquire)) {
        return;
    }

    m_decoderRunning.store(true, std::memory_order_release);
    m_decoderThread = std::thread(&TrackerPcmEngine::decoderLoop, this);
}

void TrackerPcmEngine::stopDecoderThread()
{
    if (!m_decoderRunning.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    m_decodeCondition.notify_all();
    if (m_decoderThread.joinable()) {
        m_decoderThread.join();
    }
}

void TrackerPcmEngine::decoderLoop()
{
    std::vector<float> decodeBuffer(m_decodeChunkFrames * kChannelCount, 0.0f);
    std::vector<float> reverseBuffer(m_decodeChunkFrames * kChannelCount, 0.0f);

    while (m_decoderRunning.load(std::memory_order_acquire)) {
        std::size_t framesToDecode = 0;
        {
            std::unique_lock<std::mutex> ringLock(m_ringMutex);
            m_decodeCondition.wait(ringLock, [this]() {
                return !m_decoderRunning.load(std::memory_order_acquire)
                    || (m_bufferedFrames + m_decodeChunkFrames <= m_ringCapacityFrames
                        && !m_endOfStream);
            });

            if (!m_decoderRunning.load(std::memory_order_acquire)) {
                break;
            }

            framesToDecode = std::min(m_decodeChunkFrames,
                                      m_ringCapacityFrames - m_bufferedFrames);
        }

        if (framesToDecode == 0) {
            continue;
        }

        std::size_t renderedFrames = 0;
        std::size_t outputFrames = 0;
        qint64 decodedPositionMs = 0;
        quint64 decodeGeneration = 0;
        bool reachedEndOfStream = false;
        qint64 appliedSeekPositionMs = -1;
        {
            std::lock_guard<std::mutex> decodeLock(m_decodeMutex);
            if (!m_module) {
                std::unique_lock<std::mutex> ringLock(m_ringMutex);
                m_decodeCondition.wait_for(ringLock, std::chrono::milliseconds(10));
                continue;
            }

            decodeGeneration = m_generation;
            try {
                if (m_transportStage.reversePlayback) {
                    outputFrames = renderReverseWindowLocked(reverseBuffer.data(), framesToDecode);
                    decodedPositionMs = m_reverseCursorMs;
                    m_decodePositionMs = decodedPositionMs;
                    if (outputFrames == 0) {
                        reachedEndOfStream = true;
                    } else {
                        applyEqualizerLocked(reverseBuffer.data(), outputFrames);
                        applyOutputLimiterLocked(reverseBuffer.data(), outputFrames);
                        reachedEndOfStream = (m_reverseCursorMs <= 0);
                        m_activeGraphGeneration = m_targetGraphGeneration;
                    }
                } else {
                    renderedFrames = m_module->read_interleaved_stereo(
                        m_sampleRate,
                        framesToDecode,
                        decodeBuffer.data());
                    decodedPositionMs = secondsToMs(m_module->get_position_seconds());
                    m_decodePositionMs = decodedPositionMs;
                    if (renderedFrames == 0) {
                        reachedEndOfStream = true;
                    } else {
                        applyDspGraphLocked(decodeBuffer.data(), renderedFrames);
                        outputFrames = renderedFrames;
                        m_activeGraphGeneration = m_targetGraphGeneration;
                    }
                }
            } catch (...) {
                renderedFrames = 0;
                outputFrames = 0;
                m_decodePositionMs = m_transportStage.reversePlayback ? 0 : m_durationMs;
                reachedEndOfStream = true;
            }
        }

        std::lock_guard<std::mutex> ringLock(m_ringMutex);
        if (decodeGeneration != m_generation) {
            continue;
        }

        if (appliedSeekPositionMs >= 0) {
            m_clockBaseMs = appliedSeekPositionMs;
        }

        if (outputFrames == 0 && reachedEndOfStream) {
            m_endOfStream = true;
            continue;
        }

        const float *graphOutput = reverseBuffer.data();
        if (!m_transportStage.reversePlayback) {
            graphOutput = decodeBuffer.data();
        }
        for (std::size_t frame = 0; frame < outputFrames; ++frame) {
            const std::size_t ringFrame = (m_writeFrame + frame) % m_ringCapacityFrames;
            const std::size_t dst = ringFrame * kChannelCount;
            const std::size_t src = frame * kChannelCount;
            m_ring[dst] = graphOutput[src];
            m_ring[dst + 1] = graphOutput[src + 1];
        }
        m_writeFrame = (m_writeFrame + outputFrames) % m_ringCapacityFrames;
        m_bufferedFrames += outputFrames;
        m_submittedFrames += outputFrames;

        if (reachedEndOfStream) {
            m_endOfStream = true;
        }
    }
}

std::size_t TrackerPcmEngine::renderReverseWindowLocked(float *interleavedSamples,
                                                        std::size_t maxFrames)
{
    if (!m_module || !interleavedSamples || maxFrames == 0 || m_reverseCursorMs <= 0) {
        return 0;
    }

    const std::size_t scratchFrames = std::max<std::size_t>(
        maxFrames,
        static_cast<std::size_t>(m_sampleRate / 4));
    const qint64 windowDurationMs = qMax<qint64>(
        1,
        static_cast<qint64>((scratchFrames * 1000ull) / static_cast<quint64>(m_sampleRate)));
    const qint64 windowEndMs = sanitizePosition(m_reverseCursorMs);
    const qint64 windowStartMs = qMax<qint64>(0, windowEndMs - windowDurationMs);
    m_module->set_position_seconds(windowStartMs / 1000.0);

    std::vector<float> forward(scratchFrames * kChannelCount, 0.0f);
    std::size_t totalFrames = 0;

    while (totalFrames < scratchFrames) {
        const double currentSeconds = m_module->get_position_seconds();
        if (secondsToMs(currentSeconds) >= windowEndMs) {
            break;
        }

        const std::size_t framesAvailable = scratchFrames - totalFrames;
        const std::size_t framesRead = m_module->read_interleaved_stereo(
            m_sampleRate,
            framesAvailable,
            forward.data() + totalFrames * kChannelCount);
        if (framesRead == 0) {
            break;
        }
        totalFrames += framesRead;
    }

    const std::size_t outputFrames = std::min(totalFrames, maxFrames);
    for (std::size_t frame = 0; frame < outputFrames; ++frame) {
        const std::size_t srcFrame = totalFrames - 1 - frame;
        interleavedSamples[frame * kChannelCount] = forward[srcFrame * kChannelCount];
        interleavedSamples[frame * kChannelCount + 1] =
            forward[srcFrame * kChannelCount + 1];
    }

    const qint64 outputDurationMs = qMax<qint64>(
        1,
        static_cast<qint64>((outputFrames * 1000ull) / static_cast<quint64>(m_sampleRate)));
    m_reverseCursorMs = qMax<qint64>(0, windowEndMs - outputDurationMs);
    m_decodePositionMs = m_reverseCursorMs;
    return outputFrames;
}

void TrackerPcmEngine::applyDspGraphLocked(float *interleavedSamples, std::size_t frames)
{
    if (!interleavedSamples || frames == 0) {
        return;
    }

    applyTransportStageLocked(interleavedSamples, frames);
    advanceEqualizerRampLocked();
    applyEqualizerLocked(interleavedSamples, frames);
    applyOutputLimiterLocked(interleavedSamples, frames);
}

void TrackerPcmEngine::applyTransportStageLocked(float *interleavedSamples, std::size_t frames)
{
    Q_UNUSED(interleavedSamples);
    Q_UNUSED(frames);
    // Stage 4 wires playback-rate time-stretch through an out-of-band transport stage in the
    // decoder loop because the frame count changes there. The slot stays here to keep the graph
    // order explicit for reverse / pitch work in later stages.
}

void TrackerPcmEngine::configureEqualizerLocked(const std::vector<double> &frequenciesHz,
                                                const std::vector<double> &gainsDb)
{
    m_equalizerBands.clear();
    const std::size_t count = std::min(frequenciesHz.size(), gainsDb.size());
    m_equalizerBands.reserve(count);

    const double nyquist = static_cast<double>(m_sampleRate) * 0.5;
    for (std::size_t i = 0; i < count; ++i) {
        const double gainDb = std::clamp(gainsDb[i], -24.0, 12.0);
        const double frequencyHz = std::clamp(frequenciesHz[i], 10.0, nyquist * 0.95);

        EqualizerBand band;
        band.frequencyHz = frequencyHz;
        band.gainDb = gainDb;
        band.targetGainDb = gainDb;
        rebuildEqualizerBandCoefficients(band);
        m_equalizerBands.push_back(band);
    }
}

void TrackerPcmEngine::updateEqualizerTargetsLocked(const std::vector<double> &frequenciesHz,
                                                    const std::vector<double> &gainsDb)
{
    std::vector<double> normalizedFrequenciesHz;
    std::vector<double> normalizedGainsDb;
    const std::size_t count = std::min(frequenciesHz.size(), gainsDb.size());
    normalizedFrequenciesHz.reserve(count);
    normalizedGainsDb.reserve(count);

    const double nyquist = static_cast<double>(m_sampleRate) * 0.5;
    for (std::size_t i = 0; i < count; ++i) {
        normalizedFrequenciesHz.push_back(std::clamp(frequenciesHz[i], 10.0, nyquist * 0.95));
        normalizedGainsDb.push_back(std::clamp(gainsDb[i], -24.0, 12.0));
    }

    const bool frequenciesChanged =
        normalizedFrequenciesHz.size() != m_equalizerBands.size()
        || !std::equal(normalizedFrequenciesHz.begin(),
                       normalizedFrequenciesHz.end(),
                       m_equalizerBands.begin(),
                       [](double frequencyHz, const EqualizerBand &band) {
                           return qFuzzyCompare(1.0 + frequencyHz, 1.0 + band.frequencyHz);
                       });

    m_pendingEqualizerFrequenciesHz = normalizedFrequenciesHz;
    m_pendingEqualizerGainsDb = normalizedGainsDb;
    ++m_targetGraphGeneration;

    if (frequenciesChanged || m_equalizerBands.empty()) {
        configureEqualizerLocked(m_pendingEqualizerFrequenciesHz, m_pendingEqualizerGainsDb);
        resetEqualizerStateLocked();
        m_equalizerRampBlocksRemaining = 0;
        return;
    }

    bool gainsChanged = false;
    for (std::size_t i = 0; i < normalizedGainsDb.size(); ++i) {
        m_equalizerBands[i].targetGainDb = normalizedGainsDb[i];
        gainsChanged = gainsChanged
            || !qFuzzyCompare(1.0 + m_equalizerBands[i].gainDb, 1.0 + normalizedGainsDb[i]);
    }

    if (gainsChanged) {
        m_equalizerRampBlocksRemaining = kEqualizerRampBlockCount;
    }
}

void TrackerPcmEngine::rebuildEqualizerBandCoefficients(EqualizerBand &band) const
{
    band.b0 = 1.0;
    band.b1 = 0.0;
    band.b2 = 0.0;
    band.a1 = 0.0;
    band.a2 = 0.0;

    if (std::abs(band.gainDb) < 0.01) {
        return;
    }

    const double omega = 2.0 * kPi * band.frequencyHz / static_cast<double>(m_sampleRate);
    const double sinOmega = std::sin(omega);
    const double cosOmega = std::cos(omega);
    const double alpha = sinOmega / (2.0 * kEqualizerQ);
    const double amplitude = std::pow(10.0, band.gainDb / 40.0);

    const double b0 = 1.0 + alpha * amplitude;
    const double b1 = -2.0 * cosOmega;
    const double b2 = 1.0 - alpha * amplitude;
    const double a0 = 1.0 + alpha / amplitude;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha / amplitude;

    band.b0 = b0 / a0;
    band.b1 = b1 / a0;
    band.b2 = b2 / a0;
    band.a1 = a1 / a0;
    band.a2 = a2 / a0;
}

void TrackerPcmEngine::advanceEqualizerRampLocked()
{
    if (m_equalizerRampBlocksRemaining <= 0 || m_equalizerBands.empty()) {
        return;
    }

    const int remainingBlocks = m_equalizerRampBlocksRemaining;
    for (EqualizerBand &band : m_equalizerBands) {
        if (qFuzzyCompare(1.0 + band.gainDb, 1.0 + band.targetGainDb)) {
            continue;
        }

        band.gainDb += (band.targetGainDb - band.gainDb) / static_cast<double>(remainingBlocks);
        if (remainingBlocks == 1) {
            band.gainDb = band.targetGainDb;
        }
        rebuildEqualizerBandCoefficients(band);
    }

    --m_equalizerRampBlocksRemaining;
}

void TrackerPcmEngine::resetEqualizerStateLocked()
{
    for (EqualizerBand &band : m_equalizerBands) {
        band.z1[0] = 0.0;
        band.z1[1] = 0.0;
        band.z2[0] = 0.0;
        band.z2[1] = 0.0;
    }
}

void TrackerPcmEngine::applyEqualizerLocked(float *interleavedSamples, std::size_t frames)
{
    if (!interleavedSamples || frames == 0 || m_equalizerBands.empty()) {
        return;
    }

    for (EqualizerBand &band : m_equalizerBands) {
        if (std::abs(band.gainDb) < 0.01) {
            continue;
        }

        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (int channel = 0; channel < kChannelCount; ++channel) {
                const std::size_t sampleIndex = frame * kChannelCount + static_cast<std::size_t>(channel);
                const double input = interleavedSamples[sampleIndex];
                const double output = band.b0 * input + band.z1[channel];
                band.z1[channel] = band.b1 * input - band.a1 * output + band.z2[channel];
                band.z2[channel] = band.b2 * input - band.a2 * output;
                interleavedSamples[sampleIndex] = static_cast<float>(std::clamp(output, -4.0, 4.0));
            }
        }
    }
}

void TrackerPcmEngine::applyOutputLimiterLocked(float *interleavedSamples, std::size_t frames)
{
    m_limiterEngaged = false;
    if (!interleavedSamples || frames == 0) {
        return;
    }

    const std::size_t sampleCount = frames * kChannelCount;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample = interleavedSamples[i];
        if (sample > kLimiterThreshold || sample < -kLimiterThreshold) {
            m_limiterEngaged = true;
            interleavedSamples[i] = std::clamp(sample, -kLimiterThreshold, kLimiterThreshold);
        }
    }
}

void TrackerPcmEngine::resetDspGraphStateLocked()
{
    resetEqualizerStateLocked();
    m_equalizerRampBlocksRemaining = 0;
    m_limiterEngaged = false;
}

void TrackerPcmEngine::clearRingLocked()
{
    m_readFrame = 0;
    m_writeFrame = 0;
    m_bufferedFrames = 0;
    m_submittedFrames = 0;
    m_consumedFrames = 0;
    m_endOfStream = false;
}

qint64 TrackerPcmEngine::positionFromClockLocked() const
{
    const qint64 consumedMs = static_cast<qint64>(
        (m_consumedFrames * 1000ull) / static_cast<quint64>(m_sampleRate));
    const qint64 timelineAdvanceMs = consumedMs;
    if (m_transportStage.reversePlayback) {
        return qBound<qint64>(0, m_clockBaseMs - timelineAdvanceMs, m_durationMs);
    }
    return qBound<qint64>(0, m_clockBaseMs + timelineAdvanceMs, m_durationMs);
}

qint64 TrackerPcmEngine::sanitizePosition(qint64 positionMs) const
{
    return qBound<qint64>(0, positionMs, m_durationMs);
}

} // namespace WaveFlux
