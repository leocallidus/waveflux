#ifndef WAVEFORMITEM_H
#define WAVEFORMITEM_H

#include <QQuickPaintedItem>
#include <QVector>
#include <QColor>
#include <QPainterPath>
#include <QRectF>
#include <QTimer>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QPalette>

class WaveformProvider;

/**
 * @brief WaveformItem - Custom QQuickPaintedItem for waveform visualization
 * 
 * This is the core visual component that renders the audio waveform.
 * It uses QPainter for efficient rendering and supports:
 * - Click-to-seek functionality
 * - Progress indicator overlay
 * - Customizable colors for theming
 */
class WaveformItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(QObject* provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(QVector<float> peaks READ peaks WRITE setPeaks NOTIFY peaksChanged)
    Q_PROPERTY(double progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(QColor waveformColor READ waveformColor WRITE setWaveformColor NOTIFY waveformColorChanged)
    Q_PROPERTY(QColor progressColor READ progressColor WRITE setProgressColor NOTIFY progressColorChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(bool loading READ isLoading WRITE setLoading NOTIFY loadingChanged)
    Q_PROPERTY(double generationProgress READ generationProgress WRITE setGenerationProgress NOTIFY generationProgressChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(double viewCenter READ viewCenter WRITE setViewCenter NOTIFY viewCenterChanged)
    Q_PROPERTY(bool quickScrubEnabled READ quickScrubEnabled WRITE setQuickScrubEnabled NOTIFY quickScrubEnabledChanged)
    Q_PROPERTY(double quickScrubFactor READ quickScrubFactor WRITE setQuickScrubFactor NOTIFY quickScrubFactorChanged)
    Q_PROPERTY(bool quickScrubActive READ quickScrubActive NOTIFY quickScrubActiveChanged)
    
public:
    explicit WaveformItem(QQuickItem *parent = nullptr);
    ~WaveformItem() override = default;
    
    void paint(QPainter *painter) override;
    
    QObject *provider() const { return reinterpret_cast<QObject *>(m_provider); }
    void setProvider(QObject *provider);

    QVector<float> peaks() const { return m_peaks; }
    void setPeaks(const QVector<float> &peaks);
    
    double progress() const { return m_progress; }
    void setProgress(double progress);
    
    QColor waveformColor() const { return m_waveformColor; }
    void setWaveformColor(const QColor &color);
    
    QColor progressColor() const { return m_progressColor; }
    void setProgressColor(const QColor &color);
    
    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color);
    
    bool isLoading() const { return m_loading; }
    void setLoading(bool loading);

    double generationProgress() const { return m_generationProgress; }
    void setGenerationProgress(double progress);

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

    double viewCenter() const { return m_viewCenter; }
    void setViewCenter(double center);

    bool quickScrubEnabled() const { return m_quickScrubEnabled; }
    void setQuickScrubEnabled(bool enabled);

    double quickScrubFactor() const { return m_quickScrubFactor; }
    void setQuickScrubFactor(double factor);

    bool quickScrubActive() const { return m_quickScrubActive; }

    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void zoomAround(double newZoom, double anchorX);
    Q_INVOKABLE double viewToTrack(double normalizedX) const;
    Q_INVOKABLE double trackToView(double trackPosition) const;
    
signals:
    void providerChanged();
    void peaksChanged();
    void progressChanged();
    void waveformColorChanged();
    void progressColorChanged();
    void backgroundColorChanged();
    void loadingChanged();
    void generationProgressChanged();
    void zoomChanged();
    void viewCenterChanged();
    void quickScrubEnabledChanged();
    void quickScrubFactorChanged();
    void quickScrubActiveChanged();
    void seekRequested(double position); // 0.0 to 1.0
    
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    
private:
    struct ViewportRange {
        double start = 0.0;
        double span = 1.0;
    };

    struct LodLevel {
        QVector<float> peaks;
    };

    ViewportRange viewport() const;
    double pixelToTrackPosition(qreal x) const;
    int trackPositionToPixel(double position, int widthPx) const;
    void setQuickScrubActive(bool active);
    QRectF itemBoundsRect() const;
    QRectF progressDirtyRect(int oldPixel, int newPixel) const;
    QRectF generationDirtyRect(int oldPixel, int newPixel) const;
    void requestRepaint(const QRectF &dirtyRect = QRectF());
    void flushScheduledRepaint();
    void stopPanInertia();
    void startPanInertiaIfNeeded();

    const QVector<float> &sourcePeaks() const;
    bool usesProviderSource() const;
    void refreshSourceData(bool allowLodRebuild);
    void rebuildLodLevels();
    void updateCachedPeaks();
    int effectiveTargetSamples(int widthPx) const;
    static QVector<float> downsampleMaxRange(const QVector<float> &input,
                                             int startIndex,
                                             int endIndexExclusive,
                                             int targetSamples);
    void rebuildWavePath(int fullWidth, int h);
    void rebuildWaveLayers(int w, int h);
    void invalidateWaveLayers();
    void forceFullRedraw();
    void releaseTransientCaches(bool releaseLodLevels);
    bool canCacheWaveLayers(int w, int h) const;
    qsizetype layerCacheBudgetBytes() const;
    
    WaveformProvider *m_provider = nullptr;
    QVector<float> m_peaks;
    QVector<LodLevel> m_lodLevels;
    QVector<float> m_cachedPeaks; // Resampled for current width
    QPainterPath m_cachedWavePath;
    QImage m_cachedUnplayedLayer;
    QImage m_cachedPlayedLayer;
    bool m_waveLayersDirty = true;
    int m_lastPeakCacheWidthBucket = -1;
    int m_lastWidth = 0;
    int m_lastHeight = 0;
    int m_lastProgressPixel = -1;
    
    double m_progress = 0.0;
    double m_generationProgress = 0.0;
    bool m_loading = false;
    double m_zoom = 1.0;
    double m_viewCenter = 0.5;
    bool m_quickScrubEnabled = true;
    double m_quickScrubFactor = 4.0;
    bool m_quickScrubActive = false;
    bool m_draggingSeek = false;
    bool m_draggingPan = false;
    qreal m_lastQuickScrubX = 0.0;
    qreal m_lastPanX = 0.0;
    double m_lastQuickScrubPosition = 0.0;
    QTimer m_panInertiaTimer;
    QElapsedTimer m_panClock;
    qint64 m_lastPanTickMs = 0;
    double m_panVelocity = 0.0; // normalized center units per second
    QRectF m_pendingDirtyRect;
    bool m_hasPendingDirtyRect = false;
    bool m_pendingFullRepaint = false;
    QTimer m_repaintTimer;
    QElapsedTimer m_repaintClock;
    
    QColor m_waveformColor{QGuiApplication::palette().color(QPalette::Mid)};
    QColor m_progressColor{QGuiApplication::palette().color(QPalette::Highlight)};
    QColor m_backgroundColor{QGuiApplication::palette().color(QPalette::Window)};

    static constexpr int kMinRenderSamples = 64;
    static constexpr int kMaxRenderSamples = 8192;
    static constexpr qreal kSamplesPerPixel = 1.5;
    static constexpr int kLodSelectionOversample = 2;
    static constexpr int kLodBuildStopThreshold = 96;
    static constexpr double kMinZoom = 1.0;
    static constexpr double kMaxZoom = 64.0;
    static constexpr double kWheelZoomBase = 1.15;
    static constexpr double kMinQuickScrubFactor = 1.0;
    static constexpr double kMaxQuickScrubFactor = 32.0;
    static constexpr int kRepaintFpsLimit = 60;
    static constexpr int kMinRepaintIntervalMs = 1000 / kRepaintFpsLimit;
    static constexpr int kDirtyMarginPx = 3;
    static constexpr int kPanInertiaIntervalMs = 1000 / 60;
    static constexpr double kPanVelocitySmoothing = 0.35;
    static constexpr double kPanDecayPerSecond = 7.0;
    static constexpr double kPanStopVelocity = 0.003;
    static constexpr double kPanZoomBoostMax = 1.35;
    static constexpr double kPanSpeedBoostMax = 1.55;
    static constexpr int kPeakCacheResizeBucketPx = 8;
    static constexpr qsizetype kDefaultLayerCacheBudgetBytes = 8 * 1024 * 1024;
    static constexpr qsizetype kFullscreenLayerCacheBudgetBytes = 16 * 1024 * 1024;
};

#endif // WAVEFORMITEM_H
