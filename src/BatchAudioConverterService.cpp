#include "BatchAudioConverterService.h"

#include "AppSettingsManager.h"
#include "AudioConverterService.h"
#include "TagLibPath.h"
#include "playback/PlaybackBackendRouting.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QUrl>
#include <QVector>
#include <QUuid>
#include <QtGlobal>
#include <algorithm>
#include <ctime>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_UNIX)
#include <sys/resource.h>
#endif

namespace {
constexpr auto kBatchDraftSchema = "waveflux.batch-audio-converter.draft.v1";
constexpr auto kBatchReportSchema = "waveflux.batch-audio-converter.report.v1";
constexpr int kFinishedJobHistoryRetention = 20;

QString localizedBatchText(const QString &key)
{
    return AppSettingsManager::translateForCurrentLanguage(key);
}

bool isLikelyWindowsAbsolutePath(const QString &path)
{
    return path.size() >= 3
        && path.at(0).isLetter()
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('\\') || path.at(2) == QLatin1Char('/'));
}

bool isSlashPrefixedWindowsAbsolutePath(const QString &path)
{
    return path.size() >= 4
        && path.at(0) == QLatin1Char('/')
        && path.at(1).isLetter()
        && path.at(2) == QLatin1Char(':')
        && (path.at(3) == QLatin1Char('\\') || path.at(3) == QLatin1Char('/'));
}

QString baseNameForOutput(const QString &sourceFile)
{
    const QFileInfo info(sourceFile);
    const QString completeBaseName = info.completeBaseName().trimmed();
    if (!completeBaseName.isEmpty()) {
        return completeBaseName;
    }
    return QStringLiteral("Converted Track");
}

QString extensionForFormat(const QString &format)
{
    if (format == QStringLiteral("flac")) {
        return QStringLiteral("flac");
    }
    if (format == QStringLiteral("wav")) {
        return QStringLiteral("wav");
    }
    if (format == QStringLiteral("opus")) {
        return QStringLiteral("opus");
    }
    if (format == QStringLiteral("webm")) {
        return QStringLiteral("webm");
    }
    return QStringLiteral("mp3");
}

bool hasSupportedBatchSourceExtension(const QString &filePath)
{
    static const QSet<QString> standardExtensions = {
        QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("mp4"), QStringLiteral("wma"),
        QStringLiteral("flac"), QStringLiteral("ape"), QStringLiteral("wav"), QStringLiteral("wv"),
        QStringLiteral("tta"), QStringLiteral("mpc"), QStringLiteral("spx"), QStringLiteral("opus"),
        QStringLiteral("webm"),
        QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("aiff"), QStringLiteral("alac")
    };

    const QString suffix = QFileInfo(filePath).suffix().trimmed().toLower();
    return standardExtensions.contains(suffix) || WaveFlux::isTrackerModuleExtension(suffix);
}

QStringList sortedPaths(QStringList paths)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(paths.begin(), paths.end(), [&collator](const QString &a, const QString &b) {
        const int cmp = collator.compare(a, b);
        if (cmp == 0) {
            return QString::compare(a, b, Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });
    return paths;
}

QString normalizedReportFormat(const QString &format)
{
    const QString normalized = format.trimmed().toLower();
    if (normalized == QStringLiteral("txt")
        || normalized == QStringLiteral("text")
        || normalized == QStringLiteral("csv")) {
        return normalized == QStringLiteral("text") ? QStringLiteral("txt") : normalized;
    }
    return QStringLiteral("json");
}

qint64 currentProcessCpuTimeMsImpl()
{
    const std::clock_t ticks = std::clock();
    if (ticks <= 0 || CLOCKS_PER_SEC <= 0) {
        return 0;
    }
    return static_cast<qint64>(
        (static_cast<long double>(ticks) * 1000.0L) / static_cast<long double>(CLOCKS_PER_SEC));
}

qint64 currentPeakResidentMemoryKbImpl()
{
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<qint64>(counters.PeakWorkingSetSize / 1024);
    }
    return -1;
#elif defined(Q_OS_UNIX)
    rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
#if defined(Q_OS_MACOS)
    return static_cast<qint64>(usage.ru_maxrss / 1024);
#else
    return static_cast<qint64>(usage.ru_maxrss);
#endif
#else
    return -1;
#endif
}
} // namespace

BatchAudioConverterService::BatchAudioConverterService(QObject *parent)
    : QObject(parent)
{
    m_settings.equalizerBandGains = normalizeEqualizerBandGains({});

    auto *worker = new AudioConverterService(this);
    m_worker = worker;

    connect(worker, &AudioConverterService::conversionStarted, this, [this]() {
        const int index = currentIndex();
        if (!hasItemAt(index)) {
            return;
        }
        BatchAudioConversionItem &item = m_items[index];
        item.statusText = m_worker->statusText();
        item.errorText.clear();
        item.failureType = NoFailure;
        item.terminalResult = QStringLiteral("none");
        touchItem(item);
        setStatusText(item.statusText.isEmpty()
                         ? localizedBatchText(QStringLiteral("batchConverter.started"))
                          : item.statusText);
        updateAggregateProgress();
        emit itemsChanged();
    });

    connect(worker, &AudioConverterService::progressChanged, this, [this]() {
        const int index = currentIndex();
        if (!hasItemAt(index)) {
            return;
        }
        BatchAudioConversionItem &item = m_items[index];
        item.progress = qBound(0.0, m_worker->progress(), 1.0);
        item.statusText = m_worker->statusText();
        touchItem(item);
        updateAggregateProgress();
        if (!item.statusText.isEmpty()) {
            setStatusText(item.statusText);
        }
        emit itemsChanged();
    });

    connect(worker, &AudioConverterService::statusTextChanged, this, [this]() {
        const int index = currentIndex();
        if (!hasItemAt(index)) {
            return;
        }
        BatchAudioConversionItem &item = m_items[index];
        item.statusText = m_worker->statusText();
        touchItem(item);
        if (!item.statusText.isEmpty()) {
            setStatusText(item.statusText);
        }
        emit itemsChanged();
    });

    connect(worker, &AudioConverterService::conversionFinished, this, [this](const QString &outputPath) {
        const int index = currentIndex();
        if (!hasItemAt(index)) {
            return;
        }

        BatchAudioConversionItem &item = m_items[index];
        item.resultFile = outputPath;
        item.statusText = m_worker->statusText();
        item.errorText.clear();
        applyTerminalState(item, Succeeded, NoFailure);
        absorbWorkerMetrics(item, m_worker->lastConversionMetrics());
        updateAggregateProgress();
        emit itemsChanged();
        if (m_settings.playlistAddMode == ImmediatePlaylistAddMode
            && !outputPath.trimmed().isEmpty()) {
            emit playlistResultReady(outputPath);
        }

        if (m_cancelRequested) {
            finalizeBatchRun(true);
        } else {
            startNextPendingItem();
        }
    });

    connect(worker, &AudioConverterService::conversionFailed, this, [this](const QString &message) {
        const int index = currentIndex();
        if (!hasItemAt(index)) {
            return;
        }

        BatchAudioConversionItem &item = m_items[index];
        item.errorText = message;
        item.statusText = message;
        applyTerminalState(item, Failed, classifyFailureMessage(message, InternalPipelineFailure));
        absorbWorkerMetrics(item, m_worker->lastConversionMetrics());
        setLastError(message);
        setStatusText(message);
        updateAggregateProgress();
        emit itemsChanged();

        if (m_cancelRequested) {
            finalizeBatchRun(true);
        } else {
            startNextPendingItem();
        }
    });

    connect(worker, &AudioConverterService::conversionCanceled, this, [this]() {
        const int index = currentIndex();
        if (hasItemAt(index)) {
            BatchAudioConversionItem &item = m_items[index];
            item.statusText = m_worker->statusText();
            applyTerminalState(item, Canceled, CanceledFailure);
            absorbWorkerMetrics(item, m_worker->lastConversionMetrics());
            updateAggregateProgress();
            emit itemsChanged();
        }
        finalizeBatchRun(true);
    });
}

QVariantList BatchAudioConverterService::items() const
{
    QVariantList result;
    result.reserve(m_items.size());
    for (const BatchAudioConversionItem &item : m_items) {
        result.push_back(toVariantMap(item));
    }
    return result;
}

QVariantMap BatchAudioConverterService::currentItem() const
{
    const int index = currentIndex();
    if (!hasItemAt(index)) {
        return {};
    }
    return toVariantMap(m_items.at(index));
}

QVariantMap BatchAudioConverterService::settings() const
{
    QVariantMap result;
    result.insert(QStringLiteral("outputDirectory"), m_settings.outputDirectory);
    result.insert(QStringLiteral("namingPolicy"), m_settings.namingPolicy);
    result.insert(QStringLiteral("format"), m_settings.format);
    result.insert(QStringLiteral("conflictPolicy"), conflictPolicyKey(m_settings.conflictPolicy));
    result.insert(QStringLiteral("retryPolicy"), retryPolicyKey(m_settings.retryPolicy));
    result.insert(QStringLiteral("playlistAddMode"), playlistAddModeKey(m_settings.playlistAddMode));
    result.insert(QStringLiteral("bitrate"), m_settings.bitrate);
    result.insert(QStringLiteral("sampleRate"), m_settings.sampleRate);
    result.insert(QStringLiteral("channelMode"), m_settings.channelMode);
    result.insert(QStringLiteral("playbackRate"), m_settings.playbackRate);
    result.insert(QStringLiteral("pitchSemitones"), m_settings.pitchSemitones);
    result.insert(QStringLiteral("applyEqualizer"), m_settings.applyEqualizer);
    result.insert(QStringLiteral("equalizerBandGains"), m_settings.equalizerBandGains);
    result.insert(QStringLiteral("applyReverb"), m_settings.applyReverb);
    result.insert(QStringLiteral("reverbRoomSize"), m_settings.reverbRoomSize);
    result.insert(QStringLiteral("reverbDamping"), m_settings.reverbDamping);
    result.insert(QStringLiteral("reverbWetLevel"), m_settings.reverbWetLevel);
    result.insert(QStringLiteral("addResultsToPlaylist"), m_settings.addResultsToPlaylist);
    return result;
}

QVariantMap BatchAudioConverterService::jobMetadata() const
{
    if (m_jobId.isEmpty()) {
        return {};
    }

    QVariantMap result;
    result.insert(QStringLiteral("jobId"), m_jobId);
    result.insert(QStringLiteral("createdAtMs"), m_jobCreatedAtMs);
    result.insert(QStringLiteral("startedAtMs"), m_jobStartedAtMs);
    result.insert(QStringLiteral("finishedAtMs"), m_jobFinishedAtMs);
    result.insert(QStringLiteral("isRunning"), m_isRunning);
    return result;
}

QVariantMap BatchAudioConverterService::runtimeDiagnostics() const
{
    const qint64 measurementFinishedAtMs = m_jobFinishedAtMs > 0
        ? m_jobFinishedAtMs
        : (m_runtimeMeasurementStartedAtMs > 0 ? nowMs() : 0);
    const qint64 measurementFinishedCpuTimeMs = m_jobFinishedAtMs > 0
        ? m_runtimeMeasurementCpuFinishedAtMs
        : (m_runtimeMeasurementStartedAtMs > 0 ? currentProcessCpuTimeMs() : 0);
    const qint64 wallClockDurationMs =
        (m_runtimeMeasurementStartedAtMs > 0 && measurementFinishedAtMs > 0)
        ? qMax<qint64>(0, measurementFinishedAtMs - m_runtimeMeasurementStartedAtMs)
        : 0;
    const qint64 cpuTimeMs =
        m_runtimeMeasurementStartedAtMs > 0
        ? qMax<qint64>(0, measurementFinishedCpuTimeMs - m_runtimeMeasurementCpuStartedAtMs)
        : 0;

    int runnableItemCount = 0;
    int trackerItemCount = 0;
    for (const BatchAudioConversionItem &item : m_items) {
        if (item.state == Skipped && item.reportMetadata.value(QStringLiteral("intakeIssue")).toBool()) {
            continue;
        }
        ++runnableItemCount;
        if (isTrackerSourceItem(item)) {
            ++trackerItemCount;
        }
    }

    const bool heavyDspProfile = !qFuzzyCompare(m_settings.playbackRate, 1.0)
        || m_settings.pitchSemitones != 0
        || m_settings.applyEqualizer
        || m_settings.applyReverb;
    const int dspAdjustedItemCount = heavyDspProfile ? runnableItemCount : 0;

    QVariantMap metrics;
    metrics.insert(QStringLiteral("schemaVersion"), 1);
    metrics.insert(QStringLiteral("executionMode"), QStringLiteral("sequential-single-worker"));
    metrics.insert(QStringLiteral("queueItemCount"), m_items.size());
    metrics.insert(QStringLiteral("runnableItemCount"), runnableItemCount);
    metrics.insert(QStringLiteral("measuredItemCount"), m_runtimeMeasuredItemCount);
    metrics.insert(QStringLiteral("trackerItemCount"), trackerItemCount);
    metrics.insert(QStringLiteral("dspAdjustedItemCount"), dspAdjustedItemCount);
    metrics.insert(QStringLiteral("wallClockDurationMs"), wallClockDurationMs);
    metrics.insert(QStringLiteral("cpuTimeMs"), cpuTimeMs);
    metrics.insert(QStringLiteral("cpuTimePerItemMs"),
                   m_runtimeMeasuredItemCount > 0
                       ? cpuTimeMs / m_runtimeMeasuredItemCount
                       : 0);
    metrics.insert(QStringLiteral("peakResidentMemoryKb"), m_runtimePeakResidentMemoryKb);
    metrics.insert(QStringLiteral("sourceBytesMeasured"), m_runtimeMeasuredSourceBytes);
    metrics.insert(QStringLiteral("resultBytesMeasured"), m_runtimeMeasuredResultBytes);
    metrics.insert(QStringLiteral("peakTempBytesObserved"), m_runtimePeakTempBytes);
    metrics.insert(QStringLiteral("peakTempFileCountObserved"), m_runtimePeakTempFiles);
    metrics.insert(QStringLiteral("tagCopyAttemptCount"), m_runtimeTagCopyAttemptCount);
    metrics.insert(QStringLiteral("tagCopySuccessCount"), m_runtimeTagCopySuccessCount);
    metrics.insert(QStringLiteral("tagCopyTotalDurationUs"), m_runtimeTagCopyTotalDurationUs);
    metrics.insert(QStringLiteral("tagCopyAverageDurationUs"),
                   m_runtimeTagCopyAttemptCount > 0
                       ? m_runtimeTagCopyTotalDurationUs / m_runtimeTagCopyAttemptCount
                       : 0);
    metrics.insert(QStringLiteral("maxConcurrentJobsConfigured"), 1);
    metrics.insert(QStringLiteral("maxConcurrentJobsObserved"), m_runtimeMaxConcurrentJobsObserved);
    metrics.insert(QStringLiteral("progressAggregationPolicy"),
                   QStringLiteral("completed-plus-running-fraction"));
    metrics.insert(QStringLiteral("tempFilePressurePolicy"),
                   QStringLiteral("single-temp-file-per-active-item"));
    return metrics;
}

QVariantMap BatchAudioConverterService::parallelismDecision() const
{
    const QVariantMap metrics = runtimeDiagnostics();
    const int trackerItemCount = metrics.value(QStringLiteral("trackerItemCount")).toInt();
    const int dspAdjustedItemCount = metrics.value(QStringLiteral("dspAdjustedItemCount")).toInt();

    QVariantList blockedReasons{
        QStringLiteral("v2 ships with sequential execution as the default-safe path"),
        QStringLiteral("deterministic reporting stays in queue order")
    };
    if (trackerItemCount > 0) {
        blockedReasons.push_back(QStringLiteral("tracker modules stay sequential"));
    }
    if (dspAdjustedItemCount > 0) {
        blockedReasons.push_back(QStringLiteral("heavy DSP profiles stay sequential"));
    }

    QVariantMap semantics;
    semantics.insert(QStringLiteral("playlistInsertOrdering"), QStringLiteral("queue-order-on-success"));
    semantics.insert(QStringLiteral("conflictResolution"), QStringLiteral("planned-before-start-per-item"));
    semantics.insert(QStringLiteral("cancelBehavior"),
                     QStringLiteral("cancel-all; current item cancels first, tail items become canceled"));
    semantics.insert(QStringLiteral("progressAggregation"),
                     QStringLiteral("completed-plus-running-fraction"));
    semantics.insert(QStringLiteral("reportOrdering"), QStringLiteral("queue-order"));

    QVariantMap decision;
    decision.insert(QStringLiteral("schemaVersion"), 1);
    decision.insert(QStringLiteral("basisKey"), QStringLiteral("measured-sequential-v2-queue"));
    decision.insert(QStringLiteral("decisionKey"), QStringLiteral("sequential-default-safe"));
    decision.insert(QStringLiteral("multiWorkerNeeded"), false);
    decision.insert(QStringLiteral("multiWorkerEnabled"), false);
    decision.insert(QStringLiteral("configuredMaxConcurrentJobs"), 1);
    decision.insert(QStringLiteral("recommendedMaxConcurrentJobs"), 1);
    decision.insert(QStringLiteral("futureCapabilityDrivenCap"),
                    (trackerItemCount > 0 || dspAdjustedItemCount > 0) ? 1 : 2);
    decision.insert(QStringLiteral("blockedReasons"), blockedReasons);
    decision.insert(QStringLiteral("semantics"), semantics);
    decision.insert(QStringLiteral("measurements"), metrics);
    return decision;
}

