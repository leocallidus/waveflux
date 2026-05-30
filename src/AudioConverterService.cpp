#include "AudioConverterService.h"

#include "AppSettingsManager.h"
#include "TagLibPath.h"
#include "playback/PlaybackBackendRouting.h"

#include <QDir>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTemporaryFile>
#include <QUrl>
#include <QDebug>
#include <QtGlobal>
#include <gst/gst.h>
#include <gst/gstchildproxy.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <cmath>
#include <fstream>
#include <memory>

#include <libopenmpt/libopenmpt.hpp>

namespace {
QString localizedConverterText(const QString &key)
{
    return AppSettingsManager::translateForCurrentLanguage(key);
}

constexpr AudioConverterService::FormatProfile kFormatProfiles[] = {
    {"mp3", "MP3", "mp3", "MPEG Audio", "MP3", "", "lamemp3enc",
     true, true, true, true, false, 320, 44100},
    {"flac", "FLAC", "flac", "FLAC", "FLAC", "", "flacenc",
     false, false, true, true, true, 0, 44100},
    {"wav", "WAV", "wav", "RIFF/WAVE", "PCM", "wavenc", "identity",
     false, false, true, true, false, 0, 44100},
    {"opus", "Ogg Opus", "opus", "Ogg", "Opus", "oggmux", "opusenc",
     true, true, true, true, false, 192, 48000},
    {"webm", "WebM Opus", "webm", "WebM", "Opus", "webmmux", "opusenc",
     true, true, true, true, false, 192, 48000},
};

QVariantList bitrateValuesForProfile(const AudioConverterService::FormatProfile &profile)
{
    QVariantList values;
    if (!profile.supportsBitrate) {
        return values;
    }

    const QString profileId = QString::fromLatin1(profile.id);
    if (profileId == QStringLiteral("opus") || profileId == QStringLiteral("webm")) {
        const int allowed[] = {64, 96, 128, 160, 192, 256};
        for (int value : allowed) {
            values.push_back(value);
        }
        return values;
    }

    const int allowed[] = {64, 96, 128, 160, 192, 224, 256, 320};
    for (int value : allowed) {
        values.push_back(value);
    }
    return values;
}

QVariantList sampleRateValuesForProfile(const AudioConverterService::FormatProfile &profile)
{
    QVariantList values;
    if (!profile.supportsSampleRate) {
        return values;
    }

    const QString profileId = QString::fromLatin1(profile.id);
    if (profileId == QStringLiteral("opus") || profileId == QStringLiteral("webm")) {
        values.push_back(48000);
        return values;
    }

    if (profileId == QStringLiteral("mp3")) {
        const int allowed[] = {22050, 32000, 44100, 48000};
        for (int value : allowed) {
            values.push_back(value);
        }
        return values;
    }

    const int allowed[] = {22050, 32000, 44100, 48000, 88200, 96000, 192000};
    for (int value : allowed) {
        values.push_back(value);
    }
    return values;
}

QVariantList channelModeValuesForProfile(const AudioConverterService::FormatProfile &profile)
{
    QVariantList values;
    if (!profile.supportsChannels) {
        return values;
    }

    values.push_back(QStringLiteral("mono"));
    values.push_back(QStringLiteral("stereo"));
    return values;
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

QString normalizeLocalPath(const QString &path)
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

QString baseNameForOutput(const QString &sourceFile)
{
    const QFileInfo info(sourceFile);
    const QString completeBaseName = info.completeBaseName().trimmed();
    if (!completeBaseName.isEmpty()) {
        return completeBaseName;
    }
    return QStringLiteral("Converted Track");
}

qint64 probeMetadataDurationMs(const QString &source)
{
    const QString localPath = normalizeLocalPath(source);
    if (localPath.isEmpty()) {
        return 0;
    }

    const auto file = WaveFlux::TagLibPath::makeFileRef(
        localPath,
        true,
        TagLib::AudioProperties::Fast);
    if (file.isNull() || !file.audioProperties()) {
        return 0;
    }

    return qMax<qint64>(0, file.audioProperties()->lengthInMilliseconds());
}

QString localFileUri(const QString &source)
{
    const QString localPath = normalizeLocalPath(source);
    if (localPath.isEmpty()) {
        return {};
    }
    return QUrl::fromLocalFile(localPath).toString();
}

bool hasElementProperty(GstElement *element, const char *propertyName)
{
    if (!element || !propertyName) {
        return false;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(element);
    return klass && g_object_class_find_property(klass, propertyName);
}

QString describePadCaps(GstElement *element, const char *padName, GstPadDirection direction)
{
    if (!element || !padName) {
        return QStringLiteral("<invalid>");
    }

    GstPad *pad = gst_element_get_static_pad(element, padName);
    if (!pad) {
        return QStringLiteral("<missing-pad>");
    }

    GstCaps *caps = direction == GST_PAD_SRC
        ? gst_pad_query_caps(pad, nullptr)
        : gst_pad_query_caps(pad, nullptr);
    QString description = QStringLiteral("<unknown>");
    if (caps) {
        gchar *capsString = gst_caps_to_string(caps);
        if (capsString) {
            description = QString::fromUtf8(capsString);
            g_free(capsString);
        }
        gst_caps_unref(caps);
    }

    gst_object_unref(pad);
    return description;
}

bool linkWithDiagnostics(GstElement *src,
                         GstElement *dest,
                         const QString &stageLabel,
                         QString *errorMessage)
{
    if (!src || !dest) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1: pipeline element is missing.").arg(stageLabel);
        }
        return false;
    }

    if (gst_element_link(src, dest)) {
        return true;
    }

    const QString srcName = QString::fromUtf8(GST_ELEMENT_NAME(src));
    const QString destName = QString::fromUtf8(GST_ELEMENT_NAME(dest));
    const QString srcCaps = describePadCaps(src, "src", GST_PAD_SRC);
    const QString destCaps = describePadCaps(dest, "sink", GST_PAD_SINK);
    const QString details = QStringLiteral("%1 -> %2 failed. src caps: %3 | sink caps: %4")
                                .arg(srcName, destName, srcCaps, destCaps);
    qWarning().noquote() << "[AudioConverterService]" << stageLabel << details;
    if (errorMessage) {
        *errorMessage = QStringLiteral("%1: %2").arg(stageLabel, details);
    }
    return false;
}

int channelsForMode(const QString &channelMode)
{
    return channelMode == QStringLiteral("mono") ? 1 : 2;
}

TagLib::String toTagLibString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return TagLib::String(utf8.constData(), TagLib::String::UTF8);
}

qint64 secondsToMs(double seconds)
{
    return std::max<qint64>(0, qRound64(seconds * 1000.0));
}

bool writePcmWaveHeader(QDataStream &stream,
                        quint32 sampleRate,
                        quint16 channels,
                        quint32 dataBytes)
{
    const quint16 bitsPerSample = 16;
    const quint16 bytesPerSample = bitsPerSample / 8;
    const quint32 byteRate = sampleRate * channels * bytesPerSample;
    const quint16 blockAlign = channels * bytesPerSample;

    stream.writeRawData("RIFF", 4);
    stream << quint32(36u + dataBytes);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << channels;
    stream << sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataBytes;
    return stream.status() == QDataStream::Ok;
}

bool renderTrackerModuleToWaveFile(const QString &sourcePath,
                                   const QString &outputPath,
                                   qint64 *durationMs,
                                   QString *errorMessage)
{
    if (durationMs) {
        *durationMs = 0;
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    constexpr int kSampleRate = 48000;
    constexpr int kChannelCount = 2;
    constexpr std::size_t kChunkFrames = 4096;

    std::ifstream stream(sourcePath.toStdString(), std::ios::binary);
    if (!stream.is_open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open tracker module for reading: %1").arg(sourcePath);
        }
        return false;
    }

    std::unique_ptr<openmpt::module> module;
    try {
        module = std::make_unique<openmpt::module>(stream);
        module->set_repeat_count(0);
    } catch (const openmpt::exception &exception) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker decoder rejected the source: %1")
                .arg(QString::fromUtf8(exception.what()));
        }
        return false;
    } catch (const std::exception &exception) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker decoder failed to load the source: %1")
                .arg(QString::fromUtf8(exception.what()));
        }
        return false;
    } catch (...) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker decoder failed to load the source.");
        }
        return false;
    }

    if (durationMs) {
        *durationMs = secondsToMs(module->get_duration_seconds());
    }

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create temporary tracker render file: %1")
                .arg(outputFile.errorString());
        }
        return false;
    }

    QDataStream streamOut(&outputFile);
    streamOut.setByteOrder(QDataStream::LittleEndian);
    if (!writePcmWaveHeader(streamOut, kSampleRate, kChannelCount, 0)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write tracker render header.");
        }
        outputFile.close();
        QFile::remove(outputPath);
        return false;
    }

    std::vector<std::int16_t> pcm(kChunkFrames * kChannelCount);
    quint32 totalDataBytes = 0;
    while (true) {
        std::size_t renderedFrames = 0;
        try {
            renderedFrames = module->read_interleaved_stereo(kSampleRate, kChunkFrames, pcm.data());
        } catch (const std::exception &exception) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Tracker decode failed during conversion: %1")
                    .arg(QString::fromUtf8(exception.what()));
            }
            outputFile.close();
            QFile::remove(outputPath);
            return false;
        } catch (...) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Tracker decode failed during conversion.");
            }
            outputFile.close();
            QFile::remove(outputPath);
            return false;
        }

        if (renderedFrames == 0) {
            break;
        }

        const qint64 bytesToWrite = static_cast<qint64>(renderedFrames)
            * kChannelCount
            * static_cast<qint64>(sizeof(std::int16_t));
        if (outputFile.write(reinterpret_cast<const char *>(pcm.data()), bytesToWrite) != bytesToWrite) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to write tracker render data: %1")
                    .arg(outputFile.errorString());
            }
            outputFile.close();
            QFile::remove(outputPath);
            return false;
        }
        totalDataBytes += static_cast<quint32>(bytesToWrite);
    }

    if (totalDataBytes == 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tracker module produced no audio frames during conversion.");
        }
        outputFile.close();
        QFile::remove(outputPath);
        return false;
    }

    outputFile.seek(0);
    QDataStream rewriteStream(&outputFile);
    rewriteStream.setByteOrder(QDataStream::LittleEndian);
    if (!writePcmWaveHeader(rewriteStream, kSampleRate, kChannelCount, totalDataBytes)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to finalize tracker render header.");
        }
        outputFile.close();
        QFile::remove(outputPath);
        return false;
    }

    outputFile.close();
    return true;
}
} // namespace

