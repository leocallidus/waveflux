#ifndef PERFORMANCEPROFILER_H
#define PERFORMANCEPROFILER_H

#include <QObject>
#include <QElapsedTimer>
#include <QMutex>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QVector>
#include <atomic>

class PerformanceProfiler : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible WRITE setOverlayVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool fullscreenWaveformActive READ fullscreenWaveformActive WRITE setFullscreenWaveformActive NOTIFY fullscreenWaveformActiveChanged)
    Q_PROPERTY(int playlistTrackCount READ playlistTrackCount NOTIFY playlistTrackCountChanged)
    Q_PROPERTY(QString lastExportPath READ lastExportPath NOTIFY lastExportPathChanged)
    Q_PROPERTY(QString lastExportError READ lastExportError NOTIFY lastExportErrorChanged)
    Q_PROPERTY(qint64 workingSetBytes READ workingSetBytes NOTIFY metricsChanged)
    Q_PROPERTY(qint64 peakWorkingSetBytes READ peakWorkingSetBytes NOTIFY metricsChanged)
    Q_PROPERTY(qint64 privateBytes READ privateBytes NOTIFY metricsChanged)
    Q_PROPERTY(qint64 commitBytes READ commitBytes NOTIFY metricsChanged)
    Q_PROPERTY(QString lastMemoryCheckpointLabel READ lastMemoryCheckpointLabel NOTIFY metricsChanged)

    Q_PROPERTY(double sceneFps READ sceneFps NOTIFY metricsChanged)
    Q_PROPERTY(double sceneFrameMsAvg READ sceneFrameMsAvg NOTIFY metricsChanged)
    Q_PROPERTY(double sceneFrameMsWorst READ sceneFrameMsWorst NOTIFY metricsChanged)

    Q_PROPERTY(double waveformPaintsPerSec READ waveformPaintsPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double waveformPaintMsAvg READ waveformPaintMsAvg NOTIFY metricsChanged)
    Q_PROPERTY(double waveformPaintMsWorst READ waveformPaintMsWorst NOTIFY metricsChanged)
    Q_PROPERTY(double waveformPartialRepaintsPerSec READ waveformPartialRepaintsPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double waveformFullRepaintsPerSec READ waveformFullRepaintsPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double waveformDirtyCoveragePct READ waveformDirtyCoveragePct NOTIFY metricsChanged)

    Q_PROPERTY(double playlistDataCallsPerSec READ playlistDataCallsPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double playlistDataUsAvg READ playlistDataUsAvg NOTIFY metricsChanged)
    Q_PROPERTY(double playlistDataUsWorst READ playlistDataUsWorst NOTIFY metricsChanged)
    Q_PROPERTY(double searchQueriesPerSec READ searchQueriesPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double searchQueryMsAvg READ searchQueryMsAvg NOTIFY metricsChanged)
    Q_PROPERTY(double searchQueryMsP95 READ searchQueryMsP95 NOTIFY metricsChanged)
    Q_PROPERTY(double searchQueryMsWorst READ searchQueryMsWorst NOTIFY metricsChanged)
    Q_PROPERTY(double searchSqliteQueriesPerSec READ searchSqliteQueriesPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double searchFtsQueriesPerSec READ searchFtsQueriesPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double searchLikeQueriesPerSec READ searchLikeQueriesPerSec NOTIFY metricsChanged)
    Q_PROPERTY(double searchFailuresPerSec READ searchFailuresPerSec NOTIFY metricsChanged)

public:
    explicit PerformanceProfiler(QObject *parent = nullptr);
    ~PerformanceProfiler() override;

    static PerformanceProfiler *instance();
    static void setInstance(PerformanceProfiler *instance);

    bool enabled() const;
    void setEnabled(bool enabled);

    bool overlayVisible() const;
    void setOverlayVisible(bool visible);

    bool fullscreenWaveformActive() const;
    void setFullscreenWaveformActive(bool active);

    int playlistTrackCount() const;
    void setPlaylistTrackCount(int count);

    double sceneFps() const;
    double sceneFrameMsAvg() const;
    double sceneFrameMsWorst() const;

    double waveformPaintsPerSec() const;
    double waveformPaintMsAvg() const;
    double waveformPaintMsWorst() const;
    double waveformPartialRepaintsPerSec() const;
    double waveformFullRepaintsPerSec() const;
    double waveformDirtyCoveragePct() const;

    double playlistDataCallsPerSec() const;
    double playlistDataUsAvg() const;
    double playlistDataUsWorst() const;
    double searchQueriesPerSec() const;
    double searchQueryMsAvg() const;
    double searchQueryMsP95() const;
    double searchQueryMsWorst() const;
    double searchSqliteQueriesPerSec() const;
    double searchFtsQueriesPerSec() const;
    double searchLikeQueriesPerSec() const;
    double searchFailuresPerSec() const;

    void attachWindow(QQuickWindow *window);

    void recordWaveformPaint(qint64 durationNs);
    void recordWaveformRepaintRequest(bool fullRepaint, qreal dirtyAreaPx, qreal fullAreaPx);
    void recordTrackModelDataCall(qint64 durationNs);
    void recordSearchQuery(qint64 durationNs,
                           bool usedSqlite,
                           bool usedFts,
                           bool usedLike,
                           bool success);

    Q_INVOKABLE void reset();
    Q_INVOKABLE QString exportSnapshotJson();
    Q_INVOKABLE QString exportSnapshotCsv();
    Q_INVOKABLE QString exportSnapshotBundle();

    QString lastExportPath() const;
    QString lastExportError() const;
    qint64 workingSetBytes() const;
    qint64 peakWorkingSetBytes() const;
    qint64 privateBytes() const;
    qint64 commitBytes() const;
    QString lastMemoryCheckpointLabel() const;
    Q_INVOKABLE void captureMemoryCheckpoint(const QString &label);