QString BatchAudioConverterService::conflictPolicy() const
{
    return conflictPolicyKey(m_settings.conflictPolicy);
}

QString BatchAudioConverterService::retryPolicy() const
{
    return retryPolicyKey(m_settings.retryPolicy);
}

QString BatchAudioConverterService::playlistAddMode() const
{
    return playlistAddModeKey(m_settings.playlistAddMode);
}

bool BatchAudioConverterService::canAddSucceededResultsToPlaylist() const
{
    return m_hasFinished
        && m_settings.playlistAddMode == DeferredPlaylistAddMode
        && !m_deferredPlaylistResultsAdded
        && succeededCount() > 0;
}

QVariantMap BatchAudioConverterService::finalSummary() const
{
    if (!m_hasFinished) {
        return {};
    }

    const int total = totalCount();
    const int succeeded = succeededCount();
    const int failed = failedCount();
    const int canceled = canceledCount();
    const int skipped = skippedCount();

    QVariantMap result;
    result.insert(QStringLiteral("totalCount"), total);
    result.insert(QStringLiteral("completedCount"), succeeded + failed + canceled + skipped);
    result.insert(QStringLiteral("pendingCount"), pendingCount());
    result.insert(QStringLiteral("runningCount"), runningCount());
    result.insert(QStringLiteral("succeededCount"), succeeded);
    result.insert(QStringLiteral("failedCount"), failed);
    result.insert(QStringLiteral("canceledCount"), canceled);
    result.insert(QStringLiteral("skippedCount"), skipped);
    result.insert(QStringLiteral("wasCanceled"), m_wasCanceled);
    result.insert(QStringLiteral("hasFailures"), failed > 0);
    result.insert(QStringLiteral("hasSkips"), skipped > 0);
    result.insert(QStringLiteral("statusText"), m_statusText);
    result.insert(QStringLiteral("lastError"), m_lastError);
    result.insert(QStringLiteral("jobId"), m_jobId);
    result.insert(QStringLiteral("createdAtMs"), m_jobCreatedAtMs);
    result.insert(QStringLiteral("startedAtMs"), m_jobStartedAtMs);
    result.insert(QStringLiteral("finishedAtMs"), m_jobFinishedAtMs);
    result.insert(QStringLiteral("runtimeDiagnostics"), runtimeDiagnostics());
    result.insert(QStringLiteral("parallelismDecision"), parallelismDecision());
    return result;
}

QVariantMap BatchAudioConverterService::exportDraftState() const
{
    if (m_items.isEmpty() || m_hasFinished) {
        return {};
    }

    QVariantMap draft;
    draft.insert(QStringLiteral("schema"), QString::fromLatin1(kBatchDraftSchema));
    draft.insert(QStringLiteral("persistedAtMs"), nowMs());
    draft.insert(QStringLiteral("settings"), settings());

    QVariantMap job;
    job.insert(QStringLiteral("jobId"), m_jobId);
    job.insert(QStringLiteral("createdAtMs"), m_jobCreatedAtMs);
    job.insert(QStringLiteral("startedAtMs"), m_jobStartedAtMs);
    job.insert(QStringLiteral("finishedAtMs"), 0);
    job.insert(QStringLiteral("wasRunning"), m_isRunning);
    draft.insert(QStringLiteral("jobMetadata"), job);

    QVariantList serializedItems;
    serializedItems.reserve(m_items.size());
    for (const BatchAudioConversionItem &item : m_items) {
        serializedItems.push_back(serializeDraftItem(item));
    }
    draft.insert(QStringLiteral("items"), serializedItems);
    return draft;
}

bool BatchAudioConverterService::restoreDraftState(const QVariantMap &draftState)
{
    if (m_isRunning) {
        return false;
    }

    const QVariantMap sanitized = sanitizeDraftState(draftState);
    if (sanitized.isEmpty()) {
        return false;
    }

    QList<BatchAudioConversionItem> restoredItems;
    const QVariantList serializedItems = sanitized.value(QStringLiteral("items")).toList();
    restoredItems.reserve(serializedItems.size());
    for (const QVariant &value : serializedItems) {
        BatchAudioConversionItem item;
        if (!parseDraftItem(value.toMap(), &item)) {
            return false;
        }
        restoredItems.push_back(item);
    }

    m_items = restoredItems;
    const QVariantMap restoredSettings = sanitized.value(QStringLiteral("settings")).toMap();
    applySettingsMap(restoredSettings);

    const QVariantMap job = sanitized.value(QStringLiteral("jobMetadata")).toMap();
    m_jobId = job.value(QStringLiteral("jobId")).toString().trimmed();
    m_jobCreatedAtMs = job.value(QStringLiteral("createdAtMs")).toLongLong();
    m_jobStartedAtMs = job.value(QStringLiteral("startedAtMs")).toLongLong();
    m_jobFinishedAtMs = 0;
    setIsRunning(false);
    setCancelRequested(false);
    resetFinalSummary();
    setBatchProgress(0.0);
    updateAggregateProgress();
    setStatusText(localizedBatchText(QStringLiteral("batchConverter.restoredDraft")));
    setLastError(QString());
    emit jobMetadataChanged();
    emit itemsChanged();
    return true;
}

QVariantMap BatchAudioConverterService::currentReport() const
{
    return buildReportForCurrentJob();
}

bool BatchAudioConverterService::exportCurrentReportToFile(const QString &filePath, const QString &format)
{
    return exportReportMapToFile(buildReportForCurrentJob(), filePath, format);
}

bool BatchAudioConverterService::exportHistoryReportToFile(const QString &jobId,
                                                           const QString &filePath,
                                                           const QString &format)
{
    return exportReportMapToFile(finishedJobReportById(jobId), filePath, format);
}

QString BatchAudioConverterService::currentReportText(const QString &format) const
{
    const QVariantMap report = buildReportForCurrentJob();
    if (report.isEmpty()) {
        return {};
    }

    const QString normalizedFormat = normalizedReportFormat(format);
    if (normalizedFormat == QStringLiteral("json")) {
        QByteArray jsonPayload;
        if (!reportAsJson(report, &jsonPayload)) {
            return {};
        }
        return QString::fromUtf8(jsonPayload);
    }
    if (normalizedFormat == QStringLiteral("csv")) {
        return reportAsCsv(report);
    }
    return reportAsPlainText(report);
}

QVariantList BatchAudioConverterService::succeededResultFiles() const
{
    QVariantList results;
    for (const BatchAudioConversionItem &item : m_items) {
        if (item.state != Succeeded) {
            continue;
        }
        const QString resultFile = item.resultFile.trimmed();
        if (!resultFile.isEmpty()) {
            results.push_back(resultFile);
        }
    }
    return results;
}

int BatchAudioConverterService::addSucceededResultsToPlaylist()
{
    if (!canAddSucceededResultsToPlaylist()) {
        return 0;
    }

    int emittedCount = 0;
    for (const QVariant &value : succeededResultFiles()) {
        const QString resultFile = value.toString().trimmed();
        if (resultFile.isEmpty()) {
            continue;
        }
        emit playlistResultReady(resultFile);
        ++emittedCount;
    }
    if (emittedCount > 0) {
        m_deferredPlaylistResultsAdded = true;
        emit finalSummaryChanged();
    }
    return emittedCount;
}

bool BatchAudioConverterService::replaceFinishedJobHistory(const QVariantList &history)
{
    QVariantList normalized;
    normalized.reserve(history.size());
    for (const QVariant &value : history) {
        const QVariantMap report = value.toMap();
        if (report.value(QStringLiteral("schema")).toString() != QString::fromLatin1(kBatchReportSchema)) {
            continue;
        }
        if (report.value(QStringLiteral("jobMetadata")).toMap()
                .value(QStringLiteral("jobId")).toString().trimmed().isEmpty()) {
            continue;
        }
        normalized.push_back(report);
    }
    while (normalized.size() > kFinishedJobHistoryRetention) {
        normalized.removeFirst();
    }
    if (m_finishedJobHistory == normalized) {
        return true;
    }
    m_finishedJobHistory = normalized;
    emit finishedJobHistoryChanged();
    return true;
}

QString BatchAudioConverterService::suggestedReportFileName(const QString &format) const
{
    const QString normalizedFormat = normalizedReportFormat(format);
    QString baseName = QStringLiteral("batch-audio-converter-report");
    const QVariantMap report = buildReportForCurrentJob();
    const QString jobId = report.value(QStringLiteral("jobMetadata")).toMap()
                              .value(QStringLiteral("jobId")).toString().trimmed();
    if (!jobId.isEmpty()) {
        baseName = QStringLiteral("batch-audio-converter-%1").arg(jobId.left(12));
    }
    return QStringLiteral("%1.%2").arg(baseName, normalizedFormat);
}

int BatchAudioConverterService::currentIndex() const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).state == Running) {
            return i;
        }
    }
    return -1;
}

int BatchAudioConverterService::pendingCount() const
{
    return countByState(Pending);
}

int BatchAudioConverterService::runningCount() const
{
    return countByState(Running);
}

int BatchAudioConverterService::succeededCount() const
{
    return countByState(Succeeded);
}

int BatchAudioConverterService::failedCount() const
{
    return countByState(Failed);
}

int BatchAudioConverterService::canceledCount() const
{
    return countByState(Canceled);
}

int BatchAudioConverterService::skippedCount() const
{
    return countByState(Skipped);
}

void BatchAudioConverterService::clear()
{
    if (m_isRunning) {
        return;
    }
    if (m_items.isEmpty()) {
        return;
    }
    m_items.clear();
    resetJobMetadata();
    setBatchProgress(0.0);
    setStatusText(QString());
    setLastError(QString());
    resetFinalSummary();
    emit itemsChanged();
}

void BatchAudioConverterService::setSourceFiles(const QStringList &sourceFiles)
{
    if (m_isRunning) {
        return;
    }

    QList<BatchAudioConversionItem> nextItems;
    nextItems.reserve(sourceFiles.size());
    const qint64 createdAtMs = nowMs();
    for (const QString &source : sourceFiles) {
        const QString trimmed = source.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        BatchAudioConversionItem item;
        item.itemId = newIdentity();
        item.sourceFile = normalizeLocalPath(trimmed);
        if (item.sourceFile.isEmpty()) {
            item.sourceFile = trimmed;
        }
        item.sourceDisplayName = inferDisplayName(item.sourceFile);
        item.sourceFormat = inferFormat(item.sourceFile);
        item.sourceOriginType = UnknownSourceOrigin;
        item.createdAtMs = createdAtMs;
        item.updatedAtMs = createdAtMs;
        nextItems.push_back(std::move(item));
    }

    m_items = std::move(nextItems);
    if (m_items.isEmpty()) {
        resetJobMetadata();
    } else {
        beginNewJobSession();
    }
    refreshPlannedOutputs(nullptr);
    setBatchProgress(0.0);
    setStatusText(QString());
    setLastError(QString());
    resetFinalSummary();
    emit itemsChanged();
}

void BatchAudioConverterService::setSourceFilesFromVariantList(const QVariantList &sourceFiles)
{
    QStringList paths;
    paths.reserve(sourceFiles.size());
    for (const QVariant &value : sourceFiles) {
        const QString path = value.toString().trimmed();
        if (!path.isEmpty()) {
            paths.push_back(path);
        }
    }
    setSourceFiles(paths);
}

QVariantMap BatchAudioConverterService::replaceSourceFilesFromVariantList(const QVariantList &sourceFiles,
                                                                          const QString &sourceOriginType)
{
    QStringList paths;
    paths.reserve(sourceFiles.size());
    for (const QVariant &value : sourceFiles) {
        const QString path = value.toString().trimmed();
        if (!path.isEmpty()) {
            paths.push_back(path);
        }
    }
    return ingestSources(paths, normalizeSourceOriginType(sourceOriginType), false);
}

QVariantMap BatchAudioConverterService::appendSourceFilesFromVariantList(const QVariantList &sourceFiles,
                                                                         const QString &sourceOriginType)
{
    QStringList paths;
    paths.reserve(sourceFiles.size());
    for (const QVariant &value : sourceFiles) {
        const QString path = value.toString().trimmed();
        if (!path.isEmpty()) {
            paths.push_back(path);
        }
    }
    return ingestSources(paths, normalizeSourceOriginType(sourceOriginType), true);
}

QVariantMap BatchAudioConverterService::replaceSourceFolder(const QString &folderPath)
{
    return ingestSourceFolder(folderPath, false);
}

QVariantMap BatchAudioConverterService::appendSourceFolder(const QString &folderPath)
{
    return ingestSourceFolder(folderPath, true);
}

bool BatchAudioConverterService::startBatch()
{
    if (m_isRunning) {
        const QString message = localizedBatchText(QStringLiteral("batchConverter.alreadyRunning"));
        setLastError(message);
        setStatusText(message);
        return false;
    }

    QString preparationError;
    if (!prepareItemsForBatchStart(&preparationError)) {
        resetFinalSummary();
        setLastError(preparationError);
        setStatusText(preparationError);
        return false;
    }

    resetFinalSummary();
    setLastError(QString());
    setCancelRequested(false);
    setBatchProgress(0.0);
    setJobStartedNow();
    beginRuntimeMeasurements();
    setIsRunning(true);
    setStatusText(localizedBatchText(QStringLiteral("batchConverter.started")));
    emit batchStarted();
    emit itemsChanged();

    startNextPendingItem();
    return true;
}

void BatchAudioConverterService::cancelBatch()
{
    if (!m_isRunning) {
        return;
    }

    setCancelRequested(true);
    markPendingItemsAsCanceled();
    emit itemsChanged();

    if (m_worker && m_worker->isRunning()) {
        m_worker->cancelConversion();
        return;
    }

    finalizeBatchRun(true);
}

QVariantMap BatchAudioConverterService::exportPresetSettings() const
{
    QVariantMap preset = settings();
    preset.remove(QStringLiteral("retryPolicy"));
    return preset;
}

bool BatchAudioConverterService::applySettingsMap(const QVariantMap &settings)
{
    if (!canMutateConfiguration()) {
        return false;
    }

    setOutputDirectory(settings.value(QStringLiteral("outputDirectory")).toString());
    setNamingPolicy(settings.value(QStringLiteral("namingPolicy")).toString());
    setFormat(settings.value(QStringLiteral("format")).toString());
    if (settings.contains(QStringLiteral("conflictPolicy"))) {
        setConflictPolicy(settings.value(QStringLiteral("conflictPolicy")).toString());
    }
    if (settings.contains(QStringLiteral("retryPolicy"))) {
        setRetryPolicy(settings.value(QStringLiteral("retryPolicy")).toString());
    }
    if (settings.contains(QStringLiteral("playlistAddMode"))) {
        setPlaylistAddMode(settings.value(QStringLiteral("playlistAddMode")).toString());
    } else if (settings.contains(QStringLiteral("addResultsToPlaylist"))) {
        setAddResultsToPlaylist(settings.value(QStringLiteral("addResultsToPlaylist")).toBool());
    }
    setBitrate(settings.value(QStringLiteral("bitrate"), m_settings.bitrate).toInt());
    setSampleRate(settings.value(QStringLiteral("sampleRate"), m_settings.sampleRate).toInt());
    setChannelMode(settings.value(QStringLiteral("channelMode")).toString());
    setPlaybackRate(settings.value(QStringLiteral("playbackRate"), m_settings.playbackRate).toDouble());
    setPitchSemitones(settings.value(QStringLiteral("pitchSemitones"), m_settings.pitchSemitones).toInt());
    setApplyEqualizer(settings.value(QStringLiteral("applyEqualizer"), m_settings.applyEqualizer).toBool());
    if (settings.contains(QStringLiteral("equalizerBandGains"))) {
        setEqualizerBandGains(settings.value(QStringLiteral("equalizerBandGains")).toList());
    }
    setApplyReverb(settings.value(QStringLiteral("applyReverb"), m_settings.applyReverb).toBool());
    setReverbRoomSize(settings.value(QStringLiteral("reverbRoomSize"), m_settings.reverbRoomSize).toDouble());
    setReverbDamping(settings.value(QStringLiteral("reverbDamping"), m_settings.reverbDamping).toDouble());
    setReverbWetLevel(settings.value(QStringLiteral("reverbWetLevel"), m_settings.reverbWetLevel).toDouble());
    return true;
}

QString BatchAudioConverterService::itemIdAt(int index) const
{
    if (!hasItemAt(index)) {
        return {};
    }
    return m_items.at(index).itemId;
}

int BatchAudioConverterService::indexOfItemId(const QString &itemId) const
{
    return indexOfItemIdInternal(itemId);
}

QVariantMap BatchAudioConverterService::itemById(const QString &itemId) const
{
    const int index = indexOfItemIdInternal(itemId);
    if (!hasItemAt(index)) {
        return {};
    }
    return toVariantMap(m_items.at(index));
}

bool BatchAudioConverterService::canRemoveItem(const QString &itemId) const
{
    return canRemoveItemAt(indexOfItemIdInternal(itemId));
}

bool BatchAudioConverterService::canRetryItem(const QString &itemId) const
{
    return canRetryItemAt(indexOfItemIdInternal(itemId));
}

bool BatchAudioConverterService::canMoveItemUp(const QString &itemId) const
{
    return canMoveItemAt(indexOfItemIdInternal(itemId), -1);
}

bool BatchAudioConverterService::canMoveItemDown(const QString &itemId) const
{
    return canMoveItemAt(indexOfItemIdInternal(itemId), 1);
}

