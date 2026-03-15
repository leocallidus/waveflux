#include "WaveformItem.h"
#include "PerformanceProfiler.h"
#include "WaveformProvider.h"
#include <QGuiApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QWindow>
#include <QString>
#include <algorithm>
#include <cmath>

WaveformItem::WaveformItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAntialiasing(false);
    setOpaquePainting(true);
#ifdef Q_OS_WIN
    // FBO-backed QQuickPaintedItem is faster, but on Windows it proved prone
    // to stale frames when skins/layouts switch between visible waveform items.
    setRenderTarget(QQuickPaintedItem::Image);
#else
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
#endif

    m_repaintTimer.setSingleShot(true);
    connect(&m_repaintTimer, &QTimer::timeout, this, &WaveformItem::flushScheduledRepaint);
    m_repaintClock.start();

    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if (isVisible()) {
            forceFullRedraw();
        } else {
            releaseTransientCaches(true);
        }
    });
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *) {
        if (window()) {
            forceFullRedraw();
        } else {
            releaseTransientCaches(true);
        }
    });
    connect(this, &QQuickItem::widthChanged, this, [this]() {
        forceFullRedraw();
    });
    connect(this, &QQuickItem::heightChanged, this, [this]() {
        forceFullRedraw();
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
        switch (state) {
        case Qt::ApplicationActive:
            if (isVisible() && window()) {
                forceFullRedraw();
            }
            break;
        case Qt::ApplicationHidden:
        case Qt::ApplicationSuspended:
            releaseTransientCaches(true);
            break;
        case Qt::ApplicationInactive:
            releaseTransientCaches(false);
            break;
        }
    });

    m_panInertiaTimer.setInterval(kPanInertiaIntervalMs);
    connect(&m_panInertiaTimer, &QTimer::timeout, this, [this]() {
        if (m_zoom <= 1.0 || std::abs(m_panVelocity) < kPanStopVelocity) {
            stopPanInertia();
            return;
        }

        if (!m_panClock.isValid()) {
            m_panClock.start();
            m_lastPanTickMs = 0;
        }

        const qint64 nowMs = m_panClock.elapsed();
        const qint64 dtMs = std::max<qint64>(1, nowMs - m_lastPanTickMs);
        m_lastPanTickMs = nowMs;
        const double dtSec = static_cast<double>(dtMs) / 1000.0;

        const double previousCenter = m_viewCenter;
        setViewCenter(m_viewCenter + m_panVelocity * dtSec);
        if (qFuzzyCompare(previousCenter, m_viewCenter)) {
            // Hit boundary while inertia still pushes outwards.
            m_panVelocity *= 0.2;
        }

        m_panVelocity *= std::exp(-kPanDecayPerSecond * dtSec);
        if (std::abs(m_panVelocity) < kPanStopVelocity) {
            stopPanInertia();
        }
    });
}

WaveformItem::ViewportRange WaveformItem::viewport() const
{
    const double clampedZoom = std::clamp(m_zoom, kMinZoom, kMaxZoom);
    const double span = 1.0 / clampedZoom;
    if (span >= 1.0) {
        return {0.0, 1.0};
    }

    const double maxStart = 1.0 - span;
    const double center = std::clamp(m_viewCenter, 0.0, 1.0);
    const double start = std::clamp(center - span * 0.5, 0.0, maxStart);
    return {start, span};
}

double WaveformItem::pixelToTrackPosition(qreal x) const
{
    const double w = std::max(1.0, static_cast<double>(width()));
    const double local = std::clamp(static_cast<double>(x) / w, 0.0, 1.0);
    const ViewportRange vp = viewport();
    return std::clamp(vp.start + local * vp.span, 0.0, 1.0);
}

int WaveformItem::trackPositionToPixel(double position, int widthPx) const
{
    if (widthPx <= 0) {
        return 0;
    }

    const ViewportRange vp = viewport();
    const double normalized = (position - vp.start) / vp.span;
    const int x = static_cast<int>(std::lround(normalized * static_cast<double>(widthPx)));
    return std::clamp(x, 0, widthPx);
}

void WaveformItem::setQuickScrubActive(bool active)
{
    if (m_quickScrubActive == active) {
        return;
    }
    m_quickScrubActive = active;
    emit quickScrubActiveChanged();
}

QRectF WaveformItem::itemBoundsRect() const
{
    return QRectF(0.0, 0.0, width(), height());
}

