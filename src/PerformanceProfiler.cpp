#include "PerformanceProfiler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QSysInfo>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {
constexpr int kPublishIntervalMs = 1000;
constexpr qsizetype kMaxMemoryCheckpoints = 256;

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n')) {
        escaped = QStringLiteral("\"%1\"").arg(escaped);
    }
    return escaped;
}

QString graphicsApiToString(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Unknown:
        return QStringLiteral("Unknown");
    case QSGRendererInterface::Software:
        return QStringLiteral("Software");
    case QSGRendererInterface::OpenVG:
        return QStringLiteral("OpenVG");
    case QSGRendererInterface::OpenGL:
        return QStringLiteral("OpenGL");
    case QSGRendererInterface::Direct3D11:
        return QStringLiteral("D3D11");
    case QSGRendererInterface::Vulkan:
        return QStringLiteral("Vulkan");
    case QSGRendererInterface::Metal:
        return QStringLiteral("Metal");
    case QSGRendererInterface::Null:
        return QStringLiteral("Null");
    case QSGRendererInterface::Direct3D12:
        return QStringLiteral("D3D12");
    }
    return QStringLiteral("Unknown");
}

qint64 parseStatusKilobytes(const QByteArray &line)
{
    const int colonIndex = line.indexOf(':');
    if (colonIndex < 0) {
        return 0;
    }

    QList<QByteArray> parts = line.mid(colonIndex + 1).simplified().split(' ');
    if (parts.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const qint64 kilobytes = parts.constFirst().toLongLong(&ok);
    return ok ? (kilobytes * 1024) : 0;
}
}

PerformanceProfiler *PerformanceProfiler::s_instance = nullptr;

PerformanceProfiler::PerformanceProfiler(QObject *parent)
    : QObject(parent)
{
    m_publishTimer.setInterval(kPublishIntervalMs);
    connect(&m_publishTimer, &QTimer::timeout, this, &PerformanceProfiler::publishSnapshot);

    const QByteArray envEnabled = qgetenv("WAVEFLUX_PROFILE");
    const bool startEnabled = !envEnabled.isEmpty() && envEnabled != "0";
    if (startEnabled) {
        m_enabled = true;
        m_enabledAtomic.store(true, std::memory_order_relaxed);
        m_overlayVisible = true;
        m_publishTimer.start();
        reset();
        captureMemoryCheckpoint(QStringLiteral("profiler.enabled"));
    }
}

PerformanceProfiler::~PerformanceProfiler()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

PerformanceProfiler *PerformanceProfiler::instance()
{
    return s_instance;
}

void PerformanceProfiler::setInstance(PerformanceProfiler *instance)
{
    s_instance = instance;
}

bool PerformanceProfiler::enabled() const
{
    return m_enabledAtomic.load(std::memory_order_relaxed);
}

void PerformanceProfiler::setEnabled(bool enabled)
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_enabled == enabled) {
            return;
        }
        m_enabled = enabled;
        m_enabledAtomic.store(enabled, std::memory_order_relaxed);
        if (!m_enabled) {
            m_overlayVisible = false;
        }
        changed = true;
    }

    if (changed) {
        if (enabled) {
            reset();
            m_publishTimer.start();
            captureMemoryCheckpoint(QStringLiteral("profiler.enabled"));
        } else {
            m_publishTimer.stop();
            reset();
            emit overlayVisibleChanged();
        }
        emit enabledChanged();
    }
}

bool PerformanceProfiler::overlayVisible() const
{
    QMutexLocker lock(&m_mutex);
    return m_overlayVisible;
}

void PerformanceProfiler::setOverlayVisible(bool visible)
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        const bool clamped = m_enabled ? visible : false;
        if (m_overlayVisible == clamped) {
            return;
        }
        m_overlayVisible = clamped;
        changed = true;
    }
    if (changed) {
        emit overlayVisibleChanged();
    }
}

bool PerformanceProfiler::fullscreenWaveformActive() const
{
    QMutexLocker lock(&m_mutex);
    return m_fullscreenWaveformActive;
}

void PerformanceProfiler::setFullscreenWaveformActive(bool active)
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_fullscreenWaveformActive == active) {
            return;
        }
        m_fullscreenWaveformActive = active;
        changed = true;
    }
    if (changed) {
        emit fullscreenWaveformActiveChanged();
    }
}

int PerformanceProfiler::playlistTrackCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_playlistTrackCount;
}

void PerformanceProfiler::setPlaylistTrackCount(int count)
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_playlistTrackCount == count) {
            return;
        }
        m_playlistTrackCount = count;
        changed = true;
    }
    if (changed) {
        emit playlistTrackCountChanged();
    }
}