AudioConverterService::AudioConverterService(QObject *parent)
    : QObject(parent)
{
    m_equalizerBandGains = normalizeEqualizerBandGains({});
    setStatusPresentation(QStringLiteral("audioConverter.runtimeReady"),
                          {},
                          QStringLiteral("Ready to configure audio conversion."));
    m_busPollTimer.setInterval(20);
    connect(&m_busPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_bus) {
            return;
        }
        while (true) {
            GstMessage *message = gst_bus_pop(m_bus);
            if (!message) {
                break;
            }
            handleBusMessage(message);
            gst_message_unref(message);
            if (m_completionHandled) {
                break;
            }
        }
    });

    m_progressTimer.setInterval(120);
    connect(&m_progressTimer, &QTimer::timeout, this, [this]() {
        updateProgressFromPipeline();
    });
}

void AudioConverterService::initialize(TrackModel *trackModel)
{
    m_trackModel = trackModel;
}

QVariantList AudioConverterService::formatProfiles() const
{
    QVariantList result;
    result.reserve(static_cast<int>(sizeof(kFormatProfiles) / sizeof(kFormatProfiles[0])));
    for (const FormatProfile &profile : kFormatProfiles) {
        result.push_back(toVariantMap(profile));
    }
    return result;
}

QVariantMap AudioConverterService::currentFormatProfile() const
{
    const FormatProfile *profile = findFormatProfile(m_format);
    return profile ? toVariantMap(*profile) : QVariantMap();
}

QVariantMap AudioConverterService::lastConversionMetrics() const
{
    QVariantMap metrics;
    metrics.insert(QStringLiteral("startedAtMs"), m_lastConversionStartedAtMs);
    metrics.insert(QStringLiteral("finishedAtMs"), m_lastConversionFinishedAtMs);
    metrics.insert(QStringLiteral("wallClockDurationMs"), m_lastConversionWallClockMs);
    metrics.insert(QStringLiteral("sourceBytes"), m_lastConversionSourceBytes);
    metrics.insert(QStringLiteral("tempBytes"), m_lastConversionTempBytes);
    metrics.insert(QStringLiteral("finalBytes"), m_lastConversionFinalBytes);
    metrics.insert(QStringLiteral("usedTemporaryFile"), m_lastConversionUsedTemporaryFile);
    metrics.insert(QStringLiteral("metadataCopyAttempted"), m_lastConversionMetadataCopyAttempted);
    metrics.insert(QStringLiteral("metadataCopySucceeded"), m_lastConversionMetadataCopySucceeded);
    metrics.insert(QStringLiteral("metadataCopyDurationUs"), m_lastConversionMetadataCopyDurationUs);
    metrics.insert(QStringLiteral("terminationKey"), m_lastConversionTerminationKey);
    return metrics;
}

QVariantMap AudioConverterService::preflight() const
{
    return buildPreflight();
}

QVariantMap AudioConverterService::statusPresentation() const
{
    QVariantMap result;
    result.insert(QStringLiteral("messageKey"), m_statusMessageKey);
    result.insert(QStringLiteral("messageArgs"), m_statusMessageArgs);
    result.insert(QStringLiteral("fallbackText"), m_statusText);
    return result;
}

QVariantMap AudioConverterService::errorPresentation() const
{
    QVariantMap result;
    result.insert(QStringLiteral("messageKey"), m_errorMessageKey);
    result.insert(QStringLiteral("messageArgs"), m_errorMessageArgs);
    result.insert(QStringLiteral("fallbackText"), m_lastError);
    return result;
}

bool AudioConverterService::startConversion()
{
    resetLastConversionMetrics();
    m_lastConversionStartedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_lastConversionSourceBytes = QFileInfo(m_sourceFile).exists()
        ? qMax<qint64>(0, QFileInfo(m_sourceFile).size())
        : 0;

    if (m_isRunning) {
        const QString message = QStringLiteral("Another audio conversion is already running.");
        finalizeLastConversionMetrics(QStringLiteral("rejected-already-running"));
        setErrorPresentation(QStringLiteral("audioConverter.runtimeFailedAlreadyRunning"),
                             {},
                             message);
        emit conversionFailed(message);
        return false;
    }

    const QVariantMap currentPreflight = buildPreflight();
    const QString validationError = preflightMessageText(currentPreflight);
    if (!validationError.isEmpty()) {
        finalizeLastConversionMetrics(QStringLiteral("rejected-validation"));
        setErrorPresentation(currentPreflight.value(QStringLiteral("messageKey")).toString(),
                             currentPreflight.value(QStringLiteral("messageArgs")).toList(),
                             validationError);
        setStatusPresentation(currentPreflight.value(QStringLiteral("messageKey")).toString(),
                              currentPreflight.value(QStringLiteral("messageArgs")).toList(),
                              validationError);
        emit conversionFailed(validationError);
        return false;
    }

    QString setupError;
    if (!setupConversionPipeline(&setupError)) {
        finalizeLastConversionMetrics(QStringLiteral("failed-setup"));
        failConversion(QStringLiteral("audioConverter.runtimeFailedStartPreparation"),
                       {},
                       QStringLiteral("failed-setup"),
                       setupError.isEmpty()
                           ? QStringLiteral("Failed to set up audio conversion pipeline.")
                           : setupError);
        return false;
    }

    m_cancelRequested = false;
    m_completionHandled = false;
    m_pendingFinalOutputFile = m_outputFile;
    if (!WaveFlux::isTrackerModuleSource(m_sourceFile) || m_sourceDurationMs <= 0) {
        m_sourceDurationMs = probeMetadataDurationMs(m_sourceFile);
    }
    m_progressStartMs = 0;
    m_progressDurationMs = m_sourceDurationMs;
    if (m_trimEnabled) {
        m_progressStartMs = qMax<qint64>(0, m_trimStartMs);
        const qint64 effectiveEndMs = m_trimEndMs > 0 ? m_trimEndMs : m_sourceDurationMs;
        if (effectiveEndMs > m_progressStartMs) {
            m_progressDurationMs = effectiveEndMs - m_progressStartMs;
        }
    }
    setProgress(m_pendingTrackerRenderFile.isEmpty() ? 0.0 : 0.15);
    setErrorPresentation(QString());
    setStatusPresentation(QStringLiteral("audioConverter.runtimeStarted"),
                          {},
                          QStringLiteral("Audio conversion started."));
    setIsRunning(true);
    emit conversionStarted();

    const GstState targetInitialState = m_trimEnabled ? GST_STATE_PAUSED : GST_STATE_PLAYING;
    const GstStateChangeReturn stateResult = gst_element_set_state(m_pipeline, targetInitialState);
    if (stateResult == GST_STATE_CHANGE_FAILURE) {
        failConversion(QStringLiteral("audioConverter.runtimeFailedStartPlayback"),
                       {},
                       QStringLiteral("runtime-failed-start-playback"),
                       QStringLiteral("Failed to start audio conversion pipeline."));
        return false;
    }

    refreshSourceDurationFromPipeline();
    if (m_trimEnabled) {
        GstState currentState = GST_STATE_NULL;
        GstState pendingState = GST_STATE_NULL;
        const GstStateChangeReturn prerollResult =
            gst_element_get_state(m_pipeline,
                                  &currentState,
                                  &pendingState,
                                  5 * GST_SECOND);
        if (prerollResult == GST_STATE_CHANGE_FAILURE) {
            failConversion(QStringLiteral("audioConverter.runtimeFailedStartPlayback"),
                           {},
                           QStringLiteral("runtime-failed-trim-preroll"),
                           QStringLiteral("Failed to preroll audio conversion pipeline for trimming."));
            return false;
        }

        QString trimError;
        if (!applyTrimSegmentSeek(&trimError)) {
            failConversion(QStringLiteral("audioConverter.runtimeFailedStartPlayback"),
                           {},
                           QStringLiteral("runtime-failed-trim-seek"),
                           trimError.isEmpty()
                               ? localizedConverterText(QStringLiteral("audioConverter.errorTrimSeekFailed"))
                               : trimError);
            return false;
        }

        const GstStateChangeReturn playResult = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (playResult == GST_STATE_CHANGE_FAILURE) {
            failConversion(QStringLiteral("audioConverter.runtimeFailedStartPlayback"),
                           {},
                           QStringLiteral("runtime-failed-start-playback"),
                           QStringLiteral("Failed to start trimmed audio conversion pipeline."));
            return false;
        }
    }

    m_busPollTimer.start();
    m_progressTimer.start();
    return true;
}