QRectF WaveformItem::progressDirtyRect(int oldPixel, int newPixel) const
{
    const int w = static_cast<int>(width());
    const int h = static_cast<int>(height());
    if (w <= 0 || h <= 0) {
        return {};
    }
    if (oldPixel < 0 || newPixel < 0) {
        return itemBoundsRect();
    }

    int left = std::max(0, std::min(oldPixel, newPixel) - kDirtyMarginPx);
    int right = std::min(w, std::max(oldPixel, newPixel) + kDirtyMarginPx + 1);
    if (right <= left) {
        right = std::min(w, left + 1);
    }
    return QRectF(left, 0, right - left, h);
}

QRectF WaveformItem::generationDirtyRect(int oldPixel, int newPixel) const
{
    QRectF dirty = progressDirtyRect(oldPixel, newPixel);
    if (dirty.isEmpty()) {
        return itemBoundsRect();
    }

    const qreal w = width();
    const qreal h = height();
    if (w <= 0.0 || h <= 0.0) {
        return dirty;
    }

    const qreal labelWidth = std::min<qreal>(150.0, w);
    const qreal labelHeight = std::min<qreal>(24.0, h);
    const QRectF labelRect(std::max<qreal>(0.0, w - labelWidth - 8.0), 0.0, labelWidth, labelHeight);
    return dirty.united(labelRect);
}

void WaveformItem::requestRepaint(const QRectF &dirtyRect)
{
    const QRectF bounds = itemBoundsRect();
    if (bounds.isEmpty()) {
        return;
    }

    bool requestIsFull = false;
    QRectF effectiveDirtyRect;

    if (!dirtyRect.isValid() || dirtyRect.isEmpty()) {
        m_pendingFullRepaint = true;
        requestIsFull = true;
        effectiveDirtyRect = bounds;
    } else if (!m_pendingFullRepaint) {
        const QRectF clipped = dirtyRect.intersected(bounds);
        if (!clipped.isEmpty()) {
            if (m_hasPendingDirtyRect) {
                m_pendingDirtyRect = m_pendingDirtyRect.united(clipped);
            } else {
                m_pendingDirtyRect = clipped;
                m_hasPendingDirtyRect = true;
            }
            effectiveDirtyRect = clipped;
        }
    }

    if (m_pendingFullRepaint) {
        requestIsFull = true;
        effectiveDirtyRect = bounds;
    }

    if (!m_pendingFullRepaint && !m_hasPendingDirtyRect) {
        return;
    }

    if (PerformanceProfiler *profiler = PerformanceProfiler::instance();
        profiler && profiler->enabled()) {
        const qreal fullArea = bounds.width() * bounds.height();
        const qreal dirtyArea = requestIsFull
            ? fullArea
            : (effectiveDirtyRect.width() * effectiveDirtyRect.height());
        profiler->recordWaveformRepaintRequest(requestIsFull, dirtyArea, fullArea);
    }

    if (!m_repaintClock.isValid()) {
        m_repaintClock.start();
    }

    const qint64 elapsed = m_repaintClock.elapsed();
    if (!m_repaintTimer.isActive() && elapsed >= kMinRepaintIntervalMs) {
        flushScheduledRepaint();
        return;
    }

    if (!m_repaintTimer.isActive()) {
        const int remaining = std::max(1, kMinRepaintIntervalMs - static_cast<int>(elapsed));
        m_repaintTimer.start(remaining);
    }
}

void WaveformItem::flushScheduledRepaint()
{
    if (!m_pendingFullRepaint && !m_hasPendingDirtyRect) {
        return;
    }

    if (m_pendingFullRepaint || !m_hasPendingDirtyRect) {
        update();
    } else {
        update(m_pendingDirtyRect.toAlignedRect());
    }

    m_pendingDirtyRect = QRectF();
    m_hasPendingDirtyRect = false;
    m_pendingFullRepaint = false;
    m_repaintClock.restart();
}