double PerformanceProfiler::sceneFps() const
{
    QMutexLocker lock(&m_mutex);
    return m_sceneFps;
}

double PerformanceProfiler::sceneFrameMsAvg() const
{
    QMutexLocker lock(&m_mutex);
    return m_sceneFrameMsAvg;
}

double PerformanceProfiler::sceneFrameMsWorst() const
{
    QMutexLocker lock(&m_mutex);
    return m_sceneFrameMsWorst;
}

double PerformanceProfiler::waveformPaintsPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_waveformPaintsPerSec;
}

double PerformanceProfiler::waveformPaintMsAvg() const
{
    QMutexLocker lock(&m_mutex);
    return m_waveformPaintMsAvg;
}

double PerformanceProfiler::waveformPaintMsWorst() const
{
    QMutexLocker lock(&m_mutex);
    return m_waveformPaintMsWorst;
}

double PerformanceProfiler::waveformPartialRepaintsPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_waveformPartialRepaintsPerSec;
}

double PerformanceProfiler::waveformFullRepaintsPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_waveformFullRepaintsPerSec;
}

double PerformanceProfiler::waveformDirtyCoveragePct() const
{
    QMutexLocker lock(&m_mutex);
    return m_waveformDirtyCoveragePct;
}

double PerformanceProfiler::playlistDataCallsPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_playlistDataCallsPerSec;
}

double PerformanceProfiler::playlistDataUsAvg() const
{
    QMutexLocker lock(&m_mutex);
    return m_playlistDataUsAvg;
}

double PerformanceProfiler::playlistDataUsWorst() const
{
    QMutexLocker lock(&m_mutex);
    return m_playlistDataUsWorst;
}

double PerformanceProfiler::searchQueriesPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchQueriesPerSec;
}

double PerformanceProfiler::searchQueryMsAvg() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchQueryMsAvg;
}

double PerformanceProfiler::searchQueryMsP95() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchQueryMsP95;
}

double PerformanceProfiler::searchQueryMsWorst() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchQueryMsWorst;
}

double PerformanceProfiler::searchSqliteQueriesPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchSqliteQueriesPerSec;
}

double PerformanceProfiler::searchFtsQueriesPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchFtsQueriesPerSec;
}

double PerformanceProfiler::searchLikeQueriesPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchLikeQueriesPerSec;
}

double PerformanceProfiler::searchFailuresPerSec() const
{
    QMutexLocker lock(&m_mutex);
    return m_searchFailuresPerSec;
}

qint64 PerformanceProfiler::workingSetBytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_memorySnapshot.workingSetBytes;
}

qint64 PerformanceProfiler::peakWorkingSetBytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_memorySnapshot.peakWorkingSetBytes;
}

qint64 PerformanceProfiler::privateBytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_memorySnapshot.privateBytes;
}

qint64 PerformanceProfiler::commitBytes() const
{
    QMutexLocker lock(&m_mutex);
    return m_memorySnapshot.commitBytes;
}

QString PerformanceProfiler::lastMemoryCheckpointLabel() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastMemoryCheckpointLabel;
}

QString PerformanceProfiler::lastExportPath() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastExportPath;
}

QString PerformanceProfiler::lastExportError() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastExportError;
}

void PerformanceProfiler::attachWindow(QQuickWindow *window)
{
    if (m_window == window) {
        return;
    }

    if (m_window) {
        disconnect(m_window, nullptr, this, nullptr);
    }

    m_window = window;
    m_frameClock.invalidate();

    if (m_window) {
        connect(m_window, &QQuickWindow::frameSwapped,
                this, &PerformanceProfiler::onFrameSwapped,
                Qt::QueuedConnection);
    }
}

void PerformanceProfiler::recordWaveformPaint(qint64 durationNs)
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_enabled) {
        return;
    }
    m_waveformPaintNs.count++;
    m_waveformPaintNs.total += static_cast<double>(durationNs);
    m_waveformPaintNs.worst = std::max(m_waveformPaintNs.worst, static_cast<double>(durationNs));
}

void PerformanceProfiler::recordWaveformRepaintRequest(bool fullRepaint, qreal dirtyAreaPx, qreal fullAreaPx)
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_enabled) {
        return;
    }

    if (fullRepaint) {
        m_waveformFullRepaintRequests++;
    } else {
        m_waveformPartialRepaintRequests++;
        if (fullAreaPx > 0.0) {
            const double coverage = std::clamp(static_cast<double>(dirtyAreaPx / fullAreaPx), 0.0, 1.0);
            m_waveformDirtyCoverageSum += coverage;
            m_waveformDirtyCoverageSamples++;
        }
    }
}

