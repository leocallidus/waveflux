#ifndef WAVEFORMPROVIDER_H
#define WAVEFORMPROVIDER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QFuture>
#include <QFutureWatcher>
#include <atomic>
#include <functional>
#include <memory>

class PeaksCacheManager;

/**
 * @brief WaveformProvider - Extracts audio amplitude data for visualization
 * 
 * This class performs background waveform analysis using QtConcurrent,
 * ensuring the UI remains responsive even when processing large audio files.
 * The extracted peaks are used by WaveformItem for rendering.
 */
class WaveformProvider : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QVector<float> peaks READ peaks NOTIFY peaksReady)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY peaksReady)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    
public:
    explicit WaveformProvider(QObject *parent = nullptr);
    ~WaveformProvider() override;
    
    bool isLoading() const { return m_loading; }
    QVector<float> peaks() const { return m_peaks; }
    int sampleCount() const { return m_peaks.size(); }
    double progress() const { return m_progress; }
    
    // Get peaks scaled for a specific width (for waveform rendering)
    Q_INVOKABLE QVector<float> getPeaksForWidth(int width) const;
    
public slots:
    void loadFile(const QString &filePath);
    void cancel();
    
signals:
    void loadingChanged(bool loading);
    void peaksReady();
    void progressChanged(double progress);
    void error(const QString &message);
    
private:
    using PartialCallback = std::function<void(const QVector<float> &, double)>;

    struct WaveformData {
        QVector<float> peaks;
        bool success = false;
        bool canceled = false;
        QString errorMessage;
        quint64 generationId = 0;
        QString sourceFilePath;
    };
    
    static WaveformData extractWaveform(const QString &filePath,
                                        int targetSamples,
                                        const PartialCallback &partialCallback,
                                        const std::atomic_bool *cancelRequested);
    void applyPartialPeaks(QVector<float> peaks, double progress, quint64 generationId);
    void onExtractionFinished(QFutureWatcher<WaveformData> *watcher, quint64 generationId);
    
    QVector<float> m_peaks;
    bool m_loading = false;
    double m_progress = 0.0;
    QFutureWatcher<WaveformData> *m_watcher = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancelToken;
    quint64 m_generationId = 0;
    QString m_currentFilePath;

    PeaksCacheManager *m_cache = nullptr;

    static constexpr int DEFAULT_SAMPLE_COUNT = 4096;
};

#endif // WAVEFORMPROVIDER_H
