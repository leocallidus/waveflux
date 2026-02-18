#include "AudioEngine.h"
#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QDebug>
#include <QFileInfo>
#include <QtGlobal>
#include "DiagnosticsFlags.h"
#include <cmath>
#include <vector>
#include <gst/gstchildproxy.h>
#include <gst/audio/streamvolume.h>

namespace {
bool seekDiagEnabled()
{
    return DiagnosticsFlags::detailedDiagnosticsEnabled();
}

bool isLikelyWindowsAbsolutePath(const QString &path)
{
    return path.size() >= 3
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('\\') || path.at(2) == QLatin1Char('/'));
}

QString buildPlaybackUri(const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    if (isLikelyWindowsAbsolutePath(normalized)) {
        return QUrl::fromLocalFile(normalized).toString();
    }

    const QUrl parsed(normalized);
    if (parsed.isValid() && !parsed.scheme().isEmpty()) {
        if (parsed.isLocalFile()) {
            const QString localPath = parsed.toLocalFile().trimmed();
            if (localPath.isEmpty()) {
                return {};
            }
            return QUrl::fromLocalFile(localPath).toString();
        }
        // Keep network URI (http/https/...) unchanged.
        return normalized;
    }

    return QUrl::fromLocalFile(normalized).toString();
}

QString transitionTracePayload(const char *event,
                               quint64 transitionId,
                               const QVariantMap &extra)
{
    QVariantMap payload = extra;
    payload.insert(QStringLiteral("component"), QStringLiteral("AudioEngine"));
    payload.insert(QStringLiteral("event"), QString::fromLatin1(event));
    payload.insert(QStringLiteral("transitionId"), static_cast<qulonglong>(transitionId));
    payload.insert(QStringLiteral("tsMs"), QDateTime::currentMSecsSinceEpoch());
    return QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(payload)).toJson(QJsonDocument::Compact));
}

void traceTransition(const char *event, quint64 transitionId, const QVariantMap &extra = {})
{
    if (!DiagnosticsFlags::transitionTraceEnabled()) {
        return;
    }
    qInfo().noquote() << "[TransitionTrace]"
                      << transitionTracePayload(event, transitionId, extra);
}

bool hasElementProperty(GstElement *element, const char *propertyName)
{
    if (!element || !propertyName) {
        return false;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(element);
    return klass && g_object_class_find_property(klass, propertyName);
}

void setEnumPropertyIfAvailable(GstElement *element, const char *propertyName, const char *valueNick)
{
    if (!valueNick || !hasElementProperty(element, propertyName)) {
        return;
    }

    gst_util_set_object_arg(G_OBJECT(element), propertyName, valueNick);
}
} // namespace

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
    if (kForceReliableEosTransitions) {
        m_gaplessBypassMode = true;
        m_gaplessBypassReason = QStringLiteral("forced_reliable_eos_transition");
        traceTransition("audio_gapless_bypass_boot_enabled",
                        0,
                        {
                            {QStringLiteral("reason"), m_gaplessBypassReason}
                        });
    }

    m_spectrumLevels.reserve(m_spectrumBandCount);
    for (int i = 0; i < m_spectrumBandCount; ++i) {
        m_spectrumLevels.push_back(0.0);
    }

    static const int kDefaultEqFrequencies[] = {31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    constexpr int kDefaultEqBandCount = static_cast<int>(sizeof(kDefaultEqFrequencies) / sizeof(kDefaultEqFrequencies[0]));
    m_equalizerBandCount = kDefaultEqBandCount;
    m_equalizerBandFrequencies.reserve(m_equalizerBandCount);
    m_equalizerBandGains.reserve(m_equalizerBandCount);
    m_equalizerAppliedBandGains.reserve(m_equalizerBandCount);
    for (int i = 0; i < m_equalizerBandCount; ++i) {
        m_equalizerBandFrequencies.push_back(kDefaultEqFrequencies[i]);
        m_equalizerBandGains.push_back(0.0);
        m_equalizerAppliedBandGains.push_back(0.0);
    }

    setupPipeline();
    
    // Setup position update timer
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(160); // Lower UI churn for smoother rendering on weaker GPUs/CPUs
    connect(m_positionTimer, &QTimer::timeout, this, &AudioEngine::updatePosition);

    m_gaplessEosDeferralTimer.setSingleShot(true);
    connect(&m_gaplessEosDeferralTimer, &QTimer::timeout, this, [this]() {
        if (m_gaplessPendingFile.isEmpty()) {
            m_gaplessPendingSinceWallClockMs = 0;
            return;
        }

        // Gapless handoff did not materialize quickly enough; treat as real EOS so the
        // PlaybackController can advance the playlist (manual fallback).
        const QString pendingFile = m_gaplessPendingFile;
        const quint64 pendingTransitionId = m_gaplessPendingTransitionId;
        m_gaplessPendingFile.clear();
        m_gaplessPendingTransitionId = 0;
        m_gaplessPendingSinceWallClockMs = 0;
        m_lastEndOfStreamTransitionId = m_currentTransitionId;
        traceTransition("audio_gapless_eos_deferral_timeout",
                        m_lastEndOfStreamTransitionId,
                        {
                            {QStringLiteral("pendingFile"), pendingFile},
                            {QStringLiteral("pendingTransitionId"),
                             static_cast<qulonglong>(pendingTransitionId)}
                        });

        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] gapless EOS deferral timed out; emitting endOfStream"
                << "pendingFile=" << pendingFile
                << "activeTransitionId=" << m_currentTransitionId
                << "pendingTransitionId=" << pendingTransitionId;
        }

        setState(EndedState);
        emit endOfStream();
    });

    m_seekCoalesceTimer.setSingleShot(true);
    connect(&m_seekCoalesceTimer, &QTimer::timeout, this, [this]() {
        if (m_coalescedSeekPositionMs < 0) {
            return;
        }
        const qint64 pendingSeek = m_coalescedSeekPositionMs;
        m_coalescedSeekPositionMs = -1;
        performSeek(pendingSeek);
    });

    m_equalizerApplyTimer.setSingleShot(true);
    connect(&m_equalizerApplyTimer, &QTimer::timeout, this, [this]() {
        applyEqualizerBandSettings();
    });

    m_equalizerRampTimer.setInterval(kEqualizerRampStepMs);
    connect(&m_equalizerRampTimer, &QTimer::timeout, this, [this]() {
        processEqualizerRampStep();
    });
}

AudioEngine::~AudioEngine()
{
    teardownPipeline();
}

void AudioEngine::setupPipeline()
{
    m_resampleElement = nullptr;
    m_replayGainElement = nullptr;
    m_dynamicRangeElement = nullptr;
    m_limiterElement = nullptr;
    m_outputConvertElement = nullptr;

    // Create playbin3 element for audio playback
    // playbin3 is the modern GStreamer element that handles:
    // - Automatic format detection and decoding
    // - Gapless playback via about-to-finish signal
    // - Audio sink selection
    m_pipeline = gst_element_factory_make("playbin3", "waveflux-player");
    
    if (!m_pipeline) {
        // Fallback to playbin if playbin3 is not available
        m_pipeline = gst_element_factory_make("playbin", "waveflux-player");
    }
    
    if (!m_pipeline) {
        qCritical() << "Failed to create GStreamer playbin element";
        emit error("Failed to initialize audio engine");
        return;
    }
    
    // Create custom audio sink with a high-quality DSP chain.
    // Pipeline:
    // audioconvert -> audioresample(HQ) -> capsfilter(F32LE) -> rgvolume -> EQ ->
    // pitch -> audiodynamic(soft) -> rglimiter -> audioconvert(dither) -> autoaudiosink
    GstElement *audioSinkBin = gst_bin_new("audio-sink-bin");
    GstElement *audioConvert1 = gst_element_factory_make("audioconvert", "audioconvert1");
    GstElement *audioResample = gst_element_factory_make("audioresample", "waveflux-resample");
    GstElement *processingCapsFilter = gst_element_factory_make("capsfilter", "waveflux-processing-caps");
    GstElement *replayGainElement = gst_element_factory_make("rgvolume", "waveflux-rgvolume");
    GstElement *equalizerElement = gst_element_factory_make("equalizer-nbands", "waveflux-eq");
    GstElement *pitchElement = gst_element_factory_make("pitch", "pitch");
    GstElement *dynamicElement = gst_element_factory_make("audiodynamic", "waveflux-drc");
    GstElement *limiterElement = gst_element_factory_make("rglimiter", "waveflux-limiter");
    GstElement *audioConvert2 = gst_element_factory_make("audioconvert", "audioconvert2");
    GstElement *audioSink = gst_element_factory_make("autoaudiosink", "audiosink");

    m_pitchElement = nullptr;
    m_equalizerElement = nullptr;
    bool equalizerAvailableNow = false;

    if (audioSinkBin && audioConvert1 && pitchElement && audioConvert2 && audioSink) {
        if (!audioResample) {
            qInfo() << "audioresample not available, using default resampling path";
        }

        if (processingCapsFilter) {
            GstCaps *processingCaps = gst_caps_new_simple("audio/x-raw",
                                                          "format", G_TYPE_STRING, "F32LE",
                                                          "layout", G_TYPE_STRING, "interleaved",
                                                          nullptr);
            g_object_set(processingCapsFilter, "caps", processingCaps, nullptr);
            gst_caps_unref(processingCaps);
        } else {
            qInfo() << "capsfilter not available, floating-point processing path not forced";
        }

        if (!replayGainElement) {
            qInfo() << "rgvolume not available, replaygain normalization disabled";
        }

        bool linkOk = false;
        if (equalizerElement) {
            m_equalizerElement = equalizerElement;
            g_object_set(m_equalizerElement, "num-bands", static_cast<guint>(m_equalizerBandCount), nullptr);
            syncEqualizerBandFrequenciesFromElement();
            applyEqualizerBandSettings();
        } else {
            qWarning() << "Failed to create equalizer-nbands element, EQ disabled";
        }

        if (!dynamicElement) {
            qInfo() << "audiodynamic not available, gentle dynamics processor disabled";
        }

        if (!limiterElement) {
            qInfo() << "rglimiter not available, peak limiter disabled";
        }

        m_resampleElement = audioResample;
        m_replayGainElement = replayGainElement;
        m_dynamicRangeElement = dynamicElement;
        m_limiterElement = limiterElement;
        m_outputConvertElement = audioConvert2;
        applyAudioQualityProfileToPipeline();

        std::vector<GstElement *> processingChain;
        processingChain.reserve(10);
        processingChain.push_back(audioConvert1);
        if (audioResample) {
            processingChain.push_back(audioResample);
        }
        if (processingCapsFilter) {
            processingChain.push_back(processingCapsFilter);
        }
        if (replayGainElement) {
            processingChain.push_back(replayGainElement);
        }
        if (m_equalizerElement) {
            processingChain.push_back(m_equalizerElement);
        }
        processingChain.push_back(pitchElement);
        if (dynamicElement) {
            processingChain.push_back(dynamicElement);
        }
        if (limiterElement) {
            processingChain.push_back(limiterElement);
        }
        processingChain.push_back(audioConvert2);
        processingChain.push_back(audioSink);

        for (GstElement *element : processingChain) {
            gst_bin_add(GST_BIN(audioSinkBin), element);
        }

        linkOk = true;
        for (std::size_t i = 0; i + 1 < processingChain.size(); ++i) {
            if (!gst_element_link(processingChain[i], processingChain[i + 1])) {
                linkOk = false;
                break;
            }
        }

        if (linkOk) {
            m_pitchElement = pitchElement;
            equalizerAvailableNow = (m_equalizerElement != nullptr);

            // Create ghost pad for the bin
            GstPad *sinkPad = gst_element_get_static_pad(audioConvert1, "sink");
            GstPad *ghostPad = gst_ghost_pad_new("sink", sinkPad);
            gst_pad_set_active(ghostPad, TRUE);
            gst_element_add_pad(audioSinkBin, ghostPad);
            gst_object_unref(sinkPad);

            // Set the custom audio sink on playbin
            g_object_set(m_pipeline, "audio-sink", audioSinkBin, nullptr);

            // Apply initial pitch (0 semitones = pitch 1.0)
            g_object_set(m_pitchElement, "pitch", 1.0, nullptr);
        } else {
            qWarning() << "Failed to link custom high-quality audio chain, falling back to base playback";
            m_equalizerElement = nullptr;
            m_resampleElement = nullptr;
            m_replayGainElement = nullptr;
            m_dynamicRangeElement = nullptr;
            m_limiterElement = nullptr;
            m_outputConvertElement = nullptr;
            gst_object_unref(audioSinkBin);
        }
    } else {
        qWarning() << "Failed to create custom high-quality audio chain, pitch/EQ disabled";
        m_resampleElement = nullptr;
        m_replayGainElement = nullptr;
        m_dynamicRangeElement = nullptr;
        m_limiterElement = nullptr;
        m_outputConvertElement = nullptr;
        if (audioSinkBin) gst_object_unref(audioSinkBin);
        if (audioConvert1) gst_object_unref(audioConvert1);
        if (audioResample) gst_object_unref(audioResample);
        if (processingCapsFilter) gst_object_unref(processingCapsFilter);
        if (replayGainElement) gst_object_unref(replayGainElement);
        if (equalizerElement) gst_object_unref(equalizerElement);
        if (pitchElement) gst_object_unref(pitchElement);
        if (dynamicElement) gst_object_unref(dynamicElement);
        if (limiterElement) gst_object_unref(limiterElement);
        if (audioConvert2) gst_object_unref(audioConvert2);
        if (audioSink) gst_object_unref(audioSink);
    }

    if (m_equalizerAvailable != equalizerAvailableNow) {
        m_equalizerAvailable = equalizerAvailableNow;
        if (!m_equalizerAvailable) {
            m_equalizerApplyTimer.stop();
            m_equalizerRampTimer.stop();
            m_equalizerPendingApplyAllowRamp = false;
            m_equalizerRampCurrentStep = 0;
            m_equalizerRampTotalSteps = 0;
            m_equalizerAppliedBandGains = m_equalizerBandGains;
        } else {
            m_equalizerAppliedBandGains = m_equalizerBandGains;
        }
        emit equalizerAvailableChanged();
    } else if (m_equalizerAvailable) {
        m_equalizerAppliedBandGains = m_equalizerBandGains;
    }

    m_spectrumElement = gst_element_factory_make("spectrum", "waveflux-spectrum");
    if (m_spectrumElement) {
        g_object_set(m_spectrumElement,
                     "bands", m_spectrumBandCount,
                     "threshold", -80,
                     "post-messages", m_spectrumEnabled,
                     "interval", static_cast<guint64>(80 * GST_MSECOND),
                     nullptr);
        g_object_set(m_pipeline, "audio-filter", m_spectrumElement, nullptr);
    } else {
        qWarning() << "Failed to create spectrum element, dynamic analyzer disabled";
    }
    
    // Set initial volume
    if (GST_IS_STREAM_VOLUME(m_pipeline)) {
        gst_stream_volume_set_volume(GST_STREAM_VOLUME(m_pipeline),
                                     GST_STREAM_VOLUME_FORMAT_LINEAR,
                                     m_volume);
    } else {
        g_object_set(m_pipeline, "volume", m_volume, nullptr);
    }
    
    // Connect the about-to-finish signal for gapless playback
    // This signal is emitted when the current track is about to end,
    // allowing us to queue the next track seamlessly
    g_signal_connect(m_pipeline, "about-to-finish",
                     G_CALLBACK(onAboutToFinish), this);
    
    // Setup bus for message handling
    m_bus = gst_element_get_bus(m_pipeline);
    m_busWatchId = gst_bus_add_watch(m_bus, busCallback, this);
}