void AudioConverterService::cancelConversion()
{
    if (!m_isRunning) {
        return;
    }

    m_cancelRequested = true;
    m_completionHandled = true;
    teardownConversionPipeline();
    if (!m_pendingTempOutputFile.isEmpty()) {
        m_lastConversionTempBytes = QFileInfo(m_pendingTempOutputFile).exists()
            ? qMax<qint64>(0, QFileInfo(m_pendingTempOutputFile).size())
            : 0;
        QFile::remove(m_pendingTempOutputFile);
    }
    cleanupTemporaryTrackerSource();
    m_pendingTempOutputFile.clear();
    m_pendingFinalOutputFile.clear();
    setIsRunning(false);
    setProgress(0.0);
    setStatusPresentation(QStringLiteral("audioConverter.runtimeCanceled"),
                          {},
                          QStringLiteral("Audio conversion was canceled by the user."));
    setErrorPresentation(QString());
    setOverwriteExisting(false);
    finalizeLastConversionMetrics(QStringLiteral("canceled"));
    emit conversionCanceled();
}

void AudioConverterService::resetTransientState()
{
    if (m_isRunning) {
        return;
    }

    setProgress(0.0);
    setErrorPresentation(QString());
    setStatusPresentation(QString(), {}, QString());
    setOverwriteExisting(false);
    resetLastConversionMetrics();
    m_cancelRequested = false;
    m_completionHandled = false;
    emit preflightChanged();
}

QString AudioConverterService::suggestOutputFilePath(const QString &directoryOverride) const
{
    const QString sourcePath = normalizeLocalPath(m_sourceFile);
    if (sourcePath.isEmpty()) {
        return {};
    }

    const FormatProfile *profile = findFormatProfile(m_format);
    if (!profile) {
        return {};
    }

    QString directory = normalizeLocalPath(directoryOverride);
    if (directory.isEmpty()) {
        directory = QFileInfo(sourcePath).absolutePath();
    }
    if (directory.isEmpty()) {
        return {};
    }

    const QString candidate = QDir(directory).filePath(
        QStringLiteral("%1 (converted).%2")
            .arg(baseNameForOutput(sourcePath), QString::fromLatin1(profile->extension)));
    return uniqueOutputPath(candidate);
}

bool AudioConverterService::supportsCurrentFormatBitrate() const
{
    const FormatProfile *profile = findFormatProfile(m_format);
    return profile && profile->supportsBitrate;
}

bool AudioConverterService::supportsCurrentFormatSampleRate() const
{
    const FormatProfile *profile = findFormatProfile(m_format);
    return profile && profile->supportsSampleRate;
}

bool AudioConverterService::supportsCurrentFormatChannels() const
{
    const FormatProfile *profile = findFormatProfile(m_format);
    return profile && profile->supportsChannels;
}

bool AudioConverterService::outputFileExists(const QString &path) const
{
    const QString candidate = normalizeLocalPath(path.isEmpty() ? m_outputFile : path);
    return !candidate.isEmpty() && QFileInfo::exists(candidate);
}

void AudioConverterService::setSourceFile(const QString &sourceFile)
{
    const QString normalized = normalizeLocalPath(sourceFile);
    if (m_sourceFile == normalized) {
        return;
    }

    m_sourceFile = normalized;
    emit sourceFileChanged();

    if (m_outputFile.isEmpty() && !m_sourceFile.isEmpty()) {
        const QString suggested = suggestOutputFilePath();
        if (!suggested.isEmpty()) {
            m_outputFile = suggested;
            emit outputFileChanged();
        }
    }

    emit preflightChanged();
}

void AudioConverterService::setOutputFile(const QString &outputFile)
{
    const QString normalized = normalizeLocalPath(outputFile);
    if (m_outputFile == normalized) {
        return;
    }

    m_outputFile = normalized;
    setOverwriteExisting(false);
    emit outputFileChanged();
    emit preflightChanged();
}

void AudioConverterService::setFormat(const QString &format)
{
    const QString normalized = normalizeFormat(format);
    if (m_format == normalized) {
        return;
    }

    m_format = normalized;
    const FormatProfile *profile = findFormatProfile(m_format);
    m_bitrate = normalizeBitrateForFormat(profile ? profile->defaultBitrateKbps : m_bitrate, m_format);
    m_sampleRate = normalizeSampleRate(profile ? profile->defaultSampleRateHz : m_sampleRate, m_format);
    if (!m_outputFile.isEmpty()) {
        m_outputFile = replaceExtension(m_outputFile, findFormatProfile(m_format)->extension);
        emit outputFileChanged();
    } else if (!m_sourceFile.isEmpty()) {
        const QString suggested = suggestOutputFilePath();
        if (!suggested.isEmpty()) {
            m_outputFile = suggested;
            emit outputFileChanged();
        }
    }

    emit formatChanged();
    emit bitrateChanged();
    emit sampleRateChanged();
    emit preflightChanged();
}

void AudioConverterService::setBitrate(int bitrate)
{
    const int normalized = normalizeBitrateForFormat(bitrate, m_format);
    if (m_bitrate == normalized) {
        return;
    }

    m_bitrate = normalized;
    emit bitrateChanged();
    emit preflightChanged();
}

void AudioConverterService::setSampleRate(int sampleRate)
{
    const int normalized = normalizeSampleRate(sampleRate, m_format);
    if (m_sampleRate == normalized) {
        return;
    }

    m_sampleRate = normalized;
    emit sampleRateChanged();
    emit preflightChanged();
}

void AudioConverterService::setChannelMode(const QString &channelMode)
{
    const QString normalized = normalizeChannelMode(channelMode);
    if (m_channelMode == normalized) {
        return;
    }

    m_channelMode = normalized;
    emit channelModeChanged();
    emit preflightChanged();
}

void AudioConverterService::setPlaybackRate(double playbackRate)
{
    const double normalized = normalizePlaybackRate(playbackRate);
    if (qFuzzyCompare(m_playbackRate, normalized)) {
        return;
    }

    m_playbackRate = normalized;
    emit playbackRateChanged();
    emit preflightChanged();
}

void AudioConverterService::setPitchSemitones(int pitchSemitones)
{
    const int normalized = normalizePitchSemitones(pitchSemitones);
    if (m_pitchSemitones == normalized) {
        return;
    }

    m_pitchSemitones = normalized;
    emit pitchSemitonesChanged();
    emit preflightChanged();
}

void AudioConverterService::setApplyEqualizer(bool applyEqualizer)
{
    if (m_applyEqualizer == applyEqualizer) {
        return;
    }

    m_applyEqualizer = applyEqualizer;
    emit applyEqualizerChanged();
    emit preflightChanged();
}

void AudioConverterService::setEqualizerBandGains(const QVariantList &gains)
{
    const QVariantList normalized = normalizeEqualizerBandGains(gains);
    if (m_equalizerBandGains.size() == normalized.size()) {
        bool equal = true;
        for (int i = 0; i < normalized.size(); ++i) {
            if (qAbs(m_equalizerBandGains.at(i).toDouble() - normalized.at(i).toDouble()) > 0.01) {
                equal = false;
                break;
            }
        }
        if (equal) {
            return;
        }
    }

    m_equalizerBandGains = normalized;
    emit equalizerBandGainsChanged();
}

void AudioConverterService::setApplyReverb(bool applyReverb)
{
    if (m_applyReverb == applyReverb) {
        return;
    }

    m_applyReverb = applyReverb;
    emit applyReverbChanged();
    emit preflightChanged();
}

void AudioConverterService::setReverbRoomSize(double roomSize)
{
    const double normalized = normalizeUnitInterval(roomSize, 0.55);
    if (qFuzzyCompare(m_reverbRoomSize, normalized)) {
        return;
    }

    m_reverbRoomSize = normalized;
    emit reverbRoomSizeChanged();
}

void AudioConverterService::setReverbDamping(double damping)
{
    const double normalized = normalizeUnitInterval(damping, 0.35);
    if (qFuzzyCompare(m_reverbDamping, normalized)) {
        return;
    }

    m_reverbDamping = normalized;
    emit reverbDampingChanged();
}

void AudioConverterService::setReverbWetLevel(double wetLevel)
{
    const double normalized = normalizeUnitInterval(wetLevel, 0.28);
    if (qFuzzyCompare(m_reverbWetLevel, normalized)) {
        return;
    }

    m_reverbWetLevel = normalized;
    emit reverbWetLevelChanged();
}

void AudioConverterService::setTrimEnabled(bool enabled)
{
    if (m_trimEnabled == enabled) {
        return;
    }

    m_trimEnabled = enabled;
    emit trimEnabledChanged();
    emit preflightChanged();
}

void AudioConverterService::setTrimStartMs(qint64 startMs)
{
    const qint64 normalized = qMax<qint64>(0, startMs);
    if (m_trimStartMs == normalized) {
        return;
    }

    m_trimStartMs = normalized;
    emit trimStartMsChanged();
    emit preflightChanged();
}

void AudioConverterService::setTrimEndMs(qint64 endMs)
{
    const qint64 normalized = qMax<qint64>(0, endMs);
    if (m_trimEndMs == normalized) {
        return;
    }

    m_trimEndMs = normalized;
    emit trimEndMsChanged();
    emit preflightChanged();
}

void AudioConverterService::setOverwriteExisting(bool overwriteExisting)
{
    if (m_overwriteExisting == overwriteExisting) {
        return;
    }

    m_overwriteExisting = overwriteExisting;
    emit overwriteExistingChanged();
    emit preflightChanged();
}

void AudioConverterService::teardownConversionPipeline()
{
    m_busPollTimer.stop();
    m_progressTimer.stop();

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    if (m_pipeline) {
        if (m_decodeBin) {
            g_signal_handlers_disconnect_by_data(m_decodeBin, this);
        }
        gst_object_unref(m_pipeline);
    }

    m_pipeline = nullptr;
    m_decodeBin = nullptr;
    m_convertElement = nullptr;
    m_resampleElement = nullptr;
    m_pitchElement = nullptr;
    m_reverbElement = nullptr;
    m_equalizerElement = nullptr;
    m_postConvertElement = nullptr;
    m_finalConvertElement = nullptr;
    m_capsFilterElement = nullptr;
    m_encoderElement = nullptr;
    m_muxerElement = nullptr;
    m_sinkElement = nullptr;
}

