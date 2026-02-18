#include "WaveformProvider.h"
#include "PeaksCacheManager.h"
#include <QtConcurrent>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QUrl>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>
#include <cmath>
#include <algorithm>

namespace {
constexpr int kAnalysisSampleRate = 22050;
constexpr int kWindowSamples = 512;
constexpr int kMaxWindowSamples = 16 * 1024;
constexpr int kRawPeakSoftLimitMultiplier = 16;
constexpr int kRawPeakCompactTargetMultiplier = 8;
constexpr qint64 kSamplePullTimeoutNs = 100 * GST_MSECOND;
constexpr qint64 kMaxEmptyPulls = 50;
constexpr double kMinCompletionRatio = 0.90;

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
        int start = static_cast<int>(i * ratio);
        int end = static_cast<int>((i + 1) * ratio);
        end = std::min(end, static_cast<int>(input.size()));

        float maxPeak = 0.0f;
        for (int j = start; j < end; ++j) {
            maxPeak = std::max(maxPeak, input[j]);
        }
        output[i] = maxPeak;
    }

    return output;
}

void normalizePeaks(QVector<float> &peaks)
{
    if (peaks.isEmpty()) {
        return;
    }

    const float maxPeak = *std::max_element(peaks.begin(), peaks.end());
    if (maxPeak <= 0.0f) {
        return;
    }

    for (float &peak : peaks) {
        peak /= maxPeak;
    }
}

bool sanitizePeaksForRendering(QVector<float> &peaks)
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

int chooseWindowSamples(gint64 durationNs, int targetSamples)
{
    if (durationNs <= 0 || targetSamples <= 0) {
        return kWindowSamples;
    }

    const double expectedSamples =
        (static_cast<double>(durationNs) / static_cast<double>(GST_SECOND)) * kAnalysisSampleRate;
    if (expectedSamples <= 0.0) {
        return kWindowSamples;
    }

    const double desiredRawPeaks = static_cast<double>(targetSamples) * kRawPeakCompactTargetMultiplier;
    const int adaptiveWindow = static_cast<int>(std::ceil(expectedSamples / std::max(1.0, desiredRawPeaks)));
    return std::clamp(std::max(kWindowSamples, adaptiveWindow), kWindowSamples, kMaxWindowSamples);
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

QString escapeGstLaunchString(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return value;
}

QString buildEncodedFileUri(const QString &filePath)
{
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (trimmed.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        const QUrl uri(trimmed);
        return uri.isValid() ? QString::fromUtf8(uri.toEncoded()) : trimmed;
    }

    GError *uriError = nullptr;
    gchar *encodedUri = gst_filename_to_uri(trimmed.toUtf8().constData(), &uriError);
    if (encodedUri) {
        const QString uri = QString::fromUtf8(encodedUri);
        g_free(encodedUri);
        return uri;
    }

    if (uriError) {
        qWarning() << "extractWaveform: failed to encode URI for" << filePath
                   << "-" << QString::fromUtf8(uriError->message);
        g_error_free(uriError);
    }

    return QStringLiteral("file://") + trimmed;
}

bool isRemoteSourceUri(const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QUrl url(trimmed);
    return url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile();
}
} // namespace

WaveformProvider::WaveformProvider(QObject *parent)
    : QObject(parent)
    , m_cache(new PeaksCacheManager(this))
{
}

WaveformProvider::~WaveformProvider()
{
    cancel();
}