void AudioEngine::teardownPipeline()
{
    m_equalizerApplyTimer.stop();
    m_equalizerRampTimer.stop();
    m_equalizerPendingApplyAllowRamp = false;
    m_equalizerRampCurrentStep = 0;
    m_equalizerRampTotalSteps = 0;

    if (m_pipeline) {
        g_signal_handlers_disconnect_by_func(m_pipeline,
                                             reinterpret_cast<gpointer>(onAboutToFinish),
                                             this);
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        
        if (m_busWatchId > 0) {
            g_source_remove(m_busWatchId);
            m_busWatchId = 0;
        }
        
        if (m_bus) {
            gst_object_unref(m_bus);
            m_bus = nullptr;
        }
        
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        m_pitchElement = nullptr;
        m_spectrumElement = nullptr;
        m_equalizerElement = nullptr;
        m_resampleElement = nullptr;
        m_replayGainElement = nullptr;
        m_dynamicRangeElement = nullptr;
        m_limiterElement = nullptr;
        m_outputConvertElement = nullptr;
    }
    ++m_callbackSerial;
}

void AudioEngine::play()
{
    if (!m_pipeline) return;

    GstState currentState = GST_STATE_NULL;
    GstState pendingState = GST_STATE_VOID_PENDING;
    gst_element_get_state(m_pipeline, &currentState, &pendingState, 0);
    const bool pipelineHasLoadedStream =
        (currentState != GST_STATE_NULL) || (pendingState != GST_STATE_VOID_PENDING);

    if (m_state == EndedState && pipelineHasLoadedStream) {
        if (m_reversePlayback) {
            const qint64 endPositionMs = duration();
            if (endPositionMs > 0) {
                seekWithSource(endPositionMs, QStringLiteral("audio.play.restart_reverse_end"));
            } else {
                m_pendingReverseStart = true;
                m_pendingRateApplication = true;
            }
        } else {
            seekWithSource(0, QStringLiteral("audio.play.restart_forward_start"));
        }
    }

    if (m_reversePlayback
        && (m_state == StoppedState || m_state == EndedState)
        && !m_currentFile.isEmpty()) {
        m_pendingReverseStart = true;
        m_pendingRateApplication = true;
    }
    
    if (GST_IS_STREAM_VOLUME(m_pipeline)) {
        gst_stream_volume_set_volume(GST_STREAM_VOLUME(m_pipeline),
                                     GST_STREAM_VOLUME_FORMAT_LINEAR,
                                     m_volume);
    } else {
        g_object_set(m_pipeline, "volume", m_volume, nullptr);
    }

    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "Failed to start playback";
        emit error("Failed to start playback");
        setState(ErrorState);
        return;
    }

    setState(PlayingState);
    m_positionTimer->start();
}

void AudioEngine::pause()
{
    if (!m_pipeline) return;
    
    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    setState(PausedState);
    m_positionTimer->stop();
}

void AudioEngine::stop()
{
    if (!m_pipeline) return;
    
    m_nextFile.clear();
    m_nextFileTransitionId = 0;
    m_gaplessPendingFile.clear();
    m_gaplessPendingTransitionId = 0;
    m_gaplessPendingSinceWallClockMs = 0;
    m_trackTimelineOffsetMs = 0;
    m_recentGaplessStreamStartMs = 0;
    m_gaplessProgressWatchStartedMs = 0;
    m_gaplessProgressWatchStartPositionMs = 0;
    m_gaplessProgressRecoveryApplied = false;
    m_gaplessProgressReloadIssued = false;
    m_gaplessRecalibrationResyncAttempts = 0;
    m_lastStableDurationMs = 0;
    m_lastStableDurationUpdateMs = 0;
    m_gaplessEosDeferralTimer.stop();
    m_pendingRateApplication = false;
    m_pendingReverseStart = false;
    m_seekCoalesceTimer.stop();
    m_equalizerApplyTimer.stop();
    m_equalizerRampTimer.stop();
    m_coalescedSeekPositionMs = -1;
    m_deferredSeekPositionMs = -1;
    m_lastSeekWallClockMs = 0;
    m_lastSeekTargetMs = -1;
    m_lastSeekSource.clear();
    m_pendingSeekSource.clear();
    ++m_callbackSerial;

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    setState(StoppedState);
    m_positionTimer->stop();
    m_lastEmittedPositionMs = 0;
    emit positionChanged(0);
    resetSpectrumLevels();
}

void AudioEngine::togglePlayPause()
{
    if (m_state == PlayingState) {
        pause();
    } else {
        play();
    }
}

void AudioEngine::seek(qint64 position)
{
    seekWithSource(position, QStringLiteral("direct"));
}

void AudioEngine::seekWithSource(qint64 position, const QString &source)
{
    if (!m_pipeline) return;

    QString seekSource = source.trimmed();
    if (seekSource.isEmpty()) {
        seekSource = QStringLiteral("direct");
    }
    m_pendingSeekSource = seekSource;

    // User/logic-initiated seek supersedes deferred reverse-start bootstrap.
    m_pendingReverseStart = false;
    m_pendingRateApplication = false;

    const qint64 requestedMs = position;
    qint64 targetMs = qMax<qint64>(0, position);
    const qint64 durationMs = duration();
    if (durationMs > 0) {
        const qint64 safeMaxMs = qMax<qint64>(0, durationMs - kSeekSafeEndMarginMs);
        targetMs = qMin(targetMs, safeMaxMs);
    }

    GstState currentState = GST_STATE_NULL;
    GstState pendingState = GST_STATE_VOID_PENDING;
    gst_element_get_state(m_pipeline, &currentState, &pendingState, 0);
    // A pending PLAYING transition from READY is still not seek-ready.
    // We only seek once the pipeline is actually PAUSED/PLAYING.
    const bool seekReady =
        (currentState == GST_STATE_PAUSED || currentState == GST_STATE_PLAYING);
    if (!seekReady) {
        m_deferredSeekPositionMs = targetMs;
        m_seekCoalesceTimer.stop();
        m_coalescedSeekPositionMs = -1;
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] seek deferred (pipeline not seek-ready)"
                << "source=" << seekSource
                << "file=" << m_currentFile
                << "requestedMs=" << requestedMs
                << "targetMs=" << targetMs
                << "durationMs=" << durationMs
                << "engineState=" << static_cast<int>(m_state)
                << "gstCurrentState=" << gst_element_state_get_name(currentState)
                << "gstPendingState=" << gst_element_state_get_name(pendingState);
        }
        return;
    }

    if (!m_seekCoalesceTimer.isActive()) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] seek immediate"
                << "source=" << seekSource
                << "file=" << m_currentFile
                << "requestedMs=" << requestedMs
                << "targetMs=" << targetMs
                << "durationMs=" << durationMs
                << "effectiveRate=" << effectivePlaybackRate()
                << "state=" << static_cast<int>(m_state);
        }
        performSeek(targetMs);
        m_seekCoalesceTimer.start(kSeekCoalesceIntervalMs);
        return;
    }

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] seek coalesced"
            << "source=" << seekSource
            << "file=" << m_currentFile
            << "requestedMs=" << requestedMs
            << "targetMs=" << targetMs
            << "durationMs=" << durationMs
            << "effectiveRate=" << effectivePlaybackRate()
            << "previousPendingMs=" << m_coalescedSeekPositionMs;
    }
    m_coalescedSeekPositionMs = targetMs;
}