void AudioConverterService::handleBusMessage(GstMessage *message)
{
    if (!message || m_completionHandled) {
        return;
    }

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        const QString detailText = error && error->message
            ? QString::fromUtf8(error->message)
            : QStringLiteral("unknown GStreamer error");
        if (debug && *debug) {
            qWarning().noquote() << "[AudioConverterService]" << detailText << "|" << QString::fromUtf8(debug);
        } else {
            qWarning().noquote() << "[AudioConverterService]" << detailText;
        }
        if (error) {
            g_error_free(error);
        }
        if (debug) {
            g_free(debug);
        }
        failConversion(QStringLiteral("audioConverter.runtimeFailedPipeline"),
                       {},
                       QStringLiteral("runtime-failed-pipeline"),
                       QStringLiteral("Audio conversion pipeline failed: %1").arg(detailText));
        break;
    }
    case GST_MESSAGE_EOS:
        finalizeSuccessfulConversion();
        break;
    default:
        break;
    }
}

bool AudioConverterService::setupConversionPipeline(QString *errorMessage)
{
    teardownConversionPipeline();

    QString pipelineSourcePath = m_sourceFile;
    if (!prepareTrackerSourceForConversion(&pipelineSourcePath, errorMessage)) {
        cleanupTemporaryTrackerSource();
        return false;
    }

    const QString sourceUri = localFileUri(pipelineSourcePath);
    if (sourceUri.isEmpty()) {
        if (errorMessage) {
            *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorSourcePathInvalid"));
        }
        cleanupTemporaryTrackerSource();
        return false;
    }

    const FormatProfile *profile = findFormatProfile(m_format);
    if (!profile) {
        if (errorMessage) {
            *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorFormatUnsupported"));
        }
        cleanupTemporaryTrackerSource();
        return false;
    }

    auto ensureFactory = [&](const char *factoryName, const QString &label) -> bool {
        if (!factoryName || !*factoryName) {
            return true;
        }
        GstElementFactory *factory = gst_element_factory_find(factoryName);
        if (!factory) {
            if (errorMessage) {
                *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorMissingPlugin"))
                                    .arg(label);
            }
            return false;
        }
        gst_object_unref(factory);
        return true;
    };

    if (!ensureFactory("uridecodebin", QStringLiteral("uridecodebin"))
        || !ensureFactory("audioconvert", QStringLiteral("audioconvert"))
        || !ensureFactory("audioresample", QStringLiteral("audioresample"))
        || !ensureFactory("pitch", QStringLiteral("pitch"))
        || !ensureFactory("capsfilter", QStringLiteral("capsfilter"))
        || !ensureFactory("audioconvert", QStringLiteral("post-audioconvert"))
        || (m_applyReverb && !ensureFactory("freeverb", QStringLiteral("freeverb")))
        || (m_applyEqualizer && !ensureFactory("equalizer-nbands", QStringLiteral("equalizer-nbands")))
        || !ensureFactory(profile->gstreamerMuxer, QString::fromLatin1(profile->gstreamerMuxer))
        || (!QString::fromLatin1(profile->gstreamerEncoder).isEmpty()
            && QString::fromLatin1(profile->gstreamerEncoder) != QStringLiteral("identity")
            && !ensureFactory(profile->gstreamerEncoder, QString::fromLatin1(profile->gstreamerEncoder)))
        || !ensureFactory("filesink", QStringLiteral("filesink"))) {
        return false;
    }

    m_pendingTempOutputFile = createTemporaryOutputPath();
    if (m_pendingTempOutputFile.isEmpty()) {
        if (errorMessage) {
            *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorTempOutputPath"));
        }
        cleanupTemporaryTrackerSource();
        return false;
    }
    m_lastConversionUsedTemporaryFile = true;

    m_pipeline = gst_pipeline_new("waveflux-audio-converter");
    m_decodeBin = gst_element_factory_make("uridecodebin", "converter-source");
    m_convertElement = gst_element_factory_make("audioconvert", "converter-convert");
    m_resampleElement = gst_element_factory_make("audioresample", "converter-resample");
    m_pitchElement = gst_element_factory_make("pitch", "converter-pitch");
    m_reverbElement = m_applyReverb
        ? gst_element_factory_make("freeverb", "converter-reverb")
        : nullptr;
    m_equalizerElement = m_applyEqualizer
        ? gst_element_factory_make("equalizer-nbands", "converter-equalizer")
        : nullptr;
    m_postConvertElement = gst_element_factory_make("audioconvert", "converter-post-convert");
    m_finalConvertElement = gst_element_factory_make("audioconvert", "converter-final-convert");
    m_capsFilterElement = gst_element_factory_make("capsfilter", "converter-caps");
    m_muxerElement = QString::fromLatin1(profile->gstreamerMuxer).isEmpty()
        ? nullptr
        : gst_element_factory_make(profile->gstreamerMuxer, "converter-muxer");
    m_sinkElement = gst_element_factory_make("filesink", "converter-sink");

    if (QString::fromLatin1(profile->gstreamerEncoder) == QStringLiteral("identity")) {
        m_encoderElement = nullptr;
    } else {
        m_encoderElement = gst_element_factory_make(profile->gstreamerEncoder, "converter-encoder");
    }

    if (!m_pipeline || !m_decodeBin || !m_convertElement || !m_resampleElement
        || !m_pitchElement || !m_postConvertElement || !m_finalConvertElement
        || !m_capsFilterElement || !m_sinkElement
        || (m_applyReverb && !m_reverbElement)
        || (m_applyEqualizer && !m_equalizerElement)
        || (!QString::fromLatin1(profile->gstreamerMuxer).isEmpty() && !m_muxerElement)
        || (QString::fromLatin1(profile->gstreamerEncoder) != QStringLiteral("identity") && !m_encoderElement)) {
        if (errorMessage) {
            *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorCreatePipeline"));
        }
        teardownConversionPipeline();
        cleanupTemporaryTrackerSource();
        return false;
    }

    g_object_set(m_decodeBin, "uri", sourceUri.toUtf8().constData(), nullptr);
    g_object_set(m_sinkElement, "location", m_pendingTempOutputFile.toUtf8().constData(), nullptr);

    if (hasElementProperty(m_pitchElement, "tempo")) {
        g_object_set(m_pitchElement, "tempo", m_playbackRate, nullptr);
    }
    if (hasElementProperty(m_pitchElement, "pitch")) {
        const double pitchRatio = std::pow(2.0, m_pitchSemitones / 12.0);
        g_object_set(m_pitchElement, "pitch", pitchRatio, nullptr);
    }
    if (m_equalizerElement) {
        g_object_set(m_equalizerElement, "num-bands", static_cast<guint>(m_equalizerBandGains.size()), nullptr);
        for (int i = 0; i < m_equalizerBandGains.size(); ++i) {
            const QByteArray gainProperty = QByteArray("band") + QByteArray::number(i) + QByteArray("::gain");
            gst_child_proxy_set(GST_CHILD_PROXY(m_equalizerElement),
                                gainProperty.constData(),
                                m_equalizerBandGains.at(i).toDouble(),
                                nullptr);
        }
    }
    if (m_reverbElement) {
        g_object_set(m_reverbElement,
                     "room-size", m_reverbRoomSize,
                     "damping", m_reverbDamping,
                     "level", m_reverbWetLevel,
                     "width", 1.0,
                     nullptr);
    }

    GstCaps *caps = gst_caps_new_simple("audio/x-raw",
                                        "rate", G_TYPE_INT, m_sampleRate,
                                        "channels", G_TYPE_INT, channelsForMode(m_channelMode),
                                        nullptr);
    g_object_set(m_capsFilterElement, "caps", caps, nullptr);
    gst_caps_unref(caps);

    if (profile->supportsBitrate && m_encoderElement && hasElementProperty(m_encoderElement, "bitrate")) {
        const QString profileId = QString::fromLatin1(profile->id);
        const int bitrateValue = (profileId == QStringLiteral("opus")
                                  || profileId == QStringLiteral("webm"))
            ? m_bitrate * 1000
            : m_bitrate;
        g_object_set(m_encoderElement, "bitrate", bitrateValue, nullptr);
    }

    if (QString::fromLatin1(profile->id) == QStringLiteral("mp3") && m_encoderElement) {
        if (hasElementProperty(m_encoderElement, "target")) {
            gst_util_set_object_arg(G_OBJECT(m_encoderElement), "target", "bitrate");
        }
        if (hasElementProperty(m_encoderElement, "cbr")) {
            g_object_set(m_encoderElement, "cbr", TRUE, nullptr);
        }
    }

    if (profile->supportsCompressionLevel && m_encoderElement && hasElementProperty(m_encoderElement, "compression-level")) {
        g_object_set(m_encoderElement, "compression-level", 5, nullptr);
    }

    gst_bin_add(GST_BIN(m_pipeline), m_decodeBin);
    gst_bin_add(GST_BIN(m_pipeline), m_convertElement);
    gst_bin_add(GST_BIN(m_pipeline), m_resampleElement);
    gst_bin_add(GST_BIN(m_pipeline), m_pitchElement);
    if (m_reverbElement) {
        gst_bin_add(GST_BIN(m_pipeline), m_reverbElement);
    }
    if (m_equalizerElement) {
        gst_bin_add(GST_BIN(m_pipeline), m_equalizerElement);
    }
    gst_bin_add(GST_BIN(m_pipeline), m_postConvertElement);
    gst_bin_add(GST_BIN(m_pipeline), m_finalConvertElement);
    gst_bin_add(GST_BIN(m_pipeline), m_capsFilterElement);
    if (m_encoderElement) {
        gst_bin_add(GST_BIN(m_pipeline), m_encoderElement);
    }
    if (m_muxerElement) {
        gst_bin_add(GST_BIN(m_pipeline), m_muxerElement);
    }
    gst_bin_add(GST_BIN(m_pipeline), m_sinkElement);

    bool linkOk = linkWithDiagnostics(m_convertElement,
                                      m_resampleElement,
                                      QStringLiteral("Failed to link converter -> resampler"),
                                      errorMessage)
        && linkWithDiagnostics(m_resampleElement,
                               m_pitchElement,
                               QStringLiteral("Failed to link resampler -> pitch"),
                               errorMessage)
        && linkWithDiagnostics(m_pitchElement,
                               m_postConvertElement,
                               QStringLiteral("Failed to link pitch -> post-convert"),
                               errorMessage)
        && linkWithDiagnostics(m_postConvertElement,
                               m_applyReverb ? m_reverbElement
                                             : (m_applyEqualizer ? m_equalizerElement
                                                                 : m_finalConvertElement),
                               m_applyReverb
                                   ? QStringLiteral("Failed to link post-convert -> reverb")
                                   : (m_applyEqualizer
                                      ? QStringLiteral("Failed to link post-convert -> equalizer")
                                      : QStringLiteral("Failed to link post-convert -> final convert")),
                               errorMessage)
        && (!m_applyReverb
            || linkWithDiagnostics(m_reverbElement,
                                   m_applyEqualizer ? m_equalizerElement : m_finalConvertElement,
                                   m_applyEqualizer
                                       ? QStringLiteral("Failed to link reverb -> equalizer")
                                       : QStringLiteral("Failed to link reverb -> final convert"),
                                   errorMessage))
        && (!m_applyEqualizer
            || linkWithDiagnostics(m_equalizerElement,
                                   m_finalConvertElement,
                                   QStringLiteral("Failed to link equalizer -> final convert"),
                                   errorMessage))
        && linkWithDiagnostics(m_finalConvertElement,
                               m_capsFilterElement,
                               QStringLiteral("Failed to link final convert -> caps filter"),
                               errorMessage);
    if (!linkOk) {
        teardownConversionPipeline();
        cleanupTemporaryTrackerSource();
        return false;
    }

    if (m_encoderElement) {
        if (m_muxerElement) {
            linkOk = linkWithDiagnostics(m_capsFilterElement,
                                         m_encoderElement,
                                         QStringLiteral("Failed to link caps filter -> encoder"),
                                         errorMessage)
                && linkWithDiagnostics(m_encoderElement,
                                       m_muxerElement,
                                       QStringLiteral("Failed to link encoder -> muxer"),
                                       errorMessage)
                && linkWithDiagnostics(m_muxerElement,
                                       m_sinkElement,
                                       QStringLiteral("Failed to link muxer -> sink"),
                                       errorMessage);
        } else {
            linkOk = linkWithDiagnostics(m_capsFilterElement,
                                         m_encoderElement,
                                         QStringLiteral("Failed to link caps filter -> encoder"),
                                         errorMessage)
                && linkWithDiagnostics(m_encoderElement,
                                       m_sinkElement,
                                       QStringLiteral("Failed to link encoder -> sink"),
                                       errorMessage);
        }
    } else {
        if (m_muxerElement) {
            linkOk = linkWithDiagnostics(m_capsFilterElement,
                                         m_muxerElement,
                                         QStringLiteral("Failed to link caps filter -> muxer"),
                                         errorMessage)
                && linkWithDiagnostics(m_muxerElement,
                                       m_sinkElement,
                                       QStringLiteral("Failed to link muxer -> sink"),
                                       errorMessage);
        } else {
            linkOk = linkWithDiagnostics(m_capsFilterElement,
                                         m_sinkElement,
                                         QStringLiteral("Failed to link caps filter -> sink"),
                                         errorMessage);
        }
    }

    if (!linkOk) {
        teardownConversionPipeline();
        cleanupTemporaryTrackerSource();
        return false;
    }

    g_signal_connect(m_decodeBin,
                     "pad-added",
                     G_CALLBACK(+[](GstElement *, GstPad *newPad, gpointer userData) {
                         auto *service = static_cast<AudioConverterService *>(userData);
                         if (!service || !service->m_convertElement) {
                             return;
                         }

                         GstPad *sinkPad = gst_element_get_static_pad(service->m_convertElement, "sink");
                         if (!sinkPad) {
                             return;
                         }

                         if (gst_pad_is_linked(sinkPad)) {
                             gst_object_unref(sinkPad);
                             return;
                         }

                         GstCaps *caps = gst_pad_query_caps(newPad, nullptr);
                         bool isAudio = false;
                         if (caps) {
                             const GstStructure *structure = gst_caps_get_structure(caps, 0);
                             if (structure) {
                                 const gchar *name = gst_structure_get_name(structure);
                                 isAudio = name && g_str_has_prefix(name, "audio/");
                             }
                             gst_caps_unref(caps);
                         }

                         if (isAudio) {
                             gst_pad_link(newPad, sinkPad);
                         }

                         gst_object_unref(sinkPad);
                     }),
                     this);

    m_bus = gst_element_get_bus(m_pipeline);
    if (!m_bus) {
        cleanupTemporaryTrackerSource();
        return false;
    }
    return true;
}