bool BatchAudioConverterService::removeItemById(const QString &itemId)
{
    const int index = indexOfItemIdInternal(itemId);
    if (!canRemoveItemAt(index)) {
        return false;
    }

    m_items.removeAt(index);
    if (!m_isRunning) {
        refreshPlannedOutputs(nullptr);
    }
    resetSummaryForQueueMutation();
    updateAggregateProgress();
    if (m_items.isEmpty()) {
        resetJobMetadata();
    }
    emit itemsChanged();
    return true;
}

int BatchAudioConverterService::removeItemsById(const QVariantList &itemIds)
{
    QVector<int> indexes;
    indexes.reserve(itemIds.size());
    for (const QVariant &value : itemIds) {
        const int index = indexOfItemIdInternal(value.toString());
        if (canRemoveItemAt(index) && !indexes.contains(index)) {
            indexes.push_back(index);
        }
    }
    if (indexes.isEmpty()) {
        return 0;
    }

    std::sort(indexes.begin(), indexes.end(), std::greater<int>());
    for (const int index : indexes) {
        m_items.removeAt(index);
    }
    if (!m_isRunning) {
        refreshPlannedOutputs(nullptr);
    }
    resetSummaryForQueueMutation();
    updateAggregateProgress();
    if (m_items.isEmpty()) {
        resetJobMetadata();
    }
    emit itemsChanged();
    return indexes.size();
}

int BatchAudioConverterService::clearFailedItems()
{
    QVariantList itemIds;
    for (const BatchAudioConversionItem &item : std::as_const(m_items)) {
        if (item.state == Failed) {
            itemIds.push_back(item.itemId);
        }
    }
    return removeItemsById(itemIds);
}

int BatchAudioConverterService::clearCompletedItems()
{
    QVariantList itemIds;
    for (const BatchAudioConversionItem &item : std::as_const(m_items)) {
        if (isTerminalState(item.state)) {
            itemIds.push_back(item.itemId);
        }
    }
    return removeItemsById(itemIds);
}

bool BatchAudioConverterService::retryItemById(const QString &itemId)
{
    const int index = indexOfItemIdInternal(itemId);
    if (!canRetryItemAt(index)) {
        return false;
    }

    prepareItemForRetry(m_items[index]);
    if (!m_isRunning) {
        refreshPlannedOutputs(nullptr);
    }
    resetSummaryForQueueMutation();
    updateAggregateProgress();
    emit itemsChanged();
    return true;
}

int BatchAudioConverterService::retryItemsById(const QVariantList &itemIds)
{
    QVector<int> indexes;
    indexes.reserve(itemIds.size());
    for (const QVariant &value : itemIds) {
        const int index = indexOfItemIdInternal(value.toString());
        if (canRetryItemAt(index) && !indexes.contains(index)) {
            indexes.push_back(index);
        }
    }
    if (indexes.isEmpty()) {
        return 0;
    }

    for (const int index : indexes) {
        prepareItemForRetry(m_items[index]);
    }
    if (!m_isRunning) {
        refreshPlannedOutputs(nullptr);
    }
    resetSummaryForQueueMutation();
    updateAggregateProgress();
    emit itemsChanged();
    return indexes.size();
}

int BatchAudioConverterService::retryFailedItems()
{
    QVariantList itemIds;
    for (int i = 0; i < m_items.size(); ++i) {
        if (canRetryItemAt(i, Failed)) {
            itemIds.push_back(m_items.at(i).itemId);
        }
    }
    return retryItemsById(itemIds);
}

int BatchAudioConverterService::retrySkippedItems()
{
    QVariantList itemIds;
    for (int i = 0; i < m_items.size(); ++i) {
        if (canRetryItemAt(i, Skipped)) {
            itemIds.push_back(m_items.at(i).itemId);
        }
    }
    return retryItemsById(itemIds);
}

bool BatchAudioConverterService::moveItemUp(const QString &itemId)
{
    const int index = indexOfItemIdInternal(itemId);
    if (!canMoveItemAt(index, -1)) {
        return false;
    }
    return moveItemInternal(index, index - 1);
}

bool BatchAudioConverterService::moveItemDown(const QString &itemId)
{
    const int index = indexOfItemIdInternal(itemId);
    if (!canMoveItemAt(index, 1)) {
        return false;
    }
    return moveItemInternal(index, index + 1);
}

bool BatchAudioConverterService::setItemState(int index, ItemState state)
{
    if (!hasItemAt(index) || m_items[index].state == state) {
        return hasItemAt(index);
    }
    m_items[index].state = state;
    if (state != Failed && state != Skipped) {
        m_items[index].errorText.clear();
    }
    if (state == Failed && m_items[index].failureType == NoFailure) {
        m_items[index].failureType = InternalPipelineFailure;
    } else if (state == Skipped
               && (m_items[index].failureType == NoFailure
                   || m_items[index].failureType == InternalPipelineFailure)) {
        m_items[index].failureType = ValidationFailure;
    } else if (state == Canceled) {
        m_items[index].failureType = CanceledFailure;
    } else if (state == Succeeded) {
        m_items[index].failureType = NoFailure;
    }
    if ((state == Succeeded || state == Failed || state == Canceled || state == Skipped)
        && m_items[index].progress < 1.0) {
        m_items[index].progress = 1.0;
    }
    if (state == Pending) {
        m_items[index].progress = 0.0;
        m_items[index].terminalResult = QStringLiteral("none");
        if (m_items[index].failureType != NoFailure) {
            ++m_items[index].retryCount;
        }
        m_items[index].failureType = NoFailure;
    } else {
        m_items[index].terminalResult = itemStateKey(state);
    }
    touchItem(m_items[index]);
    updateAggregateProgress();
    emit itemsChanged();
    return true;
}

bool BatchAudioConverterService::setItemStateById(const QString &itemId, ItemState state)
{
    return setItemState(indexOfItemIdInternal(itemId), state);
}

bool BatchAudioConverterService::setItemProgress(int index, double progress)
{
    if (!hasItemAt(index)) {
        return false;
    }
    const double normalized = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(m_items[index].progress, normalized)) {
        return true;
    }
    m_items[index].progress = normalized;
    touchItem(m_items[index]);
    updateAggregateProgress();
    emit itemsChanged();
    return true;
}

bool BatchAudioConverterService::setItemProgressById(const QString &itemId, double progress)
{
    return setItemProgress(indexOfItemIdInternal(itemId), progress);
}

bool BatchAudioConverterService::setItemStatusText(int index, const QString &statusText)
{
    if (!hasItemAt(index) || m_items[index].statusText == statusText) {
        return hasItemAt(index);
    }
    m_items[index].statusText = statusText;
    touchItem(m_items[index]);
    emit itemsChanged();
    return true;
}

bool BatchAudioConverterService::setItemErrorText(int index, const QString &errorText)
{
    if (!hasItemAt(index) || m_items[index].errorText == errorText) {
        return hasItemAt(index);
    }
    m_items[index].errorText = errorText;
    if (!errorText.trimmed().isEmpty() && m_items[index].failureType == NoFailure) {
        m_items[index].failureType = classifyFailureMessage(errorText, InternalPipelineFailure);
    }
    touchItem(m_items[index]);
    emit itemsChanged();
    return true;
}

bool BatchAudioConverterService::setItemOutputFile(int index, const QString &outputFile)
{
    if (!hasItemAt(index)) {
        return false;
    }

    QString normalized = normalizeLocalPath(outputFile);
    if (normalized.isEmpty()) {
        normalized = outputFile.trimmed();
    }
    if (m_items[index].outputFile == normalized) {
        return true;
    }
    m_items[index].outputFile = normalized;
    m_items[index].conflictResolution.requestedOutputFile = normalized;
    m_items[index].conflictResolution.resolvedOutputFile = normalized;
    m_items[index].conflictResolution.hadConflict = false;
    m_items[index].conflictResolution.resolutionKey = QStringLiteral("manual");
    touchItem(m_items[index]);
    emit itemsChanged();
    return true;
}

bool BatchAudioConverterService::setItemResultFile(int index, const QString &resultFile)
{
    if (!hasItemAt(index)) {
        return false;
    }

    QString normalized = normalizeLocalPath(resultFile);
    if (normalized.isEmpty()) {
        normalized = resultFile.trimmed();
    }
    if (m_items[index].resultFile == normalized) {
        return true;
    }
    m_items[index].resultFile = normalized;
    touchItem(m_items[index]);
    emit itemsChanged();
    return true;
}

bool BatchAudioConverterService::setItemSourceMetadata(int index,
                                                       const QString &sourceDisplayName,
                                                       const QString &sourceFormat,
                                                       qint64 sourceDurationMs)
{
    if (!hasItemAt(index)) {
        return false;
    }

    BatchAudioConversionItem &item = m_items[index];
    const QString normalizedDisplayName = sourceDisplayName.trimmed();
    const QString normalizedFormat = normalizeSourceFormat(sourceFormat);
    const qint64 normalizedDuration = qMax<qint64>(0, sourceDurationMs);
    if (item.sourceDisplayName == normalizedDisplayName
        && item.sourceFormat == normalizedFormat
        && item.sourceDurationMs == normalizedDuration) {
        return true;
    }

    item.sourceDisplayName = normalizedDisplayName;
    item.sourceFormat = normalizedFormat;
    item.sourceDurationMs = normalizedDuration;
    touchItem(item);
    emit itemsChanged();
    return true;
}

void BatchAudioConverterService::setOutputDirectory(const QString &outputDirectory)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const QString normalized = normalizeLocalPath(outputDirectory);
    if (m_settings.outputDirectory == normalized) {
        return;
    }
    m_settings.outputDirectory = normalized;
    emit outputDirectoryChanged();
    emitSettingsChanged();
    refreshPlannedOutputs(nullptr);
    emit itemsChanged();
}

void BatchAudioConverterService::setNamingPolicy(const QString &namingPolicy)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const QString normalized = normalizeNamingPolicy(namingPolicy);
    if (m_settings.namingPolicy == normalized) {
        return;
    }
    m_settings.namingPolicy = normalized;
    emit namingPolicyChanged();
    emitSettingsChanged();
    refreshPlannedOutputs(nullptr);
    emit itemsChanged();
}

void BatchAudioConverterService::setFormat(const QString &format)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const QString normalized = normalizeFormat(format);
    if (m_settings.format == normalized) {
        return;
    }
    m_settings.format = normalized;
    emit formatChanged();
    emitSettingsChanged();
    refreshPlannedOutputs(nullptr);
    emit itemsChanged();
}

void BatchAudioConverterService::setConflictPolicy(const QString &conflictPolicy)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const ConflictPolicy normalized = normalizeConflictPolicy(conflictPolicy);
    if (m_settings.conflictPolicy == normalized) {
        return;
    }
    m_settings.conflictPolicy = normalized;
    emit conflictPolicyChanged();
    emitSettingsChanged();
    refreshPlannedOutputs(nullptr);
    emit itemsChanged();
}