void PerformanceProfiler::recordTrackModelDataCall(qint64 durationNs)
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_enabled) {
        return;
    }
    m_playlistDataNs.count++;
    m_playlistDataNs.total += static_cast<double>(durationNs);
    m_playlistDataNs.worst = std::max(m_playlistDataNs.worst, static_cast<double>(durationNs));
}

void PerformanceProfiler::recordSearchQuery(qint64 durationNs,
                                            bool usedSqlite,
                                            bool usedFts,
                                            bool usedLike,
                                            bool success)
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_enabled) {
        return;
    }

    m_searchQueryNs.count++;
    m_searchQueryNs.total += static_cast<double>(durationNs);
    m_searchQueryNs.worst = std::max(m_searchQueryNs.worst, static_cast<double>(durationNs));
    m_searchQuerySamplesNs.push_back(static_cast<double>(durationNs));
    if (usedSqlite) {
        m_searchSqliteQueries++;
    }
    if (usedFts) {
        m_searchFtsQueries++;
    }
    if (usedLike) {
        m_searchLikeQueries++;
    }
    if (!success) {
        m_searchFailures++;
    }
}

void PerformanceProfiler::reset()
{
    const MemorySnapshot memorySnapshot = sampleProcessMemory();
    {
        QMutexLocker lock(&m_mutex);

        m_sceneFrameNs = {};
        m_waveformPaintNs = {};
        m_playlistDataNs = {};
        m_searchQueryNs = {};
        m_searchQuerySamplesNs.clear();

        m_waveformPartialRepaintRequests = 0;
        m_waveformFullRepaintRequests = 0;
        m_waveformDirtyCoverageSum = 0.0;
        m_waveformDirtyCoverageSamples = 0;
        m_searchSqliteQueries = 0;
        m_searchFtsQueries = 0;
        m_searchLikeQueries = 0;
        m_searchFailures = 0;

        m_sceneFps = 0.0;
        m_sceneFrameMsAvg = 0.0;
        m_sceneFrameMsWorst = 0.0;

        m_waveformPaintsPerSec = 0.0;
        m_waveformPaintMsAvg = 0.0;
        m_waveformPaintMsWorst = 0.0;
        m_waveformPartialRepaintsPerSec = 0.0;
        m_waveformFullRepaintsPerSec = 0.0;
        m_waveformDirtyCoveragePct = 0.0;

        m_playlistDataCallsPerSec = 0.0;
        m_playlistDataUsAvg = 0.0;
        m_playlistDataUsWorst = 0.0;
        m_searchQueriesPerSec = 0.0;
        m_searchQueryMsAvg = 0.0;
        m_searchQueryMsP95 = 0.0;
        m_searchQueryMsWorst = 0.0;
        m_searchSqliteQueriesPerSec = 0.0;
        m_searchFtsQueriesPerSec = 0.0;
        m_searchLikeQueriesPerSec = 0.0;
        m_searchFailuresPerSec = 0.0;
        m_memorySnapshot = memorySnapshot;
        m_memoryCheckpoints.clear();
        m_lastMemoryCheckpointLabel.clear();

        m_frameClock.invalidate();
    }

    emit metricsChanged();
}

void PerformanceProfiler::captureMemoryCheckpoint(const QString &label)
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }

    const QString trimmedLabel = label.trimmed().isEmpty()
        ? QStringLiteral("manual")
        : label.trimmed();
    const QString timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const MemorySnapshot memorySnapshot = sampleProcessMemory();
    int playlistTrackCount = 0;
    bool fullscreenWaveformActive = false;

    {
        QMutexLocker lock(&m_mutex);
        if (!m_enabled) {
            return;
        }

        m_memorySnapshot = memorySnapshot;
        appendMemoryCheckpointLocked(trimmedLabel, timestampUtc, memorySnapshot);
        playlistTrackCount = m_playlistTrackCount;
        fullscreenWaveformActive = m_fullscreenWaveformActive;
    }

    qInfo().noquote()
        << QStringLiteral("[MemoryProfile] label=%1 workingSetMiB=%2 privateMiB=%3 commitMiB=%4 peakWorkingSetMiB=%5 playlistTracks=%6 fullscreenWaveform=%7")
              .arg(trimmedLabel,
                   formatMemoryMiB(memorySnapshot.workingSetBytes),
                   formatMemoryMiB(memorySnapshot.privateBytes),
                   formatMemoryMiB(memorySnapshot.commitBytes),
                   formatMemoryMiB(memorySnapshot.peakWorkingSetBytes))
              .arg(playlistTrackCount)
              .arg(fullscreenWaveformActive ? QStringLiteral("yes") : QStringLiteral("no"));

    emit metricsChanged();
}