void WaveformItem::paint(QPainter *painter)
{
    const int w = static_cast<int>(width());
    const int h = static_cast<int>(height());
    
    if (w <= 0 || h <= 0) return;

    PerformanceProfiler *profiler = PerformanceProfiler::instance();
    const bool profilePaint = profiler && profiler->enabled();
    QElapsedTimer paintTimer;
    bool paintRecorded = false;
    if (profilePaint) {
        paintTimer.start();
    }
    const auto recordPaint = [&]() {
        if (!profilePaint || paintRecorded) {
            return;
        }
        profiler->recordWaveformPaint(paintTimer.nsecsElapsed());
        paintRecorded = true;
    };
    
    // Fill background
    painter->fillRect(0, 0, w, h, m_backgroundColor);
    
    bool waveGeometryChanged = false;

    // Update cached peaks/path when geometry changes.
    // For rapid resize drags, quantize width updates to reduce expensive peak recache churn.
    const int peakCacheWidthBucket = std::max(1, (w + kPeakCacheResizeBucketPx - 1) / kPeakCacheResizeBucketPx);
    if (peakCacheWidthBucket != m_lastPeakCacheWidthBucket) {
        updateCachedPeaks();
        m_lastPeakCacheWidthBucket = peakCacheWidthBucket;
        m_lastHeight = 0; // force path rebuild
        waveGeometryChanged = true;
    }

    if (w != m_lastWidth) {
        m_lastWidth = w;
        m_lastHeight = 0; // force path rebuild
        waveGeometryChanged = true;
    }

    if (h != m_lastHeight) {
        rebuildWavePath(w, h);
        m_lastHeight = h;
        waveGeometryChanged = true;
    }

    if (waveGeometryChanged) {
        invalidateWaveLayers();
    }

    if (m_cachedPeaks.isEmpty() || m_cachedWavePath.isEmpty()) {
        m_cachedUnplayedLayer = QImage();
        m_cachedPlayedLayer = QImage();
        // No waveform data
        painter->setPen(m_waveformColor.lighter(150));
        const QString label = m_loading
            ? QStringLiteral("Waveform %1%").arg(qRound(m_generationProgress * 100.0))
            : QStringLiteral("Drop audio file here");
        painter->drawText(QRectF(0, 0, w, h), Qt::AlignCenter, label);

        if (m_loading) {
            const QRectF barBg(8.0, h - 10.0, std::max(0, w - 16), 4.0);
            const QRectF barFill(barBg.x(), barBg.y(), barBg.width() * std::clamp(m_generationProgress, 0.0, 1.0), barBg.height());
            painter->fillRect(barBg, m_waveformColor.darker(160));
            painter->fillRect(barFill, m_progressColor);
        }
        recordPaint();
        return;
    }
    
    painter->setRenderHint(QPainter::Antialiasing, false);
    
    const int progressX = trackPositionToPixel(m_progress, w);
    const int generatedEnd = m_loading
        ? std::clamp(trackPositionToPixel(m_generationProgress, w), 0, w)
        : w;
    const int playedEnd = std::clamp(progressX, 0, generatedEnd);
    const QRectF playedRect(0, 0, qMax(0, playedEnd), h);
    const QRectF unplayedRect(playedEnd, 0, qMax(0, generatedEnd - playedEnd), h);

    if (m_waveLayersDirty ||
        (!m_cachedUnplayedLayer.isNull() &&
         (m_cachedUnplayedLayer.width() != w || m_cachedUnplayedLayer.height() != h)) ||
        (!m_cachedPlayedLayer.isNull() &&
         (m_cachedPlayedLayer.width() != w || m_cachedPlayedLayer.height() != h))) {
        rebuildWaveLayers(w, h);
    }

    const auto drawLayerSegment = [&](const QRectF &clipRect, const QImage &layer) {
        if (clipRect.width() <= 0.0 || clipRect.height() <= 0.0 || layer.isNull()) {
            return;
        }
        const QRect target = clipRect.toAlignedRect().intersected(QRect(0, 0, w, h));
        if (target.isEmpty()) {
            return;
        }
        painter->drawImage(target, layer, target);
    };

    if (!m_cachedUnplayedLayer.isNull() && !m_cachedPlayedLayer.isNull()) {
        drawLayerSegment(unplayedRect, m_cachedUnplayedLayer);
        drawLayerSegment(playedRect, m_cachedPlayedLayer);
    } else {
        const auto drawWaveSegment = [&](const QRectF &clipRect, const QColor &fill, const QColor &stroke) {
            if (clipRect.width() <= 0.0 || clipRect.height() <= 0.0 || m_cachedWavePath.isEmpty()) {
                return;
            }
            painter->save();
            painter->setClipRect(clipRect);
            painter->setBrush(fill);
            QPen pen(stroke);
            pen.setWidthF(1.0);
            painter->setPen(pen);
            painter->drawPath(m_cachedWavePath);
            painter->restore();
        };

        QColor unplayedFill = m_waveformColor;
        unplayedFill.setAlpha(90);
        QColor unplayedStroke = m_waveformColor;
        unplayedStroke.setAlpha(165);

        QColor playedFill = m_progressColor;
        playedFill.setAlpha(210);
        QColor playedStroke = m_progressColor.lighter(112);
        playedStroke.setAlpha(235);

        drawWaveSegment(unplayedRect, unplayedFill, unplayedStroke);
        drawWaveSegment(playedRect, playedFill, playedStroke);
    }

    if (m_loading) {
        const int generationX = generatedEnd;
        const QColor pendingOverlay = QColor(m_backgroundColor.red(), m_backgroundColor.green(),
                                             m_backgroundColor.blue(), 130);
        painter->fillRect(QRectF(generationX, 0, std::max(0, w - generationX), h), pendingOverlay);

        QPen scanPen(m_progressColor);
        scanPen.setWidth(1);
        painter->setPen(scanPen);
        painter->drawLine(generationX, 0, generationX, h);

        painter->setPen(m_progressColor.lighter(120));
        painter->drawText(QRectF(8, 0, w - 16, h), Qt::AlignTop | Qt::AlignRight,
                          QStringLiteral("%1%").arg(qRound(m_generationProgress * 100.0)));
    }

    recordPaint();
}