signals:
    void enabledChanged();
    void overlayVisibleChanged();
    void fullscreenWaveformActiveChanged();
    void playlistTrackCountChanged();
    void lastExportPathChanged();
    void lastExportErrorChanged();
    void metricsChanged();

private slots:
    void onFrameSwapped();
    void publishSnapshot();

private:
    struct Accumulator {
        qint64 count = 0;
        double total = 0.0;
        double worst = 0.0;
    };

    struct MemorySnapshot {
        qint64 workingSetBytes = 0;
        qint64 peakWorkingSetBytes = 0;
        qint64 privateBytes = 0;
        qint64 commitBytes = 0;
    };

    struct MemoryCheckpoint {
        QString label;
        QString timestampUtc;
        int playlistTrackCount = 0;
        bool fullscreenWaveformActive = false;
        MemorySnapshot memory;
    };

    static PerformanceProfiler *s_instance;

    mutable QMutex m_mutex;
    QPointer<QQuickWindow> m_window;
    QElapsedTimer m_frameClock;
    QTimer m_publishTimer;

    bool m_enabled = false;
    std::atomic_bool m_enabledAtomic{false};
    bool m_overlayVisible = false;
    bool m_fullscreenWaveformActive = false;
    int m_playlistTrackCount = 0;

    Accumulator m_sceneFrameNs;
    Accumulator m_waveformPaintNs;
    Accumulator m_playlistDataNs;
    Accumulator m_searchQueryNs;
    QVector<double> m_searchQuerySamplesNs;

    qint64 m_waveformPartialRepaintRequests = 0;
    qint64 m_waveformFullRepaintRequests = 0;
    double m_waveformDirtyCoverageSum = 0.0;
    qint64 m_waveformDirtyCoverageSamples = 0;
    qint64 m_searchSqliteQueries = 0;
    qint64 m_searchFtsQueries = 0;
    qint64 m_searchLikeQueries = 0;
    qint64 m_searchFailures = 0;

    double m_sceneFps = 0.0;
    double m_sceneFrameMsAvg = 0.0;
    double m_sceneFrameMsWorst = 0.0;

    double m_waveformPaintsPerSec = 0.0;
    double m_waveformPaintMsAvg = 0.0;
    double m_waveformPaintMsWorst = 0.0;
    double m_waveformPartialRepaintsPerSec = 0.0;
    double m_waveformFullRepaintsPerSec = 0.0;
    double m_waveformDirtyCoveragePct = 0.0;

    double m_playlistDataCallsPerSec = 0.0;
    double m_playlistDataUsAvg = 0.0;
    double m_playlistDataUsWorst = 0.0;
    double m_searchQueriesPerSec = 0.0;
    double m_searchQueryMsAvg = 0.0;
    double m_searchQueryMsP95 = 0.0;
    double m_searchQueryMsWorst = 0.0;
    double m_searchSqliteQueriesPerSec = 0.0;
    double m_searchFtsQueriesPerSec = 0.0;
    double m_searchLikeQueriesPerSec = 0.0;
    double m_searchFailuresPerSec = 0.0;
    QString m_lastExportPath;
    QString m_lastExportError;
    MemorySnapshot m_memorySnapshot;
    QVector<MemoryCheckpoint> m_memoryCheckpoints;
    QString m_lastMemoryCheckpointLabel;

    QString renderApiNameLocked() const;
    QString profilingDirectoryPath() const;
    bool setLastExportResultLocked(const QString &path, const QString &error,
                                   bool *pathChanged = nullptr, bool *errorChanged = nullptr);
    static MemorySnapshot sampleProcessMemory();
    static QString formatMemoryMiB(qint64 bytes);
    void appendMemoryCheckpointLocked(const QString &label,
                                     const QString &timestampUtc,
                                     const MemorySnapshot &memorySnapshot);
};

#endif // PERFORMANCEPROFILER_H