QString PerformanceProfiler::exportSnapshotJson()
{
    auto publishExportResult = [this](const QString &path, const QString &error) {
        bool pathChanged = false;
        bool errorChanged = false;
        {
            QMutexLocker lock(&m_mutex);
            setLastExportResultLocked(path, error, &pathChanged, &errorChanged);
        }
        if (pathChanged) {
            emit lastExportPathChanged();
        }
        if (errorChanged) {
            emit lastExportErrorChanged();
        }
    };

    if (enabled()) {
        publishSnapshot();
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QString timestampIso = nowUtc.toString(Qt::ISODateWithMs);
    const QString timestampFile = nowUtc.toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString outDirPath = profilingDirectoryPath();
    if (outDirPath.isEmpty() || !QDir().mkpath(outDirPath)) {
        publishExportResult(QString(), QStringLiteral("Failed to create profiling directory"));
        return {};
    }

    QJsonObject snapshot;
    {
        QMutexLocker lock(&m_mutex);
        QJsonObject memoryObject;
        memoryObject.insert(QStringLiteral("working_set_bytes"), static_cast<double>(m_memorySnapshot.workingSetBytes));
        memoryObject.insert(QStringLiteral("peak_working_set_bytes"), static_cast<double>(m_memorySnapshot.peakWorkingSetBytes));
        memoryObject.insert(QStringLiteral("private_bytes"), static_cast<double>(m_memorySnapshot.privateBytes));
        memoryObject.insert(QStringLiteral("commit_bytes"), static_cast<double>(m_memorySnapshot.commitBytes));

        QJsonArray memoryCheckpoints;
        for (const MemoryCheckpoint &checkpoint : m_memoryCheckpoints) {
            QJsonObject checkpointObject;
            checkpointObject.insert(QStringLiteral("label"), checkpoint.label);
            checkpointObject.insert(QStringLiteral("timestamp_utc"), checkpoint.timestampUtc);
            checkpointObject.insert(QStringLiteral("playlist_track_count"), checkpoint.playlistTrackCount);
            checkpointObject.insert(QStringLiteral("fullscreen_waveform_active"), checkpoint.fullscreenWaveformActive);
            checkpointObject.insert(QStringLiteral("working_set_bytes"), static_cast<double>(checkpoint.memory.workingSetBytes));
            checkpointObject.insert(QStringLiteral("peak_working_set_bytes"), static_cast<double>(checkpoint.memory.peakWorkingSetBytes));
            checkpointObject.insert(QStringLiteral("private_bytes"), static_cast<double>(checkpoint.memory.privateBytes));
            checkpointObject.insert(QStringLiteral("commit_bytes"), static_cast<double>(checkpoint.memory.commitBytes));
            memoryCheckpoints.append(checkpointObject);
        }

        snapshot.insert(QStringLiteral("timestamp_utc"), timestampIso);
        snapshot.insert(QStringLiteral("application"), QCoreApplication::applicationName());
        snapshot.insert(QStringLiteral("app_version"), QCoreApplication::applicationVersion());
        snapshot.insert(QStringLiteral("qt_version"), QString::fromLatin1(QT_VERSION_STR));
        snapshot.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
        snapshot.insert(QStringLiteral("cpu_arch"), QSysInfo::currentCpuArchitecture());
        snapshot.insert(QStringLiteral("scene_graph_backend"), QQuickWindow::sceneGraphBackend());
        snapshot.insert(QStringLiteral("render_api"), renderApiNameLocked());
        snapshot.insert(QStringLiteral("render_loop_env"), QString::fromUtf8(qgetenv("QSG_RENDER_LOOP")));
        snapshot.insert(QStringLiteral("fullscreen_waveform_active"), m_fullscreenWaveformActive);
        snapshot.insert(QStringLiteral("playlist_track_count"), m_playlistTrackCount);
        snapshot.insert(QStringLiteral("scene_fps"), m_sceneFps);
        snapshot.insert(QStringLiteral("scene_frame_ms_avg"), m_sceneFrameMsAvg);
        snapshot.insert(QStringLiteral("scene_frame_ms_worst"), m_sceneFrameMsWorst);
        snapshot.insert(QStringLiteral("waveform_paints_per_sec"), m_waveformPaintsPerSec);
        snapshot.insert(QStringLiteral("waveform_paint_ms_avg"), m_waveformPaintMsAvg);
        snapshot.insert(QStringLiteral("waveform_paint_ms_worst"), m_waveformPaintMsWorst);
        snapshot.insert(QStringLiteral("waveform_partial_repaints_per_sec"), m_waveformPartialRepaintsPerSec);
        snapshot.insert(QStringLiteral("waveform_full_repaints_per_sec"), m_waveformFullRepaintsPerSec);
        snapshot.insert(QStringLiteral("waveform_dirty_coverage_pct"), m_waveformDirtyCoveragePct);
        snapshot.insert(QStringLiteral("playlist_data_calls_per_sec"), m_playlistDataCallsPerSec);
        snapshot.insert(QStringLiteral("playlist_data_us_avg"), m_playlistDataUsAvg);
        snapshot.insert(QStringLiteral("playlist_data_us_worst"), m_playlistDataUsWorst);
        snapshot.insert(QStringLiteral("search_queries_per_sec"), m_searchQueriesPerSec);
        snapshot.insert(QStringLiteral("search_query_ms_avg"), m_searchQueryMsAvg);
        snapshot.insert(QStringLiteral("search_query_ms_p95"), m_searchQueryMsP95);
        snapshot.insert(QStringLiteral("search_query_ms_worst"), m_searchQueryMsWorst);
        snapshot.insert(QStringLiteral("search_sqlite_queries_per_sec"), m_searchSqliteQueriesPerSec);
        snapshot.insert(QStringLiteral("search_fts_queries_per_sec"), m_searchFtsQueriesPerSec);
        snapshot.insert(QStringLiteral("search_like_queries_per_sec"), m_searchLikeQueriesPerSec);
        snapshot.insert(QStringLiteral("search_failures_per_sec"), m_searchFailuresPerSec);
        snapshot.insert(QStringLiteral("memory"), memoryObject);
        snapshot.insert(QStringLiteral("memory_checkpoint_count"), static_cast<int>(m_memoryCheckpoints.size()));
        snapshot.insert(QStringLiteral("last_memory_checkpoint_label"), m_lastMemoryCheckpointLabel);
        snapshot.insert(QStringLiteral("memory_checkpoints"), memoryCheckpoints);
    }

    const QString outPath = QDir(outDirPath).filePath(QStringLiteral("snapshot_%1.json").arg(timestampFile));
    QSaveFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        publishExportResult(QString(), QStringLiteral("Failed to open %1").arg(outPath));
        return {};
    }

    const QByteArray payload = QJsonDocument(snapshot).toJson(QJsonDocument::Indented);
    if (outFile.write(payload) != payload.size() || !outFile.commit()) {
        publishExportResult(QString(), QStringLiteral("Failed to write %1").arg(outPath));
        return {};
    }

    publishExportResult(outPath, QString());
    return outPath;
}