void WaveformProvider::loadFile(const QString &filePath)
{
    qDebug() << "WaveformProvider::loadFile" << filePath;
    
    // Cancel any ongoing extraction
    cancel();

    m_peaks.clear();
    emit peaksReady();
    m_progress = 0.0;
    emit progressChanged(m_progress);
    m_currentFilePath = filePath.trimmed();
    if (m_currentFilePath.isEmpty()) {
        return;
    }

    if (isRemoteSourceUri(m_currentFilePath)) {
        qDebug() << "WaveformProvider: skipping extraction for remote source" << m_currentFilePath;
        return;
    }

    // ── Cache lookup ────────────────────────────────────────────────
    if (auto cached = m_cache->lookup(m_currentFilePath)) {
        QVector<float> cachedPeaks = std::move(*cached);
        if (sanitizePeaksForRendering(cachedPeaks)) {
            qDebug() << "WaveformProvider: using cached peaks for" << m_currentFilePath;
            m_peaks = std::move(cachedPeaks);
            m_progress = 1.0;
            emit peaksReady();
            emit progressChanged(m_progress);
            return; // no GStreamer work needed
        }

        qWarning() << "WaveformProvider: cached peaks are invalid, rebuilding for" << m_currentFilePath;
    }

    // ── Cache miss – full extraction ────────────────────────────────
    m_loading = true;
    emit loadingChanged(m_loading);

    const quint64 generationId = ++m_generationId;
    m_cancelToken = std::make_shared<std::atomic_bool>(false);

    const PartialCallback partialCallback = [this, generationId](const QVector<float> &peaks, double progress) {
        QMetaObject::invokeMethod(
            this,
            [this, generationId, peaks, progress]() {
                applyPartialPeaks(peaks, progress, generationId);
            },
            Qt::QueuedConnection);
    };
    
    // Start extraction in background thread
    const QString sourcePath = m_currentFilePath;
    auto *watcher = new QFutureWatcher<WaveformData>(this);
    m_watcher = watcher;

    connect(watcher, &QFutureWatcher<WaveformData>::finished, this, [this, watcher, generationId]() {
        onExtractionFinished(watcher, generationId);
        if (m_watcher == watcher) {
            m_watcher = nullptr;
        }
        watcher->deleteLater();
    });

    const std::shared_ptr<std::atomic_bool> cancelToken = m_cancelToken;
    QFuture<WaveformData> future = QtConcurrent::run([sourcePath, generationId, partialCallback, cancelToken]() {
        WaveformData data = extractWaveform(sourcePath, DEFAULT_SAMPLE_COUNT, partialCallback, cancelToken.get());
        data.generationId = generationId;
        data.sourceFilePath = sourcePath;
        return data;
    });
    watcher->setFuture(future);
}

void WaveformProvider::cancel()
{
    ++m_generationId;
    if (m_cancelToken) {
        m_cancelToken->store(true, std::memory_order_relaxed);
    }

    if (m_watcher) {
        m_watcher->disconnect(this);
        if (m_watcher->isRunning()) {
            m_watcher->cancel();
        }
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }

    if (m_loading) {
        m_loading = false;
        emit loadingChanged(m_loading);
    }
}

void WaveformProvider::applyPartialPeaks(QVector<float> peaks, double progress, quint64 generationId)
{
    if (generationId != m_generationId) {
        return;
    }

    if (!peaks.isEmpty()) {
        m_peaks = std::move(peaks);
        emit peaksReady();
    }

    const double clampedProgress = std::clamp(progress, 0.0, 1.0);
    if (!qFuzzyCompare(m_progress, clampedProgress)) {
        m_progress = clampedProgress;
        emit progressChanged(m_progress);
    }
}