void WaveformItem::setPeaks(const QVector<float> &peaks)
{
    if (m_peaks != peaks) {
        m_peaks = peaks;
        refreshSourceData(true);
        emit peaksChanged();
    }
}

void WaveformItem::setProvider(QObject *provider)
{
    auto *nextProvider = qobject_cast<WaveformProvider *>(provider);
    if (m_provider == nextProvider) {
        return;
    }

    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
    }

    m_provider = nextProvider;
    if (m_provider) {
        connect(m_provider, &QObject::destroyed, this, [this]() {
            m_provider = nullptr;
            refreshSourceData(false);
            emit providerChanged();
            emit peaksChanged();
        });
        connect(m_provider, &WaveformProvider::peaksReady, this, [this]() {
            refreshSourceData(isVisible() && window());
            emit peaksChanged();
        });
    }

    refreshSourceData(isVisible() && window());
    emit providerChanged();
    emit peaksChanged();
}

void WaveformItem::setProgress(double progress)
{
    progress = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_progress, progress)) {
        return;
    }

    const int w = static_cast<int>(width());
    const int oldProgressPixel = w > 0 ? trackPositionToPixel(m_progress, w) : m_lastProgressPixel;

    m_progress = progress;
    emit progressChanged();

    const int newProgressPixel = w > 0 ? trackPositionToPixel(m_progress, w) : -1;
    const int previousPixel = (m_lastProgressPixel >= 0) ? m_lastProgressPixel : oldProgressPixel;
    if (newProgressPixel == previousPixel) {
        return;
    }
    m_lastProgressPixel = newProgressPixel;
    requestRepaint(progressDirtyRect(previousPixel, newProgressPixel));
}

void WaveformItem::setWaveformColor(const QColor &color)
{
    if (m_waveformColor != color) {
        m_waveformColor = color;
        invalidateWaveLayers();
        requestRepaint();
        emit waveformColorChanged();
    }
}

void WaveformItem::setProgressColor(const QColor &color)
{
    if (m_progressColor != color) {
        m_progressColor = color;
        invalidateWaveLayers();
        requestRepaint();
        emit progressColorChanged();
    }
}

void WaveformItem::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        requestRepaint();
        emit backgroundColorChanged();
    }
}

void WaveformItem::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        requestRepaint();
        emit loadingChanged();
    }
}

void WaveformItem::setGenerationProgress(double progress)
{
    progress = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_generationProgress, progress)) {
        return;
    }

    const int w = static_cast<int>(width());
    const int oldGenerationPixel = w > 0 ? trackPositionToPixel(m_generationProgress, w) : -1;

    m_generationProgress = progress;
    const int newGenerationPixel = w > 0 ? trackPositionToPixel(m_generationProgress, w) : -1;
    requestRepaint(generationDirtyRect(oldGenerationPixel, newGenerationPixel));
    emit generationProgressChanged();
}

void WaveformItem::setZoom(double zoom)
{
    const double clampedZoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(m_zoom, clampedZoom)) {
        return;
    }

    m_zoom = clampedZoom;
    if (m_zoom <= 1.0) {
        m_draggingPan = false;
        stopPanInertia();
    }

    const ViewportRange vp = viewport();
    const double clampedCenter = vp.start + vp.span * 0.5;
    const bool centerChanged = !qFuzzyCompare(m_viewCenter, clampedCenter);
    m_viewCenter = clampedCenter;

    invalidateWaveLayers();
    m_lastPeakCacheWidthBucket = -1;
    m_lastWidth = 0;
    m_lastHeight = 0;
    m_lastProgressPixel = -1;
    requestRepaint();
    emit zoomChanged();
    if (centerChanged) {
        emit viewCenterChanged();
    }
}