void BatchAudioConverterService::setRetryPolicy(const QString &retryPolicy)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const RetryPolicy normalized = normalizeRetryPolicy(retryPolicy);
    if (m_settings.retryPolicy == normalized) {
        return;
    }
    m_settings.retryPolicy = normalized;
    emit retryPolicyChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setPlaylistAddMode(const QString &playlistAddMode)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const PlaylistAddMode normalized = normalizePlaylistAddMode(playlistAddMode, m_settings.addResultsToPlaylist);
    if (m_settings.playlistAddMode == normalized) {
        return;
    }
    m_settings.playlistAddMode = normalized;
    m_settings.addResultsToPlaylist = normalized != DisabledPlaylistAddMode;
    emit playlistAddModeChanged();
    emit addResultsToPlaylistChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setBitrate(int bitrate)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const int normalized = normalizeBitrate(bitrate);
    if (m_settings.bitrate == normalized) {
        return;
    }
    m_settings.bitrate = normalized;
    emit bitrateChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setSampleRate(int sampleRate)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const int normalized = normalizeSampleRate(sampleRate);
    if (m_settings.sampleRate == normalized) {
        return;
    }
    m_settings.sampleRate = normalized;
    emit sampleRateChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setChannelMode(const QString &channelMode)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const QString normalized = normalizeChannelMode(channelMode);
    if (m_settings.channelMode == normalized) {
        return;
    }
    m_settings.channelMode = normalized;
    emit channelModeChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setPlaybackRate(double playbackRate)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const double normalized = normalizePlaybackRate(playbackRate);
    if (qFuzzyCompare(m_settings.playbackRate, normalized)) {
        return;
    }
    m_settings.playbackRate = normalized;
    emit playbackRateChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setPitchSemitones(int pitchSemitones)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const int normalized = normalizePitchSemitones(pitchSemitones);
    if (m_settings.pitchSemitones == normalized) {
        return;
    }
    m_settings.pitchSemitones = normalized;
    emit pitchSemitonesChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setApplyEqualizer(bool applyEqualizer)
{
    if (!canMutateConfiguration() || m_settings.applyEqualizer == applyEqualizer) {
        return;
    }

    m_settings.applyEqualizer = applyEqualizer;
    emit applyEqualizerChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setEqualizerBandGains(const QVariantList &gains)
{
    if (!canMutateConfiguration()) {
        return;
    }

    const QVariantList normalized = normalizeEqualizerBandGains(gains);
    if (m_settings.equalizerBandGains.size() == normalized.size()) {
        bool equal = true;
        for (int i = 0; i < normalized.size(); ++i) {
            if (qAbs(m_settings.equalizerBandGains.at(i).toDouble() - normalized.at(i).toDouble()) > 0.01) {
                equal = false;
                break;
            }
        }
        if (equal) {
            return;
        }
    }

    m_settings.equalizerBandGains = normalized;
    emit equalizerBandGainsChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setApplyReverb(bool applyReverb)
{
    if (!canMutateConfiguration() || m_settings.applyReverb == applyReverb) {
        return;
    }

    m_settings.applyReverb = applyReverb;
    emit applyReverbChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setReverbRoomSize(double roomSize)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const double normalized = normalizeUnitInterval(roomSize, 0.55);
    if (qFuzzyCompare(m_settings.reverbRoomSize, normalized)) {
        return;
    }
    m_settings.reverbRoomSize = normalized;
    emit reverbRoomSizeChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setReverbDamping(double damping)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const double normalized = normalizeUnitInterval(damping, 0.35);
    if (qFuzzyCompare(m_settings.reverbDamping, normalized)) {
        return;
    }
    m_settings.reverbDamping = normalized;
    emit reverbDampingChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setReverbWetLevel(double wetLevel)
{
    if (!canMutateConfiguration()) {
        return;
    }
    const double normalized = normalizeUnitInterval(wetLevel, 0.28);
    if (qFuzzyCompare(m_settings.reverbWetLevel, normalized)) {
        return;
    }
    m_settings.reverbWetLevel = normalized;
    emit reverbWetLevelChanged();
    emitSettingsChanged();
}

void BatchAudioConverterService::setAddResultsToPlaylist(bool addResultsToPlaylist)
{
    if (!canMutateConfiguration()) {
        return;
    }
    if (m_settings.addResultsToPlaylist == addResultsToPlaylist) {
        return;
    }
    m_settings.addResultsToPlaylist = addResultsToPlaylist;
    if (!addResultsToPlaylist) {
        m_settings.playlistAddMode = DisabledPlaylistAddMode;
    } else if (m_settings.playlistAddMode == DisabledPlaylistAddMode) {
        m_settings.playlistAddMode = ImmediatePlaylistAddMode;
    }
    emit addResultsToPlaylistChanged();
    emit playlistAddModeChanged();
    emitSettingsChanged();
}

QString BatchAudioConverterService::normalizeLocalPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (isSlashPrefixedWindowsAbsolutePath(trimmed)) {
        return QDir::cleanPath(trimmed.mid(1));
    }

    if (QDir::isAbsolutePath(trimmed) || isLikelyWindowsAbsolutePath(trimmed)) {
        return QDir::cleanPath(trimmed);
    }

    const QUrl parsed(trimmed);
    if (parsed.isValid() && parsed.isLocalFile()) {
        return QDir::cleanPath(parsed.toLocalFile());
    }

    return {};
}

QString BatchAudioConverterService::inferDisplayName(const QString &path)
{
    const QFileInfo info(path);
    const QString fileName = info.fileName().trimmed();
    if (!fileName.isEmpty()) {
        return fileName;
    }
    return path.trimmed();
}

QString BatchAudioConverterService::inferFormat(const QString &path)
{
    return QFileInfo(path).suffix().trimmed().toLower();
}

QString BatchAudioConverterService::normalizeSourceFormat(const QString &format)
{
    return format.trimmed().toLower();
}

BatchAudioConverterService::SourceOriginType BatchAudioConverterService::normalizeSourceOriginType(
    const QString &sourceOriginType)
{
    const QString normalized = sourceOriginType.trimmed().toLower();
    if (normalized == QStringLiteral("playlist-selection")) {
        return PlaylistSelectionSourceOrigin;
    }
    if (normalized == QStringLiteral("file-picker")) {
        return FilePickerSourceOrigin;
    }
    if (normalized == QStringLiteral("folder-import")) {
        return FolderImportSourceOrigin;
    }
    if (normalized == QStringLiteral("dropped-url")) {
        return DroppedUrlSourceOrigin;
    }
    return UnknownSourceOrigin;
}

QString BatchAudioConverterService::normalizeNamingPolicy(const QString &namingPolicy)
{
    const QString normalized = namingPolicy.trimmed().toLower();
    if (normalized == QStringLiteral("basename")
        || normalized == QStringLiteral("artist-title")
        || normalized == QStringLiteral("album-track-title")) {
        return normalized;
    }
    return QStringLiteral("basename");
}

QString BatchAudioConverterService::normalizeFormat(const QString &format)
{
    const QString normalized = format.trimmed().toLower();
    if (normalized == QStringLiteral("mp3")
        || normalized == QStringLiteral("flac")
        || normalized == QStringLiteral("wav")
        || normalized == QStringLiteral("opus")
        || normalized == QStringLiteral("webm")) {
        return normalized;
    }
    return QStringLiteral("mp3");
}

BatchAudioConverterService::ConflictPolicy BatchAudioConverterService::normalizeConflictPolicy(
    const QString &conflictPolicy)
{
    const QString normalized = conflictPolicy.trimmed().toLower();
    if (normalized == QStringLiteral("overwrite-if-allowed")) {
        return OverwriteExistingConflictPolicy;
    }
    if (normalized == QStringLiteral("skip-on-conflict")) {
        return SkipOnConflictPolicy;
    }
    if (normalized == QStringLiteral("fail-on-conflict")) {
        return FailOnConflictPolicy;
    }
    return AutoRenameConflictPolicy;
}

BatchAudioConverterService::RetryPolicy BatchAudioConverterService::normalizeRetryPolicy(
    const QString &retryPolicy)
{
    const QString normalized = retryPolicy.trimmed().toLower();
    if (normalized == QStringLiteral("retry-failed-only")) {
        return RetryFailedOnlyPolicy;
    }
    if (normalized == QStringLiteral("retry-failed-and-skipped")) {
        return RetryFailedAndSkippedPolicy;
    }
    return ManualRetryPolicy;
}

BatchAudioConverterService::PlaylistAddMode BatchAudioConverterService::normalizePlaylistAddMode(
    const QString &playlistAddMode,
    bool addResultsToPlaylistFallback)
{
    const QString normalized = playlistAddMode.trimmed().toLower();
    if (normalized == QStringLiteral("deferred")) {
        return DeferredPlaylistAddMode;
    }
    if (normalized == QStringLiteral("disabled")
        || normalized == QStringLiteral("off")
        || normalized == QStringLiteral("never")) {
        return DisabledPlaylistAddMode;
    }
    if (normalized == QStringLiteral("immediate")) {
        return ImmediatePlaylistAddMode;
    }
    return addResultsToPlaylistFallback ? ImmediatePlaylistAddMode : DisabledPlaylistAddMode;
}

QString BatchAudioConverterService::normalizeChannelMode(const QString &channelMode)
{
    const QString normalized = channelMode.trimmed().toLower();
    if (normalized == QStringLiteral("mono") || normalized == QStringLiteral("stereo")) {
        return normalized;
    }
    return QStringLiteral("stereo");
}

int BatchAudioConverterService::normalizeBitrate(int bitrate)
{
    return qMax(0, bitrate);
}

int BatchAudioConverterService::normalizeSampleRate(int sampleRate)
{
    return qMax(0, sampleRate);
}

double BatchAudioConverterService::normalizePlaybackRate(double playbackRate)
{
    if (!qIsFinite(playbackRate)) {
        return 1.0;
    }
    return qBound(0.25, playbackRate, 4.0);
}

int BatchAudioConverterService::normalizePitchSemitones(int pitchSemitones)
{
    return qBound(-24, pitchSemitones, 24);
}

double BatchAudioConverterService::normalizeUnitInterval(double value, double fallback)
{
    if (!qIsFinite(value)) {
        return fallback;
    }

    return qBound(0.0, value, 1.0);
}

QVariantList BatchAudioConverterService::normalizeEqualizerBandGains(const QVariantList &gains)
{
    QVariantList normalized;
    normalized.reserve(10);
    for (int i = 0; i < 10; ++i) {
        const double source = i < gains.size() ? gains.at(i).toDouble() : 0.0;
        normalized.push_back(qBound(-24.0, source, 12.0));
    }
    return normalized;
}

QString BatchAudioConverterService::uniqueOutputPath(const QString &path,
                                                     const QSet<QString> &reservedPaths,
                                                     const QSet<QString> &existingPaths)
{
    QFileInfo info(path);
    const QString directory = info.absolutePath();
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate = path;
    int counter = 1;
    const QString convertedSuffix = QStringLiteral(" (converted)");
    const bool followsConvertedPattern = baseName.endsWith(convertedSuffix);
    const QString originalBaseName = followsConvertedPattern
        ? baseName.left(baseName.size() - convertedSuffix.size())
        : baseName;

    while (QFileInfo::exists(candidate)
           || existingPaths.contains(candidate)
           || reservedPaths.contains(candidate)) {
        const QString numberedBase = followsConvertedPattern
            ? QStringLiteral("%1 (converted %2)").arg(originalBaseName).arg(counter)
            : QStringLiteral("%1 %2").arg(baseName).arg(counter);
        candidate = QDir(directory).filePath(QStringLiteral("%1.%2").arg(numberedBase, suffix));
        ++counter;
    }

    return candidate;
}

QString BatchAudioConverterService::sanitizeFileNameComponent(const QString &value)
{
    QString sanitized = value.simplified();
    sanitized.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1F]")));
    sanitized.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    while (sanitized.endsWith(QLatin1Char('.')) || sanitized.endsWith(QLatin1Char(' '))) {
        sanitized.chop(1);
    }
    return sanitized.trimmed();
}

QString BatchAudioConverterService::itemStateKey(ItemState state)
{
    switch (state) {
    case Pending:
        return QStringLiteral("pending");
    case Running:
        return QStringLiteral("running");
    case Succeeded:
        return QStringLiteral("succeeded");
    case Failed:
        return QStringLiteral("failed");
    case Canceled:
        return QStringLiteral("canceled");
    case Skipped:
        return QStringLiteral("skipped");
    }
    return QStringLiteral("pending");
}

QString BatchAudioConverterService::sourceOriginTypeKey(SourceOriginType sourceOriginType)
{
    switch (sourceOriginType) {
    case PlaylistSelectionSourceOrigin:
        return QStringLiteral("playlist-selection");
    case FilePickerSourceOrigin:
        return QStringLiteral("file-picker");
    case FolderImportSourceOrigin:
        return QStringLiteral("folder-import");
    case DroppedUrlSourceOrigin:
        return QStringLiteral("dropped-url");
    case UnknownSourceOrigin:
        break;
    }
    return QStringLiteral("unknown");
}

QString BatchAudioConverterService::itemActionabilityKey(ItemActionability actionability)
{
    switch (actionability) {
    case PendingActionability:
        return QStringLiteral("pending");
    case RunningActionability:
        return QStringLiteral("running");
    case RetryableActionability:
        return QStringLiteral("retryable");
    case TerminalActionability:
        return QStringLiteral("terminal");
    case NoActionability:
        break;
    }
    return QStringLiteral("none");
}

QString BatchAudioConverterService::failureTypeKey(FailureType failureType)
{
    switch (failureType) {
    case ValidationFailure:
        return QStringLiteral("validation");
    case SourceMissingFailure:
        return QStringLiteral("source-missing");
    case OutputConflictFailure:
        return QStringLiteral("output-conflict");
    case UnsupportedFormatFailure:
        return QStringLiteral("unsupported-format");
    case EncoderUnavailableFailure:
        return QStringLiteral("encoder-unavailable");
    case PermissionDeniedFailure:
        return QStringLiteral("permission-denied");
    case InternalPipelineFailure:
        return QStringLiteral("internal-pipeline");
    case BackendFailure:
        return QStringLiteral("backend");
    case CanceledFailure:
        return QStringLiteral("canceled");
    case NoFailure:
        break;
    }
    return QStringLiteral("none");
}

QString BatchAudioConverterService::conflictPolicyKey(ConflictPolicy conflictPolicy)
{
    switch (conflictPolicy) {
    case OverwriteExistingConflictPolicy:
        return QStringLiteral("overwrite-if-allowed");
    case SkipOnConflictPolicy:
        return QStringLiteral("skip-on-conflict");
    case FailOnConflictPolicy:
        return QStringLiteral("fail-on-conflict");
    case AutoRenameConflictPolicy:
        break;
    }
    return QStringLiteral("auto-rename");
}

QString BatchAudioConverterService::retryPolicyKey(RetryPolicy retryPolicy)
{
    switch (retryPolicy) {
    case RetryFailedOnlyPolicy:
        return QStringLiteral("retry-failed-only");
    case RetryFailedAndSkippedPolicy:
        return QStringLiteral("retry-failed-and-skipped");
    case ManualRetryPolicy:
        break;
    }
    return QStringLiteral("manual");
}

QString BatchAudioConverterService::playlistAddModeKey(PlaylistAddMode playlistAddMode)
{
    switch (playlistAddMode) {
    case DeferredPlaylistAddMode:
        return QStringLiteral("deferred");
    case DisabledPlaylistAddMode:
        return QStringLiteral("disabled");
    case ImmediatePlaylistAddMode:
        break;
    }
    return QStringLiteral("immediate");
}

QVariantList BatchAudioConverterService::stringListToVariantList(const QStringList &values)
{
    QVariantList result;
    result.reserve(values.size());
    for (const QString &value : values) {
        result.push_back(value);
    }
    return result;
}

QVariantMap BatchAudioConverterService::effectiveSettingsToVariantMap(
    const EffectiveSettingsSnapshot &settings)
{
    QVariantMap result;
    result.insert(QStringLiteral("outputDirectory"), settings.outputDirectory);
    result.insert(QStringLiteral("namingPolicy"), settings.namingPolicy);
    result.insert(QStringLiteral("format"), settings.format);
    result.insert(QStringLiteral("conflictPolicy"), conflictPolicyKey(settings.conflictPolicy));
    result.insert(QStringLiteral("retryPolicy"), retryPolicyKey(settings.retryPolicy));
    result.insert(QStringLiteral("playlistAddMode"), playlistAddModeKey(settings.playlistAddMode));
    result.insert(QStringLiteral("bitrate"), settings.bitrate);
    result.insert(QStringLiteral("sampleRate"), settings.sampleRate);
    result.insert(QStringLiteral("channelMode"), settings.channelMode);
    result.insert(QStringLiteral("playbackRate"), settings.playbackRate);
    result.insert(QStringLiteral("pitchSemitones"), settings.pitchSemitones);
    result.insert(QStringLiteral("applyEqualizer"), settings.applyEqualizer);
    result.insert(QStringLiteral("equalizerBandGains"), settings.equalizerBandGains);
    result.insert(QStringLiteral("applyReverb"), settings.applyReverb);
    result.insert(QStringLiteral("reverbRoomSize"), settings.reverbRoomSize);
    result.insert(QStringLiteral("reverbDamping"), settings.reverbDamping);
    result.insert(QStringLiteral("reverbWetLevel"), settings.reverbWetLevel);
    result.insert(QStringLiteral("addResultsToPlaylist"), settings.addResultsToPlaylist);
    result.insert(QStringLiteral("capturedAtMs"), settings.capturedAtMs);
    return result;
}

BatchAudioConverterService::EffectiveSettingsSnapshot
BatchAudioConverterService::effectiveSettingsFromVariantMap(const QVariantMap &settings)
{
    EffectiveSettingsSnapshot snapshot;
    snapshot.outputDirectory = settings.value(QStringLiteral("outputDirectory")).toString().trimmed();
    snapshot.namingPolicy = normalizeNamingPolicy(settings.value(QStringLiteral("namingPolicy")).toString());
    snapshot.format = normalizeFormat(settings.value(QStringLiteral("format")).toString());
    snapshot.conflictPolicy = normalizeConflictPolicy(
        settings.value(QStringLiteral("conflictPolicy")).toString());
    snapshot.retryPolicy = normalizeRetryPolicy(settings.value(QStringLiteral("retryPolicy")).toString());
    snapshot.playlistAddMode = normalizePlaylistAddMode(
        settings.value(QStringLiteral("playlistAddMode")).toString(),
        settings.value(QStringLiteral("addResultsToPlaylist"), true).toBool());
    snapshot.bitrate = normalizeBitrate(settings.value(QStringLiteral("bitrate")).toInt());
    snapshot.sampleRate = normalizeSampleRate(settings.value(QStringLiteral("sampleRate")).toInt());
    snapshot.channelMode = normalizeChannelMode(settings.value(QStringLiteral("channelMode")).toString());
    snapshot.playbackRate = normalizePlaybackRate(settings.value(QStringLiteral("playbackRate"), 1.0).toDouble());
    snapshot.pitchSemitones = normalizePitchSemitones(
        settings.value(QStringLiteral("pitchSemitones")).toInt());
    snapshot.applyEqualizer = settings.value(QStringLiteral("applyEqualizer"), false).toBool();
    snapshot.equalizerBandGains = normalizeEqualizerBandGains(
        settings.value(QStringLiteral("equalizerBandGains")).toList());
    snapshot.applyReverb = settings.value(QStringLiteral("applyReverb"), false).toBool();
    snapshot.reverbRoomSize = normalizeUnitInterval(
        settings.value(QStringLiteral("reverbRoomSize"), 0.55).toDouble(), 0.55);
    snapshot.reverbDamping = normalizeUnitInterval(
        settings.value(QStringLiteral("reverbDamping"), 0.35).toDouble(), 0.35);
    snapshot.reverbWetLevel = normalizeUnitInterval(
        settings.value(QStringLiteral("reverbWetLevel"), 0.28).toDouble(), 0.28);
    snapshot.addResultsToPlaylist = snapshot.playlistAddMode != DisabledPlaylistAddMode;
    snapshot.capturedAtMs = qMax<qint64>(0, settings.value(QStringLiteral("capturedAtMs")).toLongLong());
    return snapshot;
}

QVariantMap BatchAudioConverterService::conflictResolutionToVariantMap(
    const ConflictResolutionInfo &info)
{
    QVariantMap result;
    result.insert(QStringLiteral("requestedOutputFile"), info.requestedOutputFile);
    result.insert(QStringLiteral("resolvedOutputFile"), info.resolvedOutputFile);
    result.insert(QStringLiteral("resolutionKey"), info.resolutionKey);
    result.insert(QStringLiteral("collisionRuleKey"), info.collisionRuleKey);
    result.insert(QStringLiteral("hadConflict"), info.hadConflict);
    result.insert(QStringLiteral("willOverwriteExisting"), info.willOverwriteExisting);
    result.insert(QStringLiteral("targetExistsOnDisk"), info.targetExistsOnDisk);
    result.insert(QStringLiteral("finalizationStrategyKey"), info.finalizationStrategyKey);
    return result;
}

BatchAudioConverterService::ConflictResolutionInfo
BatchAudioConverterService::conflictResolutionFromVariantMap(const QVariantMap &info)
{
    ConflictResolutionInfo conflict;
    conflict.requestedOutputFile = info.value(QStringLiteral("requestedOutputFile")).toString().trimmed();
    conflict.resolvedOutputFile = info.value(QStringLiteral("resolvedOutputFile")).toString().trimmed();
    conflict.resolutionKey = info.value(QStringLiteral("resolutionKey"), QStringLiteral("none")).toString();
    conflict.collisionRuleKey = info.value(QStringLiteral("collisionRuleKey"), QStringLiteral("none")).toString();
    conflict.hadConflict = info.value(QStringLiteral("hadConflict")).toBool();
    conflict.willOverwriteExisting = info.value(QStringLiteral("willOverwriteExisting")).toBool();
    conflict.targetExistsOnDisk = info.value(QStringLiteral("targetExistsOnDisk")).toBool();
    conflict.finalizationStrategyKey = info.value(QStringLiteral("finalizationStrategyKey"),
                                                  QStringLiteral("temp-commit")).toString();
    return conflict;
}

QVariantMap BatchAudioConverterService::toVariantMap(const BatchAudioConversionItem &item)
{
    QVariantMap sourceIdentity;
    sourceIdentity.insert(QStringLiteral("itemId"), item.itemId);
    sourceIdentity.insert(QStringLiteral("sourceFile"), item.sourceFile);
    sourceIdentity.insert(QStringLiteral("sourceDisplayName"), item.sourceDisplayName);
    sourceIdentity.insert(QStringLiteral("sourceFormat"), item.sourceFormat);
    sourceIdentity.insert(QStringLiteral("sourceDurationMs"), item.sourceDurationMs);
    sourceIdentity.insert(QStringLiteral("sourceOriginType"), sourceOriginTypeKey(item.sourceOriginType));

    QVariantMap queueMetadata;
    queueMetadata.insert(QStringLiteral("retryCount"), item.retryCount);
    queueMetadata.insert(QStringLiteral("createdAtMs"), item.createdAtMs);
    queueMetadata.insert(QStringLiteral("updatedAtMs"), item.updatedAtMs);
    queueMetadata.insert(QStringLiteral("plannedOutputFile"), item.outputFile);
    queueMetadata.insert(QStringLiteral("conflictResolution"), conflictResolutionToVariantMap(item.conflictResolution));

    QVariantMap runtimeState;
    runtimeState.insert(QStringLiteral("state"), itemStateKey(item.state));
    runtimeState.insert(QStringLiteral("progress"), item.progress);
    runtimeState.insert(QStringLiteral("statusText"), item.statusText);
    runtimeState.insert(QStringLiteral("itemActionability"),
                        itemActionabilityKey(item.state == Pending ? PendingActionability
                                               : item.state == Running ? RunningActionability
                                               : ((item.state == Failed || item.state == Skipped)
                                                  && isRetryEligibleFailureType(item.failureType))
                                                   ? RetryableActionability
                                                   : TerminalActionability));
    runtimeState.insert(QStringLiteral("effectiveSettingsSnapshot"),
                        effectiveSettingsToVariantMap(item.effectiveSettings));

    QVariantMap finalResultState;
    finalResultState.insert(QStringLiteral("resultFile"), item.resultFile);
    finalResultState.insert(QStringLiteral("errorText"), item.errorText);
    finalResultState.insert(QStringLiteral("terminalResult"), item.terminalResult);
    finalResultState.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
    finalResultState.insert(QStringLiteral("reportMetadata"), item.reportMetadata);

    QVariantMap result;
    result.insert(QStringLiteral("itemId"), item.itemId);
    result.insert(QStringLiteral("sourceFile"), item.sourceFile);
    result.insert(QStringLiteral("sourceDisplayName"), item.sourceDisplayName);
    result.insert(QStringLiteral("sourceFormat"), item.sourceFormat);
    result.insert(QStringLiteral("sourceDurationMs"), item.sourceDurationMs);
    result.insert(QStringLiteral("sourceOriginType"), sourceOriginTypeKey(item.sourceOriginType));
    result.insert(QStringLiteral("outputFile"), item.outputFile);
    result.insert(QStringLiteral("state"), itemStateKey(item.state));
    result.insert(QStringLiteral("progress"), item.progress);
    result.insert(QStringLiteral("statusText"), item.statusText);
    result.insert(QStringLiteral("errorText"), item.errorText);
    result.insert(QStringLiteral("resultFile"), item.resultFile);
    result.insert(QStringLiteral("retryCount"), item.retryCount);
    result.insert(QStringLiteral("createdAtMs"), item.createdAtMs);
    result.insert(QStringLiteral("updatedAtMs"), item.updatedAtMs);
    result.insert(QStringLiteral("terminalResult"), item.terminalResult);
    result.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
    result.insert(QStringLiteral("itemActionability"), runtimeState.value(QStringLiteral("itemActionability")));
    result.insert(QStringLiteral("effectiveSettingsSnapshot"),
                  runtimeState.value(QStringLiteral("effectiveSettingsSnapshot")));
    result.insert(QStringLiteral("conflictResolution"),
                  queueMetadata.value(QStringLiteral("conflictResolution")));
    result.insert(QStringLiteral("previewDiagnostics"),
                  item.reportMetadata.value(QStringLiteral("previewDiagnostics")).toMap());
    result.insert(QStringLiteral("reportMetadata"), item.reportMetadata);
    result.insert(QStringLiteral("sourceIdentity"), sourceIdentity);
    result.insert(QStringLiteral("queueMetadata"), queueMetadata);
    result.insert(QStringLiteral("runtimeState"), runtimeState);
    result.insert(QStringLiteral("finalResultState"), finalResultState);
    return result;
}

bool BatchAudioConverterService::hasItemAt(int index) const
{
    return index >= 0 && index < m_items.size();
}

bool BatchAudioConverterService::isTerminalState(ItemState state)
{
    return state == Succeeded
        || state == Failed
        || state == Canceled
        || state == Skipped;
}

bool BatchAudioConverterService::isRetryEligibleState(ItemState state)
{
    return state == Failed || state == Skipped;
}

bool BatchAudioConverterService::isRetryEligibleFailureType(FailureType failureType)
{
    switch (failureType) {
    case SourceMissingFailure:
    case OutputConflictFailure:
    case EncoderUnavailableFailure:
    case PermissionDeniedFailure:
    case InternalPipelineFailure:
    case BackendFailure:
        return true;
    case NoFailure:
    case ValidationFailure:
    case UnsupportedFormatFailure:
    case CanceledFailure:
        break;
    }
    return false;
}

BatchAudioConverterService::FailureType BatchAudioConverterService::classifyFailureMessage(
    const QString &message,
    FailureType fallbackFailureType)
{
    const QString normalized = message.trimmed().toLower();
    if (normalized.isEmpty()) {
        return fallbackFailureType;
    }

    if (normalized.contains(QStringLiteral("output file already exists"))
        || normalized.contains(QStringLiteral("output path conflict"))
        || normalized.contains(QStringLiteral("already targets the same output path"))) {
        return OutputConflictFailure;
    }
    if (normalized.contains(QStringLiteral("selected output format is not supported"))
        || normalized.contains(QStringLiteral("source format is not supported"))
        || normalized.contains(QStringLiteral("unsupported source type"))) {
        return UnsupportedFormatFailure;
    }
    if (normalized.contains(QStringLiteral("source file is missing"))
        || normalized.contains(QStringLiteral("source file does not exist"))
        || normalized.contains(QStringLiteral("missing or unreadable"))) {
        return SourceMissingFailure;
    }
    if (normalized.contains(QStringLiteral("plugin is unavailable"))
        || normalized.contains(QStringLiteral("missing encoder"))
        || normalized.contains(QStringLiteral("missing encoder or muxer"))) {
        return EncoderUnavailableFailure;
    }
    if (normalized.contains(QStringLiteral("permission denied"))
        || normalized.contains(QStringLiteral("not writable"))
        || normalized.contains(QStringLiteral("cannot write to the output directory"))
        || normalized.contains(QStringLiteral("failed to replace the existing output file"))
        || normalized.contains(QStringLiteral("failed to finalize the converted output file"))) {
        return PermissionDeniedFailure;
    }
    if (normalized.contains(QStringLiteral("pipeline failed"))
        || normalized.contains(QStringLiteral("failed to create the gstreamer conversion pipeline"))
        || normalized.contains(QStringLiteral("failed to start audio conversion pipeline"))
        || normalized.contains(QStringLiteral("failed to link "))
        || normalized.contains(QStringLiteral("source file path is invalid"))) {
        return InternalPipelineFailure;
    }
    return fallbackFailureType;
}

QStringList BatchAudioConverterService::settingsDiffKeys(const QVariantMap &previousSettings,
                                                        const QVariantMap &currentSettings)
{
    static const QStringList trackedKeys = {
        QStringLiteral("outputDirectory"),
        QStringLiteral("namingPolicy"),
        QStringLiteral("format"),
        QStringLiteral("conflictPolicy"),
        QStringLiteral("retryPolicy"),
        QStringLiteral("playlistAddMode"),
        QStringLiteral("bitrate"),
        QStringLiteral("sampleRate"),
        QStringLiteral("channelMode"),
        QStringLiteral("playbackRate"),
        QStringLiteral("pitchSemitones"),
        QStringLiteral("applyEqualizer"),
        QStringLiteral("equalizerBandGains"),
        QStringLiteral("applyReverb"),
        QStringLiteral("reverbRoomSize"),
        QStringLiteral("reverbDamping"),
        QStringLiteral("reverbWetLevel"),
        QStringLiteral("addResultsToPlaylist")
    };

    QStringList changedKeys;
    for (const QString &key : trackedKeys) {
        if (previousSettings.value(key) != currentSettings.value(key)) {
            changedKeys.push_back(key);
        }
    }
    return changedKeys;
}

void BatchAudioConverterService::recordTerminalAttempt(BatchAudioConversionItem &item)
{
    if (!isTerminalState(item.state)) {
        return;
    }

    const int attemptNumber = qMax(1, item.retryCount + 1);
    if (item.reportMetadata.value(QStringLiteral("lastRecordedAttemptNumber")).toInt() == attemptNumber) {
        return;
    }

    QVariantList attempts = item.reportMetadata.value(QStringLiteral("attempts")).toList();
    const QVariantMap previousAttempt = attempts.isEmpty() ? QVariantMap{} : attempts.constLast().toMap();

    QVariantMap effectiveSettings = effectiveSettingsToVariantMap(item.effectiveSettings);
    if (!item.effectiveSettings.capturedAtMs) {
        EffectiveSettingsSnapshot fallbackSettings;
        fallbackSettings.outputDirectory =
            item.reportMetadata.value(QStringLiteral("plannedSettingsOutputDirectory")).toString();
        fallbackSettings.namingPolicy =
            item.reportMetadata.value(QStringLiteral("plannedSettingsNamingPolicy")).toString();
        fallbackSettings.format =
            item.reportMetadata.value(QStringLiteral("plannedSettingsFormat")).toString();
        fallbackSettings.conflictPolicy = normalizeConflictPolicy(
            item.reportMetadata.value(QStringLiteral("plannedSettingsConflictPolicy")).toString());
        fallbackSettings.retryPolicy = normalizeRetryPolicy(
            item.reportMetadata.value(QStringLiteral("plannedSettingsRetryPolicy")).toString());
        fallbackSettings.playlistAddMode = normalizePlaylistAddMode(
            item.reportMetadata.value(QStringLiteral("plannedSettingsPlaylistAddMode")).toString(),
            item.reportMetadata.value(QStringLiteral("plannedSettingsAddResultsToPlaylist"), true).toBool());
        fallbackSettings.bitrate =
            item.reportMetadata.value(QStringLiteral("plannedSettingsBitrate")).toInt();
        fallbackSettings.sampleRate =
            item.reportMetadata.value(QStringLiteral("plannedSettingsSampleRate")).toInt();
        fallbackSettings.channelMode =
            item.reportMetadata.value(QStringLiteral("plannedSettingsChannelMode")).toString();
        fallbackSettings.playbackRate =
            item.reportMetadata.value(QStringLiteral("plannedSettingsPlaybackRate"), 1.0).toDouble();
        fallbackSettings.pitchSemitones =
            item.reportMetadata.value(QStringLiteral("plannedSettingsPitchSemitones")).toInt();
        fallbackSettings.addResultsToPlaylist =
            fallbackSettings.playlistAddMode != DisabledPlaylistAddMode;
        effectiveSettings = effectiveSettingsToVariantMap(fallbackSettings);
    }
    if (effectiveSettings.isEmpty()) {
        effectiveSettings = item.reportMetadata.value(QStringLiteral("lastKnownSettingsSnapshot")).toMap();
    }

    const QStringList changedSettingKeys = previousAttempt.isEmpty()
        ? QStringList{}
        : settingsDiffKeys(previousAttempt.value(QStringLiteral("effectiveSettingsSnapshot")).toMap(),
                           effectiveSettings);
    const qint64 finishedAtMs = nowMs();
    const qint64 startedAtMs = item.reportMetadata.value(QStringLiteral("activeAttemptStartedAtMs")).toLongLong();

    QVariantMap attempt;
    attempt.insert(QStringLiteral("attemptNumber"), attemptNumber);
    attempt.insert(QStringLiteral("state"), itemStateKey(item.state));
    attempt.insert(QStringLiteral("terminalResult"), item.terminalResult);
    attempt.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
    attempt.insert(QStringLiteral("errorText"), item.errorText);
    attempt.insert(QStringLiteral("resultFile"), item.resultFile);
    attempt.insert(QStringLiteral("outputFile"), item.outputFile);
    attempt.insert(QStringLiteral("startedAtMs"), startedAtMs > 0 ? startedAtMs : finishedAtMs);
    attempt.insert(QStringLiteral("finishedAtMs"), finishedAtMs);
    attempt.insert(QStringLiteral("updatedAtMs"), item.updatedAtMs);
    attempt.insert(QStringLiteral("effectiveSettingsSnapshot"), effectiveSettings);
    attempt.insert(QStringLiteral("settingsChangedFromPreviousAttempt"), !changedSettingKeys.isEmpty());
    attempt.insert(QStringLiteral("changedSettingKeys"), stringListToVariantList(changedSettingKeys));
    attempts.push_back(attempt);

    item.reportMetadata.insert(QStringLiteral("attempts"), attempts);
    item.reportMetadata.insert(QStringLiteral("lastRecordedAttemptNumber"), attemptNumber);
    item.reportMetadata.insert(QStringLiteral("lastKnownSettingsSnapshot"), effectiveSettings);
    item.reportMetadata.remove(QStringLiteral("activeAttemptStartedAtMs"));
}

QString BatchAudioConverterService::csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

bool BatchAudioConverterService::isTrackerSourceItem(const BatchAudioConversionItem &item)
{
    if (WaveFlux::isTrackerModuleExtension(item.sourceFormat)) {
        return true;
    }
    const QString suffix = QFileInfo(item.sourceFile).suffix().trimmed().toLower();
    return WaveFlux::isTrackerModuleExtension(suffix);
}

qint64 BatchAudioConverterService::currentProcessCpuTimeMs()
{
    return currentProcessCpuTimeMsImpl();
}

qint64 BatchAudioConverterService::currentPeakResidentMemoryKb()
{
    return currentPeakResidentMemoryKbImpl();
}

QVariantMap BatchAudioConverterService::buildReportForCurrentJob() const
{
    if (!m_hasFinished || m_jobId.isEmpty()) {
        return {};
    }

    QVariantMap report;
    report.insert(QStringLiteral("schema"), QString::fromLatin1(kBatchReportSchema));
    report.insert(QStringLiteral("exportedAtMs"), nowMs());
    report.insert(QStringLiteral("jobMetadata"), jobMetadata());
    report.insert(QStringLiteral("settings"), settings());
    report.insert(QStringLiteral("finalSummary"), finalSummary());
    report.insert(QStringLiteral("runtimeDiagnostics"), runtimeDiagnostics());
    report.insert(QStringLiteral("parallelismDecision"), parallelismDecision());

    QVariantList itemReports;
    itemReports.reserve(m_items.size());
    for (const BatchAudioConversionItem &item : m_items) {
        QVariantMap itemReport;
        itemReport.insert(QStringLiteral("itemId"), item.itemId);
        itemReport.insert(QStringLiteral("sourceFile"), item.sourceFile);
        itemReport.insert(QStringLiteral("sourceDisplayName"), item.sourceDisplayName);
        itemReport.insert(QStringLiteral("sourceFormat"), item.sourceFormat);
        itemReport.insert(QStringLiteral("sourceDurationMs"), item.sourceDurationMs);
        itemReport.insert(QStringLiteral("sourceOriginType"), sourceOriginTypeKey(item.sourceOriginType));
        itemReport.insert(QStringLiteral("outputFile"), item.outputFile);
        itemReport.insert(QStringLiteral("resultFile"), item.resultFile);
        itemReport.insert(QStringLiteral("finalState"), itemStateKey(item.state));
        itemReport.insert(QStringLiteral("terminalResult"), item.terminalResult);
        itemReport.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
        itemReport.insert(QStringLiteral("errorReason"), item.errorText);
        itemReport.insert(QStringLiteral("retryCount"), item.retryCount);
        itemReport.insert(QStringLiteral("createdAtMs"), item.createdAtMs);
        itemReport.insert(QStringLiteral("updatedAtMs"), item.updatedAtMs);
        itemReport.insert(QStringLiteral("effectiveSettingsSnapshot"),
                          effectiveSettingsToVariantMap(item.effectiveSettings));
        itemReport.insert(QStringLiteral("conflictResolution"),
                          conflictResolutionToVariantMap(item.conflictResolution));
        itemReport.insert(QStringLiteral("reportMetadata"), item.reportMetadata);
        itemReport.insert(QStringLiteral("attempts"),
                          item.reportMetadata.value(QStringLiteral("attempts")).toList());
        itemReports.push_back(itemReport);
    }
    report.insert(QStringLiteral("items"), itemReports);
    return report;
}

QVariantMap BatchAudioConverterService::buildReportSummaryFromMap(const QVariantMap &report) const
{
    return report.value(QStringLiteral("finalSummary")).toMap();
}

QVariantMap BatchAudioConverterService::serializeDraftItem(const BatchAudioConversionItem &item) const
{
    QVariantMap serialized = toVariantMap(item);
    if (item.state == Running) {
        serialized.insert(QStringLiteral("state"), QStringLiteral("pending"));
        serialized.insert(QStringLiteral("progress"), 0.0);
        serialized.insert(QStringLiteral("statusText"),
                          QStringLiteral("Queued after restoring an interrupted batch."));
        serialized.insert(QStringLiteral("errorText"), QString());
        serialized.insert(QStringLiteral("terminalResult"), QStringLiteral("none"));
        serialized.insert(QStringLiteral("failureType"), QStringLiteral("none"));
        serialized.insert(QStringLiteral("effectiveSettingsSnapshot"), QVariantMap());
        QVariantMap reportMetadata = serialized.value(QStringLiteral("reportMetadata")).toMap();
        reportMetadata.remove(QStringLiteral("activeAttemptStartedAtMs"));
        reportMetadata.insert(QStringLiteral("interruptedDuringPersistence"), true);
        serialized.insert(QStringLiteral("reportMetadata"), reportMetadata);
    }
    return serialized;
}

bool BatchAudioConverterService::parseDraftItem(const QVariantMap &serialized,
                                                BatchAudioConversionItem *itemOut) const
{
    if (!itemOut) {
        return false;
    }

    const QString sourceFile = normalizeLocalPath(serialized.value(QStringLiteral("sourceFile")).toString());
    if (sourceFile.isEmpty()) {
        return false;
    }

    BatchAudioConversionItem item;
    item.itemId = serialized.value(QStringLiteral("itemId")).toString().trimmed();
    if (item.itemId.isEmpty()) {
        item.itemId = newIdentity();
    }
    item.sourceFile = sourceFile;
    item.sourceDisplayName = serialized.value(QStringLiteral("sourceDisplayName")).toString().trimmed();
    item.sourceFormat = normalizeSourceFormat(serialized.value(QStringLiteral("sourceFormat")).toString());
    item.sourceDurationMs = qMax<qint64>(0, serialized.value(QStringLiteral("sourceDurationMs")).toLongLong());
    item.sourceOriginType = normalizeSourceOriginType(
        serialized.value(QStringLiteral("sourceOriginType")).toString());
    item.outputFile = normalizeLocalPath(serialized.value(QStringLiteral("outputFile")).toString());
    item.state = Pending;
    const QString stateKey = serialized.value(QStringLiteral("state")).toString().trimmed().toLower();
    if (stateKey == QStringLiteral("succeeded")) {
        item.state = Succeeded;
    } else if (stateKey == QStringLiteral("failed")) {
        item.state = Failed;
    } else if (stateKey == QStringLiteral("canceled")) {
        item.state = Canceled;
    } else if (stateKey == QStringLiteral("skipped")) {
        item.state = Skipped;
    }
    item.progress = item.state == Pending ? 0.0 : qBound(0.0, serialized.value(QStringLiteral("progress")).toDouble(), 1.0);
    item.statusText = serialized.value(QStringLiteral("statusText")).toString();
    item.errorText = serialized.value(QStringLiteral("errorText")).toString();
    item.resultFile = normalizeLocalPath(serialized.value(QStringLiteral("resultFile")).toString());
    item.retryCount = qMax(0, serialized.value(QStringLiteral("retryCount")).toInt());
    item.createdAtMs = qMax<qint64>(0, serialized.value(QStringLiteral("createdAtMs")).toLongLong());
    item.updatedAtMs = qMax<qint64>(item.createdAtMs, serialized.value(QStringLiteral("updatedAtMs")).toLongLong());
    item.terminalResult = serialized.value(QStringLiteral("terminalResult"), QStringLiteral("none")).toString();
    item.failureType = classifyFailureMessage(serialized.value(QStringLiteral("failureType")).toString(),
                                              NoFailure);
    const QString failureTypeKeyValue = serialized.value(QStringLiteral("failureType")).toString().trimmed().toLower();
    if (failureTypeKeyValue == QStringLiteral("validation")) {
        item.failureType = ValidationFailure;
    } else if (failureTypeKeyValue == QStringLiteral("source-missing")) {
        item.failureType = SourceMissingFailure;
    } else if (failureTypeKeyValue == QStringLiteral("output-conflict")) {
        item.failureType = OutputConflictFailure;
    } else if (failureTypeKeyValue == QStringLiteral("unsupported-format")) {
        item.failureType = UnsupportedFormatFailure;
    } else if (failureTypeKeyValue == QStringLiteral("encoder-unavailable")) {
        item.failureType = EncoderUnavailableFailure;
    } else if (failureTypeKeyValue == QStringLiteral("permission-denied")) {
        item.failureType = PermissionDeniedFailure;
    } else if (failureTypeKeyValue == QStringLiteral("internal-pipeline")) {
        item.failureType = InternalPipelineFailure;
    } else if (failureTypeKeyValue == QStringLiteral("backend")) {
        item.failureType = BackendFailure;
    } else if (failureTypeKeyValue == QStringLiteral("canceled")) {
        item.failureType = CanceledFailure;
    } else {
        item.failureType = NoFailure;
    }
    item.effectiveSettings = effectiveSettingsFromVariantMap(
        serialized.value(QStringLiteral("effectiveSettingsSnapshot")).toMap());
    item.conflictResolution = conflictResolutionFromVariantMap(
        serialized.value(QStringLiteral("conflictResolution")).toMap());
    item.reportMetadata = serialized.value(QStringLiteral("reportMetadata")).toMap();
    item.reportMetadata.remove(QStringLiteral("activeAttemptStartedAtMs"));
    *itemOut = item;
    return true;
}

QVariantMap BatchAudioConverterService::sanitizeDraftState(const QVariantMap &draftState) const
{
    if (draftState.value(QStringLiteral("schema")).toString() != QString::fromLatin1(kBatchDraftSchema)) {
        return {};
    }
    const QVariantMap settingsMap = draftState.value(QStringLiteral("settings")).toMap();
    const QVariantList items = draftState.value(QStringLiteral("items")).toList();
    if (settingsMap.isEmpty() || items.isEmpty()) {
        return {};
    }

    QVariantMap sanitized = draftState;
    QVariantMap job = draftState.value(QStringLiteral("jobMetadata")).toMap();
    job.insert(QStringLiteral("wasRunning"), false);
    job.insert(QStringLiteral("finishedAtMs"), 0);
    sanitized.insert(QStringLiteral("jobMetadata"), job);
    return sanitized;
}

void BatchAudioConverterService::appendFinishedJobReport(const QVariantMap &report)
{
    if (report.isEmpty()) {
        return;
    }
    m_finishedJobHistory.push_back(report);
    while (m_finishedJobHistory.size() > kFinishedJobHistoryRetention) {
        m_finishedJobHistory.removeFirst();
    }
    emit finishedJobHistoryChanged();
}

QVariantMap BatchAudioConverterService::finishedJobReportById(const QString &jobId) const
{
    const QString trimmed = jobId.trimmed();
    if (trimmed.isEmpty()) {
        return buildReportForCurrentJob();
    }
    for (auto it = m_finishedJobHistory.crbegin(); it != m_finishedJobHistory.crend(); ++it) {
        const QVariantMap report = it->toMap();
        if (report.value(QStringLiteral("jobMetadata")).toMap()
                .value(QStringLiteral("jobId")).toString() == trimmed) {
            return report;
        }
    }
    return {};
}

bool BatchAudioConverterService::reportAsJson(const QVariantMap &report, QByteArray *jsonOut) const
{
    if (!jsonOut || report.isEmpty()) {
        return false;
    }
    *jsonOut = QJsonDocument(QJsonObject::fromVariantMap(report)).toJson(QJsonDocument::Indented);
    return true;
}

QString BatchAudioConverterService::reportAsPlainText(const QVariantMap &report) const
{
    QString output;
    QTextStream stream(&output);
    const QVariantMap job = report.value(QStringLiteral("jobMetadata")).toMap();
    const QVariantMap summary = report.value(QStringLiteral("finalSummary")).toMap();
    const QVariantMap diagnostics = report.value(QStringLiteral("runtimeDiagnostics")).toMap();
    const QVariantMap parallelism = report.value(QStringLiteral("parallelismDecision")).toMap();
    const QVariantList items = report.value(QStringLiteral("items")).toList();

    stream << "WaveFlux Batch Audio Converter Report\n";
    stream << "Job ID: " << job.value(QStringLiteral("jobId")).toString() << "\n";
    stream << "Started: " << QDateTime::fromMSecsSinceEpoch(job.value(QStringLiteral("startedAtMs")).toLongLong()).toString(Qt::ISODate) << "\n";
    stream << "Finished: " << QDateTime::fromMSecsSinceEpoch(job.value(QStringLiteral("finishedAtMs")).toLongLong()).toString(Qt::ISODate) << "\n";
    stream << "Succeeded: " << summary.value(QStringLiteral("succeededCount")).toInt()
           << ", Failed: " << summary.value(QStringLiteral("failedCount")).toInt()
           << ", Canceled: " << summary.value(QStringLiteral("canceledCount")).toInt()
           << ", Skipped: " << summary.value(QStringLiteral("skippedCount")).toInt() << "\n";
    stream << "Parallelism decision: "
           << parallelism.value(QStringLiteral("decisionKey")).toString() << "\n";
    stream << "Measured execution mode: "
           << diagnostics.value(QStringLiteral("executionMode")).toString() << "\n";
    stream << "Wall clock ms: " << diagnostics.value(QStringLiteral("wallClockDurationMs")).toLongLong()
           << ", CPU ms: " << diagnostics.value(QStringLiteral("cpuTimeMs")).toLongLong()
           << ", Peak RSS KB: " << diagnostics.value(QStringLiteral("peakResidentMemoryKb")).toLongLong() << "\n";
    stream << "Source bytes: " << diagnostics.value(QStringLiteral("sourceBytesMeasured")).toLongLong()
           << ", Result bytes: " << diagnostics.value(QStringLiteral("resultBytesMeasured")).toLongLong()
           << ", Peak temp bytes: " << diagnostics.value(QStringLiteral("peakTempBytesObserved")).toLongLong() << "\n\n";

    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        stream << "- Source: " << item.value(QStringLiteral("sourceFile")).toString() << "\n";
        stream << "  Output: " << item.value(QStringLiteral("outputFile")).toString() << "\n";
        stream << "  Final state: " << item.value(QStringLiteral("finalState")).toString() << "\n";
        stream << "  Error: " << item.value(QStringLiteral("errorReason")).toString() << "\n";
        stream << "  Retry count: " << item.value(QStringLiteral("retryCount")).toInt() << "\n";
        const QVariantList attempts = item.value(QStringLiteral("attempts")).toList();
        stream << "  Attempts: " << attempts.size() << "\n";
    }
    return output;
}