WaveformProvider::WaveformData WaveformProvider::extractWaveform(
    const QString &filePath,
    int targetSamples,
    const PartialCallback &partialCallback,
    const std::atomic_bool *cancelRequested)
{
    WaveformData result;
    const auto isCanceled = [cancelRequested]() {
        return cancelRequested && cancelRequested->load(std::memory_order_relaxed);
    };
    
    qDebug() << "extractWaveform: Starting for" << filePath;
    
    // Build a GStreamer pipeline for decoding audio.
    // Primary path uses encoded URI, fallback uses filesrc/decodebin for tricky filenames.
    const QString uri = buildEncodedFileUri(filePath);
    QString localPath = filePath.trimmed();
    if (localPath.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        const QUrl localUri(localPath);
        if (localUri.isLocalFile()) {
            localPath = localUri.toLocalFile();
        }
    }

    qDebug() << "extractWaveform: URI =" << uri;

    QVector<QString> pipelineCandidates;
    pipelineCandidates.reserve(2);
    if (!uri.isEmpty()) {
        pipelineCandidates.push_back(QString(
            "uridecodebin uri=\"%1\" ! audioconvert ! audioresample ! "
            "audio/x-raw,format=F32LE,channels=1,rate=22050 ! appsink name=sink sync=false"
        ).arg(uri));
    }
    if (!localPath.isEmpty()) {
        pipelineCandidates.push_back(QString(
            "filesrc location=\"%1\" ! decodebin ! audioconvert ! audioresample ! "
            "audio/x-raw,format=F32LE,channels=1,rate=22050 ! appsink name=sink sync=false"
        ).arg(escapeGstLaunchString(localPath)));
    }

    GstElement *pipeline = nullptr;
    GstElement *sink = nullptr;
    GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
    QString startupError;

    for (int attempt = 0; attempt < pipelineCandidates.size(); ++attempt) {
        const bool fallbackAttempt = attempt > 0;

        GError *error = nullptr;
        pipeline = gst_parse_launch(pipelineCandidates.at(attempt).toUtf8().constData(), &error);
        if (error) {
            startupError = QString::fromUtf8(error->message);
            qWarning() << "extractWaveform: pipeline parse error"
                       << (fallbackAttempt ? "(fallback)" : "(primary)")
                       << startupError;
            g_error_free(error);
            pipeline = nullptr;
            continue;
        }

        if (!pipeline) {
            startupError = QStringLiteral("Failed to create pipeline");
            qWarning() << "extractWaveform:" << startupError
                       << (fallbackAttempt ? "(fallback)" : "(primary)");
            continue;
        }

        sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        if (!sink) {
            startupError = QStringLiteral("Failed to get appsink element");
            qWarning() << "extractWaveform:" << startupError
                       << (fallbackAttempt ? "(fallback)" : "(primary)");
            gst_object_unref(pipeline);
            pipeline = nullptr;
            continue;
        }

        // Configure appsink - enable emit-signals for better EOS detection
        g_object_set(sink,
                     "emit-signals", TRUE,
                     "max-buffers", 100,
                     "drop", FALSE,
                     nullptr);

        // Start pipeline
        ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            startupError = QStringLiteral("Failed to start pipeline");
            qWarning() << "extractWaveform:" << startupError
                       << (fallbackAttempt ? "(fallback)" : "(primary)");
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(sink);
            sink = nullptr;
            gst_object_unref(pipeline);
            pipeline = nullptr;
            continue;
        }

        // Wait for state change to complete
        GstState state;
        ret = gst_element_get_state(pipeline, &state, nullptr, 5 * GST_SECOND);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            startupError = QStringLiteral("Pipeline failed to reach playing state");
            qWarning() << "extractWaveform:" << startupError
                       << (fallbackAttempt ? "(fallback)" : "(primary)");
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(sink);
            sink = nullptr;
            gst_object_unref(pipeline);
            pipeline = nullptr;
            continue;
        }

        if (fallbackAttempt) {
            qWarning() << "extractWaveform: fallback pipeline succeeded for" << filePath;
        }
        break;
    }

    if (!pipeline || !sink) {
        result.errorMessage = startupError.isEmpty()
            ? QStringLiteral("Failed to start pipeline")
            : startupError;
        qWarning() << "extractWaveform:" << result.errorMessage;
        return result;
    }
    
    qDebug() << "extractWaveform: Pipeline started, collecting samples...";
    
    gint64 durationNs = GST_CLOCK_TIME_NONE;
    const bool hasKnownDuration = gst_element_query_duration(pipeline, GST_FORMAT_TIME, &durationNs);
    const int effectiveWindowSamples = hasKnownDuration
        ? chooseWindowSamples(durationNs, targetSamples)
        : kWindowSamples;

    QVector<float> rawPeaks;
    if (hasKnownDuration && durationNs > 0) {
        const double expectedSamples =
            (static_cast<double>(durationNs) / static_cast<double>(GST_SECOND)) * kAnalysisSampleRate;
        const int estimatedRawPeaks = static_cast<int>(
            std::ceil(expectedSamples / static_cast<double>(std::max(1, effectiveWindowSamples))));
        const int reserveCount = std::max(
            targetSamples,
            std::min(estimatedRawPeaks, targetSamples * kRawPeakCompactTargetMultiplier));
        rawPeaks.reserve(reserveCount);
    } else {
        rawPeaks.reserve(targetSamples * 2);
    }

    float currentWindowPeak = 0.0f;
    int windowSamples = 0;
    qint64 totalSamplesProcessed = 0;

    QElapsedTimer partialUpdateTimer;
    partialUpdateTimer.start();

    int emptyPulls = 0;
    bool reachedEos = false;
    bool interrupted = false;
    QString interruptionReason;
    while (true) {
        if (isCanceled()) {
            result.canceled = true;
            break;
        }

        GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), kSamplePullTimeoutNs);
        
        if (!sample) {
            // Check for EOS
            if (gst_app_sink_is_eos(GST_APP_SINK(sink))) {
                qDebug() << "extractWaveform: EOS reached";
                reachedEos = true;
                break;
            }
            
            // Check pipeline state for errors
            GstState currentState;
            gst_element_get_state(pipeline, &currentState, nullptr, 0);
            if (currentState != GST_STATE_PLAYING && currentState != GST_STATE_PAUSED) {
                qWarning() << "extractWaveform: Pipeline not playing, state:" << currentState;
                interrupted = true;
                interruptionReason = QStringLiteral("Waveform extraction interrupted (pipeline state changed)");
                break;
            }
            
            emptyPulls++;
            if (emptyPulls >= kMaxEmptyPulls) {
                qWarning() << "extractWaveform: Too many empty pulls, assuming done";
                interrupted = true;
                interruptionReason = QStringLiteral("Waveform extraction interrupted (decoder timeout)");
                break;
            }
            continue;
        }
        
        emptyPulls = 0; // Reset on successful pull
        
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        if (buffer) {
            GstMapInfo map;
            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                const float *data = reinterpret_cast<const float*>(map.data);
                int numSamples = map.size / sizeof(float);
                for (int i = 0; i < numSamples; ++i) {
                    const float absVal = std::abs(data[i]);
                    currentWindowPeak = std::max(currentWindowPeak, absVal);
                    ++windowSamples;
                    ++totalSamplesProcessed;
                    if (windowSamples >= effectiveWindowSamples) {
                        rawPeaks.append(currentWindowPeak);
                        compactRawPeaksIfNeeded(rawPeaks, targetSamples);
                        currentWindowPeak = 0.0f;
                        windowSamples = 0;
                    }
                }
                
                gst_buffer_unmap(buffer, &map);
            }
        }
        
        gst_sample_unref(sample);

        if (partialUpdateTimer.elapsed() >= 90) {
            gint64 queriedDurationNs = GST_CLOCK_TIME_NONE;
            gint64 positionNs = 0;
            const bool hasDuration = gst_element_query_duration(pipeline, GST_FORMAT_TIME, &queriedDurationNs);
            const bool hasPosition = gst_element_query_position(pipeline, GST_FORMAT_TIME, &positionNs);

            double progressBySamples = 0.0;
            double progressByPosition = 0.0;
            if (hasDuration && queriedDurationNs > 0) {
                const double expectedSamples =
                    (static_cast<double>(queriedDurationNs) / static_cast<double>(GST_SECOND)) * kAnalysisSampleRate;
                if (expectedSamples > 0.0) {
                    progressBySamples = static_cast<double>(totalSamplesProcessed) / expectedSamples;
                }
                if (hasPosition && positionNs >= 0) {
                    progressByPosition = static_cast<double>(positionNs) / static_cast<double>(queriedDurationNs);
                }
            }

            double progress = std::max(progressBySamples, progressByPosition);
            progress = std::clamp(progress, 0.0, 0.99);

            QVector<float> partialPeaks = resampleMax(rawPeaks, targetSamples);
            normalizePeaks(partialPeaks);
            if (partialCallback) {
                partialCallback(partialPeaks, progress);
            }
            partialUpdateTimer.restart();
        }
    }
    
    // Cleanup GStreamer objects
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pipeline);

    if (result.canceled) {
        return result;
    }

    if (interrupted && !reachedEos) {
        double completionRatio = 0.0;
        if (hasKnownDuration && durationNs > 0) {
            const double expectedSamples =
                (static_cast<double>(durationNs) / static_cast<double>(GST_SECOND)) * kAnalysisSampleRate;
            if (expectedSamples > 0.0) {
                completionRatio = static_cast<double>(totalSamplesProcessed) / expectedSamples;
            }
        }

        if (completionRatio < kMinCompletionRatio) {
            result.errorMessage = interruptionReason.isEmpty()
                ? QStringLiteral("Waveform extraction interrupted before completion")
                : interruptionReason;
            qWarning() << "extractWaveform:" << result.errorMessage
                       << "completion ratio:" << completionRatio;
            return result;
        }
    }

    if (windowSamples > 0) {
        rawPeaks.append(currentWindowPeak);
        compactRawPeaksIfNeeded(rawPeaks, targetSamples);
    }

    if (rawPeaks.isEmpty()) {
        result.errorMessage = "No audio samples extracted";
        qWarning() << "extractWaveform:" << result.errorMessage;
        return result;
    }

    result.peaks = resampleMax(rawPeaks, targetSamples);
    normalizePeaks(result.peaks);
    if (!sanitizePeaksForRendering(result.peaks)) {
        result.errorMessage = QStringLiteral("Generated waveform contains invalid samples");
        result.peaks.clear();
        qWarning() << "extractWaveform:" << result.errorMessage;
        return result;
    }
    if (partialCallback) {
        partialCallback(result.peaks, 1.0);
    }

    qDebug() << "extractWaveform: Generated" << result.peaks.size() << "peaks";

    result.success = true;
    return result;
}