void AudioEngine::applyDeferredSeekIfNeeded()
{
    if (!m_pipeline || m_deferredSeekPositionMs < 0) {
        return;
    }

    GstState currentState = GST_STATE_NULL;
    GstState pendingState = GST_STATE_VOID_PENDING;
    gst_element_get_state(m_pipeline, &currentState, &pendingState, 0);
    const bool seekReady =
        (currentState == GST_STATE_PAUSED || currentState == GST_STATE_PLAYING);
    if (!seekReady) {
        return;
    }

    const qint64 targetMs = m_deferredSeekPositionMs;
    m_deferredSeekPositionMs = -1;
    QString seekSource = m_pendingSeekSource.trimmed();
    if (seekSource.isEmpty()) {
        seekSource = QStringLiteral("audio.deferred_seek");
        m_pendingSeekSource = seekSource;
    }
    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] apply deferred seek"
            << "source=" << seekSource
            << "file=" << m_currentFile
            << "targetMs=" << targetMs
            << "gstCurrentState=" << gst_element_state_get_name(currentState)
            << "gstPendingState=" << gst_element_state_get_name(pendingState);
    }
    performSeek(targetMs);
}

void AudioEngine::performSeek(qint64 positionMs)
{
    if (!m_pipeline) {
        return;
    }

    QString seekSource = m_pendingSeekSource.trimmed();
    if (seekSource.isEmpty()) {
        seekSource = QStringLiteral("audio.internal");
        m_pendingSeekSource = seekSource;
    }

    GstState currentState = GST_STATE_NULL;
    GstState pendingState = GST_STATE_VOID_PENDING;
    gst_element_get_state(m_pipeline, &currentState, &pendingState, 0);
    const bool seekReady =
        (currentState == GST_STATE_PAUSED || currentState == GST_STATE_PLAYING);
    if (!seekReady) {
        const qint64 durationMsNow = duration();
        qint64 deferredTargetMs = qMax<qint64>(0, positionMs);
        if (durationMsNow > 0) {
            const qint64 safeMaxMs = qMax<qint64>(0, durationMsNow - kSeekSafeEndMarginMs);
            deferredTargetMs = qMin(deferredTargetMs, safeMaxMs);
        }
        m_deferredSeekPositionMs = deferredTargetMs;
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] performSeek deferred (not seek-ready)"
                << "source=" << seekSource
                << "file=" << m_currentFile
                << "targetMs=" << positionMs
                << "deferredTargetMs=" << deferredTargetMs
                << "gstCurrentState=" << gst_element_state_get_name(currentState)
                << "gstPendingState=" << gst_element_state_get_name(pendingState)
                << "engineState=" << static_cast<int>(m_state);
        }
        return;
    }
    const bool pipelinePlaying = (currentState == GST_STATE_PLAYING || pendingState == GST_STATE_PLAYING);
    const bool playbackIntentActive = (m_positionTimer && m_positionTimer->isActive());
    const bool shouldResumePlayback = (playbackIntentActive
                                       || m_state == PlayingState
                                       || m_state == EndedState
                                       || pipelinePlaying);
    const qint64 durationMs = duration();
    const qint64 currentPositionMs = position();
    const double seekRate = effectivePlaybackRate();
    const bool reverseSeek = seekRate < 0.0;
    const qint64 timelineOffsetMs = qMax<qint64>(0, m_trackTimelineOffsetMs);
    qint64 boundedTargetMs = qMax<qint64>(0, positionMs);
    if (durationMs > 0) {
        const qint64 safeMaxMs = qMax<qint64>(0, durationMs - kSeekSafeEndMarginMs);
        boundedTargetMs = qMin(boundedTargetMs, safeMaxMs);
    }

    GstSeekType startType = GST_SEEK_TYPE_SET;
    GstSeekType stopType = GST_SEEK_TYPE_NONE;
    gint64 startPos = (timelineOffsetMs + boundedTargetMs) * GST_MSECOND;
    gint64 stopPos = GST_CLOCK_TIME_NONE;
    if (reverseSeek) {
        qint64 reverseStopMs = qMax<qint64>(1, boundedTargetMs);
        if (durationMs > 0) {
            reverseStopMs = qMin(reverseStopMs, safeReverseStartPositionMs(durationMs));
        }
        startType = GST_SEEK_TYPE_SET;
        startPos = timelineOffsetMs * GST_MSECOND;
        stopType = GST_SEEK_TYPE_SET;
        stopPos = (timelineOffsetMs + reverseStopMs) * GST_MSECOND;
    } else if (durationMs > 0) {
        // Explicitly reset forward segment end; otherwise old reverse stop can persist.
        stopType = GST_SEEK_TYPE_SET;
        stopPos = (timelineOffsetMs + durationMs) * GST_MSECOND;
    }

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] performSeek begin"
            << "source=" << seekSource
            << "file=" << m_currentFile
            << "targetMs=" << positionMs
            << "boundedTargetMs=" << boundedTargetMs
            << "currentPosMs=" << currentPositionMs
            << "durationMs=" << durationMs
            << "requestedRate=" << m_playbackRate
            << "effectiveRate=" << seekRate
            << "engineState=" << static_cast<int>(m_state)
            << "gstCurrentState=" << gst_element_state_get_name(currentState)
            << "gstPendingState=" << gst_element_state_get_name(pendingState)
            << "intentActive=" << playbackIntentActive
            << "pipelinePlaying=" << pipelinePlaying
            << "reverseSeek=" << reverseSeek
            << "resumeAfterSeek=" << shouldResumePlayback;
    }

    const GstSeekFlags primaryFlags = reverseSeek
        ? static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT)
        : static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE);
    const bool allowReversePauseFallback = reverseSeek && pipelinePlaying;
    bool usedPauseFallback = false;
    bool usedFastFallback = false;

    auto doSeekAttempt = [&](GstSeekFlags flags, bool pauseBeforeSeek) -> gboolean {
        if (pauseBeforeSeek) {
            // Keep pause-before-seek only as fallback: on long tracks doing this on every
            // reverse seek introduces visible PAUSED latency while scrubbing.
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            usedPauseFallback = true;
        }
        return gst_element_seek(m_pipeline,
                                seekRate,
                                GST_FORMAT_TIME,
                                flags,
                                startType,
                                startPos,
                                stopType,
                                stopPos);
    };

    const gboolean primarySeekOk = doSeekAttempt(primaryFlags, false);
    gboolean seekOk = primarySeekOk;
    if (!seekOk && allowReversePauseFallback) {
        seekOk = doSeekAttempt(primaryFlags, true);
    }

    if (!seekOk) {
        // Under heavy scrubbing some backends reject seeks transiently.
        // Fallback to plain FLUSH seek to keep playback responsive.
        const GstSeekFlags fastFlags = static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH);
        usedFastFallback = true;
        seekOk = doSeekAttempt(fastFlags, false);
        if (!seekOk && allowReversePauseFallback && !usedPauseFallback) {
            seekOk = doSeekAttempt(fastFlags, true);
        }
    }
    if (!seekOk) {
        qWarning().noquote()
            << "[SeekDiag][Audio] Seek failed"
            << "source=" << seekSource
            << "file=" << m_currentFile
            << "targetMs=" << positionMs
            << "boundedTargetMs=" << boundedTargetMs
            << "currentPosMs=" << currentPositionMs
            << "durationMs=" << durationMs
            << "requestedRate=" << m_playbackRate
            << "effectiveRate=" << seekRate
            << "engineState=" << static_cast<int>(m_state)
            << "gstCurrentState=" << gst_element_state_get_name(currentState)
            << "gstPendingState=" << gst_element_state_get_name(pendingState)
            << "primarySeekOk=" << primarySeekOk
            << "usedFastFallback=" << usedFastFallback
            << "usedPauseFallback=" << usedPauseFallback
            << "resumeAfterSeek=" << shouldResumePlayback;
        // Preserve playback continuity after seek bursts: even on seek failure
        // keep pipeline in PLAYING if user intent is playback.
        if (shouldResumePlayback) {
            const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
            if (ret != GST_STATE_CHANGE_FAILURE) {
                m_positionTimer->start();
                setState(PlayingState);
            }
        }
        m_lastSeekSource = seekSource;
        m_pendingSeekSource.clear();
        return;
    }

    m_lastSeekWallClockMs = QDateTime::currentMSecsSinceEpoch();
    m_lastSeekTargetMs = boundedTargetMs;
    m_lastSeekSource = seekSource;
    m_pendingSeekSource.clear();

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] performSeek success"
            << "source=" << m_lastSeekSource
            << "file=" << m_currentFile
            << "targetMs=" << m_lastSeekTargetMs
            << "primarySeekOk=" << primarySeekOk
            << "usedFastFallback=" << usedFastFallback
            << "usedPauseFallback=" << usedPauseFallback;
    }

    if (m_lastEmittedPositionMs != m_lastSeekTargetMs) {
        m_lastEmittedPositionMs = m_lastSeekTargetMs;
        emit positionChanged(m_lastEmittedPositionMs);
    }

    if (shouldResumePlayback) {
        const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (ret != GST_STATE_CHANGE_FAILURE) {
            m_positionTimer->start();
            setState(PlayingState);
        }
    }
}

void AudioEngine::setVolume(double volume)
{
    const double clamped = qBound(0.0, volume, 1.0);
    const bool changed = !qFuzzyCompare(m_volume, clamped);
    m_volume = clamped;

    if (m_pipeline) {
        if (GST_IS_STREAM_VOLUME(m_pipeline)) {
            gst_stream_volume_set_volume(GST_STREAM_VOLUME(m_pipeline),
                                         GST_STREAM_VOLUME_FORMAT_LINEAR,
                                         m_volume);
        } else {
            g_object_set(m_pipeline, "volume", m_volume, nullptr);
        }
    }
    
    if (changed) {
        emit volumeChanged(m_volume);
    }
}

void AudioEngine::setPlaybackRate(double rate)
{
    const double clampedRate = qBound(0.25, rate, 2.0);
    const bool changed = !qFuzzyCompare(m_playbackRate, clampedRate);
    m_playbackRate = clampedRate;

    if (changed) {
        emit playbackRateChanged(m_playbackRate);
    }

    if (!changed) {
        return;
    }

    m_pendingRateApplication = true;

    if (!m_pipeline || m_state == StoppedState) {
        return;
    }

    // Apply rate change instantly using playbin rate (not pitch tempo)
    // This keeps position/duration reporting correct
    applyPlaybackRateToPipeline();
}

void AudioEngine::setReversePlayback(bool enabled)
{
    if (m_reversePlayback == enabled) {
        return;
    }

    m_reversePlayback = enabled;
    emit reversePlaybackChanged(m_reversePlayback);

    if (!m_pipeline) {
        return;
    }

    if (!m_reversePlayback) {
        m_pendingReverseStart = false;
        if (m_state == StoppedState) {
            m_pendingRateApplication = !qFuzzyCompare(m_playbackRate, 1.0);
            return;
        }

        // Reverse->forward must rebuild segment bounds explicitly.
        // Instant rate change can keep stale reverse stop and trigger invalid seeks.
        m_pendingRateApplication = false;
        m_pendingSeekSource = QStringLiteral("audio.reverse_disable_resync");
        performSeek(qMax<qint64>(0, position()));
        return;
    }

    if (m_state == StoppedState) {
        if (!m_currentFile.isEmpty()) {
            m_pendingReverseStart = true;
        }
        m_pendingRateApplication = true;
        return;
    }

    qint64 targetPositionMs = qMax<qint64>(0, position());
    if (targetPositionMs <= 0) {
        const qint64 durMs = duration();
        if (durMs > 0) {
            targetPositionMs = safeReverseStartPositionMs(durMs);
        } else {
            m_pendingReverseStart = true;
            m_pendingRateApplication = true;
            return;
        }
    }

    m_pendingRateApplication = false;
    m_pendingSeekSource = QStringLiteral("audio.reverse_enable_seek");
    performSeek(targetPositionMs);
}