void WaveformItem::stopPanInertia()
{
    if (m_panInertiaTimer.isActive()) {
        m_panInertiaTimer.stop();
    }
    m_panVelocity = 0.0;
    m_lastPanTickMs = 0;
}

void WaveformItem::startPanInertiaIfNeeded()
{
    if (m_zoom <= 1.0 || std::abs(m_panVelocity) < kPanStopVelocity) {
        stopPanInertia();
        return;
    }

    if (!m_panClock.isValid()) {
        m_panClock.start();
    }
    m_lastPanTickMs = m_panClock.elapsed();
    if (!m_panInertiaTimer.isActive()) {
        m_panInertiaTimer.start();
    }
}

void WaveformItem::setViewCenter(double center)
{
    const double clampedInput = std::clamp(center, 0.0, 1.0);
    const double span = 1.0 / std::clamp(m_zoom, kMinZoom, kMaxZoom);
    const double halfSpan = span * 0.5;
    const double clampedCenter = (span >= 1.0)
        ? 0.5
        : std::clamp(clampedInput, halfSpan, 1.0 - halfSpan);

    if (qFuzzyCompare(m_viewCenter, clampedCenter)) {
        return;
    }

    m_viewCenter = clampedCenter;
    invalidateWaveLayers();
    m_lastPeakCacheWidthBucket = -1;
    m_lastWidth = 0;
    m_lastHeight = 0;
    m_lastProgressPixel = -1;
    requestRepaint();
    emit viewCenterChanged();
}

void WaveformItem::setQuickScrubEnabled(bool enabled)
{
    if (m_quickScrubEnabled == enabled) {
        return;
    }
    m_quickScrubEnabled = enabled;
    if (!m_quickScrubEnabled) {
        setQuickScrubActive(false);
    }
    emit quickScrubEnabledChanged();
}

void WaveformItem::setQuickScrubFactor(double factor)
{
    const double clamped = std::clamp(factor, kMinQuickScrubFactor, kMaxQuickScrubFactor);
    if (qFuzzyCompare(m_quickScrubFactor, clamped)) {
        return;
    }
    m_quickScrubFactor = clamped;
    emit quickScrubFactorChanged();
}

void WaveformItem::resetZoom()
{
    setZoom(1.0);
    setViewCenter(0.5);
}

void WaveformItem::zoomAround(double newZoom, double anchorX)
{
    const double clampedAnchor = std::clamp(anchorX, 0.0, 1.0);
    const ViewportRange oldVp = viewport();
    const double anchorTrackPos = oldVp.start + clampedAnchor * oldVp.span;

    setZoom(newZoom);

    const double newSpan = 1.0 / std::clamp(m_zoom, kMinZoom, kMaxZoom);
    const double maxStart = std::max(0.0, 1.0 - newSpan);
    const double newStart = std::clamp(anchorTrackPos - clampedAnchor * newSpan, 0.0, maxStart);
    setViewCenter(newStart + newSpan * 0.5);
}

double WaveformItem::viewToTrack(double normalizedX) const
{
    const ViewportRange vp = viewport();
    const double clampedX = std::clamp(normalizedX, 0.0, 1.0);
    return std::clamp(vp.start + clampedX * vp.span, 0.0, 1.0);
}

double WaveformItem::trackToView(double trackPosition) const
{
    const ViewportRange vp = viewport();
    const double clampedPos = std::clamp(trackPosition, 0.0, 1.0);
    const double view = (clampedPos - vp.start) / vp.span;
    return std::clamp(view, 0.0, 1.0);
}

void WaveformItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && width() > 0 && m_zoom > 1.0) {
        stopPanInertia();
        m_draggingPan = true;
        m_lastPanX = event->position().x();
        if (!m_panClock.isValid()) {
            m_panClock.start();
        }
        m_lastPanTickMs = m_panClock.elapsed();
        m_draggingSeek = false;
        setQuickScrubActive(false);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && width() > 0) {
        stopPanInertia();
        m_draggingSeek = true;
        m_draggingPan = false;

        const bool quick = m_quickScrubEnabled && (event->modifiers() & Qt::ShiftModifier);
        setQuickScrubActive(quick);

        const double seekPos = pixelToTrackPosition(event->position().x());
        emit seekRequested(seekPos);

        if (m_quickScrubActive) {
            m_lastQuickScrubX = event->position().x();
            m_lastQuickScrubPosition = seekPos;
            if (m_zoom > 1.0) {
                setViewCenter(seekPos);
            }
        }
        event->accept();
        return;
    }

    event->ignore();
}

void WaveformItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingPan && (event->buttons() & Qt::RightButton) && width() > 0) {
        const double widthPx = std::max(1.0, static_cast<double>(width()));
        const double deltaX = static_cast<double>(event->position().x() - m_lastPanX);
        m_lastPanX = event->position().x();

        if (!m_panClock.isValid()) {
            m_panClock.start();
            m_lastPanTickMs = 0;
        }
        const qint64 nowMs = m_panClock.elapsed();
        const qint64 dtMs = std::max<qint64>(1, nowMs - m_lastPanTickMs);
        m_lastPanTickMs = nowMs;

        // Pan the zoomed viewport horizontally with RMB drag.
        const ViewportRange vp = viewport();
        const double zoomT = std::clamp((m_zoom - 1.0) / (kMaxZoom - 1.0), 0.0, 1.0);
        const double zoomBoost = 1.0 + zoomT * (kPanZoomBoostMax - 1.0);
        const double speedPxPerMs = std::abs(deltaX) / static_cast<double>(dtMs);
        const double speedT = std::clamp(speedPxPerMs / 1.2, 0.0, 1.0);
        const double speedBoost = 1.0 + speedT * (kPanSpeedBoostMax - 1.0);
        const double deltaCenter = (deltaX / widthPx) * vp.span * zoomBoost * speedBoost;
        setViewCenter(m_viewCenter + deltaCenter);

        const double dtSec = static_cast<double>(dtMs) / 1000.0;
        if (dtSec > 0.0) {
            const double instantVelocity = deltaCenter / dtSec;
            m_panVelocity = (1.0 - kPanVelocitySmoothing) * m_panVelocity
                + kPanVelocitySmoothing * instantVelocity;
        }
        event->accept();
        return;
    }

    if (event->buttons() & Qt::LeftButton && width() > 0) {
        if (!m_draggingSeek) {
            m_draggingSeek = true;
        }

        const bool quickNow = m_quickScrubEnabled && (event->modifiers() & Qt::ShiftModifier);
        if (quickNow != m_quickScrubActive) {
            setQuickScrubActive(quickNow);
            m_lastQuickScrubX = event->position().x();
            m_lastQuickScrubPosition = pixelToTrackPosition(event->position().x());
        }

        if (m_quickScrubActive) {
            const ViewportRange vp = viewport();
            const double deltaX = static_cast<double>(event->position().x() - m_lastQuickScrubX);
            const double widthPx = std::max(1.0, static_cast<double>(width()));
            const double deltaPos = (deltaX / widthPx) * (vp.span / std::max(m_quickScrubFactor, 1.0));
            const double seekPos = std::clamp(m_lastQuickScrubPosition + deltaPos, 0.0, 1.0);

            m_lastQuickScrubX = event->position().x();
            m_lastQuickScrubPosition = seekPos;
            emit seekRequested(seekPos);

            if (m_zoom > 1.0) {
                setViewCenter(seekPos);
            }
            event->accept();
            return;
        }

        const double seekPos = pixelToTrackPosition(event->position().x());
        emit seekRequested(seekPos);
        event->accept();
        return;
    }

    event->ignore();
}

void WaveformItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        m_draggingPan = false;
        startPanInertiaIfNeeded();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_draggingSeek = false;
        setQuickScrubActive(false);
        event->accept();
        return;
    }

    event->ignore();
}

void WaveformItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        resetZoom();
    }
}

void WaveformItem::wheelEvent(QWheelEvent *event)
{
    const QPoint angleDelta = event->angleDelta();
    if (width() <= 0 || angleDelta.y() == 0) {
        event->ignore();
        return;
    }

    const double steps = static_cast<double>(angleDelta.y()) / 120.0;
    const double targetZoom = m_zoom * std::pow(kWheelZoomBase, steps);
    const double anchorX = static_cast<double>(event->position().x()) / std::max(1.0, static_cast<double>(width()));
    zoomAround(targetZoom, anchorX);
    event->accept();
}