QString BatchAudioConverterService::reportAsCsv(const QVariantMap &report) const
{
    QString output;
    QTextStream stream(&output);
    stream << "\"sourceFile\",\"outputFile\",\"finalState\",\"failureType\",\"errorReason\",\"retryCount\",\"startedAtMs\",\"finishedAtMs\"\n";
    const QVariantList items = report.value(QStringLiteral("items")).toList();
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        const QVariantList attempts = item.value(QStringLiteral("attempts")).toList();
        const QVariantMap lastAttempt = attempts.isEmpty() ? QVariantMap{} : attempts.constLast().toMap();
        stream << csvEscape(item.value(QStringLiteral("sourceFile")).toString()) << ','
               << csvEscape(item.value(QStringLiteral("outputFile")).toString()) << ','
               << csvEscape(item.value(QStringLiteral("finalState")).toString()) << ','
               << csvEscape(item.value(QStringLiteral("failureType")).toString()) << ','
               << csvEscape(item.value(QStringLiteral("errorReason")).toString()) << ','
               << item.value(QStringLiteral("retryCount")).toInt() << ','
               << lastAttempt.value(QStringLiteral("startedAtMs")).toLongLong() << ','
               << lastAttempt.value(QStringLiteral("finishedAtMs")).toLongLong() << "\n";
    }
    return output;
}