QString AudioEngine::normalizeAudioQualityProfile(const QString &profile)
{
    const QString normalized = profile.trimmed().toLower();
    if (normalized == QStringLiteral("hifi") || normalized == QStringLiteral("hi-fi")) {
        return QStringLiteral("hifi");
    }
    if (normalized == QStringLiteral("studio")) {
        return QStringLiteral("studio");
    }
    return QStringLiteral("standard");
}

void AudioEngine::setAudioQualityProfile(const QString &profile)
{
    const QString normalized = normalizeAudioQualityProfile(profile);
    if (m_audioQualityProfile == normalized) {
        return;
    }

    m_audioQualityProfile = normalized;
    emit audioQualityProfileChanged(m_audioQualityProfile);
    applyAudioQualityProfileToPipeline();
}

void AudioEngine::applyAudioQualityProfileToPipeline()
{
    const QString profile = normalizeAudioQualityProfile(m_audioQualityProfile);
    m_audioQualityProfile = profile;

    struct ProfileConfig {
        int resampleQuality;
        const char *resampleMethod;
        const char *sincMode;
        const char *sincInterpolation;
        double replayHeadroomDb;
        float dynamicRatio;
        float dynamicThreshold;
        guint ditheringThresholdBits;
        const char *ditheringMode;
        const char *noiseShapingMode;
    };

    ProfileConfig cfg;
    if (profile == QStringLiteral("studio")) {
        cfg = ProfileConfig{
            10,
            "kaiser",
            "full",
            "cubic",
            3.5,
            1.04f,
            0.985f,
            24u,
            "tpdf-hf",
            "high"
        };
    } else if (profile == QStringLiteral("hifi")) {
        cfg = ProfileConfig{
            9,
            "kaiser",
            "full",
            "cubic",
            2.5,
            1.14f,
            0.94f,
            24u,
            "tpdf-hf",
            "high"
        };
    } else {
        cfg = ProfileConfig{
            6,
            "kaiser",
            "auto",
            "linear",
            1.5,
            1.08f,
            0.96f,
            20u,
            "tpdf",
            "medium"
        };
    }

    if (m_resampleElement) {
        if (hasElementProperty(m_resampleElement, "quality")) {
            g_object_set(m_resampleElement, "quality", cfg.resampleQuality, nullptr);
        }
        setEnumPropertyIfAvailable(m_resampleElement, "resample-method", cfg.resampleMethod);
        setEnumPropertyIfAvailable(m_resampleElement, "sinc-filter-mode", cfg.sincMode);
        setEnumPropertyIfAvailable(m_resampleElement, "sinc-filter-interpolation", cfg.sincInterpolation);
    }

    if (m_replayGainElement) {
        if (hasElementProperty(m_replayGainElement, "headroom")) {
            g_object_set(m_replayGainElement, "headroom", cfg.replayHeadroomDb, nullptr);
        }
        if (hasElementProperty(m_replayGainElement, "fallback-gain")) {
            g_object_set(m_replayGainElement, "fallback-gain", 0.0, nullptr);
        }
        if (hasElementProperty(m_replayGainElement, "pre-amp")) {
            g_object_set(m_replayGainElement, "pre-amp", 0.0, nullptr);
        }
        if (hasElementProperty(m_replayGainElement, "album-mode")) {
            g_object_set(m_replayGainElement, "album-mode", TRUE, nullptr);
        }
    }

    if (m_dynamicRangeElement) {
        setEnumPropertyIfAvailable(m_dynamicRangeElement, "mode", "compressor");
        setEnumPropertyIfAvailable(m_dynamicRangeElement, "characteristics", "soft-knee");
        if (hasElementProperty(m_dynamicRangeElement, "ratio")) {
            g_object_set(m_dynamicRangeElement, "ratio", cfg.dynamicRatio, nullptr);
        }
        if (hasElementProperty(m_dynamicRangeElement, "threshold")) {
            g_object_set(m_dynamicRangeElement, "threshold", cfg.dynamicThreshold, nullptr);
        }
    }

    if (m_limiterElement && hasElementProperty(m_limiterElement, "enabled")) {
        g_object_set(m_limiterElement, "enabled", TRUE, nullptr);
    }

    if (m_outputConvertElement) {
        if (hasElementProperty(m_outputConvertElement, "dithering-threshold")) {
            g_object_set(m_outputConvertElement, "dithering-threshold", cfg.ditheringThresholdBits, nullptr);
        }
        setEnumPropertyIfAvailable(m_outputConvertElement, "dithering", cfg.ditheringMode);
        setEnumPropertyIfAvailable(m_outputConvertElement, "noise-shaping", cfg.noiseShapingMode);
    }
}

void AudioEngine::setPitchSemitones(int semitones)
{
    const int clamped = qBound(-6, semitones, 6);
    if (m_pitchSemitones == clamped) {
        return;
    }
    
    m_pitchSemitones = clamped;
    emit pitchSemitonesChanged(m_pitchSemitones);
    
    if (m_pitchElement) {
        // Convert semitones to pitch ratio: 2^(semitones/12)
        const double pitchRatio = std::pow(2.0, m_pitchSemitones / 12.0);
        g_object_set(m_pitchElement, "pitch", pitchRatio, nullptr);
    }
}

void AudioEngine::setSpectrumEnabled(bool enabled)
{
    if (m_spectrumEnabled == enabled) {
        return;
    }

    m_spectrumEnabled = enabled;
    if (m_spectrumElement) {
        g_object_set(m_spectrumElement, "post-messages", m_spectrumEnabled, nullptr);
    }

    if (!m_spectrumEnabled) {
        resetSpectrumLevels();
    }
}

void AudioEngine::setEqualizerBandGain(int bandIndex, double gainDb)
{
    if (bandIndex < 0 || bandIndex >= m_equalizerBandGains.size()) {
        return;
    }

    const double clampedGain = qBound(-24.0, gainDb, 12.0);
    const double previousGain = m_equalizerBandGains.at(bandIndex).toDouble();
    if (std::abs(previousGain - clampedGain) < 0.01) {
        return;
    }

    m_equalizerBandGains[bandIndex] = clampedGain;
    emit equalizerBandGainsChanged();

    if (!m_equalizerElement) {
        m_equalizerAppliedBandGains = m_equalizerBandGains;
        return;
    }

    // Coalesce high-frequency slider updates to reduce DSP churn.
    queueEqualizerApply(false, false);
}

void AudioEngine::setEqualizerBandGains(const QVariantList &gainsDb)
{
    bool changed = false;
    for (int i = 0; i < m_equalizerBandGains.size(); ++i) {
        const double source = (i < gainsDb.size()) ? gainsDb.at(i).toDouble() : 0.0;
        const double clampedGain = qBound(-24.0, source, 12.0);
        const double previous = m_equalizerBandGains.at(i).toDouble();
        if (std::abs(previous - clampedGain) < 0.01) {
            continue;
        }

        m_equalizerBandGains[i] = clampedGain;
        changed = true;
    }

    if (!changed) {
        return;
    }

    emit equalizerBandGainsChanged();

    if (!m_equalizerElement) {
        m_equalizerAppliedBandGains = m_equalizerBandGains;
        return;
    }

    // Batch preset/reset applies should avoid abrupt spectral jumps.
    queueEqualizerApply(true, true);
}

void AudioEngine::resetEqualizerBands()
{
    QVariantList flat;
    flat.reserve(m_equalizerBandGains.size());
    for (int i = 0; i < m_equalizerBandGains.size(); ++i) {
        flat.push_back(0.0);
    }
    setEqualizerBandGains(flat);
}

void AudioEngine::applyPlaybackRateToPipeline()
{
    if (!m_pipeline || m_state == StoppedState) {
        return;
    }

    const double effectiveRate = effectivePlaybackRate();
    const bool reverseRate = effectiveRate < 0.0;
    if (qFuzzyIsNull(effectiveRate)) {
        return;
    }

    // Try instant rate change first (GStreamer 1.18+)
    // This changes playback rate without flushing, providing seamless audio
    if (effectiveRate > 0.0) {
        GstEvent *event = gst_event_new_seek(
            effectiveRate,
            GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_INSTANT_RATE_CHANGE),
            GST_SEEK_TYPE_NONE,
            GST_CLOCK_TIME_NONE,
            GST_SEEK_TYPE_NONE,
            GST_CLOCK_TIME_NONE
        );

        if (event && gst_element_send_event(m_pipeline, event)) {
            // Instant rate change succeeded
            m_pendingRateApplication = false;
            return;
        }
    }

    // Fallback: use regular seek with current position (may cause brief audio glitch)
    gint64 pos = GST_CLOCK_TIME_NONE;
    const bool hasPosition = gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos);
    const qint64 durationMs = duration();
    const qint64 timelineOffsetMs = qMax<qint64>(0, m_trackTimelineOffsetMs);
    const qint64 safeReverseStopMs = durationMs > 0
        ? safeReverseStartPositionMs(durationMs)
        : 0;

    const GstSeekFlags flags = reverseRate
        ? static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT)
        : static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE);

    GstSeekType startType = GST_SEEK_TYPE_NONE;
    GstSeekType stopType = GST_SEEK_TYPE_NONE;
    gint64 startPos = GST_CLOCK_TIME_NONE;
    gint64 stopPos = GST_CLOCK_TIME_NONE;
    if (effectiveRate > 0.0) {
        if (hasPosition) {
            startType = GST_SEEK_TYPE_SET;
            startPos = pos;
            if (durationMs > 0) {
                const gint64 durationPos = (timelineOffsetMs + durationMs) * GST_MSECOND;
                if (startPos > durationPos) {
                    startPos = durationPos;
                }
            }
        }
        if (durationMs > 0) {
            stopType = GST_SEEK_TYPE_SET;
            stopPos = (timelineOffsetMs + durationMs) * GST_MSECOND;
            if (startType == GST_SEEK_TYPE_SET && startPos > stopPos) {
                startPos = stopPos;
            }
        }
    } else {
        qint64 reverseStopMs = hasPosition ? qMax<qint64>(0, (pos / GST_MSECOND) - timelineOffsetMs) : safeReverseStopMs;
        if (durationMs > 0) {
            reverseStopMs = qMin(reverseStopMs, safeReverseStopMs);
        }
        reverseStopMs = qMax<qint64>(1, reverseStopMs);
        startType = GST_SEEK_TYPE_SET;
        startPos = timelineOffsetMs * GST_MSECOND;
        stopType = GST_SEEK_TYPE_SET;
        stopPos = (timelineOffsetMs + reverseStopMs) * GST_MSECOND;
    }

    const gboolean ok = gst_element_seek(
        m_pipeline,
        effectiveRate,
        GST_FORMAT_TIME,
        flags,
        startType,
        startPos,
        stopType,
        stopPos
    );

    if (!ok) {
        const QString message = QStringLiteral("Failed to set playback rate to %1x (effective %2x)")
                                    .arg(m_playbackRate, 0, 'f', 2)
                                    .arg(effectiveRate, 0, 'f', 2);
        qWarning() << message;
        emit error(message);
        return;
    }

    m_pendingRateApplication = false;
}

quint64 AudioEngine::resolveTransitionId(quint64 requestedTransitionId)
{
    if (requestedTransitionId > 0) {
        if (requestedTransitionId > m_transitionIdCounter) {
            m_transitionIdCounter = requestedTransitionId;
        }
        return requestedTransitionId;
    }

    ++m_transitionIdCounter;
    return m_transitionIdCounter;
}

void AudioEngine::loadFile(const QString &filePath)
{
    loadFileWithTransition(filePath, 0);
}