void WaveformItem::rebuildLodLevels()
{
    m_lodLevels.clear();

    const QVector<float> &peaks = sourcePeaks();
    if (peaks.isEmpty()) {
        return;
    }

    const QVector<float> *source = &peaks;
    while (source->size() > kLodBuildStopThreshold) {
        const int sourceSize = static_cast<int>(source->size());
        QVector<float> next;
        next.resize((sourceSize + 1) / 2);

        for (int i = 0; i < next.size(); ++i) {
            const int firstIndex = i * 2;
            const int secondIndex = std::min(firstIndex + 1, sourceSize - 1);
            next[i] = std::max((*source)[firstIndex], (*source)[secondIndex]);
        }

        if (next.size() == source->size()) {
            break;
        }

        LodLevel level;
        level.peaks = std::move(next);
        m_lodLevels.push_back(std::move(level));
        source = &m_lodLevels.back().peaks;
    }
}

int WaveformItem::effectiveTargetSamples(int widthPx) const
{
    if (widthPx <= 0) {
        return 0;
    }

    qreal dpr = 1.0;
    if (const QQuickWindow *win = window()) {
        dpr = std::max<qreal>(1.0, win->devicePixelRatio());
    }

    const int desired = static_cast<int>(std::lround(widthPx * dpr * kSamplesPerPixel));
    return std::clamp(desired, kMinRenderSamples, kMaxRenderSamples);
}

QVector<float> WaveformItem::downsampleMaxRange(const QVector<float> &input,
                                                int startIndex,
                                                int endIndexExclusive,
                                                int targetSamples)
{
    const int inputSize = static_cast<int>(input.size());
    const int clampedStart = std::clamp(startIndex, 0, std::max(0, inputSize - 1));
    const int clampedEnd = std::clamp(endIndexExclusive, clampedStart + 1, inputSize);
    const int rangeSize = clampedEnd - clampedStart;

    if (rangeSize <= 0 || targetSamples <= 0) {
        return {};
    }
    if (rangeSize <= targetSamples) {
        return input.mid(clampedStart, rangeSize);
    }

    QVector<float> output(targetSamples);
    const double ratio = static_cast<double>(rangeSize) / static_cast<double>(targetSamples);
    for (int i = 0; i < targetSamples; ++i) {
        const int binStart = clampedStart + static_cast<int>(std::floor(i * ratio));
        int binEnd = clampedStart + static_cast<int>(std::floor((i + 1) * ratio));
        binEnd = std::clamp(binEnd, binStart + 1, clampedEnd);

        float maxPeak = 0.0f;
        for (int j = binStart; j < binEnd; ++j) {
            maxPeak = std::max(maxPeak, input[j]);
        }
        output[i] = maxPeak;
    }

    return output;
}

void WaveformItem::updateCachedPeaks()
{
    const int widthPx = static_cast<int>(width());
    const QVector<float> &peaks = sourcePeaks();

    if (peaks.isEmpty() || widthPx <= 0) {
        QVector<float>().swap(m_cachedPeaks);
        return;
    }

    const int targetSamples = effectiveTargetSamples(widthPx);
    if (targetSamples <= 0) {
        QVector<float>().swap(m_cachedPeaks);
        return;
    }

    const int preferredSourceSamples = targetSamples * kLodSelectionOversample;
    const QVector<float> *source = &peaks;

    // Pick the first level that is close enough to the render budget.
    // This keeps detail while avoiding expensive per-frame paths.
    for (const LodLevel &level : m_lodLevels) {
        if (level.peaks.size() <= preferredSourceSamples) {
            source = &level.peaks;
            break;
        }
    }

    const int sourceSize = static_cast<int>(source->size());
    if (sourceSize <= 0) {
        QVector<float>().swap(m_cachedPeaks);
        return;
    }

    const ViewportRange vp = viewport();
    const int startIndex = std::clamp(static_cast<int>(std::floor(vp.start * sourceSize)), 0, sourceSize - 1);
    const int endIndex = std::clamp(static_cast<int>(std::ceil((vp.start + vp.span) * sourceSize)),
                                    startIndex + 1, sourceSize);
    m_cachedPeaks = downsampleMaxRange(*source, startIndex, endIndex, targetSamples);
}