QString PerformanceProfiler::exportSnapshotCsv()
{
    auto publishExportResult = [this](const QString &path, const QString &error) {
        bool pathChanged = false;
        bool errorChanged = false;
        {
            QMutexLocker lock(&m_mutex);
            setLastExportResultLocked(path, error, &pathChanged, &errorChanged);
        }
        if (pathChanged) {
            emit lastExportPathChanged();
        }
        if (errorChanged) {
            emit lastExportErrorChanged();
        }
    };

    if (enabled()) {
        publishSnapshot();
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QString timestampIso = nowUtc.toString(Qt::ISODateWithMs);
    const QString outDirPath = profilingDirectoryPath();
    if (outDirPath.isEmpty() || !QDir().mkpath(outDirPath)) {
        publishExportResult(QString(), QStringLiteral("Failed to create profiling directory"));
        return {};
    }

    QStringList values;
    {
        QMutexLocker lock(&m_mutex);
        values = {
            timestampIso,
            QCoreApplication::applicationName(),
            QCoreApplication::applicationVersion(),
            QString::fromLatin1(QT_VERSION_STR),
            QSysInfo::prettyProductName(),
            QSysInfo::currentCpuArchitecture(),
            QQuickWindow::sceneGraphBackend(),
            renderApiNameLocked(),
            QString::fromUtf8(qgetenv("QSG_RENDER_LOOP")),
            m_fullscreenWaveformActive ? QStringLiteral("1") : QStringLiteral("0"),
            QString::number(m_playlistTrackCount),
            QString::number(m_sceneFps, 'f', 3),
            QString::number(m_sceneFrameMsAvg, 'f', 4),
            QString::number(m_sceneFrameMsWorst, 'f', 4),
            QString::number(m_waveformPaintsPerSec, 'f', 3),
            QString::number(m_waveformPaintMsAvg, 'f', 4),
            QString::number(m_waveformPaintMsWorst, 'f', 4),
            QString::number(m_waveformPartialRepaintsPerSec, 'f', 3),
            QString::number(m_waveformFullRepaintsPerSec, 'f', 3),
            QString::number(m_waveformDirtyCoveragePct, 'f', 3),
            QString::number(m_playlistDataCallsPerSec, 'f', 3),
            QString::number(m_playlistDataUsAvg, 'f', 4),
            QString::number(m_playlistDataUsWorst, 'f', 4),
            QString::number(m_searchQueriesPerSec, 'f', 3),
            QString::number(m_searchQueryMsAvg, 'f', 4),
            QString::number(m_searchQueryMsP95, 'f', 4),
            QString::number(m_searchQueryMsWorst, 'f', 4),
            QString::number(m_searchSqliteQueriesPerSec, 'f', 3),
            QString::number(m_searchFtsQueriesPerSec, 'f', 3),
            QString::number(m_searchLikeQueriesPerSec, 'f', 3),
            QString::number(m_searchFailuresPerSec, 'f', 3),
            QString::number(m_memorySnapshot.workingSetBytes),
            QString::number(m_memorySnapshot.peakWorkingSetBytes),
            QString::number(m_memorySnapshot.privateBytes),
            QString::number(m_memorySnapshot.commitBytes),
            m_lastMemoryCheckpointLabel,
            QString::number(m_memoryCheckpoints.size()),
        };
    }

    const QStringList header = {
        QStringLiteral("timestamp_utc"),
        QStringLiteral("application"),
        QStringLiteral("app_version"),
        QStringLiteral("qt_version"),
        QStringLiteral("os"),
        QStringLiteral("cpu_arch"),
        QStringLiteral("scene_graph_backend"),
        QStringLiteral("render_api"),
        QStringLiteral("render_loop_env"),
        QStringLiteral("fullscreen_waveform_active"),
        QStringLiteral("playlist_track_count"),
        QStringLiteral("scene_fps"),
        QStringLiteral("scene_frame_ms_avg"),
        QStringLiteral("scene_frame_ms_worst"),
        QStringLiteral("waveform_paints_per_sec"),
        QStringLiteral("waveform_paint_ms_avg"),
        QStringLiteral("waveform_paint_ms_worst"),
        QStringLiteral("waveform_partial_repaints_per_sec"),
        QStringLiteral("waveform_full_repaints_per_sec"),
        QStringLiteral("waveform_dirty_coverage_pct"),
        QStringLiteral("playlist_data_calls_per_sec"),
        QStringLiteral("playlist_data_us_avg"),
        QStringLiteral("playlist_data_us_worst"),
        QStringLiteral("search_queries_per_sec"),
        QStringLiteral("search_query_ms_avg"),
        QStringLiteral("search_query_ms_p95"),
        QStringLiteral("search_query_ms_worst"),
        QStringLiteral("search_sqlite_queries_per_sec"),
        QStringLiteral("search_fts_queries_per_sec"),
        QStringLiteral("search_like_queries_per_sec"),
        QStringLiteral("search_failures_per_sec"),
        QStringLiteral("working_set_bytes"),
        QStringLiteral("peak_working_set_bytes"),
        QStringLiteral("private_bytes"),
        QStringLiteral("commit_bytes"),
        QStringLiteral("last_memory_checkpoint_label"),
        QStringLiteral("memory_checkpoint_count"),
    };

    QStringList escapedHeader;
    escapedHeader.reserve(header.size());
    for (const QString &h : header) {
        escapedHeader.append(csvEscape(h));
    }

    QStringList escapedValues;
    escapedValues.reserve(values.size());
    for (const QString &v : values) {
        escapedValues.append(csvEscape(v));
    }

    const QString outPath = QDir(outDirPath).filePath(QStringLiteral("profiling_history.csv"));
    QFile inFile(outPath);
    QByteArray existing;
    if (inFile.exists()) {
        if (!inFile.open(QIODevice::ReadOnly)) {
            publishExportResult(QString(), QStringLiteral("Failed to read %1").arg(outPath));
            return {};
        }
        existing = inFile.readAll();
        inFile.close();
    }

    const QByteArray headerLine = escapedHeader.join(',').toUtf8();
    const QByteArray valueLine = escapedValues.join(',').toUtf8();

    QByteArray newContent = existing;
    if (newContent.isEmpty()) {
        newContent.append(headerLine);
        newContent.append('\n');
    } else if (!newContent.startsWith(headerLine)) {
        // Prefix header if file has unexpected content.
        QByteArray prefixed = headerLine;
        prefixed.append('\n');
        prefixed.append(newContent);
        newContent = prefixed;
    }

    if (!newContent.isEmpty() && !newContent.endsWith('\n')) {
        newContent.append('\n');
    }
    newContent.append(valueLine);
    newContent.append('\n');

    QSaveFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        publishExportResult(QString(), QStringLiteral("Failed to open %1").arg(outPath));
        return {};
    }

    if (outFile.write(newContent) != newContent.size() || !outFile.commit()) {
        publishExportResult(QString(), QStringLiteral("Failed to write %1").arg(outPath));
        return {};
    }

    publishExportResult(outPath, QString());
    return outPath;
}