void AudioEngine::loadFileWithTransition(const QString &filePath, quint64 transitionId)
{
    if (filePath.isEmpty()) return;

    const quint64 resolvedTransitionId = resolveTransitionId(transitionId);
    traceTransition("audio_load_file_request", resolvedTransitionId, {
        {QStringLiteral("filePath"), filePath},
        {QStringLiteral("requestedTransitionId"),
         static_cast<qulonglong>(transitionId)}
    });

    ++m_callbackSerial;
    m_nextFile.clear();
    m_nextFileTransitionId = 0;
    m_gaplessPendingFile.clear();
    m_gaplessPendingTransitionId = 0;
    m_gaplessPendingSinceWallClockMs = 0;
    m_trackTimelineOffsetMs = 0;
    m_recentGaplessStreamStartMs = 0;
    m_gaplessProgressWatchStartedMs = 0;
    m_gaplessProgressWatchStartPositionMs = 0;
    m_gaplessProgressRecoveryApplied = false;
    m_gaplessProgressReloadIssued = false;
    m_gaplessRecalibrationResyncAttempts = 0;
    m_lastStableDurationMs = 0;
    m_lastStableDurationUpdateMs = 0;
    m_gaplessEosDeferralTimer.stop();

    // Mark that we need to apply playback rate when pipeline reaches PLAYING state
    m_pendingRateApplication = m_reversePlayback || !qFuzzyCompare(m_playbackRate, 1.0);
    m_pendingReverseStart = m_reversePlayback;
    m_seekCoalesceTimer.stop();
    m_coalescedSeekPositionMs = -1;
    m_deferredSeekPositionMs = -1;
    m_lastSeekWallClockMs = 0;
    m_lastSeekTargetMs = -1;
    m_lastSeekSource.clear();
    m_pendingSeekSource.clear();
    m_lastTrackSwitchWallClockMs = QDateTime::currentMSecsSinceEpoch();

    const QString uri = buildPlaybackUri(filePath);
    if (uri.isEmpty()) {
        const QString message = QStringLiteral("Failed to resolve playback URI for: %1").arg(filePath);
        qWarning() << message;
        emit error(message);
        return;
    }
    
    // Stop current playback to ensure we don't get mixed messages
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }
    
    // Check if we have a bus and flush it to remove any stale messages (e.g. EOS from previous track)
    if (m_bus) {
        gst_bus_set_flushing(m_bus, TRUE);
        gst_bus_set_flushing(m_bus, FALSE);
    }

    m_isLoading = true;
    m_currentTransitionId = resolvedTransitionId;
    m_currentFile = filePath;
    m_lastEmittedPositionMs = -1;
    resetSpectrumLevels();

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] load file"
            << "file=" << m_currentFile
            << "transitionId=" << m_currentTransitionId;
    }

    emit currentFileChanged(m_currentFile);
    traceTransition("audio_current_file_changed_emitted", m_currentTransitionId, {
        {QStringLiteral("filePath"), m_currentFile}
    });
    
    // Set the new URI
    g_object_set(m_pipeline, "uri", uri.toUtf8().constData(), nullptr);
    
    // Start playing
    play();
}

void AudioEngine::loadUrl(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }

    if (url.isLocalFile()) {
        loadFile(url.toLocalFile());
        return;
    }

    loadFile(url.toString());
}

void AudioEngine::setNextFile(const QString &filePath)
{
    setNextFileWithTransition(filePath, 0);
}

void AudioEngine::setNextFileWithTransition(const QString &filePath, quint64 transitionId)
{
    if (filePath.isEmpty()) {
        m_nextFile.clear();
        m_nextFileTransitionId = 0;
        traceTransition("audio_next_file_cleared", m_currentTransitionId);
        return;
    }

    m_nextFile = filePath;
    m_nextFileTransitionId = resolveTransitionId(transitionId);
    traceTransition("audio_set_next_file", m_nextFileTransitionId, {
        {QStringLiteral("filePath"), filePath},
        {QStringLiteral("requestedTransitionId"),
         static_cast<qulonglong>(transitionId)},
        {QStringLiteral("currentTransitionId"),
         static_cast<qulonglong>(m_currentTransitionId)}
    });

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] set next file"
            << "file=" << m_nextFile
            << "transitionId=" << m_nextFileTransitionId;
    }
}

qint64 AudioEngine::position() const
{
    if (!m_pipeline) {
        return 0;
    }

    // Expose a stable UI-facing value aligned with positionChanged emissions.
    if (m_lastEmittedPositionMs >= 0) {
        return m_lastEmittedPositionMs;
    }

    gint64 pos = 0;
    if (!gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos) || pos < 0) {
        return 0;
    }

    const qint64 rawMs = pos / GST_MSECOND;
    return qMax<qint64>(0, rawMs - m_trackTimelineOffsetMs);
}

qint64 AudioEngine::stabilizeDurationValue(qint64 rawDurationMs, qint64 nowMs) const
{
    qint64 candidateMs = rawDurationMs;
    if (candidateMs <= 0 || candidateMs > kDurationMaxReasonableMs) {
        candidateMs = 0;
    }

    const bool recentTrackSwitch = (m_lastTrackSwitchWallClockMs > 0)
        && ((nowMs - m_lastTrackSwitchWallClockMs) <= kDurationStabilizationWindowMs);

    // Keep duration stable while the next track is only queued but not committed yet.
    if (!m_gaplessPendingFile.isEmpty() && m_lastStableDurationMs > 0) {
        return m_lastStableDurationMs;
    }

    // On some gapless paths duration can include a running timeline offset.
    // Prefer per-track duration when offset-adjusted value is more plausible.
    if (candidateMs > 0 && m_trackTimelineOffsetMs > 0) {
        const qint64 adjustedMs = candidateMs - m_trackTimelineOffsetMs;
        if (adjustedMs > 0 && adjustedMs <= kDurationMaxReasonableMs) {
            const bool noStableBaseline = (m_lastStableDurationMs <= 0);
            const qint64 rawDelta = qAbs(candidateMs - m_lastStableDurationMs);
            const qint64 adjustedDelta = qAbs(adjustedMs - m_lastStableDurationMs);
            if (noStableBaseline || (recentTrackSwitch && adjustedDelta < rawDelta)) {
                candidateMs = adjustedMs;
            }
        }
    }

    if (candidateMs <= 0) {
        return qMax<qint64>(0, m_lastStableDurationMs);
    }

    if (m_lastStableDurationMs <= 0) {
        m_lastStableDurationMs = candidateMs;
        m_lastStableDurationUpdateMs = nowMs;
        return candidateMs;
    }

    const qint64 deltaMs = candidateMs - m_lastStableDurationMs;
    const qint64 absDeltaMs = qAbs(deltaMs);
    const qint64 elapsedMs = qMax<qint64>(1, nowMs - m_lastStableDurationUpdateMs);

    const bool suspiciousLargeJumpDuringSwitch =
        recentTrackSwitch && absDeltaMs >= kDurationJumpAbsoluteThresholdMs;
    const bool suspiciousGrowthDuringSwitch =
        recentTrackSwitch && deltaMs > (elapsedMs + 1500);
    const bool suspiciousDropDuringSwitch =
        recentTrackSwitch && (-deltaMs) > (elapsedMs + 1500);
    const bool suspiciousValue = suspiciousLargeJumpDuringSwitch
        || suspiciousGrowthDuringSwitch
        || suspiciousDropDuringSwitch;

    if (suspiciousValue) {
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] suppressing unstable duration sample"
                << "rawDurationMs=" << rawDurationMs
                << "candidateMs=" << candidateMs
                << "stableDurationMs=" << m_lastStableDurationMs
                << "deltaMs=" << deltaMs
                << "elapsedMs=" << elapsedMs
                << "timelineOffsetMs=" << m_trackTimelineOffsetMs;
        }
        return m_lastStableDurationMs;
    }

    m_lastStableDurationMs = candidateMs;
    m_lastStableDurationUpdateMs = nowMs;
    return candidateMs;
}

qint64 AudioEngine::duration() const
{
    if (!m_pipeline) {
        return qMax<qint64>(0, m_lastStableDurationMs);
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    gint64 dur = 0;
    if (gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &dur) && dur >= 0) {
        return stabilizeDurationValue(dur / GST_MSECOND, nowMs);
    }
    return stabilizeDurationValue(0, nowMs);
}

