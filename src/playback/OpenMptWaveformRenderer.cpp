#include "playback/OpenMptWaveformRenderer.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

#include <libopenmpt/libopenmpt.hpp>

namespace {

constexpr int kChannelCount = 2;
constexpr int kDecodeChunkFrames = 4096;
constexpr int kMaxWindowFrames = 16 * 1024;
constexpr int kRawPeakSoftLimitMultiplier = 16;
constexpr int kRawPeakCompactTargetMultiplier = 8;

QString localPathFromSource(const QString &source)
{
    QString localPath = source.trimmed();
    if (localPath.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        const QUrl url(localPath);
        if (url.isLocalFile()) {
            localPath = url.toLocalFile();
        }
    }
    return localPath;
}

qint64 secondsToMs(double seconds)
{
    return std::max<qint64>(0, qRound64(seconds * 1000.0));
}

QVector<float> resampleMax(const QVector<float> &input, int targetSamples)
{
    if (input.isEmpty() || targetSamples <= 0) {
        return {};
    }

    if (input.size() <= targetSamples) {
        return input;
    }

    QVector<float> output(targetSamples);
    const float ratio = static_cast<float>(input.size()) / static_cast<float>(targetSamples);
    for (int i = 0; i < targetSamples; ++i) {
        const int start = static_cast<int>(i * ratio);
        const int end = std::min(static_cast<int>((i + 1) * ratio), static_cast<int>(input.size()));

        float peak = 0.0f;
        for (int j = start; j < end; ++j) {
            peak = std::max(peak, input[j]);
        }
        output[i] = peak;
    }
    return output;
}

void normalizePeaks(QVector<float> &peaks)
{
    if (peaks.isEmpty()) {
        return;
    }

    const float maxPeak = *std::max_element(peaks.begin(), peaks.end());
    if (maxPeak <= 0.0f || !std::isfinite(maxPeak)) {
        return;
    }

    for (float &peak : peaks) {
        peak /= maxPeak;
    }
}

bool sanitizePeaks(QVector<float> &peaks)
{
    if (peaks.isEmpty()) {
        return false;
    }

    for (float &peak : peaks) {
        if (!std::isfinite(peak)) {
            return false;
        }
        peak = std::clamp(peak, 0.0f, 1.0f);
    }
    return true;
}

int chooseWindowFrames(qint64 durationMs, int targetSamples)
{
    if (durationMs <= 0 || targetSamples <= 0) {
        return WaveFlux::OpenMptWaveformRenderer::kDefaultWindowFrames;
    }

    const double expectedFrames =
        (static_cast<double>(durationMs) / 1000.0) * WaveFlux::OpenMptWaveformRenderer::kRenderSampleRate;
    if (expectedFrames <= 0.0) {
        return WaveFlux::OpenMptWaveformRenderer::kDefaultWindowFrames;
    }

    const double desiredRawPeaks = static_cast<double>(targetSamples) * kRawPeakCompactTargetMultiplier;
    const int adaptiveWindow = static_cast<int>(std::ceil(expectedFrames / std::max(1.0, desiredRawPeaks)));
    return std::clamp(std::max(WaveFlux::OpenMptWaveformRenderer::kDefaultWindowFrames, adaptiveWindow),
                      WaveFlux::OpenMptWaveformRenderer::kDefaultWindowFrames,
                      kMaxWindowFrames);
}

void compactRawPeaksIfNeeded(QVector<float> &rawPeaks, int targetSamples)
{
    if (targetSamples <= 0 || rawPeaks.isEmpty()) {
        return;
    }

    const int softLimit = std::max(targetSamples * kRawPeakSoftLimitMultiplier, targetSamples);
    if (rawPeaks.size() <= softLimit) {
        return;
    }

    const int compactedTarget = std::max(targetSamples * kRawPeakCompactTargetMultiplier, targetSamples);
    rawPeaks = resampleMax(rawPeaks, compactedTarget);
}

} // namespace