bool BatchAudioConverterService::exportReportMapToFile(const QVariantMap &report,
                                                       const QString &filePath,
                                                       const QString &format)
{
    setReportExportError(QString());
    const QString normalizedPath = filePath.trimmed();
    if (report.isEmpty()) {
        setReportExportError(QStringLiteral("There is no finished batch report to export."));
        return false;
    }
    if (normalizedPath.isEmpty()) {
        setReportExportError(QStringLiteral("Choose a valid report file path."));
        return false;
    }

    QByteArray payload;
    const QString reportFormat = normalizedReportFormat(format);
    if (reportFormat == QStringLiteral("json")) {
        if (!reportAsJson(report, &payload)) {
            setReportExportError(QStringLiteral("Failed to serialize the batch report as JSON."));
            return false;
        }
    } else if (reportFormat == QStringLiteral("csv")) {
        payload = reportAsCsv(report).toUtf8();
    } else {
        payload = reportAsPlainText(report).toUtf8();
    }

    QSaveFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setReportExportError(QStringLiteral("Failed to open the report file: %1").arg(file.errorString()));
        return false;
    }
    if (file.write(payload) != payload.size()) {
        setReportExportError(QStringLiteral("Failed to write the report file: %1").arg(file.errorString()));
        return false;
    }
    if (!file.commit()) {
        setReportExportError(QStringLiteral("Failed to finalize the report file: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

int BatchAudioConverterService::indexOfItemIdInternal(const QString &itemId) const
{
    const QString trimmed = itemId.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).itemId == trimmed) {
            return i;
        }
    }
    return -1;
}

int BatchAudioConverterService::countByState(ItemState state) const
{
    int count = 0;
    for (const BatchAudioConversionItem &item : m_items) {
        if (item.state == state) {
            ++count;
        }
    }
    return count;
}

bool BatchAudioConverterService::canMutateConfiguration() const
{
    return !m_isRunning;
}

bool BatchAudioConverterService::canRemoveItemAt(int index) const
{
    if (!hasItemAt(index)) {
        return false;
    }
    if (!m_isRunning) {
        return true;
    }
    return m_items.at(index).state == Pending;
}

bool BatchAudioConverterService::canRetryItemAt(int index) const
{
    if (!hasItemAt(index)) {
        return false;
    }
    return isRetryEligibleState(m_items.at(index).state)
        && isRetryEligibleFailureType(m_items.at(index).failureType);
}

bool BatchAudioConverterService::canRetryItemAt(int index, ItemState expectedState) const
{
    if (!hasItemAt(index) || m_items.at(index).state != expectedState) {
        return false;
    }
    return canRetryItemAt(index);
}

bool BatchAudioConverterService::canMoveItemAt(int index, int delta) const
{
    if (!hasItemAt(index) || m_isRunning) {
        return false;
    }
    const int targetIndex = index + delta;
    return hasItemAt(targetIndex);
}

void BatchAudioConverterService::prepareItemForRetry(BatchAudioConversionItem &item)
{
    recordTerminalAttempt(item);
    ++item.retryCount;
    item.state = Pending;
    item.progress = 0.0;
    item.statusText = QStringLiteral("Queued for retry.");
    item.errorText.clear();
    item.resultFile.clear();
    item.terminalResult = QStringLiteral("none");
    item.failureType = NoFailure;
    item.effectiveSettings = {};
    item.reportMetadata.insert(QStringLiteral("lastRetryQueuedAtMs"), nowMs());
    item.reportMetadata.remove(QStringLiteral("activeAttemptStartedAtMs"));
    touchItem(item);
}

void BatchAudioConverterService::resetSummaryForQueueMutation()
{
    if (m_hasFinished || m_wasCanceled) {
        resetFinalSummary();
    }
    if (!m_isRunning) {
        setLastError(QString());
    }
}

bool BatchAudioConverterService::moveItemInternal(int from, int to)
{
    if (!hasItemAt(from) || !hasItemAt(to) || from == to || m_isRunning) {
        return false;
    }

    m_items.move(from, to);
    refreshPlannedOutputs(nullptr);
    resetSummaryForQueueMutation();
    updateAggregateProgress();
    emit itemsChanged();
    return true;
}

void BatchAudioConverterService::setIsRunning(bool running)
{
    if (m_isRunning == running) {
        return;
    }
    m_isRunning = running;
    emit isRunningChanged();
    if (!m_jobId.isEmpty()) {
        emit jobMetadataChanged();
    }
}

void BatchAudioConverterService::setCancelRequested(bool cancelRequested)
{
    if (m_cancelRequested == cancelRequested) {
        return;
    }
    m_cancelRequested = cancelRequested;
    emit cancelRequestedChanged();
}

void BatchAudioConverterService::setBatchProgress(double batchProgress)
{
    const double normalized = qBound(0.0, batchProgress, 1.0);
    if (qFuzzyCompare(m_batchProgress, normalized)) {
        return;
    }
    m_batchProgress = normalized;
    emit batchProgressChanged();
}

void BatchAudioConverterService::setStatusText(const QString &statusText)
{
    if (m_statusText == statusText) {
        return;
    }
    m_statusText = statusText;
    emit statusTextChanged();
    if (m_hasFinished) {
        emit finalSummaryChanged();
    }
}

void BatchAudioConverterService::setLastError(const QString &lastError)
{
    if (m_lastError == lastError) {
        return;
    }
    m_lastError = lastError;
    emit lastErrorChanged();
    if (m_hasFinished) {
        emit finalSummaryChanged();
    }
}

void BatchAudioConverterService::setReportExportError(const QString &errorText)
{
    if (m_reportExportError == errorText) {
        return;
    }
    m_reportExportError = errorText;
    emit reportExportErrorChanged();
}

void BatchAudioConverterService::resetFinalSummary()
{
    setFinalSummaryState(false, false);
}

void BatchAudioConverterService::setFinalSummaryState(bool hasFinished, bool wasCanceled)
{
    if (m_hasFinished == hasFinished && m_wasCanceled == wasCanceled) {
        return;
    }
    m_hasFinished = hasFinished;
    m_wasCanceled = wasCanceled;
    emit finalSummaryChanged();
}

void BatchAudioConverterService::resetJobMetadata()
{
    const bool hadMetadata = !m_jobId.isEmpty()
        || m_jobCreatedAtMs != 0
        || m_jobStartedAtMs != 0
        || m_jobFinishedAtMs != 0;
    m_jobId.clear();
    m_jobCreatedAtMs = 0;
    m_jobStartedAtMs = 0;
    m_jobFinishedAtMs = 0;
    m_deferredPlaylistResultsAdded = false;
    resetRuntimeMeasurements();
    if (hadMetadata) {
        emit jobMetadataChanged();
    }
}

void BatchAudioConverterService::beginNewJobSession()
{
    m_jobId = newIdentity();
    m_jobCreatedAtMs = nowMs();
    m_jobStartedAtMs = 0;
    m_jobFinishedAtMs = 0;
    m_deferredPlaylistResultsAdded = false;
    resetRuntimeMeasurements();
    emit jobMetadataChanged();
}

void BatchAudioConverterService::setJobStartedNow()
{
    m_jobStartedAtMs = nowMs();
    m_jobFinishedAtMs = 0;
    emit jobMetadataChanged();
}

void BatchAudioConverterService::setJobFinishedNow()
{
    m_jobFinishedAtMs = nowMs();
    emit jobMetadataChanged();
}

void BatchAudioConverterService::resetRuntimeMeasurements()
{
    m_runtimeMeasurementStartedAtMs = 0;
    m_runtimeMeasurementCpuStartedAtMs = 0;
    m_runtimeMeasurementCpuFinishedAtMs = 0;
    m_runtimePeakResidentMemoryKb = -1;
    m_runtimeMeasuredSourceBytes = 0;
    m_runtimeMeasuredResultBytes = 0;
    m_runtimePeakTempBytes = 0;
    m_runtimePeakTempFiles = 0;
    m_runtimeMeasuredItemCount = 0;
    m_runtimeMaxConcurrentJobsObserved = 0;
    m_runtimeTagCopyAttemptCount = 0;
    m_runtimeTagCopySuccessCount = 0;
    m_runtimeTagCopyTotalDurationUs = 0;
}