void AudioEngine::updatePosition()
{
    if (!m_pipeline) {
        return;
    }

    gint64 pos = 0;
    const gboolean ok = gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos);
    if (!ok || pos < 0) {
        return;
    }

    const qint64 rawMs = pos / GST_MSECOND;
    if (m_recentGaplessStreamStartMs > 0) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 ageMs = nowMs - m_recentGaplessStreamStartMs;
        const bool recentSeekForRecalibration = (m_lastSeekWallClockMs > 0)
            && ((nowMs - m_lastSeekWallClockMs) <= kGaplessTimelineRecalibrationWindowMs);
        if (ageMs <= kGaplessTimelineRecalibrationWindowMs
            && m_trackTimelineOffsetMs <= 0
            && rawMs >= kGaplessTimelineRecalibrationThresholdMs
            && !recentSeekForRecalibration) {
            const bool severeRawJump = rawMs >= kGaplessRecalibrationResyncThresholdMs;
            const bool earlyGaplessAge = ageMs <= kGaplessRecalibrationResyncMaxAgeMs;
            if (!m_reversePlayback
                && severeRawJump
                && earlyGaplessAge
                && m_gaplessRecalibrationResyncAttempts < kGaplessRecalibrationResyncMaxAttempts) {
                ++m_gaplessRecalibrationResyncAttempts;
                const QString reloadFile = m_currentFile;
                const quint64 reloadTransitionId = m_currentTransitionId;
                enableGaplessBypass(QStringLiteral("recalibration_severe_jump_reload"),
                                    reloadTransitionId,
                                    {
                                        {QStringLiteral("rawMs"), rawMs},
                                        {QStringLiteral("ageMs"), ageMs},
                                        {QStringLiteral("attempt"),
                                         m_gaplessRecalibrationResyncAttempts}
                                    });
                traceTransition("audio_gapless_recalibration_reload_requested",
                                m_currentTransitionId,
                                {
                                    {QStringLiteral("rawMs"), rawMs},
                                    {QStringLiteral("ageMs"), ageMs},
                                    {QStringLiteral("attempt"), m_gaplessRecalibrationResyncAttempts}
                                });
                if (seekDiagEnabled()) {
                    qInfo().noquote()
                        << "[SeekDiag][Audio] gapless recalibration severe jump,"
                        << "reloading current track"
                        << "attempt=" << m_gaplessRecalibrationResyncAttempts
                        << "rawMs=" << rawMs
                        << "ageMs=" << ageMs
                        << "file=" << reloadFile
                        << "transitionId=" << reloadTransitionId;
                }
                loadFileWithTransition(reloadFile, reloadTransitionId);
                return;
            }

            // When a severe jump is detected but resync attempts are exhausted,
            // avoid applying a misleading offset that can mask wrong playback position.
            if (!m_reversePlayback && severeRawJump && earlyGaplessAge) {
                enableGaplessBypass(QStringLiteral("recalibration_severe_jump_exhausted"),
                                    m_currentTransitionId,
                                    {
                                        {QStringLiteral("rawMs"), rawMs},
                                        {QStringLiteral("ageMs"), ageMs},
                                        {QStringLiteral("attempts"),
                                         m_gaplessRecalibrationResyncAttempts}
                                    });
                traceTransition("audio_gapless_recalibration_resync_exhausted",
                                m_currentTransitionId,
                                {
                                    {QStringLiteral("rawMs"), rawMs},
                                    {QStringLiteral("ageMs"), ageMs},
                                    {QStringLiteral("attempts"),
                                     m_gaplessRecalibrationResyncAttempts}
                                });
                return;
            }
            m_trackTimelineOffsetMs = rawMs;
            traceTransition("audio_gapless_timeline_recalibrated", m_currentTransitionId, {
                {QStringLiteral("rawMs"), rawMs},
                {QStringLiteral("offsetMs"), m_trackTimelineOffsetMs},
                {QStringLiteral("ageMs"), ageMs}
            });
            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Audio] recalibrated gapless timeline offset"
                    << "rawMs=" << rawMs
                    << "offsetMs=" << m_trackTimelineOffsetMs
                    << "ageMs=" << ageMs;
            }
        }
        if (ageMs > kGaplessTimelineRecalibrationWindowMs) {
            m_recentGaplessStreamStartMs = 0;
        }
    }

    if (m_trackTimelineOffsetMs > 0 && (rawMs + 250) < m_trackTimelineOffsetMs) {
        // Timeline reset (e.g. backend switched from continuous running-time to per-track time).
        // Drop the offset so UI doesn't stick at 0.
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] clearing track timeline offset (timeline reset)"
                << "rawMs=" << rawMs
                << "offsetMs=" << m_trackTimelineOffsetMs;
        }
        m_trackTimelineOffsetMs = 0;
    }

    const qint64 adjustedMs = qMax<qint64>(0, rawMs - m_trackTimelineOffsetMs);

    if (m_gaplessProgressWatchStartedMs > 0) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 ageMs = nowMs - m_gaplessProgressWatchStartedMs;
        const qint64 advanceMs = adjustedMs - m_gaplessProgressWatchStartPositionMs;
        if (advanceMs >= kGaplessProgressWatchMinAdvanceMs) {
            m_gaplessProgressWatchStartedMs = 0;
            m_gaplessProgressWatchStartPositionMs = 0;
            m_gaplessProgressRecoveryApplied = false;
            m_gaplessProgressReloadIssued = false;
            m_gaplessRecalibrationResyncAttempts = 0;
        } else if (ageMs >= kGaplessProgressWatchDelayMs && !m_gaplessProgressRecoveryApplied) {
            m_gaplessProgressRecoveryApplied = true;
            traceTransition("audio_gapless_progress_stall_recovery", m_currentTransitionId, {
                {QStringLiteral("ageMs"), ageMs},
                {QStringLiteral("advanceMs"), advanceMs},
                {QStringLiteral("positionMs"), adjustedMs}
            });
            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Audio] gapless progress stall detected, forcing recovery seek"
                    << "ageMs=" << ageMs
                    << "advanceMs=" << advanceMs
                    << "positionMs=" << adjustedMs
                    << "file=" << m_currentFile
                    << "transitionId=" << m_currentTransitionId;
            }

            const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
            if (ret != GST_STATE_CHANGE_FAILURE) {
                m_positionTimer->start();
                setState(PlayingState);
            }
            m_pendingSeekSource = QStringLiteral("audio.gapless_progress_stall_recovery");
            performSeek(0);

            m_gaplessProgressWatchStartedMs = nowMs;
            m_gaplessProgressWatchStartPositionMs = 0;
        } else if (ageMs >= (kGaplessProgressWatchDelayMs * 2) && !m_gaplessProgressReloadIssued) {
            m_gaplessProgressReloadIssued = true;
            const QString reloadFile = m_currentFile;
            const quint64 reloadTransitionId = m_currentTransitionId;
            enableGaplessBypass(QStringLiteral("progress_stall_reload"),
                                reloadTransitionId,
                                {
                                    {QStringLiteral("ageMs"), ageMs},
                                    {QStringLiteral("advanceMs"), advanceMs},
                                    {QStringLiteral("positionMs"), adjustedMs}
                                });
            traceTransition("audio_gapless_progress_stall_reload_requested",
                            reloadTransitionId,
                            {
                                {QStringLiteral("ageMs"), ageMs},
                                {QStringLiteral("advanceMs"), advanceMs},
                                {QStringLiteral("positionMs"), adjustedMs},
                                {QStringLiteral("filePath"), reloadFile}
                            });
            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Audio] gapless stall persisted after recovery,"
                    << "reloading current track"
                    << "ageMs=" << ageMs
                    << "advanceMs=" << advanceMs
                    << "positionMs=" << adjustedMs
                    << "file=" << reloadFile
                    << "transitionId=" << reloadTransitionId;
            }
            loadFileWithTransition(reloadFile, reloadTransitionId);
            return;
        } else if (ageMs >= (kGaplessProgressWatchDelayMs * 3)) {
            m_gaplessProgressWatchStartedMs = 0;
            m_gaplessProgressWatchStartPositionMs = 0;
            m_gaplessProgressReloadIssued = false;
            m_gaplessRecalibrationResyncAttempts = 0;
        }
    }

    if (m_lastEmittedPositionMs >= 0 && qAbs(adjustedMs - m_lastEmittedPositionMs) < 40) {
        return;
    }

    m_lastEmittedPositionMs = adjustedMs;
    emit positionChanged(adjustedMs);
}

gboolean AudioEngine::busCallback(GstBus *bus, GstMessage *message, gpointer userData)
{
    Q_UNUSED(bus)
    auto *engine = static_cast<AudioEngine*>(userData);
    engine->handleBusMessage(message);
    return TRUE;
}

