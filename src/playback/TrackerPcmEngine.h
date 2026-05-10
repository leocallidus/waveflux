#ifndef TRACKERPCMENGINE_H
#define TRACKERPCMENGINE_H

#include <QtGlobal>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace openmpt {
class module;
}

namespace WaveFlux {

struct TrackerPcmEngineSnapshot {
    qint64 positionMs = 0;
    qint64 decodePositionMs = 0;
    quint64 submittedFrames = 0;
    quint64 consumedFrames = 0;
    quint64 bufferedFrames = 0;
    quint64 activeGraphGeneration = 0;
    quint64 targetGraphGeneration = 0;
    quint64 lastFlushGeneration = 0;
    double playbackRate = 1.0;
    int pitchSemitones = 0;
    bool reversePlayback = false;
    bool reverseRatePitchStageConfigured = false;
    bool equalizerStageActive = false;
    int equalizerBandCount = 0;
    int pendingEqualizerBandCount = 0;
    int pitchRampBlocksRemaining = 0;
    int equalizerRampBlocksRemaining = 0;
    bool limiterStageActive = false;
    bool limiterEngaged = false;
    bool endOfStream = false;
};

class TrackerPcmEngine final
{
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannelCount = 2;
    static constexpr int kOutputBytesPerFrame =
        kChannelCount * static_cast<int>(sizeof(std::int16_t));

    explicit TrackerPcmEngine(int sampleRate = kSampleRate);
    ~TrackerPcmEngine();

    TrackerPcmEngine(const TrackerPcmEngine &) = delete;
    TrackerPcmEngine &operator=(const TrackerPcmEngine &) = delete;

    void load(std::unique_ptr<openmpt::module> module,
              std::unique_ptr<std::istream> stream,
              std::unique_ptr<std::ostream> logStream,
              qint64 durationMs);
    void clear();
    bool hasModule() const;
    qint64 seek(qint64 positionMs);
    qint64 resetToStart();
    void setPlaybackRate(double playbackRate);
    void setPitchSemitones(int pitchSemitones);
    void setReversePlayback(bool reversePlayback);
    void setEqualizerBands(const std::vector<double> &frequenciesHz,
                           const std::vector<double> &gainsDb);
    qint64 positionMs() const;
    qint64 durationMs() const;
    qint64 readInt16(char *data, qint64 maxSize);
    qint64 availableOutputBytes() const;
    bool isEndOfStream() const;
    bool supportsTimeStretch() const;
    bool supportsPitchShift() const;
    bool supportsReversePlayback() const;
    TrackerPcmEngineSnapshot snapshot() const;

private:
    struct TransportStageState {
        double playbackRate = 1.0;
        int pitchSemitones = 0;
        bool reversePlayback = false;
    };

    struct EqualizerBand {
        double frequencyHz = 0.0;
        double gainDb = 0.0;
        double targetGainDb = 0.0;
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1[2] = {0.0, 0.0};
        double z2[2] = {0.0, 0.0};
    };

    void ensureDecoderThread();
    void stopDecoderThread();
    void decoderLoop();
    std::size_t renderReverseWindowLocked(float *interleavedSamples, std::size_t maxFrames);
    void applyDspGraphLocked(float *interleavedSamples, std::size_t frames);
    void applyTransportStageLocked(float *interleavedSamples, std::size_t frames);
    void configureEqualizerLocked(const std::vector<double> &frequenciesHz,
                                  const std::vector<double> &gainsDb);
    void updateEqualizerTargetsLocked(const std::vector<double> &frequenciesHz,
                                      const std::vector<double> &gainsDb);
    void rebuildEqualizerBandCoefficients(EqualizerBand &band) const;
    void advanceEqualizerRampLocked();
    void resetEqualizerStateLocked();
    void applyEqualizerLocked(float *interleavedSamples, std::size_t frames);
    void applyOutputLimiterLocked(float *interleavedSamples, std::size_t frames);
    void resetDspGraphStateLocked();
    void clearRingLocked();
    qint64 positionFromClockLocked() const;
    qint64 sanitizePosition(qint64 positionMs) const;

    const int m_sampleRate;
    const std::size_t m_decodeChunkFrames = 1024;
    const std::size_t m_ringCapacityFrames;

    mutable std::mutex m_decodeMutex;
    std::unique_ptr<openmpt::module> m_module;
    std::unique_ptr<std::istream> m_stream;
    std::unique_ptr<std::ostream> m_logStream;
    qint64 m_durationMs = 0;
    qint64 m_decodePositionMs = 0;
    qint64 m_reverseCursorMs = 0;
    quint64 m_generation = 0;
    quint64 m_targetGraphGeneration = 0;
    quint64 m_activeGraphGeneration = 0;
    quint64 m_lastFlushGeneration = 0;
    bool m_limiterEngaged = false;
    std::vector<EqualizerBand> m_equalizerBands;
    std::vector<double> m_pendingEqualizerFrequenciesHz;
    std::vector<double> m_pendingEqualizerGainsDb;
    int m_equalizerRampBlocksRemaining = 0;
    static constexpr int kEqualizerRampBlockCount = 6;
    TransportStageState m_transportStage;

    mutable std::mutex m_ringMutex;
    std::condition_variable m_decodeCondition;
    std::vector<float> m_ring;
    std::size_t m_readFrame = 0;
    std::size_t m_writeFrame = 0;
    std::size_t m_bufferedFrames = 0;
    qint64 m_clockBaseMs = 0;
    quint64 m_submittedFrames = 0;
    quint64 m_consumedFrames = 0;
    bool m_endOfStream = false;

    std::thread m_decoderThread;
    std::atomic_bool m_decoderRunning {false};
};

} // namespace WaveFlux

#endif // TRACKERPCMENGINE_H