void BatchAudioConverterService::beginRuntimeMeasurements()
{
    resetRuntimeMeasurements();
    m_runtimeMeasurementStartedAtMs = nowMs();
    m_runtimeMeasurementCpuStartedAtMs = currentProcessCpuTimeMs();
    refreshRuntimeMeasurementPeaks();
}

void BatchAudioConverterService::refreshRuntimeMeasurementPeaks()
{
    const qint64 peakResidentMemoryKb = currentPeakResidentMemoryKb();
    if (peakResidentMemoryKb >= 0) {
        m_runtimePeakResidentMemoryKb = qMax(m_runtimePeakResidentMemoryKb, peakResidentMemoryKb);
    }
}

void BatchAudioConverterService::absorbWorkerMetrics(const BatchAudioConversionItem &item,
                                                     const QVariantMap &workerMetrics)
{
    refreshRuntimeMeasurementPeaks();
    ++m_runtimeMeasuredItemCount;
    m_runtimeMeasuredSourceBytes += qMax<qint64>(0, workerMetrics.value(QStringLiteral("sourceBytes")).toLongLong());
    m_runtimeMeasuredResultBytes += qMax<qint64>(0, workerMetrics.value(QStringLiteral("finalBytes")).toLongLong());
    m_runtimePeakTempBytes = qMax(
        m_runtimePeakTempBytes,
        qMax<qint64>(0, workerMetrics.value(QStringLiteral("tempBytes")).toLongLong()));
    if (workerMetrics.value(QStringLiteral("usedTemporaryFile")).toBool()) {
        m_runtimePeakTempFiles = qMax(m_runtimePeakTempFiles, 1);
    }
    if (workerMetrics.value(QStringLiteral("metadataCopyAttempted")).toBool()) {
        ++m_runtimeTagCopyAttemptCount;
        m_runtimeTagCopyTotalDurationUs +=
            qMax<qint64>(0, workerMetrics.value(QStringLiteral("metadataCopyDurationUs")).toLongLong());
        if (workerMetrics.value(QStringLiteral("metadataCopySucceeded")).toBool()) {
            ++m_runtimeTagCopySuccessCount;
        }
    }
    if (item.state == Running || item.state == Succeeded || item.state == Failed || item.state == Canceled) {
        m_runtimeMaxConcurrentJobsObserved = qMax(m_runtimeMaxConcurrentJobsObserved, 1);
    }
}

void BatchAudioConverterService::touchItem(BatchAudioConversionItem &item)
{
    item.updatedAtMs = nowMs();
}

void BatchAudioConverterService::applyTerminalState(BatchAudioConversionItem &item,
                                                    ItemState state,
                                                    FailureType failureType)
{
    item.state = state;
    item.progress = 1.0;
    item.failureType = failureType;
    item.terminalResult = itemStateKey(state);
    item.reportMetadata.insert(QStringLiteral("lastTerminalState"), item.terminalResult);
    item.reportMetadata.insert(QStringLiteral("lastUpdatedAtMs"), nowMs());
    touchItem(item);
    recordTerminalAttempt(item);
}

BatchAudioConverterService::EffectiveSettingsSnapshot
BatchAudioConverterService::currentSettingsSnapshot() const
{
    EffectiveSettingsSnapshot snapshot;
    snapshot.outputDirectory = m_settings.outputDirectory;
    snapshot.namingPolicy = m_settings.namingPolicy;
    snapshot.format = m_settings.format;
    snapshot.conflictPolicy = m_settings.conflictPolicy;
    snapshot.retryPolicy = m_settings.retryPolicy;
    snapshot.playlistAddMode = m_settings.playlistAddMode;
    snapshot.bitrate = m_settings.bitrate;
    snapshot.sampleRate = m_settings.sampleRate;
    snapshot.channelMode = m_settings.channelMode;
    snapshot.playbackRate = m_settings.playbackRate;
    snapshot.pitchSemitones = m_settings.pitchSemitones;
    snapshot.applyEqualizer = m_settings.applyEqualizer;
    snapshot.equalizerBandGains = m_settings.equalizerBandGains;
    snapshot.applyReverb = m_settings.applyReverb;
    snapshot.reverbRoomSize = m_settings.reverbRoomSize;
    snapshot.reverbDamping = m_settings.reverbDamping;
    snapshot.reverbWetLevel = m_settings.reverbWetLevel;
    snapshot.addResultsToPlaylist = m_settings.playlistAddMode != DisabledPlaylistAddMode;
    snapshot.capturedAtMs = nowMs();
    return snapshot;
}

QVariantMap BatchAudioConverterService::ingestSources(const QStringList &sourceFiles,
                                                      SourceOriginType sourceOriginType,
                                                      bool append)
{
    QVariantMap summary;
    summary.insert(QStringLiteral("append"), append);
    summary.insert(QStringLiteral("sourceOriginType"), sourceOriginTypeKey(sourceOriginType));
    summary.insert(QStringLiteral("recursive"), false);

    if (m_isRunning) {
        const QString message = QStringLiteral("Cannot modify batch intake while a conversion is running.");
        summary.insert(QStringLiteral("error"), message);
        summary.insert(QStringLiteral("acceptedCount"), 0);
        summary.insert(QStringLiteral("skippedCount"), 0);
        summary.insert(QStringLiteral("duplicateCount"), 0);
        summary.insert(QStringLiteral("missingCount"), 0);
        summary.insert(QStringLiteral("unsupportedCount"), 0);
        summary.insert(QStringLiteral("hiddenSkippedCount"), 0);
        summary.insert(QStringLiteral("totalCount"), 0);
        summary.insert(QStringLiteral("queueCount"), m_items.size());
        return summary;
    }

    QList<BatchAudioConversionItem> nextItems = append ? m_items : QList<BatchAudioConversionItem>{};
    QSet<QString> seenPaths;
    seenPaths.reserve(nextItems.size() + sourceFiles.size());
    for (const BatchAudioConversionItem &existingItem : std::as_const(nextItems)) {
        const QString normalized = normalizeLocalPath(existingItem.sourceFile);
        if (!normalized.isEmpty()) {
            seenPaths.insert(normalized);
        }
    }

    int acceptedCount = 0;
    int skippedCount = 0;
    int duplicateCount = 0;
    int missingCount = 0;
    int unsupportedCount = 0;
    const qint64 createdAtMs = nowMs();

    for (const QString &source : sourceFiles) {
        const QString trimmed = source.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        BatchAudioConversionItem item;
        item.itemId = newIdentity();
        item.createdAtMs = createdAtMs;
        item.updatedAtMs = createdAtMs;
        item.sourceOriginType = sourceOriginType;
        item.sourceFile = normalizeLocalPath(trimmed);
        if (item.sourceFile.isEmpty()) {
            item.sourceFile = trimmed;
        }
        item.sourceDisplayName = inferDisplayName(item.sourceFile);
        item.sourceFormat = inferFormat(item.sourceFile);

        const QString normalizedLocalPath = normalizeLocalPath(trimmed);
        if (normalizedLocalPath.isEmpty()) {
            item.state = Skipped;
            item.statusText = QStringLiteral("Skipped unsupported non-local source.");
            item.errorText = QStringLiteral("Only local files are supported in batch conversion.");
            item.failureType = ValidationFailure;
            item.terminalResult = QStringLiteral("skipped");
            item.reportMetadata.insert(QStringLiteral("intakeIssue"), true);
            item.reportMetadata.insert(QStringLiteral("intakeReason"), QStringLiteral("non-local"));
            ++skippedCount;
            ++unsupportedCount;
            nextItems.push_back(std::move(item));
            continue;
        }

        item.sourceFile = normalizedLocalPath;
        item.sourceDisplayName = inferDisplayName(item.sourceFile);
        item.sourceFormat = inferFormat(item.sourceFile);

        if (!hasSupportedBatchSourceExtension(item.sourceFile)) {
            item.state = Skipped;
            item.statusText = QStringLiteral("Skipped unsupported source type.");
            item.errorText = QStringLiteral("The source format is not supported for batch conversion.");
            item.failureType = UnsupportedFormatFailure;
            item.terminalResult = QStringLiteral("skipped");
            item.reportMetadata.insert(QStringLiteral("intakeIssue"), true);
            item.reportMetadata.insert(QStringLiteral("intakeReason"), QStringLiteral("unsupported-format"));
            ++skippedCount;
            ++unsupportedCount;
            nextItems.push_back(std::move(item));
            continue;
        }

        if (!QFileInfo::exists(item.sourceFile)) {
            item.state = Failed;
            item.statusText = QStringLiteral("Source file is missing.");
            item.errorText = QStringLiteral("The source file does not exist.");
            item.failureType = SourceMissingFailure;
            item.terminalResult = QStringLiteral("failed");
            item.reportMetadata.insert(QStringLiteral("intakeIssue"), true);
            item.reportMetadata.insert(QStringLiteral("intakeReason"), QStringLiteral("missing-source"));
            ++skippedCount;
            ++missingCount;
            nextItems.push_back(std::move(item));
            continue;
        }

        if (seenPaths.contains(item.sourceFile)) {
            item.state = Skipped;
            item.statusText = QStringLiteral("Skipped duplicate source.");
            item.errorText = QStringLiteral("This source file is already present in the batch queue.");
            item.failureType = ValidationFailure;
            item.terminalResult = QStringLiteral("skipped");
            item.reportMetadata.insert(QStringLiteral("intakeIssue"), true);
            item.reportMetadata.insert(QStringLiteral("intakeReason"), QStringLiteral("duplicate-source"));
            ++skippedCount;
            ++duplicateCount;
            nextItems.push_back(std::move(item));
            continue;
        }

        seenPaths.insert(item.sourceFile);
        item.reportMetadata.insert(QStringLiteral("intakeIssue"), false);
        ++acceptedCount;
        nextItems.push_back(std::move(item));
    }

    m_items = std::move(nextItems);
    if (m_items.isEmpty()) {
        resetJobMetadata();
    } else if (!append || m_jobId.isEmpty()) {
        beginNewJobSession();
    } else {
        emit jobMetadataChanged();
    }

    refreshPlannedOutputs(nullptr);
    setBatchProgress(0.0);
    setStatusText(QString());
    setLastError(QString());
    resetFinalSummary();
    emit itemsChanged();

    summary.insert(QStringLiteral("acceptedCount"), acceptedCount);
    summary.insert(QStringLiteral("skippedCount"), skippedCount);
    summary.insert(QStringLiteral("duplicateCount"), duplicateCount);
    summary.insert(QStringLiteral("missingCount"), missingCount);
    summary.insert(QStringLiteral("unsupportedCount"), unsupportedCount);
    summary.insert(QStringLiteral("hiddenSkippedCount"), 0);
    summary.insert(QStringLiteral("totalCount"), sourceFiles.size());
    summary.insert(QStringLiteral("queueCount"), m_items.size());
    summary.insert(QStringLiteral("hasRunnableItems"), pendingCount() > 0);
    return summary;
}

QVariantMap BatchAudioConverterService::ingestSourceFolder(const QString &folderPath, bool append)
{
    QVariantMap summary;
    summary.insert(QStringLiteral("append"), append);
    summary.insert(QStringLiteral("sourceOriginType"), sourceOriginTypeKey(FolderImportSourceOrigin));
    summary.insert(QStringLiteral("recursive"), true);

    if (m_isRunning) {
        summary.insert(QStringLiteral("error"),
                       QStringLiteral("Cannot modify batch intake while a conversion is running."));
        summary.insert(QStringLiteral("acceptedCount"), 0);
        summary.insert(QStringLiteral("skippedCount"), 0);
        summary.insert(QStringLiteral("duplicateCount"), 0);
        summary.insert(QStringLiteral("missingCount"), 0);
        summary.insert(QStringLiteral("unsupportedCount"), 0);
        summary.insert(QStringLiteral("hiddenSkippedCount"), 0);
        summary.insert(QStringLiteral("totalCount"), 0);
        summary.insert(QStringLiteral("queueCount"), m_items.size());
        return summary;
    }

    const QString normalizedRoot = normalizeLocalPath(folderPath);
    if (normalizedRoot.isEmpty() || !QFileInfo::exists(normalizedRoot) || !QFileInfo(normalizedRoot).isDir()) {
        summary.insert(QStringLiteral("error"), QStringLiteral("The selected source folder is invalid."));
        summary.insert(QStringLiteral("acceptedCount"), 0);
        summary.insert(QStringLiteral("skippedCount"), 0);
        summary.insert(QStringLiteral("duplicateCount"), 0);
        summary.insert(QStringLiteral("missingCount"), 0);
        summary.insert(QStringLiteral("unsupportedCount"), 0);
        summary.insert(QStringLiteral("hiddenSkippedCount"), 0);
        summary.insert(QStringLiteral("totalCount"), 0);
        summary.insert(QStringLiteral("queueCount"), m_items.size());
        return summary;
    }

    QStringList candidatePaths;
    int hiddenSkippedCount = 0;
    int unsupportedCount = 0;
    QDirIterator it(normalizedRoot,
                    QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (info.isHidden()) {
            ++hiddenSkippedCount;
            continue;
        }
        if (!hasSupportedBatchSourceExtension(path)) {
            ++unsupportedCount;
            continue;
        }
        candidatePaths.push_back(path);
    }

    candidatePaths = sortedPaths(std::move(candidatePaths));
    summary = ingestSources(candidatePaths, FolderImportSourceOrigin, append);
    summary.insert(QStringLiteral("recursive"), true);
    summary.insert(QStringLiteral("hiddenSkippedCount"), hiddenSkippedCount);
    summary.insert(QStringLiteral("unsupportedCount"),
                   summary.value(QStringLiteral("unsupportedCount")).toInt() + unsupportedCount);
    summary.insert(QStringLiteral("folderPath"), normalizedRoot);
    return summary;
}

QString BatchAudioConverterService::previewBaseNameForItem(const BatchAudioConversionItem &item) const
{
    return previewNamingDecisionForItem(item).baseName;
}

BatchAudioConverterService::PreviewNamingDecision
BatchAudioConverterService::previewNamingDecisionForItem(const BatchAudioConversionItem &item) const
{
    PreviewNamingDecision decision;
    decision.requestedNamingPolicy = m_settings.namingPolicy;
    decision.appliedNamingPolicy = m_settings.namingPolicy;
    decision.sourceDirectoryPolicy = m_settings.outputDirectory.isEmpty()
        ? QStringLiteral("source-folder")
        : QStringLiteral("batch-output-directory");

    QString candidate;
    if (m_settings.namingPolicy == QStringLiteral("artist-title")) {
        candidate = artistTitleBaseNameForItem(item.sourceFile, &decision.missingMetadataFields);
    } else if (m_settings.namingPolicy == QStringLiteral("album-track-title")) {
        candidate = albumTrackTitleBaseNameForItem(item.sourceFile, &decision.missingMetadataFields);
    }

    if (candidate.isEmpty()) {
        decision.usedFallback = m_settings.namingPolicy != QStringLiteral("basename");
        if (decision.usedFallback) {
            decision.appliedNamingPolicy = QStringLiteral("basename");
            decision.fallbackNamingPolicy = QStringLiteral("basename");
        }
        candidate = sanitizeFileNameComponent(baseNameForOutput(item.sourceFile));
    }

    if (candidate.isEmpty()) {
        candidate = QStringLiteral("Converted Track");
    }

    decision.baseName = candidate;
    return decision;
}

QString BatchAudioConverterService::artistTitleBaseNameForItem(
    const QString &sourceFile,
    QStringList *missingMetadataFieldsOut) const
{
    QStringList missingMetadataFields;
    const auto file = WaveFlux::TagLibPath::makeFileRef(sourceFile, false);
    if (file.isNull() || !file.tag()) {
        missingMetadataFields = {QStringLiteral("artist"), QStringLiteral("title")};
        if (missingMetadataFieldsOut) {
            *missingMetadataFieldsOut = missingMetadataFields;
        }
        return {};
    }

    const QString title = QString::fromUtf8(file.tag()->title().toCString(true)).trimmed();
    const QString artist = QString::fromUtf8(file.tag()->artist().toCString(true)).trimmed();
    if (artist.isEmpty()) {
        missingMetadataFields.push_back(QStringLiteral("artist"));
    }
    if (title.isEmpty()) {
        missingMetadataFields.push_back(QStringLiteral("title"));
    }
    if (missingMetadataFieldsOut) {
        *missingMetadataFieldsOut = missingMetadataFields;
    }
    if (!missingMetadataFields.isEmpty()) {
        return {};
    }

    return sanitizeFileNameComponent(QStringLiteral("%1 - %2").arg(artist, title));
}

QString BatchAudioConverterService::albumTrackTitleBaseNameForItem(
    const QString &sourceFile,
    QStringList *missingMetadataFieldsOut) const
{
    QStringList missingMetadataFields;
    const auto file = WaveFlux::TagLibPath::makeFileRef(sourceFile, false);
    if (file.isNull() || !file.tag()) {
        missingMetadataFields = {
            QStringLiteral("album"),
            QStringLiteral("track-number"),
            QStringLiteral("title")
        };
        if (missingMetadataFieldsOut) {
            *missingMetadataFieldsOut = missingMetadataFields;
        }
        return {};
    }

    const QString title = QString::fromUtf8(file.tag()->title().toCString(true)).trimmed();
    const QString album = QString::fromUtf8(file.tag()->album().toCString(true)).trimmed();
    const uint track = file.tag()->track();

    if (album.isEmpty()) {
        missingMetadataFields.push_back(QStringLiteral("album"));
    }
    if (track == 0) {
        missingMetadataFields.push_back(QStringLiteral("track-number"));
    }
    if (title.isEmpty()) {
        missingMetadataFields.push_back(QStringLiteral("title"));
    }
    if (missingMetadataFieldsOut) {
        *missingMetadataFieldsOut = missingMetadataFields;
    }
    if (!missingMetadataFields.isEmpty()) {
        return {};
    }

    return sanitizeFileNameComponent(
        QStringLiteral("%1 - %2 - %3")
            .arg(album,
                 QStringLiteral("%1").arg(track, 2, 10, QLatin1Char('0')),
                 title));
}