QString PerformanceProfiler::exportSnapshotBundle()
{
    const QString jsonPath = exportSnapshotJson();
    const QString csvPath = exportSnapshotCsv();
    if (jsonPath.isEmpty() || csvPath.isEmpty()) {
        return {};
    }
    return profilingDirectoryPath();
}

void PerformanceProfiler::onFrameSwapped()
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_enabled) {
        return;
    }

    if (!m_frameClock.isValid()) {
        m_frameClock.start();
        return;
    }

    const qint64 dtNs = m_frameClock.nsecsElapsed();
    m_frameClock.restart();
    if (dtNs <= 0) {
        return;
    }

    m_sceneFrameNs.count++;
    m_sceneFrameNs.total += static_cast<double>(dtNs);
    m_sceneFrameNs.worst = std::max(m_sceneFrameNs.worst, static_cast<double>(dtNs));
}

void PerformanceProfiler::publishSnapshot()
{
    if (!m_enabledAtomic.load(std::memory_order_relaxed)) {
        return;
    }
    const MemorySnapshot memorySnapshot = sampleProcessMemory();
    {
        QMutexLocker lock(&m_mutex);
        if (!m_enabled) {
            return;
        }

        const double seconds = static_cast<double>(kPublishIntervalMs) / 1000.0;

        m_sceneFps = m_sceneFrameNs.count / seconds;
        m_sceneFrameMsAvg = m_sceneFrameNs.count > 0
            ? (m_sceneFrameNs.total / static_cast<double>(m_sceneFrameNs.count)) / 1'000'000.0
            : 0.0;
        m_sceneFrameMsWorst = m_sceneFrameNs.worst / 1'000'000.0;

        m_waveformPaintsPerSec = m_waveformPaintNs.count / seconds;
        m_waveformPaintMsAvg = m_waveformPaintNs.count > 0
            ? (m_waveformPaintNs.total / static_cast<double>(m_waveformPaintNs.count)) / 1'000'000.0
            : 0.0;
        m_waveformPaintMsWorst = m_waveformPaintNs.worst / 1'000'000.0;
        m_waveformPartialRepaintsPerSec = m_waveformPartialRepaintRequests / seconds;
        m_waveformFullRepaintsPerSec = m_waveformFullRepaintRequests / seconds;
        m_waveformDirtyCoveragePct = m_waveformDirtyCoverageSamples > 0
            ? (m_waveformDirtyCoverageSum / static_cast<double>(m_waveformDirtyCoverageSamples)) * 100.0
            : 0.0;

        m_playlistDataCallsPerSec = m_playlistDataNs.count / seconds;
        m_playlistDataUsAvg = m_playlistDataNs.count > 0
            ? (m_playlistDataNs.total / static_cast<double>(m_playlistDataNs.count)) / 1'000.0
            : 0.0;
        m_playlistDataUsWorst = m_playlistDataNs.worst / 1'000.0;

        m_searchQueriesPerSec = m_searchQueryNs.count / seconds;
        m_searchQueryMsAvg = m_searchQueryNs.count > 0
            ? (m_searchQueryNs.total / static_cast<double>(m_searchQueryNs.count)) / 1'000'000.0
            : 0.0;
        m_searchQueryMsWorst = m_searchQueryNs.worst / 1'000'000.0;
        m_searchSqliteQueriesPerSec = m_searchSqliteQueries / seconds;
        m_searchFtsQueriesPerSec = m_searchFtsQueries / seconds;
        m_searchLikeQueriesPerSec = m_searchLikeQueries / seconds;
        m_searchFailuresPerSec = m_searchFailures / seconds;
        m_memorySnapshot = memorySnapshot;
        if (!m_searchQuerySamplesNs.isEmpty()) {
            std::sort(m_searchQuerySamplesNs.begin(), m_searchQuerySamplesNs.end());
            const qsizetype sampleCount = m_searchQuerySamplesNs.size();
            const qsizetype p95Index = qMax<qsizetype>(
                0,
                ((sampleCount * 95 + 99) / 100) - 1);
            m_searchQueryMsP95 = m_searchQuerySamplesNs.at(p95Index) / 1'000'000.0;
        } else {
            m_searchQueryMsP95 = 0.0;
        }

        m_sceneFrameNs = {};
        m_waveformPaintNs = {};
        m_playlistDataNs = {};
        m_searchQueryNs = {};
        m_searchQuerySamplesNs.clear();
        m_waveformPartialRepaintRequests = 0;
        m_waveformFullRepaintRequests = 0;
        m_waveformDirtyCoverageSum = 0.0;
        m_waveformDirtyCoverageSamples = 0;
        m_searchSqliteQueries = 0;
        m_searchFtsQueries = 0;
        m_searchLikeQueries = 0;
        m_searchFailures = 0;
    }

    emit metricsChanged();
}