void WaveformProvider::onExtractionFinished(QFutureWatcher<WaveformData> *watcher, quint64 generationId)
{
    if (!watcher) {
        return;
    }

    const QFuture<WaveformData> future = watcher->future();
    if (!future.isValid() || !future.isFinished() || future.resultCount() <= 0) {
        if (generationId == m_generationId && m_loading) {
            m_loading = false;
            emit loadingChanged(m_loading);
        }
        return;
    }

    WaveformData result = future.result();

    if (result.generationId != generationId
        || generationId != m_generationId
        || result.sourceFilePath != m_currentFilePath) {
        qDebug() << "WaveformProvider: stale extraction result ignored for"
                 << result.sourceFilePath;
        return;
    }

    if (m_loading) {
        m_loading = false;
        emit loadingChanged(m_loading);
    }
    
    if (result.canceled) {
        qDebug() << "WaveformProvider: Extraction canceled by request";
        return;
    }

    if (result.success && !result.peaks.isEmpty() && sanitizePeaksForRendering(result.peaks)) {
        m_peaks = std::move(result.peaks);
        qDebug() << "WaveformProvider: Peaks ready, count:" << m_peaks.size();

        // Persist to disk cache for next time
        m_cache->store(m_currentFilePath, m_peaks);

        emit peaksReady();
        if (!qFuzzyCompare(m_progress, 1.0)) {
            m_progress = 1.0;
            emit progressChanged(m_progress);
        }
    } else {
        qWarning() << "Waveform extraction failed:" << result.errorMessage;
        if (!qFuzzyCompare(m_progress, 0.0)) {
            m_progress = 0.0;
            emit progressChanged(m_progress);
        }
        const QString fallbackError = result.errorMessage.trimmed().isEmpty()
            ? QStringLiteral("Waveform extraction failed")
            : result.errorMessage.trimmed();
        emit error(fallbackError);
    }
}

QVector<float> WaveformProvider::getPeaksForWidth(int width) const
{
    if (m_peaks.isEmpty() || width <= 0) {
        return {};
    }
    
    if (width >= m_peaks.size()) {
        return m_peaks;
    }
    
    // Resample peaks to match desired width
    QVector<float> resampled(width);
    float ratio = static_cast<float>(m_peaks.size()) / width;
    
    for (int i = 0; i < width; ++i) {
        int start = static_cast<int>(i * ratio);
        int end = static_cast<int>((i + 1) * ratio);
        end = std::min(end, static_cast<int>(m_peaks.size()));
        
        float maxPeak = 0.0f;
        for (int j = start; j < end; ++j) {
            if (m_peaks[j] > maxPeak) {
                maxPeak = m_peaks[j];
            }
        }
        resampled[i] = maxPeak;
    }
    
    return resampled;
}