QString AudioConverterService::createTemporaryOutputPath() const
{
    const QString finalPath = QFileInfo(m_outputFile).absoluteFilePath();
    if (finalPath.isEmpty()) {
        return {};
    }

    const QFileInfo info(finalPath);
    return QDir(info.absolutePath()).filePath(
        QStringLiteral(".%1.waveflux-tmp-%2")
            .arg(info.fileName(),
                 QString::number(QDateTime::currentMSecsSinceEpoch())));
}

QString AudioConverterService::createTemporaryTrackerRenderPath() const
{
    const QString finalPath = QFileInfo(m_outputFile).absoluteFilePath();
    if (finalPath.isEmpty()) {
        return {};
    }

    const QFileInfo info(finalPath);
    return QDir(info.absolutePath()).filePath(
        QStringLiteral(".%1.waveflux-tracker-render-%2.wav")
            .arg(info.completeBaseName(),
                 QString::number(QDateTime::currentMSecsSinceEpoch())));
}

bool AudioConverterService::prepareTrackerSourceForConversion(QString *pipelineSourcePath,
                                                              QString *errorMessage)
{
    if (pipelineSourcePath) {
        *pipelineSourcePath = m_sourceFile;
    }

    cleanupTemporaryTrackerSource();
    if (!WaveFlux::isTrackerModuleSource(m_sourceFile)) {
        return true;
    }

    const QString renderPath = createTemporaryTrackerRenderPath();
    if (renderPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorTrackerRenderPath"));
        }
        return false;
    }

    qint64 renderedDurationMs = 0;
    QString renderError;
    if (!renderTrackerModuleToWaveFile(m_sourceFile, renderPath, &renderedDurationMs, &renderError)) {
        if (errorMessage) {
            *errorMessage = renderError.isEmpty()
                ? localizedConverterText(QStringLiteral("audioConverter.errorTrackerRenderFailed"))
                : renderError;
        }
        QFile::remove(renderPath);
        return false;
    }

    m_pendingTrackerRenderFile = renderPath;
    if (renderedDurationMs > 0) {
        m_sourceDurationMs = renderedDurationMs;
    }
    if (pipelineSourcePath) {
        *pipelineSourcePath = renderPath;
    }
    return true;
}

void AudioConverterService::cleanupTemporaryTrackerSource()
{
    if (m_pendingTrackerRenderFile.isEmpty()) {
        return;
    }

    QFile::remove(m_pendingTrackerRenderFile);
    m_pendingTrackerRenderFile.clear();
}