namespace WaveFlux {

OpenMptWaveformRenderResult OpenMptWaveformRenderer::render(
    const QString &source,
    int targetSamples,
    const PartialCallback &partialCallback,
    const std::atomic_bool *cancelRequested)
{
    OpenMptWaveformRenderResult result;
    const auto isCanceled = [cancelRequested]() {
        return cancelRequested && cancelRequested->load(std::memory_order_relaxed);
    };

    if (isCanceled()) {
        result.canceled = true;
        return result;
    }

    const QString localPath = localPathFromSource(source);
    if (localPath.isEmpty()) {
        result.errorMessage = QStringLiteral("Tracker waveform source is empty");
        return result;
    }

    auto stream = std::make_unique<std::ifstream>(localPath.toStdString(), std::ios::binary);
    if (!stream->is_open()) {
        result.errorMessage = QStringLiteral("Tracker waveform file could not be opened for reading: %1")
            .arg(localPath);
        return result;
    }

    auto logStream = std::make_unique<std::ostringstream>();
    std::unique_ptr<openmpt::module> module;
    try {
        module = std::make_unique<openmpt::module>(*stream, *logStream);
        module->set_repeat_count(0);
        result.durationMs = secondsToMs(module->get_duration_seconds());
    } catch (const openmpt::exception &exception) {
        result.errorMessage = QStringLiteral("Tracker waveform load failed: %1")
            .arg(QString::fromUtf8(exception.what()));
        return result;
    } catch (const std::exception &exception) {
        result.errorMessage = QStringLiteral("Tracker waveform load failed: %1")
            .arg(QString::fromUtf8(exception.what()));
        return result;
    } catch (...) {
        result.errorMessage = QStringLiteral("Tracker waveform load failed: unknown libopenmpt error");
        return result;
    }

    const int windowFrames = chooseWindowFrames(result.durationMs, targetSamples);
    const double expectedFrames =
        result.durationMs > 0
            ? (static_cast<double>(result.durationMs) / 1000.0) * kRenderSampleRate
            : 0.0;

    QVector<float> rawPeaks;
    rawPeaks.reserve(std::max(1, targetSamples));
    std::vector<float> interleaved(kDecodeChunkFrames * kChannelCount);
    QElapsedTimer partialTimer;
    partialTimer.start();

    int framesInWindow = 0;
    float currentPeak = 0.0f;
    qint64 totalFramesDecoded = 0;

    while (!isCanceled()) {
        std::size_t renderedFrames = 0;
        try {
            renderedFrames = module->read_interleaved_stereo(kRenderSampleRate,
                                                             kDecodeChunkFrames,
                                                             interleaved.data());
        } catch (const openmpt::exception &exception) {
            result.errorMessage = QStringLiteral("Tracker waveform decode failed: %1")
                .arg(QString::fromUtf8(exception.what()));
            return result;
        } catch (const std::exception &exception) {
            result.errorMessage = QStringLiteral("Tracker waveform decode failed: %1")
                .arg(QString::fromUtf8(exception.what()));
            return result;
        } catch (...) {
            result.errorMessage = QStringLiteral("Tracker waveform decode failed: unknown libopenmpt error");
            return result;
        }

        if (renderedFrames == 0) {
            break;
        }

        for (std::size_t frame = 0; frame < renderedFrames; ++frame) {
            const std::size_t base = frame * kChannelCount;
            const float merged = std::max(std::abs(interleaved[base]), std::abs(interleaved[base + 1]));
            currentPeak = std::max(currentPeak, merged);
            ++framesInWindow;
            ++totalFramesDecoded;

            if (framesInWindow >= windowFrames) {
                rawPeaks.append(currentPeak);
                compactRawPeaksIfNeeded(rawPeaks, targetSamples);
                currentPeak = 0.0f;
                framesInWindow = 0;
            }
        }

        if (partialCallback && partialTimer.elapsed() >= 90) {
            QVector<float> partialPeaks = resampleMax(rawPeaks, targetSamples);
            normalizePeaks(partialPeaks);
            if (sanitizePeaks(partialPeaks)) {
                const double progress = expectedFrames > 0.0
                    ? std::clamp(static_cast<double>(totalFramesDecoded) / expectedFrames, 0.0, 0.99)
                    : 0.0;
                partialCallback(std::move(partialPeaks), progress);
            }
            partialTimer.restart();
        }
    }

    if (isCanceled()) {
        result.canceled = true;
        return result;
    }

    if (framesInWindow > 0) {
        rawPeaks.append(currentPeak);
        compactRawPeaksIfNeeded(rawPeaks, targetSamples);
    }

    if (rawPeaks.isEmpty()) {
        result.errorMessage = QStringLiteral("No tracker audio samples extracted");
        return result;
    }

    result.peaks = resampleMax(rawPeaks, targetSamples);
    normalizePeaks(result.peaks);
    if (!sanitizePeaks(result.peaks)) {
        result.peaks.clear();
        result.errorMessage = QStringLiteral("Tracker waveform contains invalid samples");
        return result;
    }

    if (partialCallback) {
        partialCallback(result.peaks, 1.0);
    }

    result.success = true;
    return result;
}

} // namespace WaveFlux