QString PerformanceProfiler::renderApiNameLocked() const
{
    if (!m_window || !m_window->rendererInterface()) {
        return QStringLiteral("Unknown");
    }
    return graphicsApiToString(m_window->rendererInterface()->graphicsApi());
}

QString PerformanceProfiler::profilingDirectoryPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".waveflux"));
    }
    return QDir(base).filePath(QStringLiteral("profiling"));
}

PerformanceProfiler::MemorySnapshot PerformanceProfiler::sampleProcessMemory()
{
    MemorySnapshot snapshot;

#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters))) {
        snapshot.workingSetBytes = static_cast<qint64>(counters.WorkingSetSize);
        snapshot.peakWorkingSetBytes = static_cast<qint64>(counters.PeakWorkingSetSize);
        snapshot.privateBytes = static_cast<qint64>(counters.PrivateUsage);
        snapshot.commitBytes = static_cast<qint64>(counters.PagefileUsage);
    }
#elif defined(Q_OS_LINUX)
    QFile statusFile(QStringLiteral("/proc/self/status"));
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!statusFile.atEnd()) {
            const QByteArray line = statusFile.readLine();
            if (line.startsWith("VmRSS:")) {
                snapshot.workingSetBytes = parseStatusKilobytes(line);
            } else if (line.startsWith("VmHWM:")) {
                snapshot.peakWorkingSetBytes = parseStatusKilobytes(line);
            } else if (line.startsWith("VmData:")) {
                snapshot.privateBytes = parseStatusKilobytes(line);
            } else if (line.startsWith("VmSize:")) {
                snapshot.commitBytes = parseStatusKilobytes(line);
            }
        }
    }
    if (snapshot.peakWorkingSetBytes < snapshot.workingSetBytes) {
        snapshot.peakWorkingSetBytes = snapshot.workingSetBytes;
    }
    if (snapshot.privateBytes <= 0) {
        snapshot.privateBytes = snapshot.workingSetBytes;
    }
    if (snapshot.commitBytes <= 0) {
        snapshot.commitBytes = snapshot.privateBytes;
    }