void AudioConverterService::finalizeSuccessfulConversion()
{
    if (m_completionHandled) {
        return;
    }
    m_completionHandled = true;

    teardownConversionPipeline();

    if (QFileInfo::exists(m_pendingFinalOutputFile)) {
        if (!m_overwriteExisting) {
            QFile::remove(m_pendingTempOutputFile);
            failConversion(QStringLiteral("audioConverter.runtimeFailedExistingOutput"),
                           {m_pendingFinalOutputFile},
                           QStringLiteral("runtime-failed-existing-output"),
                           localizedConverterText(QStringLiteral("audioConverter.errorOutputAlreadyExists"))
                               .arg(m_pendingFinalOutputFile));
            return;
        }

        QFile existingOutputFile(m_pendingFinalOutputFile);
        if (!existingOutputFile.remove()) {
            QFile::remove(m_pendingTempOutputFile);
            failConversion(QStringLiteral("audioConverter.runtimeFailedReplaceExisting"),
                           {m_pendingFinalOutputFile},
                           QStringLiteral("runtime-failed-replace-existing"),
                           localizedConverterText(QStringLiteral("audioConverter.errorReplaceExistingOutput"))
                               .arg(m_pendingFinalOutputFile, existingOutputFile.errorString()));
            return;
        }
    }

    QFile tempOutputFile(m_pendingTempOutputFile);
    if (!tempOutputFile.rename(m_pendingFinalOutputFile)) {
        QFile::remove(m_pendingTempOutputFile);
        failConversion(QStringLiteral("audioConverter.runtimeFailedFinalizeOutput"),
                       {m_pendingFinalOutputFile},
                       QStringLiteral("runtime-failed-finalize-output"),
                       localizedConverterText(QStringLiteral("audioConverter.errorFinalizeOutput"))
                           .arg(m_pendingFinalOutputFile, tempOutputFile.errorString()));
        return;
    }

    const QString outputPath = m_pendingFinalOutputFile;
    m_lastConversionTempBytes = QFileInfo(m_pendingTempOutputFile).exists()
        ? qMax<qint64>(0, QFileInfo(m_pendingTempOutputFile).size())
        : 0;
    m_lastConversionUsedTemporaryFile = !m_pendingTempOutputFile.isEmpty();
    m_pendingTempOutputFile.clear();
    m_pendingFinalOutputFile.clear();
    QString metadataWarning;
    QElapsedTimer metadataTimer;
    metadataTimer.start();
    const bool metadataCopied = copyBasicSourceTagsToOutput(outputPath, &metadataWarning);
    m_lastConversionMetadataCopyDurationUs = metadataTimer.nsecsElapsed() / 1000;
    m_lastConversionMetadataCopyAttempted = true;
    m_lastConversionMetadataCopySucceeded = metadataCopied;
    m_lastConversionFinalBytes = QFileInfo(outputPath).exists()
        ? qMax<qint64>(0, QFileInfo(outputPath).size())
        : 0;
    cleanupTemporaryTrackerSource();
    setProgress(1.0);
    setErrorPresentation(QString());
    setStatusPresentation(metadataCopied || metadataWarning.isEmpty()
                              ? QStringLiteral("audioConverter.runtimeSucceeded")
                              : QStringLiteral("audioConverter.runtimeSucceededMetadataSkipped"),
                          metadataWarning.isEmpty() ? QVariantList() : QVariantList{metadataWarning},
                          metadataCopied
                              ? AppSettingsManager::translateForCurrentLanguage(
                                    QStringLiteral("audioConverter.runtimeSucceeded"))
                              : (!metadataWarning.isEmpty()
                                     ? AppSettingsManager::translateForCurrentLanguage(
                                           QStringLiteral("audioConverter.runtimeSucceededMetadataSkipped"))
                                           .arg(metadataWarning)
                                     : AppSettingsManager::translateForCurrentLanguage(
                                           QStringLiteral("audioConverter.runtimeSucceeded"))));
    setOverwriteExisting(false);
    setIsRunning(false);
    finalizeLastConversionMetrics(QStringLiteral("succeeded"));
    emit conversionFinished(outputPath);
}

void AudioConverterService::failConversion(const QString &messageKey,
                                           const QVariantList &messageArgs,
                                           const QString &terminationKey,
                                           const QString &diagnosticMessage)
{
    const bool wasRunning = m_isRunning;
    teardownConversionPipeline();

    if (!diagnosticMessage.trimmed().isEmpty()) {
        qWarning().noquote() << "[AudioConverterService]" << diagnosticMessage;
    }

    if (!m_pendingTempOutputFile.isEmpty()) {
        m_lastConversionTempBytes = QFileInfo(m_pendingTempOutputFile).exists()
            ? qMax<qint64>(0, QFileInfo(m_pendingTempOutputFile).size())
            : 0;
        QFile::remove(m_pendingTempOutputFile);
    }
    cleanupTemporaryTrackerSource();
    m_pendingTempOutputFile.clear();
    m_pendingFinalOutputFile.clear();
    setIsRunning(false);
    setProgress(0.0);
    setErrorPresentation(messageKey, messageArgs, diagnosticMessage);
    setStatusPresentation(messageKey, messageArgs, diagnosticMessage);
    setOverwriteExisting(false);
    finalizeLastConversionMetrics(terminationKey);
    if (wasRunning || !diagnosticMessage.isEmpty()) {
        emit conversionFailed(diagnosticMessage);
    }
}

bool AudioConverterService::applyTrimSegmentSeek(QString *errorMessage)
{
    if (!m_pipeline || !m_trimEnabled) {
        return true;
    }

    const qint64 startMs = qMax<qint64>(0, m_trimStartMs);
    qint64 stopMs = qMax<qint64>(0, m_trimEndMs);
    if (stopMs <= 0 && m_sourceDurationMs > 0) {
        stopMs = m_sourceDurationMs;
    }
    if (stopMs > 0 && stopMs <= startMs) {
        if (errorMessage) {
            *errorMessage = localizedConverterText(QStringLiteral("audioConverter.preflightTrimInvalid"));
        }
        return false;
    }

    const GstSeekType stopType = stopMs > 0 ? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_NONE;
    const gint64 stopNs = stopMs > 0 ? static_cast<gint64>(stopMs) * GST_MSECOND : GST_CLOCK_TIME_NONE;
    const gboolean ok = gst_element_seek(m_pipeline,
                                         1.0,
                                         GST_FORMAT_TIME,
                                         static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                         GST_SEEK_TYPE_SET,
                                         static_cast<gint64>(startMs) * GST_MSECOND,
                                         stopType,
                                         stopNs);
    if (!ok && errorMessage) {
        *errorMessage = localizedConverterText(QStringLiteral("audioConverter.errorTrimSeekFailed"));
    }
    return ok;
}

void AudioConverterService::updateProgressFromPipeline()
{
    if (!m_pipeline) {
        return;
    }

    if (m_sourceDurationMs <= 0 && !refreshSourceDurationFromPipeline()) {
        return;
    }

    gint64 positionNs = 0;
    if (!gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &positionNs)) {
        return;
    }

    const qint64 positionMs = static_cast<qint64>(positionNs / GST_MSECOND);
    if (positionMs <= 0) {
        return;
    }

    const qint64 denominator = m_progressDurationMs > 0 ? m_progressDurationMs : m_sourceDurationMs;
    const qint64 adjustedPositionMs = m_progressDurationMs > 0
        ? (positionMs >= m_progressStartMs ? positionMs - m_progressStartMs : positionMs)
        : positionMs;
    if (denominator <= 0) {
        return;
    }
    setProgress(static_cast<double>(adjustedPositionMs) / static_cast<double>(denominator));
}

bool AudioConverterService::refreshSourceDurationFromPipeline()
{
    if (!m_pipeline || m_sourceDurationMs > 0) {
        return m_sourceDurationMs > 0;
    }

    gint64 durationNs = 0;
    if (!gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &durationNs)) {
        return false;
    }

    const qint64 durationMs = static_cast<qint64>(durationNs / GST_MSECOND);
    if (durationMs <= 0) {
        return false;
    }

    m_sourceDurationMs = durationMs;
    return true;
}

bool AudioConverterService::copyBasicSourceTagsToOutput(const QString &outputPath, QString *warningMessage)
{
    if (warningMessage) {
        warningMessage->clear();
    }

    const QString sourcePath = normalizeLocalPath(m_sourceFile);
    const QString targetPath = normalizeLocalPath(outputPath);
    if (sourcePath.isEmpty() || targetPath.isEmpty()) {
        if (warningMessage) {
            *warningMessage = QStringLiteral("source or output path is invalid");
        }
        return false;
    }

    auto sourceFile = WaveFlux::TagLibPath::makeFileRef(sourcePath, false);
    if (sourceFile.isNull() || !sourceFile.tag()) {
        if (warningMessage) {
            *warningMessage = QStringLiteral("failed to read source tags");
        }
        return false;
    }

    const QString title = QString::fromUtf8(sourceFile.tag()->title().toCString(true)).trimmed();
    const QString artist = QString::fromUtf8(sourceFile.tag()->artist().toCString(true)).trimmed();
    const QString album = QString::fromUtf8(sourceFile.tag()->album().toCString(true)).trimmed();
    if (title.isEmpty() && artist.isEmpty() && album.isEmpty()) {
        return false;
    }

    auto targetFile = WaveFlux::TagLibPath::makeFileRef(targetPath, false);
    if (targetFile.isNull() || !targetFile.tag()) {
        if (warningMessage) {
            *warningMessage = QStringLiteral("failed to open output tags");
        }
        return false;
    }

    targetFile.tag()->setTitle(toTagLibString(title));
    targetFile.tag()->setArtist(toTagLibString(artist));
    targetFile.tag()->setAlbum(toTagLibString(album));
    if (!targetFile.save()) {
        if (warningMessage) {
            *warningMessage = QStringLiteral("failed to save output tags");
        }
        return false;
    }

    return true;
}

void AudioConverterService::resetLastConversionMetrics()
{
    m_lastConversionStartedAtMs = 0;
    m_lastConversionFinishedAtMs = 0;
    m_lastConversionWallClockMs = 0;
    m_lastConversionSourceBytes = 0;
    m_lastConversionTempBytes = 0;
    m_lastConversionFinalBytes = 0;
    m_lastConversionUsedTemporaryFile = false;
    m_lastConversionMetadataCopyAttempted = false;
    m_lastConversionMetadataCopySucceeded = false;
    m_lastConversionMetadataCopyDurationUs = 0;
    m_lastConversionTerminationKey = QStringLiteral("not-started");
}

void AudioConverterService::finalizeLastConversionMetrics(const QString &terminationKey)
{
    if (!m_lastConversionFinishedAtMs) {
        m_lastConversionFinishedAtMs = QDateTime::currentMSecsSinceEpoch();
    }
    if (m_lastConversionStartedAtMs > 0) {
        m_lastConversionWallClockMs =
            qMax<qint64>(0, m_lastConversionFinishedAtMs - m_lastConversionStartedAtMs);
    }
    m_lastConversionTerminationKey = terminationKey;
}

QString AudioConverterService::normalizeFormat(const QString &format)
{
    const QString normalized = format.trimmed().toLower();
    for (const FormatProfile &profile : kFormatProfiles) {
        if (normalized == QLatin1String(profile.id)) {
            return normalized;
        }
    }
    return QStringLiteral("mp3");
}

QString AudioConverterService::normalizeChannelMode(const QString &channelMode)
{
    const QString normalized = channelMode.trimmed().toLower();
    if (normalized == QStringLiteral("mono") || normalized == QStringLiteral("stereo")) {
        return normalized;
    }
    return QStringLiteral("stereo");
}

