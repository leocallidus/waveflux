#ifndef OPENMPTWAVEFORMRENDERER_H
#define OPENMPTWAVEFORMRENDERER_H

#include <QVector>
#include <QString>

#include <atomic>
#include <functional>

namespace WaveFlux {

struct OpenMptWaveformRenderResult {
    QVector<float> peaks;
    qint64 durationMs = 0;
    bool success = false;
    bool canceled = false;
    QString errorMessage;
};

class OpenMptWaveformRenderer
{
public:
    using PartialCallback = std::function<void(QVector<float>, double)>;

    static constexpr int kRenderSampleRate = 22050;
    static constexpr int kDefaultWindowFrames = 512;

    static OpenMptWaveformRenderResult render(const QString &source,
                                              int targetSamples,
                                              const PartialCallback &partialCallback = {},
                                              const std::atomic_bool *cancelRequested = nullptr);
};

} // namespace WaveFlux

#endif // OPENMPTWAVEFORMRENDERER_H