#endif

    return snapshot;
}

QString PerformanceProfiler::formatMemoryMiB(qint64 bytes)
{
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QString::number(mib, 'f', 1);
}

void PerformanceProfiler::appendMemoryCheckpointLocked(const QString &label,
                                                       const QString &timestampUtc,
                                                       const MemorySnapshot &memorySnapshot)
{
    if (m_memoryCheckpoints.size() >= kMaxMemoryCheckpoints) {
        m_memoryCheckpoints.remove(0, m_memoryCheckpoints.size() - kMaxMemoryCheckpoints + 1);
    }

    MemoryCheckpoint checkpoint;
    checkpoint.label = label;
    checkpoint.timestampUtc = timestampUtc;
    checkpoint.playlistTrackCount = m_playlistTrackCount;
    checkpoint.fullscreenWaveformActive = m_fullscreenWaveformActive;
    checkpoint.memory = memorySnapshot;
    m_memoryCheckpoints.append(checkpoint);
    m_lastMemoryCheckpointLabel = label;
}

bool PerformanceProfiler::setLastExportResultLocked(const QString &path, const QString &error,
                                                    bool *pathChangedOut, bool *errorChangedOut)
{
    const bool pathChanged = (m_lastExportPath != path);
    const bool errorChanged = (m_lastExportError != error);
    m_lastExportPath = path;
    m_lastExportError = error;
    if (pathChangedOut) {
        *pathChangedOut = pathChanged;
    }
    if (errorChangedOut) {
        *errorChangedOut = errorChanged;
    }
    return pathChanged || errorChanged;
}