int AudioConverterService::normalizeBitrateForFormat(int bitrate, const QString &format)
{
    const FormatProfile *profile = findFormatProfile(format);
    if (!profile || !profile->supportsBitrate) {
        return 0;
    }

    const QString profileId = QString::fromLatin1(profile->id);
    if (profileId == QStringLiteral("opus") || profileId == QStringLiteral("webm")) {
        static const int kAllowedBitrates[] = {64, 96, 128, 160, 192, 256};
        int best = kAllowedBitrates[0];
        int bestDistance = qAbs(kAllowedBitrates[0] - bitrate);
        for (int candidate : kAllowedBitrates) {
            const int distance = qAbs(candidate - bitrate);
            if (distance < bestDistance) {
                best = candidate;
                bestDistance = distance;
            }
        }
        return best;
    }

    static const int kAllowedBitrates[] = {64, 96, 128, 160, 192, 224, 256, 320};
    int best = kAllowedBitrates[0];
    int bestDistance = qAbs(kAllowedBitrates[0] - bitrate);
    for (int candidate : kAllowedBitrates) {
        const int distance = qAbs(candidate - bitrate);
        if (distance < bestDistance) {
            best = candidate;
            bestDistance = distance;
        }
    }
    return best;
}

int AudioConverterService::normalizeSampleRate(int sampleRate, const QString &format)
{
    const FormatProfile *profile = findFormatProfile(format);
    const QString profileId = profile ? QString::fromLatin1(profile->id) : QString();
    if (profileId == QStringLiteral("opus") || profileId == QStringLiteral("webm")) {
        return 48000;
    }

    if (profileId == QStringLiteral("mp3")) {
        static const int kAllowedRates[] = {22050, 32000, 44100, 48000};
        int best = kAllowedRates[0];
        int bestDistance = qAbs(kAllowedRates[0] - sampleRate);
        for (int candidate : kAllowedRates) {
            const int distance = qAbs(candidate - sampleRate);
            if (distance < bestDistance) {
                best = candidate;
                bestDistance = distance;
            }
        }
        return best;
    }

    static const int kAllowedRates[] = {22050, 32000, 44100, 48000, 88200, 96000, 192000};
    int best = kAllowedRates[0];
    int bestDistance = qAbs(kAllowedRates[0] - sampleRate);
    for (int candidate : kAllowedRates) {
        const int distance = qAbs(candidate - sampleRate);
        if (distance < bestDistance) {
            best = candidate;
            bestDistance = distance;
        }
    }
    return best;
}

double AudioConverterService::normalizePlaybackRate(double playbackRate)
{
    if (!qIsFinite(playbackRate)) {
        return 1.0;
    }

    return qBound(0.25, playbackRate, 4.0);
}

int AudioConverterService::normalizePitchSemitones(int pitchSemitones)
{
    return qBound(-24, pitchSemitones, 24);
}

double AudioConverterService::normalizeUnitInterval(double value, double fallback)
{
    if (!qIsFinite(value)) {
        return fallback;
    }

    return qBound(0.0, value, 1.0);
}

QVariantList AudioConverterService::normalizeEqualizerBandGains(const QVariantList &gains)
{
    QVariantList normalized;
    normalized.reserve(10);
    for (int i = 0; i < 10; ++i) {
        const double source = i < gains.size() ? gains.at(i).toDouble() : 0.0;
        normalized.push_back(qBound(-24.0, source, 12.0));
    }
    return normalized;
}

const AudioConverterService::FormatProfile *AudioConverterService::findFormatProfile(const QString &format)
{
    const QString normalized = normalizeFormat(format);
    for (const FormatProfile &profile : kFormatProfiles) {
        if (normalized == QLatin1String(profile.id)) {
            return &profile;
        }
    }
    return nullptr;
}

QVariantMap AudioConverterService::toVariantMap(const FormatProfile &profile)
{
    const QStringList requiredElements = requiredGStreamerElements(&profile);
    const QStringList missingElements = missingGStreamerElements(requiredElements);
    QVariantList requiredElementVariants;
    for (const QString &factoryName : requiredElements) {
        requiredElementVariants.push_back(factoryName);
    }
    QVariantList missingElementVariants;
    for (const QString &factoryName : missingElements) {
        missingElementVariants.push_back(factoryName);
    }

    QVariantMap item;
    item.insert(QStringLiteral("id"), QString::fromLatin1(profile.id));
    item.insert(QStringLiteral("label"), QString::fromLatin1(profile.label));
    item.insert(QStringLiteral("extension"), QString::fromLatin1(profile.extension));
    item.insert(QStringLiteral("containerLabel"), QString::fromLatin1(profile.containerLabel));
    item.insert(QStringLiteral("codecLabel"), QString::fromLatin1(profile.codecLabel));
    item.insert(QStringLiteral("gstreamerMuxer"), QString::fromLatin1(profile.gstreamerMuxer));
    item.insert(QStringLiteral("gstreamerEncoder"), QString::fromLatin1(profile.gstreamerEncoder));
    item.insert(QStringLiteral("lossy"), profile.lossy);
    item.insert(QStringLiteral("supportsBitrate"), profile.supportsBitrate);
    item.insert(QStringLiteral("supportsSampleRate"), profile.supportsSampleRate);
    item.insert(QStringLiteral("supportsChannels"), profile.supportsChannels);
    item.insert(QStringLiteral("supportsCompressionLevel"), profile.supportsCompressionLevel);
    item.insert(QStringLiteral("defaultBitrateKbps"), profile.defaultBitrateKbps);
    item.insert(QStringLiteral("defaultSampleRateHz"), profile.defaultSampleRateHz);
    item.insert(QStringLiteral("bitrateValues"), bitrateValuesForProfile(profile));
    item.insert(QStringLiteral("sampleRateValues"), sampleRateValuesForProfile(profile));
    item.insert(QStringLiteral("channelModes"), channelModeValuesForProfile(profile));
    item.insert(QStringLiteral("available"), missingElements.isEmpty());
    item.insert(QStringLiteral("requiredGStreamerElements"), requiredElementVariants);
    item.insert(QStringLiteral("missingGStreamerElements"), missingElementVariants);
    return item;
}

QString AudioConverterService::replaceExtension(const QString &path, const QString &extension)
{
    QFileInfo info(path);
    const QString directory = info.absolutePath();
    const QString baseName = info.completeBaseName().trimmed().isEmpty()
        ? info.fileName()
        : info.completeBaseName();
    return QDir(directory).filePath(QStringLiteral("%1.%2").arg(baseName, extension));
}

QString AudioConverterService::uniqueOutputPath(const QString &path)
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
    while (QFileInfo::exists(candidate)) {
        const QString numberedBase = followsConvertedPattern
            ? QStringLiteral("%1 (converted %2)").arg(originalBaseName).arg(counter)
            : QStringLiteral("%1 %2").arg(baseName).arg(counter);
        candidate = QDir(directory).filePath(QStringLiteral("%1.%2").arg(numberedBase, suffix));
        ++counter;
    }

    return candidate;
}

QStringList AudioConverterService::requiredGStreamerElements(const FormatProfile *profile,
                                                             bool includeEqualizer,
                                                             bool includeReverb)
{
    QStringList elements = {
        QStringLiteral("uridecodebin"),
        QStringLiteral("audioconvert"),
        QStringLiteral("audioresample"),
        QStringLiteral("pitch"),
        QStringLiteral("capsfilter"),
        QStringLiteral("filesink")
    };

    if (includeEqualizer) {
        elements.push_back(QStringLiteral("equalizer-nbands"));
    }
    if (includeReverb) {
        elements.push_back(QStringLiteral("freeverb"));
    }

    if (profile) {
        const QString muxer = QString::fromLatin1(profile->gstreamerMuxer).trimmed();
        const QString encoder = QString::fromLatin1(profile->gstreamerEncoder).trimmed();
        if (!muxer.isEmpty()) {
            elements.push_back(muxer);
        }
        if (!encoder.isEmpty() && encoder != QStringLiteral("identity")) {
            elements.push_back(encoder);
        }
    }

    elements.removeDuplicates();
    return elements;
}

bool AudioConverterService::hasGStreamerElementFactory(const QString &factoryName)
{
    if (factoryName.trimmed().isEmpty()) {
        return true;
    }

    GstElementFactory *factory = gst_element_factory_find(factoryName.toUtf8().constData());
    if (!factory) {
        return false;
    }

    gst_object_unref(factory);
    return true;
}

QStringList AudioConverterService::missingGStreamerElements(const QStringList &requiredElements)
{
    QStringList missing;
    for (const QString &factoryName : requiredElements) {
        if (!hasGStreamerElementFactory(factoryName)) {
            missing.push_back(factoryName);
        }
    }
    missing.removeDuplicates();
    return missing;
}