void AudioEngine::handleBusMessage(GstMessage *message)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError *err = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &err, &debug);
        
        QString errorMsg = QString::fromUtf8(err->message);
        qWarning() << "GStreamer error:" << errorMsg;
        qDebug() << "Debug info:" << (debug ? debug : "none");
        
        emit error(errorMsg);
        setState(ErrorState);
        
        g_error_free(err);
        g_free(debug);
        
        stop();
        m_isLoading = false;
        break;
    }
    
    case GST_MESSAGE_EOS:
        if (m_isLoading) {
             qWarning() << "Ignoring premature EOS during track load";
             traceTransition("audio_eos_ignored_loading", m_currentTransitionId);
             break; 
        }

        // If we queued a next URI for gapless playback, some backends can still post EOS
        // right at the handoff boundary. Defer EOS briefly and cancel it if STREAM_START
        // of the next track arrives.
        if (!m_gaplessPendingFile.isEmpty()) {
            traceTransition("audio_eos_deferred_for_gapless", m_currentTransitionId, {
                {QStringLiteral("currentFile"), m_currentFile},
                {QStringLiteral("pendingFile"), m_gaplessPendingFile},
                {QStringLiteral("pendingTransitionId"),
                 static_cast<qulonglong>(m_gaplessPendingTransitionId)}
            });
            if (seekDiagEnabled()) {
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const qint64 pendingAgeMs = (m_gaplessPendingSinceWallClockMs > 0)
                    ? (nowMs - m_gaplessPendingSinceWallClockMs)
                    : -1;
                qInfo().noquote()
                    << "[SeekDiag][Audio] deferring EOS due to gapless pending"
                    << "currentFile=" << m_currentFile
                    << "pendingFile=" << m_gaplessPendingFile
                    << "pendingAgeMs=" << pendingAgeMs
                    << "windowMs=" << kGaplessEosDeferralWindowMs;
            }

            if (!m_gaplessEosDeferralTimer.isActive()) {
                m_gaplessEosDeferralTimer.start(static_cast<int>(kGaplessEosDeferralWindowMs));
            }

            const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
            if (ret != GST_STATE_CHANGE_FAILURE) {
                m_positionTimer->start();
                setState(PlayingState);
            }
            break;
        }

        // Some backends can emit trailing EOS from the previous stream shortly after a
        // successful gapless STREAM_START. Ignore these EOS on the audio-engine level
        // to keep playback state in PLAYING and avoid UI transport desync.
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const bool recentGaplessStart = (m_recentGaplessStreamStartMs > 0)
                && ((nowMs - m_recentGaplessStreamStartMs) <= kGaplessTrailingEosGuardWindowMs);
            const qint64 posMs = position();
            const qint64 durMs = duration();
            const bool nearStartOfNewTrack =
                posMs <= kGaplessTrailingEosMaxPositionMs
                && (durMs <= 0 || (posMs + kGaplessTrailingEosMinRemainingMs) < durMs);
            if (recentGaplessStart && nearStartOfNewTrack) {
                traceTransition("audio_eos_ignored_trailing_gapless", m_currentTransitionId, {
                    {QStringLiteral("positionMs"), posMs},
                    {QStringLiteral("durationMs"), durMs},
                    {QStringLiteral("sinceGaplessStartMs"),
                     nowMs - m_recentGaplessStreamStartMs}
                });
                if (seekDiagEnabled()) {
                    qInfo().noquote()
                        << "[SeekDiag][Audio] ignoring trailing gapless EOS"
                        << "currentFile=" << m_currentFile
                        << "positionMs=" << posMs
                        << "durationMs=" << durMs
                        << "sinceGaplessStartMs=" << (nowMs - m_recentGaplessStreamStartMs);
                }
                const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
                if (ret != GST_STATE_CHANGE_FAILURE) {
                    m_positionTimer->start();
                    setState(PlayingState);
                }
                break;
            }
        }

        // End of stream (guard against occasional spurious EOS after rapid seek bursts).
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const bool recentSeek = (m_lastSeekWallClockMs > 0)
                    && ((nowMs - m_lastSeekWallClockMs) <= kSeekEosGuardWindowMs);
            const bool recentTrackSwitch = (m_lastTrackSwitchWallClockMs > 0)
                    && ((nowMs - m_lastTrackSwitchWallClockMs) <= kTrackSwitchEosGuardWindowMs);
            const qint64 durMs = duration();
            const qint64 posMs = position();
            const bool reversePlayback = effectivePlaybackRate() < 0.0;
            bool nearPlaybackBoundary = false;
            bool targetNearPlaybackBoundary = false;
            if (durMs > 0) {
                if (reversePlayback) {
                    nearPlaybackBoundary = posMs <= 350;
                    targetNearPlaybackBoundary = (m_lastSeekTargetMs >= 0)
                        && (m_lastSeekTargetMs <= 350);
                } else {
                    nearPlaybackBoundary = posMs >= qMax<qint64>(0, durMs - 350);
                    targetNearPlaybackBoundary = (m_lastSeekTargetMs >= 0)
                        && (m_lastSeekTargetMs >= qMax<qint64>(0, durMs - 350));
                }
            }
            const bool suspiciousEarlyEosAfterTrackSwitch = recentTrackSwitch
                && !nearPlaybackBoundary
                && !targetNearPlaybackBoundary
                && ((durMs > 0) || (posMs >= 0 && posMs <= 1200));
            if ((recentSeek || suspiciousEarlyEosAfterTrackSwitch)
                    && !nearPlaybackBoundary
                    && !targetNearPlaybackBoundary) {
                traceTransition("audio_eos_ignored_suspicious", m_currentTransitionId, {
                    {QStringLiteral("positionMs"), posMs},
                    {QStringLiteral("durationMs"), durMs},
                    {QStringLiteral("recentSeek"), recentSeek},
                    {QStringLiteral("recentTrackSwitch"), recentTrackSwitch}
                });
                qWarning() << "Ignoring suspicious EOS at" << posMs << "ms of" << durMs
                           << "ms (recentSeek=" << recentSeek
                           << ", recentTrackSwitch=" << recentTrackSwitch << ")";
                const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
                if (ret != GST_STATE_CHANGE_FAILURE) {
                    m_positionTimer->start();
                    setState(PlayingState);
                }
                break;
            }
        }

        m_lastEndOfStreamTransitionId = m_currentTransitionId;
        m_recentGaplessStreamStartMs = 0;
        m_gaplessProgressWatchStartedMs = 0;
        m_gaplessProgressWatchStartPositionMs = 0;
        m_gaplessProgressRecoveryApplied = false;
        m_gaplessProgressReloadIssued = false;
        m_gaplessRecalibrationResyncAttempts = 0;
        traceTransition("audio_eos_emitted", m_lastEndOfStreamTransitionId, {
            {QStringLiteral("currentFile"), m_currentFile}
        });
        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] emitting EOS"
                << "activeTransitionId=" << m_lastEndOfStreamTransitionId
                << "currentFile=" << m_currentFile;
        }

        setState(EndedState);
        emit endOfStream();
        break;

    case GST_MESSAGE_STREAM_START: {
        if (!m_gaplessPendingFile.isEmpty()) {
            m_gaplessEosDeferralTimer.stop();

            // For some backends the pipeline position is continuous across a gapless switch.
            // Capture the current raw position as a per-track base so UI position starts at ~0.
            {
                gint64 rawPos = 0;
                if (gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &rawPos) && rawPos >= 0) {
                    m_trackTimelineOffsetMs = rawPos / GST_MSECOND;
                } else {
                    m_trackTimelineOffsetMs = 0;
                }
                if (seekDiagEnabled()) {
                    qInfo().noquote()
                        << "[SeekDiag][Audio] gapless track timeline base"
                        << "offsetMs=" << m_trackTimelineOffsetMs;
                }
            }

            m_currentFile = m_gaplessPendingFile;
            m_currentTransitionId = resolveTransitionId(m_gaplessPendingTransitionId);
            m_gaplessPendingFile.clear();
            m_gaplessPendingTransitionId = 0;
            m_gaplessPendingSinceWallClockMs = 0;
            m_lastEmittedPositionMs = 0;
            m_lastStableDurationMs = 0;
            m_lastStableDurationUpdateMs = 0;
            m_gaplessProgressWatchStartedMs = QDateTime::currentMSecsSinceEpoch();
            m_gaplessProgressWatchStartPositionMs = 0;
            m_gaplessProgressRecoveryApplied = false;
            m_gaplessProgressReloadIssued = false;
            m_gaplessRecalibrationResyncAttempts = 0;
            emit positionChanged(0);
            resetSpectrumLevels();
            m_lastTrackSwitchWallClockMs = QDateTime::currentMSecsSinceEpoch();
            m_recentGaplessStreamStartMs = m_lastTrackSwitchWallClockMs;

            if (seekDiagEnabled()) {
                qInfo().noquote()
                    << "[SeekDiag][Audio] gapless stream committed"
                    << "file=" << m_currentFile
                    << "transitionId=" << m_currentTransitionId;
            }

            traceTransition("audio_stream_start_gapless_committed", m_currentTransitionId, {
                {QStringLiteral("filePath"), m_currentFile},
                {QStringLiteral("timelineOffsetMs"), m_trackTimelineOffsetMs}
            });

            // Keep output path deterministic after handoff: some backends can leave
            // the pipeline effectively paused/muted right after stream switch.
            if (GST_IS_STREAM_VOLUME(m_pipeline)) {
                gst_stream_volume_set_volume(GST_STREAM_VOLUME(m_pipeline),
                                             GST_STREAM_VOLUME_FORMAT_LINEAR,
                                             m_volume);
            } else {
                g_object_set(m_pipeline, "volume", m_volume, nullptr);
            }
            const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
            if (ret != GST_STATE_CHANGE_FAILURE) {
                m_positionTimer->start();
                setState(PlayingState);
            }

            emit currentFileChanged(m_currentFile);
            emit durationChanged(duration());
        }
        break;
    }
	        
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_pipeline)) {
            GstState oldState, newState, pendingState;
            gst_message_parse_state_changed(message, &oldState, &newState, &pendingState);

            PlaybackState mappedState = m_state;
            switch (newState) {
            case GST_STATE_NULL:
                mappedState = StoppedState;
                break;
            case GST_STATE_READY:
                mappedState = ReadyState;
                break;
            case GST_STATE_PAUSED:
                {
                    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                    const bool recentSeek = (m_lastSeekWallClockMs > 0)
                            && ((nowMs - m_lastSeekWallClockMs) <= kSeekEosGuardWindowMs);
                    const bool transientSeekPause = (m_positionTimer && m_positionTimer->isActive())
                            && (pendingState == GST_STATE_PLAYING || recentSeek);
                    mappedState = transientSeekPause ? PlayingState : PausedState;
                }
                break;
            case GST_STATE_PLAYING:
                mappedState = PlayingState;
                break;
            case GST_STATE_VOID_PENDING:
            default:
                break;
            }
            setState(mappedState);
            traceTransition("audio_pipeline_state_changed", m_currentTransitionId, {
                {QStringLiteral("oldState"),
                 QString::fromLatin1(gst_element_state_get_name(oldState))},
                {QStringLiteral("newState"),
                 QString::fromLatin1(gst_element_state_get_name(newState))},
                {QStringLiteral("pendingState"),
                 QString::fromLatin1(gst_element_state_get_name(pendingState))},
                {QStringLiteral("mappedState"), static_cast<int>(mappedState)},
                {QStringLiteral("currentFile"), m_currentFile}
            });

            if (newState == GST_STATE_PAUSED || newState == GST_STATE_PLAYING) {
                m_isLoading = false;
                applyDeferredSeekIfNeeded();
            }

            if (newState == GST_STATE_PLAYING && oldState != GST_STATE_PLAYING) {
                // Emit duration when we start playing
                emit durationChanged(duration());

                if (m_pendingReverseStart) {
                    applyPendingReverseStartIfNeeded();
                }
                
                // Apply pending playback rate immediately when pipeline is ready
                if (m_pendingRateApplication && !m_pendingReverseStart) {
                    applyPlaybackRateToPipeline();
                }
            }
        }
        break;
    }
    
    case GST_MESSAGE_TAG: {
        GstTagList *tags = nullptr;
        gst_message_parse_tag(message, &tags);
        
        gchar *value = nullptr;
        
        if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &value)) {
            m_title = QString::fromUtf8(value);
            g_free(value);
        }
        
        if (gst_tag_list_get_string(tags, GST_TAG_ARTIST, &value)) {
            m_artist = QString::fromUtf8(value);
            g_free(value);
        }
        
        if (gst_tag_list_get_string(tags, GST_TAG_ALBUM, &value)) {
            m_album = QString::fromUtf8(value);
            g_free(value);
        }
        
        gst_tag_list_unref(tags);
        emit metadataChanged();
        break;
    }
    
    case GST_MESSAGE_DURATION_CHANGED:
        emit durationChanged(duration());
        if (m_pendingReverseStart) {
            applyPendingReverseStartIfNeeded();
        }
        applyDeferredSeekIfNeeded();
        break;

    case GST_MESSAGE_ELEMENT: {
        const GstStructure *structure = gst_message_get_structure(message);
        if (structure && gst_structure_has_name(structure, "spectrum")) {
            handleSpectrumMessage(structure);
        }
        break;
    }
        
    default:
        break;
    }
}

void AudioEngine::handleSpectrumMessage(const GstStructure *structure)
{
    if (!m_spectrumEnabled || !structure) {
        return;
    }

    const GValue *magnitudeValues = gst_structure_get_value(structure, "magnitude");
    if (!magnitudeValues || !GST_VALUE_HOLDS_LIST(magnitudeValues)) {
        return;
    }

    const int count = static_cast<int>(gst_value_list_get_size(magnitudeValues));
    if (count <= 0) {
        return;
    }

    auto valueToDb = [](const GValue *value) -> double {
        if (!value) {
            return -80.0;
        }
        if (G_VALUE_HOLDS_FLOAT(value)) {
            return static_cast<double>(g_value_get_float(value));
        }
        if (G_VALUE_HOLDS_DOUBLE(value)) {
            return g_value_get_double(value);
        }
        if (G_VALUE_HOLDS_INT(value)) {
            return static_cast<double>(g_value_get_int(value));
        }
        if (G_VALUE_HOLDS_UINT(value)) {
            return static_cast<double>(g_value_get_uint(value));
        }
        return -80.0;
    };

    QVariantList levels;
    levels.reserve(m_spectrumBandCount);
    bool hasMeaningfulChange = false;
    constexpr double updateEpsilon = 0.003;

    for (int i = 0; i < m_spectrumBandCount; ++i) {
        double db = -80.0;
        if (i < count) {
            db = valueToDb(gst_value_list_get_value(magnitudeValues, i));
        }

        const double normalized = qBound(0.0, (db + 80.0) / 80.0, 1.0);
        const double previous = (i < m_spectrumLevels.size()) ? m_spectrumLevels.at(i).toDouble() : 0.0;
        const double alpha = (normalized > previous) ? 0.62 : 0.25;
        const double smoothed = previous + alpha * (normalized - previous);
        hasMeaningfulChange = hasMeaningfulChange || (std::abs(smoothed - previous) > updateEpsilon);
        levels.push_back(smoothed);
    }

    if (!hasMeaningfulChange) {
        return;
    }

    m_spectrumLevels = levels;
    emit spectrumLevelsChanged();
}

void AudioEngine::applyEqualizerBandSettings()
{
    if (!m_equalizerElement) {
        m_equalizerAppliedBandGains = m_equalizerBandGains;
        m_equalizerPendingApplyAllowRamp = false;
        m_equalizerRampTimer.stop();
        m_equalizerRampCurrentStep = 0;
        m_equalizerRampTotalSteps = 0;
        return;
    }

    if (m_equalizerAppliedBandGains.size() != m_equalizerBandGains.size()) {
        m_equalizerAppliedBandGains = m_equalizerBandGains;
    }

    double maxDeltaDb = 0.0;
    for (int i = 0; i < m_equalizerBandGains.size(); ++i) {
        const double current = m_equalizerAppliedBandGains.at(i).toDouble();
        const double target = m_equalizerBandGains.at(i).toDouble();
        maxDeltaDb = qMax(maxDeltaDb, std::abs(target - current));
    }

    const bool allowRamp = m_equalizerPendingApplyAllowRamp;
    m_equalizerPendingApplyAllowRamp = false;

    if (allowRamp && maxDeltaDb >= kEqualizerRampThresholdDb) {
        startEqualizerRamp(m_equalizerBandGains);
        return;
    }

    m_equalizerRampTimer.stop();
    m_equalizerRampCurrentStep = 0;
    m_equalizerRampTotalSteps = 0;
    applyEqualizerBandValues(m_equalizerBandGains);
}

void AudioEngine::queueEqualizerApply(bool allowRamp, bool immediate)
{
    m_equalizerPendingApplyAllowRamp = m_equalizerPendingApplyAllowRamp || allowRamp;

    if (!m_equalizerElement) {
        m_equalizerPendingApplyAllowRamp = false;
        m_equalizerApplyTimer.stop();
        m_equalizerRampTimer.stop();
        m_equalizerAppliedBandGains = m_equalizerBandGains;
        return;
    }

    if (immediate) {
        m_equalizerApplyTimer.stop();
        applyEqualizerBandSettings();
        return;
    }

    m_equalizerApplyTimer.start(kEqualizerApplyCoalesceMs);
}

void AudioEngine::applyEqualizerBandValues(const QVariantList &gainsDb)
{
    if (!m_equalizerElement) {
        return;
    }

    const int count = qMin(m_equalizerBandCount, gainsDb.size());
    for (int i = 0; i < count; ++i) {
        const QByteArray gainProperty = QByteArray("band") + QByteArray::number(i) + "::gain";
        const double gainDb = qBound(-24.0, gainsDb.at(i).toDouble(), 12.0);
        gst_child_proxy_set(GST_CHILD_PROXY(m_equalizerElement), gainProperty.constData(), gainDb, nullptr);
    }

    m_equalizerAppliedBandGains = gainsDb;
}