void WaveformItem::rebuildWavePath(int fullWidth, int h)
{
    m_cachedWavePath = QPainterPath();
    if (m_cachedPeaks.isEmpty() || fullWidth <= 1 || h <= 1) {
        return;
    }

    const int count = m_cachedPeaks.size();
    if (count < 2) {
        return;
    }

    const int widthToUse = fullWidth;
    const qreal centerY = h * 0.5;
    const qreal maxHeight = (h * 0.5) * 0.88;

    QPainterPath path;
    path.moveTo(0, centerY);

    for (int i = 0; i < count; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(count - 1);
        const qreal x = t * (widthToUse - 1);
        const qreal peak = std::clamp<qreal>(m_cachedPeaks[i], 0.0, 1.0);
        path.lineTo(x, centerY - peak * maxHeight);
    }

    for (int i = count - 1; i >= 0; --i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(count - 1);
        const qreal x = t * (widthToUse - 1);
        const qreal peak = std::clamp<qreal>(m_cachedPeaks[i], 0.0, 1.0);
        path.lineTo(x, centerY + peak * maxHeight);
    }

    path.closeSubpath();
    m_cachedWavePath = path;
}

void WaveformItem::invalidateWaveLayers()
{
    m_waveLayersDirty = true;
}

const QVector<float> &WaveformItem::sourcePeaks() const
{
    if (m_provider) {
        return m_provider->peaksRef();
    }
    return m_peaks;
}

bool WaveformItem::usesProviderSource() const
{
    return m_provider != nullptr;
}

void WaveformItem::refreshSourceData(bool allowLodRebuild)
{
    if (!allowLodRebuild) {
        releaseTransientCaches(true);
        if (!isVisible()) {
            return;
        }
        requestRepaint();
        return;
    }

    rebuildLodLevels();
    releaseTransientCaches(false);
    requestRepaint();
}

void WaveformItem::releaseTransientCaches(bool releaseLodLevels)
{
    m_lastPeakCacheWidthBucket = -1;
    m_lastWidth = 0;
    m_lastHeight = 0;
    m_lastProgressPixel = -1;
    m_cachedWavePath = QPainterPath();
    m_cachedUnplayedLayer = QImage();
    m_cachedPlayedLayer = QImage();
    QVector<float>().swap(m_cachedPeaks);
    if (releaseLodLevels) {
        QVector<LodLevel>().swap(m_lodLevels);
    }
    invalidateWaveLayers();
}

void WaveformItem::forceFullRedraw()
{
    if (m_lodLevels.isEmpty() && !sourcePeaks().isEmpty()) {
        rebuildLodLevels();
    }
    releaseTransientCaches(false);
    requestRepaint();
}

qsizetype WaveformItem::layerCacheBudgetBytes() const
{
    const QQuickWindow *win = window();
    const bool fullscreen = win && win->visibility() == QWindow::FullScreen;
    return fullscreen ? kFullscreenLayerCacheBudgetBytes : kDefaultLayerCacheBudgetBytes;
}

bool WaveformItem::canCacheWaveLayers(int w, int h) const
{
    if (w <= 0 || h <= 0) {
        return false;
    }

    const qreal dpr = window() ? std::max<qreal>(1.0, window()->devicePixelRatio()) : 1.0;
    const qsizetype bytesPerLayer = static_cast<qsizetype>(
        std::ceil(static_cast<double>(w) * dpr) * std::ceil(static_cast<double>(h) * dpr) * 4.0);
    if (bytesPerLayer <= 0) {
        return false;
    }

    return bytesPerLayer * 2 <= layerCacheBudgetBytes();
}

void WaveformItem::rebuildWaveLayers(int w, int h)
{
    m_cachedUnplayedLayer = QImage();
    m_cachedPlayedLayer = QImage();
    m_waveLayersDirty = false;

    if (w <= 0 || h <= 0 || m_cachedWavePath.isEmpty()) {
        return;
    }

    if (!canCacheWaveLayers(w, h)) {
        return;
    }

    const auto renderLayer = [&](const QColor &fill, const QColor &stroke) {
        QImage layer(w, h, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);

        QPainter layerPainter(&layer);
        layerPainter.setRenderHint(QPainter::Antialiasing, false);
        layerPainter.setBrush(fill);
        QPen pen(stroke);
        pen.setWidthF(1.0);
        layerPainter.setPen(pen);
        layerPainter.drawPath(m_cachedWavePath);
        return layer;
    };

    QColor unplayedFill = m_waveformColor;
    unplayedFill.setAlpha(90);
    QColor unplayedStroke = m_waveformColor;
    unplayedStroke.setAlpha(165);

    QColor playedFill = m_progressColor;
    playedFill.setAlpha(210);
    QColor playedStroke = m_progressColor.lighter(112);
    playedStroke.setAlpha(235);

    m_cachedUnplayedLayer = renderLayer(unplayedFill, unplayedStroke);
    m_cachedPlayedLayer = renderLayer(playedFill, playedStroke);
}