bool BatchAudioConverterService::refreshPlannedOutputs(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    if (m_items.isEmpty()) {
        return false;
    }

    QSet<QString> reservedPaths;
    QSet<QString> existingPaths;
    bool hasRunnableItems = false;

    for (BatchAudioConversionItem &item : m_items) {
        item.progress = 0.0;
        item.resultFile.clear();
        item.effectiveSettings = {};
        item.reportMetadata.insert(QStringLiteral("jobId"), m_jobId);

        if (item.reportMetadata.value(QStringLiteral("intakeIssue")).toBool()) {
            touchItem(item);
            continue;
        }

        const QString normalizedSource = normalizeLocalPath(item.sourceFile);
        if (normalizedSource.isEmpty()) {
            item.outputFile.clear();
            item.state = Skipped;
            item.statusText = QStringLiteral("Skipped unsupported non-local source.");
            item.errorText = QStringLiteral("Only local files are supported in batch conversion.");
            item.failureType = ValidationFailure;
            item.terminalResult = QStringLiteral("skipped");
            item.conflictResolution = {};
            touchItem(item);
            continue;
        }

        item.sourceFile = normalizedSource;
        item.sourceDisplayName = inferDisplayName(item.sourceFile);
        if (item.sourceFormat.isEmpty()) {
            item.sourceFormat = inferFormat(item.sourceFile);
        }

        const PreviewNamingDecision namingDecision = previewNamingDecisionForItem(item);
        const PlannedOutputInfo plan = planOutputForItem(item,
                                                         namingDecision,
                                                         reservedPaths,
                                                         existingPaths);

        item.outputFile = plan.outputFile;
        item.reportMetadata.insert(QStringLiteral("plannedSettingsOutputDirectory"),
                                   m_settings.outputDirectory);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsNamingPolicy"),
                                   m_settings.namingPolicy);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsFormat"), m_settings.format);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsConflictPolicy"),
                                   conflictPolicyKey(m_settings.conflictPolicy));
        item.reportMetadata.insert(QStringLiteral("plannedSettingsRetryPolicy"),
                                   retryPolicyKey(m_settings.retryPolicy));
        item.reportMetadata.insert(QStringLiteral("plannedSettingsPlaylistAddMode"),
                                   playlistAddModeKey(m_settings.playlistAddMode));
        item.reportMetadata.insert(QStringLiteral("plannedSettingsBitrate"), m_settings.bitrate);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsSampleRate"), m_settings.sampleRate);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsChannelMode"), m_settings.channelMode);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsPlaybackRate"),
                                   m_settings.playbackRate);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsPitchSemitones"),
                                   m_settings.pitchSemitones);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsApplyReverb"),
                                   m_settings.applyReverb);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsReverbRoomSize"),
                                   m_settings.reverbRoomSize);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsReverbDamping"),
                                   m_settings.reverbDamping);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsReverbWetLevel"),
                                   m_settings.reverbWetLevel);
        item.reportMetadata.insert(QStringLiteral("plannedSettingsAddResultsToPlaylist"),
                                   m_settings.playlistAddMode != DisabledPlaylistAddMode);
        if (item.state == Pending || item.state == Running) {
            item.state = plan.state;
            item.statusText = plan.statusText;
            item.errorText = plan.errorText;
            item.failureType = plan.failureType;
            item.terminalResult = isTerminalState(plan.state)
                ? itemStateKey(plan.state)
                : QStringLiteral("none");
        }
        item.conflictResolution = plan.conflictResolution;
        item.reportMetadata.insert(QStringLiteral("previewDiagnostics"), plan.previewDiagnostics);
        touchItem(item);
        if (plan.runnable) {
            reservedPaths.insert(plan.outputFile);
            hasRunnableItems = true;
        }
    }

    updateAggregateProgress();
    if (!hasRunnableItems && errorMessage) {
        *errorMessage = QStringLiteral("Batch conversion has no supported items to run.");
    }
    return hasRunnableItems;
}

bool BatchAudioConverterService::prepareItemsForBatchStart(QString *errorMessage)
{
    if (m_items.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Choose at least one source track for batch conversion.");
        }
        return false;
    }

    if (!refreshPlannedOutputs(errorMessage)) {
        emit itemsChanged();
        return false;
    }

    emit itemsChanged();
    return true;
}

BatchAudioConverterService::PlannedOutputInfo BatchAudioConverterService::planOutputForItem(
    const BatchAudioConversionItem &item,
    const PreviewNamingDecision &namingDecision,
    const QSet<QString> &reservedPaths,
    const QSet<QString> &existingPaths) const
{
    PlannedOutputInfo result;
    QString directory = m_settings.outputDirectory;
    if (directory.isEmpty()) {
        directory = QFileInfo(item.sourceFile).absolutePath();
    }
    if (directory.isEmpty()) {
        result.state = Failed;
        result.failureType = ValidationFailure;
        result.statusText = QStringLiteral("Failed to prepare output path.");
        result.errorText = QStringLiteral("Cannot resolve an output directory for the current item.");
        return result;
    }

    const QString extension = extensionForFormat(m_settings.format);
    const QString requestedOutputPath = QDir(directory).filePath(
        QStringLiteral("%1 (converted).%2")
            .arg(namingDecision.baseName, extension));
    const bool targetExists = QFileInfo::exists(requestedOutputPath)
        || existingPaths.contains(requestedOutputPath);
    const bool queueConflict = reservedPaths.contains(requestedOutputPath);
    const bool hasConflict = targetExists || queueConflict;

    result.outputFile = requestedOutputPath;
    result.conflictResolution.requestedOutputFile = requestedOutputPath;
    result.conflictResolution.resolvedOutputFile = requestedOutputPath;
    result.conflictResolution.hadConflict = hasConflict;
    result.conflictResolution.collisionRuleKey = queueConflict
        ? QStringLiteral("queue-conflict")
        : targetExists ? QStringLiteral("existing-target")
                       : QStringLiteral("none");
    result.conflictResolution.targetExistsOnDisk = targetExists;

    switch (m_settings.conflictPolicy) {
    case AutoRenameConflictPolicy:
        result.outputFile = uniqueOutputPath(requestedOutputPath, reservedPaths, existingPaths);
        result.conflictResolution.resolvedOutputFile = result.outputFile;
        result.conflictResolution.hadConflict = requestedOutputPath != result.outputFile;
        result.conflictResolution.resolutionKey = result.conflictResolution.hadConflict
            ? QStringLiteral("auto-renamed")
            : QStringLiteral("planned");
        result.conflictResolution.finalizationStrategyKey = QStringLiteral("temp-commit");
        result.runnable = true;
        break;
    case OverwriteExistingConflictPolicy:
        if (queueConflict) {
            result.state = Failed;
            result.failureType = OutputConflictFailure;
            result.statusText = QStringLiteral("Failed output path conflict.");
            result.errorText = QStringLiteral("Another queued item already targets the same output path.");
            result.conflictResolution.resolutionKey = QStringLiteral("overwrite-blocked-queue-conflict");
            result.conflictResolution.finalizationStrategyKey = QStringLiteral("temp-not-started");
            break;
        }
        result.conflictResolution.willOverwriteExisting = targetExists;
        result.conflictResolution.resolutionKey = targetExists
            ? QStringLiteral("overwrite-existing")
            : QStringLiteral("planned");
        result.conflictResolution.finalizationStrategyKey = targetExists
            ? QStringLiteral("temp-replace-existing")
            : QStringLiteral("temp-commit");
        result.runnable = true;
        break;
    case SkipOnConflictPolicy:
        if (hasConflict) {
            result.state = Skipped;
            result.failureType = OutputConflictFailure;
            result.statusText = QStringLiteral("Skipped output path conflict.");
            result.errorText = queueConflict
                ? QStringLiteral("Another queued item already targets the same output path.")
                : QStringLiteral("An output file already exists at the planned path.");
            result.conflictResolution.resolutionKey = queueConflict
                ? QStringLiteral("skip-queue-conflict")
                : QStringLiteral("skip-existing-conflict");
            result.conflictResolution.finalizationStrategyKey = QStringLiteral("temp-not-started");
            break;
        }
        result.conflictResolution.resolutionKey = QStringLiteral("planned");
        result.conflictResolution.finalizationStrategyKey = QStringLiteral("temp-commit");
        result.runnable = true;
        break;
    case FailOnConflictPolicy:
        if (hasConflict) {
            result.state = Failed;
            result.failureType = OutputConflictFailure;
            result.statusText = QStringLiteral("Failed output path conflict.");
            result.errorText = queueConflict
                ? QStringLiteral("Another queued item already targets the same output path.")
                : QStringLiteral("An output file already exists at the planned path.");
            result.conflictResolution.resolutionKey = queueConflict
                ? QStringLiteral("fail-queue-conflict")
                : QStringLiteral("fail-existing-conflict");
            result.conflictResolution.finalizationStrategyKey = QStringLiteral("temp-not-started");
            break;
        }
        result.conflictResolution.resolutionKey = QStringLiteral("planned");
        result.conflictResolution.finalizationStrategyKey = QStringLiteral("temp-commit");
        result.runnable = true;
        break;
    }

    QVariantMap previewDiagnostics;
    previewDiagnostics.insert(QStringLiteral("requestedNamingPolicy"), namingDecision.requestedNamingPolicy);
    previewDiagnostics.insert(QStringLiteral("appliedNamingPolicy"), namingDecision.appliedNamingPolicy);
    previewDiagnostics.insert(QStringLiteral("fallbackNamingPolicy"), namingDecision.fallbackNamingPolicy);
    previewDiagnostics.insert(QStringLiteral("usedFallback"), namingDecision.usedFallback);
    previewDiagnostics.insert(QStringLiteral("missingMetadataFields"),
                              stringListToVariantList(namingDecision.missingMetadataFields));
    previewDiagnostics.insert(QStringLiteral("baseName"), namingDecision.baseName);
    previewDiagnostics.insert(QStringLiteral("requestedOutputFile"), requestedOutputPath);
    previewDiagnostics.insert(QStringLiteral("resolvedOutputFile"), result.outputFile);
    previewDiagnostics.insert(QStringLiteral("sourceDirectoryPolicy"), namingDecision.sourceDirectoryPolicy);
    previewDiagnostics.insert(QStringLiteral("collisionRuleKey"),
                              result.conflictResolution.collisionRuleKey);
    previewDiagnostics.insert(QStringLiteral("resolutionKey"),
                              result.conflictResolution.resolutionKey);
    previewDiagnostics.insert(QStringLiteral("willOverwriteExisting"),
                              result.conflictResolution.willOverwriteExisting);
    previewDiagnostics.insert(QStringLiteral("targetExistsOnDisk"),
                              result.conflictResolution.targetExistsOnDisk);
    previewDiagnostics.insert(QStringLiteral("finalizationStrategyKey"),
                              result.conflictResolution.finalizationStrategyKey);
    result.previewDiagnostics = previewDiagnostics;
    return result;
}

void BatchAudioConverterService::startNextPendingItem()
{
    if (!m_isRunning) {
        return;
    }

    int nextIndex = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).state == Pending) {
            nextIndex = i;
            break;
        }
    }

    if (nextIndex < 0) {
        finalizeBatchRun(m_cancelRequested);
        return;
    }

    m_items[nextIndex].state = Running;
    m_items[nextIndex].progress = 0.0;
    m_items[nextIndex].statusText = localizedBatchText(QStringLiteral("batchConverter.itemStarting"));
    m_items[nextIndex].effectiveSettings = currentSettingsSnapshot();
    m_items[nextIndex].reportMetadata.insert(QStringLiteral("jobId"), m_jobId);
    m_items[nextIndex].reportMetadata.insert(QStringLiteral("activeAttemptStartedAtMs"), nowMs());
    m_items[nextIndex].reportMetadata.insert(QStringLiteral("lastKnownSettingsSnapshot"),
                                             effectiveSettingsToVariantMap(m_items[nextIndex].effectiveSettings));
    touchItem(m_items[nextIndex]);
    m_runtimeMaxConcurrentJobsObserved = qMax(m_runtimeMaxConcurrentJobsObserved, 1);
    refreshRuntimeMeasurementPeaks();
    setStatusText(localizedBatchText(QStringLiteral("batchConverter.itemConverting"))
                      .arg(m_items[nextIndex].sourceDisplayName));
    updateAggregateProgress();
    emit itemsChanged();

    if (!m_worker) {
        m_items[nextIndex].errorText = localizedBatchText(QStringLiteral("batchConverter.workerUnavailable"));
        m_items[nextIndex].statusText = m_items[nextIndex].errorText;
        applyTerminalState(m_items[nextIndex], Failed, BackendFailure);
        absorbWorkerMetrics(m_items[nextIndex], QVariantMap{});
        setLastError(m_items[nextIndex].errorText);
        updateAggregateProgress();
        emit itemsChanged();
        startNextPendingItem();
        return;
    }

    m_worker->setSourceFile(m_items[nextIndex].sourceFile);
    m_worker->setOutputFile(m_items[nextIndex].outputFile);
    m_worker->setFormat(m_settings.format);
    m_worker->setBitrate(m_settings.bitrate);
    m_worker->setSampleRate(m_settings.sampleRate);
    m_worker->setChannelMode(m_settings.channelMode);
    m_worker->setPlaybackRate(m_settings.playbackRate);
    m_worker->setPitchSemitones(m_settings.pitchSemitones);
    m_worker->setApplyEqualizer(m_settings.applyEqualizer);
    m_worker->setEqualizerBandGains(m_settings.equalizerBandGains);
    m_worker->setApplyReverb(m_settings.applyReverb);
    m_worker->setReverbRoomSize(m_settings.reverbRoomSize);
    m_worker->setReverbDamping(m_settings.reverbDamping);
    m_worker->setReverbWetLevel(m_settings.reverbWetLevel);
    m_worker->setOverwriteExisting(m_items[nextIndex].conflictResolution.willOverwriteExisting);

    if (!m_worker->startConversion()) {
        if (!hasItemAt(nextIndex) || m_items.at(nextIndex).state != Running) {
            return;
        }

        const QString workerError = m_worker->lastError().trimmed();
        const QString errorText = workerError.isEmpty()
            ? localizedBatchText(QStringLiteral("batchConverter.startFailed"))
            : workerError;

        m_items[nextIndex].errorText = errorText;
        m_items[nextIndex].statusText = errorText;
        applyTerminalState(m_items[nextIndex],
                           Failed,
                           classifyFailureMessage(errorText, BackendFailure));
        absorbWorkerMetrics(m_items[nextIndex], m_worker->lastConversionMetrics());
        setLastError(errorText);
        updateAggregateProgress();
        emit itemsChanged();
        if (m_cancelRequested) {
            finalizeBatchRun(true);
        } else {
            startNextPendingItem();
        }
    }
}

void BatchAudioConverterService::finalizeBatchRun(bool canceled)
{
    if (!m_isRunning) {
        return;
    }

    if (canceled) {
        markPendingItemsAsCanceled();
    }

    refreshRuntimeMeasurementPeaks();
    m_runtimeMeasurementCpuFinishedAtMs = currentProcessCpuTimeMs();
    updateAggregateProgress();
    setIsRunning(false);
    setCancelRequested(false);
    setJobFinishedNow();
    if (canceled) {
        setStatusText(QStringLiteral("Batch conversion canceled."));
    } else if (failedCount() > 0 || skippedCount() > 0) {
        setStatusText(QStringLiteral("Batch conversion finished with issues."));
    } else {
        setStatusText(QStringLiteral("Batch conversion finished successfully."));
    }
    setFinalSummaryState(true, canceled);
    appendFinishedJobReport(buildReportForCurrentJob());
    emit itemsChanged();

    if (canceled) {
        emit batchCanceled();
    } else {
        emit batchFinished();
    }
}

void BatchAudioConverterService::updateAggregateProgress()
{
    if (m_items.isEmpty()) {
        setBatchProgress(0.0);
        return;
    }

    double completedWeight = 0.0;
    for (const BatchAudioConversionItem &item : m_items) {
        switch (item.state) {
        case Pending:
            break;
        case Running:
            completedWeight += qBound(0.0, item.progress, 1.0);
            break;
        case Succeeded:
        case Failed:
        case Canceled:
        case Skipped:
            completedWeight += 1.0;
            break;
        }
    }

    setBatchProgress(completedWeight / static_cast<double>(m_items.size()));
}

void BatchAudioConverterService::markPendingItemsAsCanceled()
{
    bool changed = false;
    for (BatchAudioConversionItem &item : m_items) {
        if (item.state == Pending) {
            item.statusText = QStringLiteral("Canceled before start.");
            applyTerminalState(item, Canceled, CanceledFailure);
            changed = true;
        }
    }
    if (changed) {
        updateAggregateProgress();
    }
}

void BatchAudioConverterService::emitSettingsChanged()
{
    emit settingsChanged();
}

QString BatchAudioConverterService::newIdentity()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

qint64 BatchAudioConverterService::nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}