void AudioEngine::startEqualizerRamp(const QVariantList &targetGainsDb)
{
    if (!m_equalizerElement) {
        return;
    }

    if (targetGainsDb.size() != m_equalizerBandGains.size()) {
        applyEqualizerBandValues(m_equalizerBandGains);
        return;
    }

    m_equalizerRampStartGains = m_equalizerAppliedBandGains;
    if (m_equalizerRampStartGains.size() != m_equalizerBandGains.size()) {
        m_equalizerRampStartGains = m_equalizerBandGains;
    }
    m_equalizerRampTargetGains = targetGainsDb;
    m_equalizerRampCurrentStep = 0;
    m_equalizerRampTotalSteps = qMax(1, kEqualizerRampDurationMs / kEqualizerRampStepMs);

    if (m_equalizerRampTotalSteps <= 1) {
        applyEqualizerBandValues(m_equalizerRampTargetGains);
        return;
    }

    m_equalizerRampTimer.start();
    processEqualizerRampStep();
}

void AudioEngine::processEqualizerRampStep()
{
    if (!m_equalizerElement || m_equalizerRampTotalSteps <= 0) {
        m_equalizerRampTimer.stop();
        m_equalizerRampCurrentStep = 0;
        m_equalizerRampTotalSteps = 0;
        return;
    }

    m_equalizerRampCurrentStep = qMin(m_equalizerRampCurrentStep + 1, m_equalizerRampTotalSteps);
    const double t = static_cast<double>(m_equalizerRampCurrentStep)
                   / static_cast<double>(m_equalizerRampTotalSteps);

    QVariantList frame;
    frame.reserve(m_equalizerBandGains.size());
    for (int i = 0; i < m_equalizerBandGains.size(); ++i) {
        const double startGain = (i < m_equalizerRampStartGains.size())
                ? m_equalizerRampStartGains.at(i).toDouble()
                : 0.0;
        const double targetGain = (i < m_equalizerRampTargetGains.size())
                ? m_equalizerRampTargetGains.at(i).toDouble()
                : startGain;
        frame.push_back(startGain + (targetGain - startGain) * t);
    }

    applyEqualizerBandValues(frame);

    if (m_equalizerRampCurrentStep >= m_equalizerRampTotalSteps) {
        m_equalizerRampTimer.stop();
        m_equalizerRampCurrentStep = 0;
        m_equalizerRampTotalSteps = 0;
        applyEqualizerBandValues(m_equalizerRampTargetGains);
    }
}

void AudioEngine::syncEqualizerBandFrequenciesFromElement()
{
    if (!m_equalizerElement) {
        return;
    }

    QVariantList frequencies;
    frequencies.reserve(m_equalizerBandCount);

    for (int i = 0; i < m_equalizerBandCount; ++i) {
        const QByteArray freqProperty = QByteArray("band") + QByteArray::number(i) + "::freq";
        gdouble freqHz = 0.0;
        gst_child_proxy_get(GST_CHILD_PROXY(m_equalizerElement), freqProperty.constData(), &freqHz, nullptr);
        if (freqHz <= 0.0 && i < m_equalizerBandFrequencies.size()) {
            freqHz = m_equalizerBandFrequencies.at(i).toDouble();
        }
        frequencies.push_back(freqHz);
    }

    if (frequencies.size() == m_equalizerBandFrequencies.size()) {
        m_equalizerBandFrequencies = frequencies;
    }
}

void AudioEngine::applyEqualizerBandGain(int bandIndex)
{
    if (!m_equalizerElement || bandIndex < 0 || bandIndex >= m_equalizerBandGains.size()) {
        return;
    }

    const QByteArray gainProperty = QByteArray("band") + QByteArray::number(bandIndex) + "::gain";
    const double gainDb = m_equalizerBandGains.at(bandIndex).toDouble();
    gst_child_proxy_set(GST_CHILD_PROXY(m_equalizerElement), gainProperty.constData(), gainDb, nullptr);
    if (bandIndex < m_equalizerAppliedBandGains.size()) {
        m_equalizerAppliedBandGains[bandIndex] = gainDb;
    }
}

void AudioEngine::onAboutToFinish(GstElement *playbin, gpointer userData)
{
    Q_UNUSED(playbin)
    auto *engine = static_cast<AudioEngine *>(userData);
    if (!engine) {
        return;
    }

    const quint64 callbackSerial = engine->m_callbackSerial.load(std::memory_order_relaxed);
    if (QThread::currentThread() == engine->thread()) {
        engine->handleAboutToFinishOnMainThread(callbackSerial);
        return;
    }

    // GStreamer's about-to-finish expects the next URI to be set before returning
    // from the callback. Use a blocking hop to the main thread so QML can provide
    // the next track and we can queue it in time for gapless playback.
    QMetaObject::invokeMethod(engine,
                             [engine, callbackSerial]() {
                                 engine->handleAboutToFinishOnMainThread(callbackSerial);
                             },
                             Qt::BlockingQueuedConnection);
}

void AudioEngine::handleAboutToFinishOnMainThread(quint64 callbackSerial)
{
    if (callbackSerial != m_callbackSerial.load(std::memory_order_relaxed)) {
        traceTransition("audio_about_to_finish_ignored_stale_callback", m_currentTransitionId, {
            {QStringLiteral("callbackSerial"),
             static_cast<qulonglong>(callbackSerial)}
        });
        return;
    }

    // Reverse playback relies on explicit EOS handling in PlaybackController.
    // Gapless pre-queue can desynchronize UI/file state in reverse segments.
    if (m_reversePlayback) {
        traceTransition("audio_about_to_finish_skipped_reverse", m_currentTransitionId);
        return;
    }

    m_lastAboutToFinishTransitionId = m_currentTransitionId;
    traceTransition("audio_about_to_finish_emitted", m_lastAboutToFinishTransitionId, {
        {QStringLiteral("currentFile"), m_currentFile}
    });

    // Emit signal so the playlist can provide the next track.
    emit aboutToFinish();

    if (kForceReliableEosTransitions && !m_gaplessBypassMode) {
        enableGaplessBypass(QStringLiteral("forced_reliable_eos_transition"),
                            m_currentTransitionId);
    }

    if (m_gaplessBypassMode) {
        const QString bypassedNextFile = m_nextFile;
        const quint64 bypassedNextTransitionId = m_nextFileTransitionId;
        m_nextFile.clear();
        m_nextFileTransitionId = 0;
        m_gaplessPendingFile.clear();
        m_gaplessPendingTransitionId = 0;
        m_gaplessPendingSinceWallClockMs = 0;
        m_gaplessEosDeferralTimer.stop();

        traceTransition("audio_gapless_bypass_skip_queue", m_currentTransitionId, {
            {QStringLiteral("reason"), m_gaplessBypassReason},
            {QStringLiteral("nextFile"), bypassedNextFile},
            {QStringLiteral("nextTransitionId"),
             static_cast<qulonglong>(bypassedNextTransitionId)}
        });

        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] gapless bypass active, skipping prequeue"
                << "reason=" << m_gaplessBypassReason
                << "nextFile=" << bypassedNextFile
                << "nextTransitionId=" << bypassedNextTransitionId
                << "sourceTransitionId=" << m_lastAboutToFinishTransitionId;
        }
        return;
    }

    // If we have a next file queued, set it for gapless playback.
    if (m_nextFile.isEmpty() || !m_pipeline) {
        return;
    }

    const QString nextFile = m_nextFile;
    const quint64 nextTransitionId = resolveTransitionId(m_nextFileTransitionId);
    m_nextFile.clear();
    m_nextFileTransitionId = 0;
    m_gaplessPendingFile = nextFile;
    m_gaplessPendingTransitionId = nextTransitionId;
    m_gaplessPendingSinceWallClockMs = QDateTime::currentMSecsSinceEpoch();
    m_gaplessEosDeferralTimer.stop();

    const QString uri = buildPlaybackUri(nextFile);
    if (uri.isEmpty()) {
        qWarning() << "Failed to resolve gapless playback URI for:" << nextFile;
        m_gaplessPendingFile.clear();
        m_gaplessPendingTransitionId = 0;
        m_gaplessPendingSinceWallClockMs = 0;
        m_gaplessEosDeferralTimer.stop();
        return;
    }

    g_object_set(m_pipeline, "uri", uri.toUtf8().constData(), nullptr);

    // Mark for rate/direction reapplication if needed (gapless transition).
    m_pendingRateApplication = m_reversePlayback || !qFuzzyCompare(m_playbackRate, 1.0);
    m_pendingReverseStart = m_reversePlayback;
    traceTransition("audio_gapless_candidate_queued", m_gaplessPendingTransitionId, {
        {QStringLiteral("nextFile"), nextFile},
        {QStringLiteral("sourceTransitionId"),
         static_cast<qulonglong>(m_lastAboutToFinishTransitionId)}
    });

    if (seekDiagEnabled()) {
        qInfo().noquote()
            << "[SeekDiag][Audio] gapless candidate queued"
            << "file=" << nextFile
            << "sourceTransitionId=" << m_lastAboutToFinishTransitionId
            << "nextTransitionId=" << m_gaplessPendingTransitionId;
    }
}

void AudioEngine::applyPendingReverseStartIfNeeded()
{
    if (!m_pendingReverseStart || !m_reversePlayback || !m_pipeline) {
        return;
    }

    const qint64 durationMs = duration();
    if (durationMs <= 0) {
        return;
    }

    const qint64 startMs = safeReverseStartPositionMs(durationMs);
    m_pendingReverseStart = false;
    m_pendingRateApplication = false;
    m_pendingSeekSource = QStringLiteral("audio.reverse_pending_start");
    performSeek(startMs);
}

qint64 AudioEngine::safeReverseStartPositionMs(qint64 durationMs) const
{
    if (durationMs <= 0) {
        return 1;
    }
    return qMax<qint64>(1, durationMs - kReverseStartMarginMs);
}

double AudioEngine::effectivePlaybackRate() const
{
    return m_reversePlayback ? -m_playbackRate : m_playbackRate;
}

void AudioEngine::enableGaplessBypass(const QString &reason,
                                      quint64 transitionId,
                                      const QVariantMap &extra)
{
    const bool wasEnabled = m_gaplessBypassMode;
    const bool reasonChanged = (m_gaplessBypassReason != reason);
    m_gaplessBypassMode = true;
    m_gaplessBypassReason = reason;

    if (!wasEnabled || reasonChanged) {
        QVariantMap payload = extra;
        payload.insert(QStringLiteral("reason"), reason);
        payload.insert(QStringLiteral("currentFile"), m_currentFile);
        payload.insert(QStringLiteral("wasEnabled"), wasEnabled);
        traceTransition("audio_gapless_bypass_enabled", transitionId, payload);

        if (seekDiagEnabled()) {
            qInfo().noquote()
                << "[SeekDiag][Audio] enabling gapless bypass mode"
                << "reason=" << reason
                << "file=" << m_currentFile
                << "transitionId=" << transitionId;
        }
    }
}

void AudioEngine::extractMetadata()
{
    // Metadata is extracted via tag messages in handleBusMessage
}

void AudioEngine::resetSpectrumLevels()
{
    QVariantList resetLevels;
    resetLevels.reserve(m_spectrumBandCount);
    for (int i = 0; i < m_spectrumBandCount; ++i) {
        resetLevels.push_back(0.0);
    }

    if (m_spectrumLevels == resetLevels) {
        return;
    }

    m_spectrumLevels = resetLevels;
    emit spectrumLevelsChanged();
}

void AudioEngine::setState(PlaybackState state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    emit stateChanged(m_state);
}