QVariantMap AudioConverterService::buildPreflight() const
{
    QVariantMap result;
    const QString sourcePath = normalizeLocalPath(m_sourceFile);
    const QString outputPath = normalizeLocalPath(m_outputFile);
    const bool sameAsSource = !sourcePath.isEmpty() && sourcePath == outputPath;
    const FormatProfile *profile = findFormatProfile(m_format);
    const QStringList requiredElements = requiredGStreamerElements(profile,
                                                                   m_applyEqualizer,
                                                                   m_applyReverb);
    const QStringList missingElements = missingGStreamerElements(requiredElements);

    result.insert(QStringLiteral("canStart"), false);
    result.insert(QStringLiteral("severity"), QStringLiteral("none"));
    result.insert(QStringLiteral("messageKey"), QString());
    result.insert(QStringLiteral("messageArgs"), QVariantList());
    result.insert(QStringLiteral("messageText"), QString());
    result.insert(QStringLiteral("sourcePath"), sourcePath);
    result.insert(QStringLiteral("outputPath"), outputPath);
    result.insert(QStringLiteral("sameAsSource"), sameAsSource);
    result.insert(QStringLiteral("outputExists"), !outputPath.isEmpty() && QFileInfo::exists(outputPath));
    result.insert(QStringLiteral("writableDirectory"), false);
    result.insert(QStringLiteral("requiresOverwriteConfirmation"), false);
    QVariantList requiredElementVariants;
    for (const QString &factoryName : requiredElements) {
        requiredElementVariants.push_back(factoryName);
    }
    QVariantList missingElementVariants;
    for (const QString &factoryName : missingElements) {
        missingElementVariants.push_back(factoryName);
    }
    result.insert(QStringLiteral("requiredGStreamerElements"), requiredElementVariants);
    result.insert(QStringLiteral("missingGStreamerElements"), missingElementVariants);

    QVariantMap summary;
    summary.insert(QStringLiteral("format"), m_format);
    summary.insert(QStringLiteral("bitrateKbps"), m_bitrate);
    summary.insert(QStringLiteral("sampleRateHz"), m_sampleRate);
    summary.insert(QStringLiteral("channelMode"), m_channelMode);
    summary.insert(QStringLiteral("playbackRate"), m_playbackRate);
    summary.insert(QStringLiteral("pitchSemitones"), m_pitchSemitones);
    summary.insert(QStringLiteral("applyEqualizer"), m_applyEqualizer);
    summary.insert(QStringLiteral("equalizerBandGains"), m_equalizerBandGains);
    summary.insert(QStringLiteral("applyReverb"), m_applyReverb);
    summary.insert(QStringLiteral("reverbRoomSize"), m_reverbRoomSize);
    summary.insert(QStringLiteral("reverbDamping"), m_reverbDamping);
    summary.insert(QStringLiteral("reverbWetLevel"), m_reverbWetLevel);
    summary.insert(QStringLiteral("trimEnabled"), m_trimEnabled);
    summary.insert(QStringLiteral("trimStartMs"), m_trimStartMs);
    summary.insert(QStringLiteral("trimEndMs"), m_trimEndMs);
    summary.insert(QStringLiteral("codecLabel"), profile ? QString::fromLatin1(profile->codecLabel) : QString());
    summary.insert(QStringLiteral("containerLabel"), profile ? QString::fromLatin1(profile->containerLabel) : QString());
    result.insert(QStringLiteral("estimatedTransformSummary"), summary);

    auto setMessage = [&](const QString &severity,
                          const QString &messageKey,
                          const QVariantList &messageArgs = QVariantList(),
                          bool canStart = false,
                          bool requiresOverwriteConfirmation = false,
                          bool writableDirectory = false) {
        result.insert(QStringLiteral("severity"), severity);
        result.insert(QStringLiteral("messageKey"), messageKey);
        result.insert(QStringLiteral("messageArgs"), messageArgs);
        result.insert(QStringLiteral("canStart"), canStart);
        result.insert(QStringLiteral("requiresOverwriteConfirmation"), requiresOverwriteConfirmation);
        result.insert(QStringLiteral("writableDirectory"), writableDirectory);
        result.insert(QStringLiteral("messageText"), preflightMessageText(result));
    };

    if (sourcePath.isEmpty()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightSourceRequired"));
        return result;
    }

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightSourceUnreadable"),
                   {sourcePath});
        return result;
    }

    if (!profile) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightFormatUnsupported"),
                   {m_format});
        return result;
    }

    if (outputPath.isEmpty()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightOutputRequired"));
        return result;
    }

    if (sameAsSource) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightOutputMatchesSource"));
        return result;
    }

    const QFileInfo outputInfo(outputPath);
    const QDir outputDir(outputInfo.absolutePath());
    if (!outputDir.exists()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightOutputDirectoryMissing"),
                   {outputDir.absolutePath()});
        return result;
    }

    const QFileInfo outputDirInfo(outputDir.absolutePath());
    if (!outputDirInfo.isWritable()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightOutputDirectoryNotWritable"),
                   {outputDir.absolutePath()});
        return result;
    }

    QTemporaryFile writeProbe(outputDir.filePath(QStringLiteral(".waveflux-write-test-XXXXXX")));
    writeProbe.setAutoRemove(true);
    if (!writeProbe.open()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightOutputDirectoryWriteProbeFailed"),
                   {outputDir.absolutePath()});
        return result;
    }

    result.insert(QStringLiteral("writableDirectory"), true);

    if (!missingElements.isEmpty()) {
        setMessage(QStringLiteral("error"),
                   QStringLiteral("audioConverter.preflightMissingPlugins"),
                   {QString::fromLatin1(profile->label), missingElements.join(QStringLiteral(", "))},
                   false,
                   false,
                   true);
        return result;
    }

    if (m_trimEnabled) {
        qint64 sourceDurationMs = m_sourceDurationMs;
        if (sourceDurationMs <= 0) {
            sourceDurationMs = probeMetadataDurationMs(sourcePath);
        }
        const qint64 startMs = qMax<qint64>(0, m_trimStartMs);
        const qint64 endMs = qMax<qint64>(0, m_trimEndMs);
        const bool invalidKnownDuration = sourceDurationMs > 0
            && (startMs >= sourceDurationMs || (endMs > 0 && endMs > sourceDurationMs));
        const bool invalidRange = (endMs > 0 && endMs - startMs < 1000)
            || (sourceDurationMs > 0 && endMs <= 0 && sourceDurationMs - startMs < 1000);
        if (invalidKnownDuration || invalidRange) {
            setMessage(QStringLiteral("error"),
                       QStringLiteral("audioConverter.preflightTrimInvalid"),
                       QVariantList(),
                       false,
                       false,
                       true);
            return result;
        }
    }

    if (outputInfo.exists()) {
        if (!outputInfo.isWritable()) {
            setMessage(QStringLiteral("error"),
                       QStringLiteral("audioConverter.preflightExistingOutputNotWritable"),
                       {outputPath},
                       false,
                       false,
                       true);
            return result;
        }

        if (!m_overwriteExisting) {
            setMessage(QStringLiteral("warning"),
                       QStringLiteral("audioConverter.preflightExistingOutputConfirm"),
                       {outputPath},
                       false,
                       true,
                       true);
            return result;
        }
    }

    setMessage(QStringLiteral("none"), QString(), QVariantList(), true, false, true);
    return result;
}

QString AudioConverterService::preflightMessageText(const QVariantMap &preflight)
{
    const QString key = preflight.value(QStringLiteral("messageKey")).toString();
    const QVariantList args = preflight.value(QStringLiteral("messageArgs")).toList();
    const QString arg0 = args.size() > 0 ? args.at(0).toString() : QString();
    const QString arg1 = args.size() > 1 ? args.at(1).toString() : QString();

    if (key == QStringLiteral("audioConverter.preflightSourceRequired")) {
        return AppSettingsManager::translateForCurrentLanguage(key);
    }
    if (key == QStringLiteral("audioConverter.preflightSourceUnreadable")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightFormatUnsupported")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightOutputRequired")) {
        return AppSettingsManager::translateForCurrentLanguage(key);
    }
    if (key == QStringLiteral("audioConverter.preflightOutputMatchesSource")) {
        return AppSettingsManager::translateForCurrentLanguage(key);
    }
    if (key == QStringLiteral("audioConverter.preflightOutputDirectoryMissing")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightOutputDirectoryNotWritable")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightOutputDirectoryWriteProbeFailed")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightExistingOutputConfirm")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightExistingOutputNotWritable")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0);
    }
    if (key == QStringLiteral("audioConverter.preflightMissingPlugins")) {
        return AppSettingsManager::translateForCurrentLanguage(key).arg(arg0, arg1);
    }
    if (key == QStringLiteral("audioConverter.preflightTrimInvalid")) {
        return AppSettingsManager::translateForCurrentLanguage(key);
    }
    return QString();
}

QString AudioConverterService::validateForStart() const
{
    const QVariantMap currentPreflight = buildPreflight();
    if (currentPreflight.value(QStringLiteral("canStart")).toBool()) {
        return {};
    }
    return preflightMessageText(currentPreflight);
}

void AudioConverterService::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

void AudioConverterService::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    emit statusTextChanged();
}

void AudioConverterService::setStatusPresentation(const QString &messageKey,
                                                  const QVariantList &messageArgs,
                                                  const QString &fallbackText)
{
    const bool keyChanged = m_statusMessageKey != messageKey;
    const bool argsChanged = m_statusMessageArgs != messageArgs;
    m_statusMessageKey = messageKey;
    m_statusMessageArgs = messageArgs;
    setStatusText(fallbackText);
    if (keyChanged || argsChanged) {
        emit statusPresentationChanged();
    }
}

void AudioConverterService::setErrorPresentation(const QString &messageKey,
                                                 const QVariantList &messageArgs,
                                                 const QString &fallbackText)
{
    const bool keyChanged = m_errorMessageKey != messageKey;
    const bool argsChanged = m_errorMessageArgs != messageArgs;
    m_errorMessageKey = messageKey;
    m_errorMessageArgs = messageArgs;
    setLastError(fallbackText);
    if (keyChanged || argsChanged) {
        emit errorPresentationChanged();
    }
}

void AudioConverterService::setProgress(double progress)
{
    const double normalized = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(m_progress, normalized)) {
        return;
    }
    m_progress = normalized;
    emit progressChanged();
}

void AudioConverterService::setIsRunning(bool running)
{
    if (m_isRunning == running) {
        return;
    }
    m_isRunning = running;
    emit isRunningChanged();
    emit preflightChanged();
}
