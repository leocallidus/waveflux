#include <QtTest>

#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <gst/gst.h>
#include <taglib/tag.h>
#include <limits>

#include "AudioConverterService.h"
#include "AppSettingsManager.h"
#include "TagLibPath.h"
#include "TrackModel.h"

namespace {
void writeSilentWavFile(const QString &path,
                        int sampleRate = 44100,
                        int channels = 1,
                        int durationMs = 250)
{
    const int bytesPerSample = 2;
    const int sampleCount = qMax(1, (sampleRate * durationMs) / 1000);
    const int dataBytes = sampleCount * channels * bytesPerSample;

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataBytes);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << quint16(channels);
    stream << quint32(sampleRate);
    stream << quint32(sampleRate * channels * bytesPerSample);
    stream << quint16(channels * bytesPerSample);
    stream << quint16(16);
    stream.writeRawData("data", 4);
    stream << quint32(dataBytes);

    QByteArray samples(dataBytes, '\0');
    QVERIFY(stream.writeRawData(samples.constData(), samples.size()) == samples.size());
    file.close();
}

bool hasFactory(const char *name)
{
    GstElementFactory *factory = gst_element_factory_find(name);
    if (!factory) {
        return false;
    }
    gst_object_unref(factory);
    return true;
}

bool writeBasicTags(const QString &path,
                    const QString &title,
                    const QString &artist,
                    const QString &album)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, false);
    if (file.isNull() || !file.tag()) {
        return false;
    }

    const QByteArray titleUtf8 = title.toUtf8();
    const QByteArray artistUtf8 = artist.toUtf8();
    const QByteArray albumUtf8 = album.toUtf8();
    file.tag()->setTitle(TagLib::String(titleUtf8.constData(), TagLib::String::UTF8));
    file.tag()->setArtist(TagLib::String(artistUtf8.constData(), TagLib::String::UTF8));
    file.tag()->setAlbum(TagLib::String(albumUtf8.constData(), TagLib::String::UTF8));
    return file.save();
}

QString readTitleTag(const QString &path)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, false);
    if (file.isNull() || !file.tag()) {
        return {};
    }
    return QString::fromUtf8(file.tag()->title().toCString(true));
}

QString readArtistTag(const QString &path)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, false);
    if (file.isNull() || !file.tag()) {
        return {};
    }
    return QString::fromUtf8(file.tag()->artist().toCString(true));
}

QString readAlbumTag(const QString &path)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, false);
    if (file.isNull() || !file.tag()) {
        return {};
    }
    return QString::fromUtf8(file.tag()->album().toCString(true));
}

qint64 readDurationMs(const QString &path)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, true, TagLib::AudioProperties::Fast);
    if (file.isNull() || !file.audioProperties()) {
        return 0;
    }
    return qMax<qint64>(0, file.audioProperties()->lengthInMilliseconds());
}

int readBitrateKbps(const QString &path)
{
    auto file = WaveFlux::TagLibPath::makeFileRef(path, true, TagLib::AudioProperties::Fast);
    if (file.isNull() || !file.audioProperties()) {
        return 0;
    }
    return qMax(0, file.audioProperties()->bitrate());
}

QStringList temporaryConverterArtifacts(const QString &directoryPath)
{
    QDir dir(directoryPath);
    return dir.entryList(QStringList() << QStringLiteral("*.waveflux-tmp-*"),
                         QDir::Files | QDir::Hidden | QDir::System,
                         QDir::Name);
}

QString trackerFixturePath(const QString &fileName)
{
#ifdef WAVEFLUX_TESTDATA_DIR
    return QString::fromUtf8(WAVEFLUX_TESTDATA_DIR) + QStringLiteral("/tracker/") + fileName;
#else
    const QByteArray relativePath = (QStringLiteral("testdata/tracker/") + fileName).toUtf8();
    return QTest::qFindTestData(relativePath.constData());
#endif
}
} // namespace

class AudioConverterServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exposesStableDefaults();
    void normalizesMutableProperties();
    void normalizesSlashPrefixedWindowsOutputPath();
    void exposesFormatCapabilityMatrix();
    void exposesStructuredPreflightStates();
    void suggestsOutputPathAndSyncsExtension();
    void rejectsInvalidStartRequests();
    void resetsTransientStateForNewEditingSession();
    void completesWavConversionForValidInput();
    void completesTrimmedWavConversion();
    void completesMp3ConversionForValidInput();
    void completesReverbedMonoWavConversion();
    void completesWebmConversionForValidInput();
    void completesMp3ToMp3ConversionForValidInput();
    void preservesReasonableDurationForPitchShiftedMp3();
    void overwritesExistingOutputWhenExplicitlyAllowed();
    void copiesBasicTagsOnSuccessfulConversion();
    void reportsProgressForTrackerModules();
    void converts669TrackerModulesToAudibleOutput();
    void cancelsRunningConversion();
    void preservesExistingOutputOnRuntimeFailureAndCleansTemporaryFiles();
    void supportsNonAsciiPathsForWavAndMp3();
    void addsConvertedTrackToPlaylistOnFinished();
    void acceptsDroppedWebmFilesIntoPlaylist();
};

void AudioConverterServiceTest::initTestCase()
{
    int argc = 0;
    char **argv = nullptr;
    gst_init(&argc, &argv);
}

void AudioConverterServiceTest::exposesStableDefaults()
{
    AudioConverterService service;

    QCOMPARE(service.sourceFile(), QString());
    QCOMPARE(service.outputFile(), QString());
    QCOMPARE(service.format(), QStringLiteral("mp3"));
    QCOMPARE(service.bitrate(), 320);
    QCOMPARE(service.sampleRate(), 44100);
    QCOMPARE(service.channelMode(), QStringLiteral("stereo"));
    QCOMPARE(service.playbackRate(), 1.0);
    QCOMPARE(service.pitchSemitones(), 0);
    QCOMPARE(service.applyEqualizer(), false);
    QCOMPARE(service.equalizerBandGains().size(), 10);
    QCOMPARE(service.applyReverb(), false);
    QCOMPARE(service.reverbRoomSize(), 0.55);
    QCOMPARE(service.reverbDamping(), 0.35);
    QCOMPARE(service.reverbWetLevel(), 0.28);
    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.progress(), 0.0);
    QVERIFY(!service.statusText().trimmed().isEmpty());
    QCOMPARE(service.lastError(), QString());

    const QVariantList profiles = service.formatProfiles();
    QCOMPARE(profiles.size(), 5);
    QCOMPARE(profiles.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("mp3"));
    QCOMPARE(service.currentFormatProfile().value(QStringLiteral("id")).toString(), QStringLiteral("mp3"));
}

void AudioConverterServiceTest::normalizesMutableProperties()
{
    AudioConverterService service;

    QSignalSpy formatSpy(&service, &AudioConverterService::formatChanged);
    QSignalSpy bitrateSpy(&service, &AudioConverterService::bitrateChanged);
    QSignalSpy sampleRateSpy(&service, &AudioConverterService::sampleRateChanged);
    QSignalSpy channelSpy(&service, &AudioConverterService::channelModeChanged);
    QSignalSpy rateSpy(&service, &AudioConverterService::playbackRateChanged);
    QSignalSpy pitchSpy(&service, &AudioConverterService::pitchSemitonesChanged);
    QSignalSpy equalizerEnabledSpy(&service, &AudioConverterService::applyEqualizerChanged);
    QSignalSpy equalizerGainsSpy(&service, &AudioConverterService::equalizerBandGainsChanged);
    QSignalSpy reverbEnabledSpy(&service, &AudioConverterService::applyReverbChanged);
    QSignalSpy reverbRoomSpy(&service, &AudioConverterService::reverbRoomSizeChanged);
    QSignalSpy reverbDampingSpy(&service, &AudioConverterService::reverbDampingChanged);
    QSignalSpy reverbWetSpy(&service, &AudioConverterService::reverbWetLevelChanged);

    service.setFormat(QStringLiteral("FLAC"));
    service.setBitrate(111);
    service.setSampleRate(47999);
    service.setChannelMode(QStringLiteral("mono"));
    service.setPlaybackRate(99.0);
    service.setPitchSemitones(-200);
    service.setApplyEqualizer(true);
    service.setEqualizerBandGains({-99.0, -1.5, 0.0, 3.25, 99.0});
    service.setApplyReverb(true);
    service.setReverbRoomSize(99.0);
    service.setReverbDamping(-2.0);
    service.setReverbWetLevel(std::numeric_limits<double>::quiet_NaN());

    QCOMPARE(service.format(), QStringLiteral("flac"));
    QCOMPARE(service.bitrate(), 0);
    QCOMPARE(service.sampleRate(), 48000);
    QCOMPARE(service.channelMode(), QStringLiteral("mono"));
    QCOMPARE(service.playbackRate(), 4.0);
    QCOMPARE(service.pitchSemitones(), -24);
    QCOMPARE(service.applyEqualizer(), true);
    QCOMPARE(service.equalizerBandGains().size(), 10);
    QCOMPARE(service.equalizerBandGains().at(0).toDouble(), -24.0);
    QCOMPARE(service.equalizerBandGains().at(1).toDouble(), -1.5);
    QCOMPARE(service.equalizerBandGains().at(3).toDouble(), 3.25);
    QCOMPARE(service.equalizerBandGains().at(4).toDouble(), 12.0);
    QCOMPARE(service.equalizerBandGains().at(9).toDouble(), 0.0);
    QCOMPARE(service.applyReverb(), true);
    QCOMPARE(service.reverbRoomSize(), 1.0);
    QCOMPARE(service.reverbDamping(), 0.0);
    QCOMPARE(service.reverbWetLevel(), 0.28);
    QCOMPARE(formatSpy.count(), 1);
    QVERIFY(bitrateSpy.count() >= 1);
    QCOMPARE(sampleRateSpy.count(), 2);
    QCOMPARE(channelSpy.count(), 1);
    QCOMPARE(rateSpy.count(), 1);
    QCOMPARE(pitchSpy.count(), 1);
    QCOMPARE(equalizerEnabledSpy.count(), 1);
    QCOMPARE(equalizerGainsSpy.count(), 1);
    QCOMPARE(reverbEnabledSpy.count(), 1);
    QCOMPARE(reverbRoomSpy.count(), 1);
    QCOMPARE(reverbDampingSpy.count(), 1);
    QCOMPARE(reverbWetSpy.count(), 0);

    service.setFormat(QStringLiteral("mp3"));
    service.setSampleRate(88200);
    QCOMPARE(service.sampleRate(), 48000);

    service.setFormat(QStringLiteral("opus"));
    service.setSampleRate(22050);
    QCOMPARE(service.sampleRate(), 48000);

    service.setFormat(QStringLiteral("webm"));
    service.setBitrate(111);
    service.setSampleRate(22050);
    QCOMPARE(service.bitrate(), 96);
    QCOMPARE(service.sampleRate(), 48000);
}

void AudioConverterServiceTest::normalizesSlashPrefixedWindowsOutputPath()
{
#if defined(Q_OS_WIN)
    AudioConverterService service;

    service.setOutputFile(QStringLiteral("/C:/Users/leo/Desktop/DEADLOCK (converted).mp3"));

    QCOMPARE(service.outputFile(),
             QStringLiteral("C:/Users/leo/Desktop/DEADLOCK (converted).mp3"));
#else
    QSKIP("Windows drive-letter path normalization only applies on Windows.");
#endif
}

void AudioConverterServiceTest::exposesFormatCapabilityMatrix()
{
    AudioConverterService service;

    const QVariantList profiles = service.formatProfiles();
    QCOMPARE(profiles.size(), 5);

    const QVariantMap mp3 = profiles.at(0).toMap();
    QCOMPARE(mp3.value(QStringLiteral("containerLabel")).toString(), QStringLiteral("MPEG Audio"));
    QCOMPARE(mp3.value(QStringLiteral("codecLabel")).toString(), QStringLiteral("MP3"));
    QCOMPARE(mp3.value(QStringLiteral("gstreamerMuxer")).toString(), QStringLiteral(""));
    QCOMPARE(mp3.value(QStringLiteral("gstreamerEncoder")).toString(), QStringLiteral("lamemp3enc"));
    QCOMPARE(mp3.value(QStringLiteral("supportsBitrate")).toBool(), true);
    QCOMPARE(mp3.value(QStringLiteral("supportsCompressionLevel")).toBool(), false);
    QCOMPARE(mp3.value(QStringLiteral("bitrateValues")).toList().size(), 8);
    QCOMPARE(mp3.value(QStringLiteral("sampleRateValues")).toList().size(), 4);
    QVERIFY(mp3.contains(QStringLiteral("available")));
    QVERIFY(mp3.contains(QStringLiteral("requiredGStreamerElements")));
    QVERIFY(mp3.contains(QStringLiteral("missingGStreamerElements")));
    const bool mp3Available = mp3.value(QStringLiteral("available")).toBool();
    const bool mp3MissingAny = !mp3.value(QStringLiteral("missingGStreamerElements")).toList().isEmpty();
    QCOMPARE(mp3Available, !mp3MissingAny);

    service.setFormat(QStringLiteral("opus"));
    const QVariantMap opus = service.currentFormatProfile();
    QCOMPARE(opus.value(QStringLiteral("containerLabel")).toString(), QStringLiteral("Ogg"));
    QCOMPARE(opus.value(QStringLiteral("codecLabel")).toString(), QStringLiteral("Opus"));
    QCOMPARE(opus.value(QStringLiteral("defaultSampleRateHz")).toInt(), 48000);
    QCOMPARE(opus.value(QStringLiteral("sampleRateValues")).toList().size(), 1);
    QCOMPARE(service.supportsCurrentFormatBitrate(), true);
    QCOMPARE(service.supportsCurrentFormatSampleRate(), true);
    QCOMPARE(service.supportsCurrentFormatChannels(), true);
    QCOMPARE(opus.value(QStringLiteral("available")).toBool(),
             opus.value(QStringLiteral("missingGStreamerElements")).toList().isEmpty());

    service.setFormat(QStringLiteral("webm"));
    const QVariantMap webm = service.currentFormatProfile();
    QCOMPARE(webm.value(QStringLiteral("containerLabel")).toString(), QStringLiteral("WebM"));
    QCOMPARE(webm.value(QStringLiteral("codecLabel")).toString(), QStringLiteral("Opus"));
    QCOMPARE(webm.value(QStringLiteral("gstreamerMuxer")).toString(), QStringLiteral("webmmux"));
    QCOMPARE(webm.value(QStringLiteral("gstreamerEncoder")).toString(), QStringLiteral("opusenc"));
    QCOMPARE(webm.value(QStringLiteral("defaultSampleRateHz")).toInt(), 48000);
    const QVariantList expectedWebmBitrates{64, 96, 128, 160, 192, 256};
    const QVariantList expectedWebmSampleRates{48000};
    QCOMPARE(webm.value(QStringLiteral("bitrateValues")).toList(), expectedWebmBitrates);
    QCOMPARE(webm.value(QStringLiteral("sampleRateValues")).toList(), expectedWebmSampleRates);
    QVERIFY(webm.value(QStringLiteral("requiredGStreamerElements")).toList().contains(QStringLiteral("webmmux")));

    service.setFormat(QStringLiteral("flac"));
    const QVariantMap flac = service.currentFormatProfile();
    QCOMPARE(flac.value(QStringLiteral("supportsBitrate")).toBool(), false);
    QCOMPARE(flac.value(QStringLiteral("supportsCompressionLevel")).toBool(), true);
    QCOMPARE(flac.value(QStringLiteral("gstreamerMuxer")).toString(), QStringLiteral(""));
    QCOMPARE(flac.value(QStringLiteral("gstreamerEncoder")).toString(), QStringLiteral("flacenc"));
    QCOMPARE(flac.value(QStringLiteral("available")).toBool(),
             flac.value(QStringLiteral("missingGStreamerElements")).toList().isEmpty());
}

void AudioConverterServiceTest::exposesStructuredPreflightStates()
{
    AudioConverterService service;

    const QVariantMap initialPreflight = service.preflight();
    QCOMPARE(initialPreflight.value(QStringLiteral("canStart")).toBool(), false);
    QCOMPARE(initialPreflight.value(QStringLiteral("severity")).toString(), QStringLiteral("error"));
    QCOMPARE(initialPreflight.value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightSourceRequired"));

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("preflight-source.wav"));
    writeSilentWavFile(sourcePath);

    service.setSourceFile(sourcePath);
    service.setOutputFile(QString());
    QVariantMap currentPreflight = service.preflight();
    QCOMPARE(currentPreflight.value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightOutputRequired"));

    service.setOutputFile(sourcePath);
    currentPreflight = service.preflight();
    QCOMPARE(currentPreflight.value(QStringLiteral("sameAsSource")).toBool(), true);
    QCOMPARE(currentPreflight.value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightOutputMatchesSource"));

    service.setOutputFile(tempDir.filePath(QStringLiteral("missing/subdir/output.wav")));
    currentPreflight = service.preflight();
    QCOMPARE(currentPreflight.value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightOutputDirectoryMissing"));

    const QString existingOutputPath = tempDir.filePath(QStringLiteral("existing.wav"));
    writeSilentWavFile(existingOutputPath);
    service.setOutputFile(existingOutputPath);
    currentPreflight = service.preflight();
    QCOMPARE(currentPreflight.value(QStringLiteral("outputExists")).toBool(), true);
    QCOMPARE(currentPreflight.value(QStringLiteral("requiresOverwriteConfirmation")).toBool(), true);
    QCOMPARE(currentPreflight.value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightExistingOutputConfirm"));

    service.setOverwriteExisting(true);
    currentPreflight = service.preflight();
    const QVariantList requiredElements = currentPreflight.value(QStringLiteral("requiredGStreamerElements")).toList();
    const QVariantList missingElements = currentPreflight.value(QStringLiteral("missingGStreamerElements")).toList();
    QVERIFY(requiredElements.contains(QStringLiteral("uridecodebin")));
    QVERIFY(requiredElements.contains(QStringLiteral("pitch")));
    QVERIFY(!requiredElements.contains(QStringLiteral("freeverb")));
    if (missingElements.isEmpty()) {
        QCOMPARE(currentPreflight.value(QStringLiteral("canStart")).toBool(), true);
        QCOMPARE(currentPreflight.value(QStringLiteral("severity")).toString(), QStringLiteral("none"));
    } else {
        QCOMPARE(currentPreflight.value(QStringLiteral("canStart")).toBool(), false);
        QCOMPARE(currentPreflight.value(QStringLiteral("messageKey")).toString(),
                 QStringLiteral("audioConverter.preflightMissingPlugins"));
    }
}

void AudioConverterServiceTest::suggestsOutputPathAndSyncsExtension()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("Track01.flac"));
    QFile sourceFile(sourcePath);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly));
    QVERIFY(sourceFile.write("dummy") > 0);
    sourceFile.close();

    QFile existingMp3(tempDir.filePath(QStringLiteral("Track01 (converted).mp3")));
    QVERIFY(existingMp3.open(QIODevice::WriteOnly));
    existingMp3.close();

    AudioConverterService service;
    service.setSourceFile(sourcePath);

    const QString suggested = service.suggestOutputFilePath();
    QVERIFY(suggested.endsWith(QStringLiteral("Track01 (converted 1).mp3"))
            || suggested.endsWith(QStringLiteral("Track01 (converted 1).mp3").replace('/', QDir::separator())));

    service.setOutputFile(tempDir.filePath(QStringLiteral("custom_name.mp3")));
    service.setFormat(QStringLiteral("wav"));
    QVERIFY(service.outputFile().endsWith(QStringLiteral("custom_name.wav"))
            || service.outputFile().endsWith(QStringLiteral("custom_name.wav").replace('/', QDir::separator())));

    service.setFormat(QStringLiteral("webm"));
    QVERIFY(service.outputFile().endsWith(QStringLiteral("custom_name.webm"))
            || service.outputFile().endsWith(QStringLiteral("custom_name.webm").replace('/', QDir::separator())));
}

void AudioConverterServiceTest::rejectsInvalidStartRequests()
{
    AudioConverterService service;
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    QVERIFY(!service.startConversion());
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(!service.lastError().trimmed().isEmpty());
    QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightSourceRequired"));

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("source.mp3"));
    QFile file(sourcePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    service.setSourceFile(sourcePath);
    service.setOutputFile(sourcePath);
    QVERIFY(!service.startConversion());
    QVERIFY(service.lastError().contains(
        AppSettingsManager::translateForCurrentLanguage(QStringLiteral("audioConverter.preflightOutputMatchesSource")),
        Qt::CaseInsensitive));
    QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightOutputMatchesSource"));

    service.setOutputFile(tempDir.filePath(QStringLiteral("missing/subdir/output.wav")));
    QVERIFY(!service.startConversion());
    QVERIFY(service.lastError().contains(
        AppSettingsManager::translateForCurrentLanguage(QStringLiteral("audioConverter.preflightOutputDirectoryMissing")).arg(QString()).trimmed(),
        Qt::CaseInsensitive));
    QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.preflightOutputDirectoryMissing"));

    const QString existingOutputPath = tempDir.filePath(QStringLiteral("existing.wav"));
    QFile existingOutput(existingOutputPath);
    QVERIFY(existingOutput.open(QIODevice::WriteOnly));
    existingOutput.close();

    service.setOutputFile(existingOutputPath);
    QVERIFY(!service.startConversion());
    const QVariantMap currentPreflight = service.preflight();
    if (!currentPreflight.value(QStringLiteral("missingGStreamerElements")).toList().isEmpty()) {
        QVERIFY(service.lastError().contains(QStringLiteral("plugin"), Qt::CaseInsensitive)
                || service.lastError().contains(QStringLiteral("gstreamer"), Qt::CaseInsensitive));
        QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(),
                 QStringLiteral("audioConverter.preflightMissingPlugins"));
    } else {
        QVERIFY(service.lastError().contains(
        AppSettingsManager::translateForCurrentLanguage(QStringLiteral("audioConverter.preflightExistingOutputConfirm")).arg(QString()).trimmed(),
        Qt::CaseInsensitive));
        QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(),
                 QStringLiteral("audioConverter.preflightExistingOutputConfirm"));
    }
}

void AudioConverterServiceTest::resetsTransientStateForNewEditingSession()
{
    AudioConverterService service;
    QSignalSpy lastErrorSpy(&service, &AudioConverterService::lastErrorChanged);
    QSignalSpy statusSpy(&service, &AudioConverterService::statusTextChanged);

    QVERIFY(!service.startConversion());
    QVERIFY(!service.lastError().isEmpty());
    QVERIFY(!service.statusText().isEmpty());

    service.resetTransientState();

    QCOMPARE(service.lastError(), QString());
    QCOMPARE(service.statusText(), QString());
    QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(), QString());
    QCOMPARE(service.statusPresentation().value(QStringLiteral("messageKey")).toString(), QString());
    QCOMPARE(service.progress(), 0.0);
    QVERIFY(lastErrorSpy.count() >= 2);
    QVERIFY(statusSpy.count() >= 2);
}

void AudioConverterServiceTest::completesWavConversionForValidInput()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("song.wav"));
    writeSilentWavFile(sourcePath);

    AudioConverterService service;
    QSignalSpy startedSpy(&service, &AudioConverterService::conversionStarted);
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    const QString outputPath = tempDir.filePath(QStringLiteral("song (converted).wav"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QCOMPARE(startedSpy.count(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(service.isRunning(), false);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(service.progress(), 1.0);
    QCOMPARE(finishedSpy.at(0).at(0).toString(), outputPath);
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::completesTrimmedWavConversion()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("long-song.wav"));
    writeSilentWavFile(sourcePath, 44100, 1, 4000);

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setTrimEnabled(true);
    service.setTrimStartMs(1000);
    service.setTrimEndMs(2500);
    const QString outputPath = tempDir.filePath(QStringLiteral("trimmed.wav"));
    service.setOutputFile(outputPath);

    QVERIFY(service.preflight().value(QStringLiteral("canStart")).toBool());
    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));

    const qint64 outputDurationMs = readDurationMs(outputPath);
    QVERIFY2(outputDurationMs >= 1200 && outputDurationMs <= 1900,
             qPrintable(QStringLiteral("Unexpected trimmed duration: %1 ms").arg(outputDurationMs)));
    QCOMPARE(service.progress(), 1.0);
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::completesMp3ConversionForValidInput()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("lamemp3enc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer MP3 conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("song.wav"));
    writeSilentWavFile(sourcePath, 44100, 2, 500);

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("mp3"));
    service.setOutputFile(tempDir.filePath(QStringLiteral("song.mp3")));

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(service.isRunning(), false);
    QVERIFY(QFileInfo::exists(service.outputFile()));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::completesReverbedMonoWavConversion()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("freeverb")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer reverb conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("dry.wav"));
    writeSilentWavFile(sourcePath, 44100, 1, 750);

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    const QString outputPath = tempDir.filePath(QStringLiteral("reverbed-mono.wav"));
    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setChannelMode(QStringLiteral("mono"));
    service.setApplyReverb(true);
    service.setReverbRoomSize(0.85);
    service.setReverbWetLevel(0.42);
    service.setOutputFile(outputPath);

    const QVariantList requiredElements =
        service.preflight().value(QStringLiteral("requiredGStreamerElements")).toList();
    QVERIFY(requiredElements.contains(QStringLiteral("freeverb")));
    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::completesWebmConversionForValidInput()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("opusenc")
        || !hasFactory("webmmux")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer WEBM conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("song.wav"));
    writeSilentWavFile(sourcePath, 44100, 2, 500);

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    const QString outputPath = tempDir.filePath(QStringLiteral("song.webm"));
    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("webm"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(service.isRunning(), false);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::completesMp3ToMp3ConversionForValidInput()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("lamemp3enc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer MP3 conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString wavSourcePath = tempDir.filePath(QStringLiteral("source.wav"));
    writeSilentWavFile(wavSourcePath, 44100, 2, 600);

    AudioConverterService bootstrapService;
    QSignalSpy bootstrapFinishedSpy(&bootstrapService, &AudioConverterService::conversionFinished);
    QSignalSpy bootstrapFailedSpy(&bootstrapService, &AudioConverterService::conversionFailed);
    const QString mp3SourcePath = tempDir.filePath(QStringLiteral("source.mp3"));
    bootstrapService.setSourceFile(wavSourcePath);
    bootstrapService.setFormat(QStringLiteral("mp3"));
    bootstrapService.setOutputFile(mp3SourcePath);

    QVERIFY(bootstrapService.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(bootstrapFinishedSpy.count(), 1, 10000);
    QCOMPARE(bootstrapFailedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(mp3SourcePath));

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    const QString outputPath = tempDir.filePath(QStringLiteral("source_reencoded.mp3"));
    service.setSourceFile(mp3SourcePath);
    service.setFormat(QStringLiteral("mp3"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::preservesReasonableDurationForPitchShiftedMp3()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("lamemp3enc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer MP3 conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("source.wav"));
    writeSilentWavFile(sourcePath, 44100, 2, 4500);

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    const QString outputPath = tempDir.filePath(QStringLiteral("pitched.mp3"));
    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("mp3"));
    service.setPitchSemitones(-3);
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));

    const qint64 sourceDurationMs = readDurationMs(sourcePath);
    const qint64 outputDurationMs = readDurationMs(outputPath);
    QVERIFY(sourceDurationMs > 0);
    QVERIFY(outputDurationMs > 0);
    QVERIFY2(qAbs(outputDurationMs - sourceDurationMs) < 1000,
             qPrintable(QStringLiteral("Unexpected duration drift: source=%1 output=%2")
                            .arg(sourceDurationMs)
                            .arg(outputDurationMs)));
    QCOMPARE(readBitrateKbps(outputPath), 320);
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::overwritesExistingOutputWhenExplicitlyAllowed()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("source.wav"));
    writeSilentWavFile(sourcePath, 44100, 2, 400);

    const QString outputPath = tempDir.filePath(QStringLiteral("existing.wav"));
    writeSilentWavFile(outputPath, 22050, 1, 120);
    const qint64 oldSize = QFileInfo(outputPath).size();

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setOutputFile(outputPath);

    QVERIFY(!service.startConversion());
    QVERIFY(service.lastError().contains(
        AppSettingsManager::translateForCurrentLanguage(QStringLiteral("audioConverter.preflightExistingOutputConfirm")).arg(QString()).trimmed(),
        Qt::CaseInsensitive));

    service.setOverwriteExisting(true);
    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(QFileInfo::exists(outputPath));
    QVERIFY(QFileInfo(outputPath).size() != oldSize);
    QCOMPARE(service.overwriteExisting(), false);
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::copiesBasicTagsOnSuccessfulConversion()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("tagged.wav"));
    writeSilentWavFile(sourcePath);
    if (!writeBasicTags(sourcePath,
                        QStringLiteral("Test Title"),
                        QStringLiteral("Test Artist"),
                        QStringLiteral("Test Album"))) {
        QSKIP("TagLib cannot write source tags for this test format.");
    }

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    const QString outputPath = tempDir.filePath(QStringLiteral("tagged (converted).wav"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(readTitleTag(outputPath), QStringLiteral("Test Title"));
    QCOMPARE(readArtistTag(outputPath), QStringLiteral("Test Artist"));
    QCOMPARE(readAlbumTag(outputPath), QStringLiteral("Test Album"));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::reportsProgressForTrackerModules()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("openmptdec")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer tracker conversion elements are unavailable.");
    }

    const QString sourcePath = trackerFixturePath(QStringLiteral("stress-long-loopish.mod"));
    QVERIFY2(QFileInfo::exists(sourcePath), qPrintable(sourcePath));

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);
    bool sawIntermediateProgress = false;

    QObject::connect(&service, &AudioConverterService::progressChanged, &service, [&service, &sawIntermediateProgress]() {
        const double progress = service.progress();
        if (progress > 0.0 && progress < 1.0) {
            sawIntermediateProgress = true;
        }
    });

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setPlaybackRate(0.25);
    service.setOutputFile(tempDir.filePath(QStringLiteral("tracker-converted.wav")));

    QVERIFY(service.startConversion());
    QTRY_VERIFY_WITH_TIMEOUT(sawIntermediateProgress, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 30000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(service.progress(), 1.0);
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::converts669TrackerModulesToAudibleOutput()
{
    const QString sourcePath = trackerFixturePath(QStringLiteral("tiny.669"));
    QVERIFY2(QFileInfo::exists(sourcePath), qPrintable(sourcePath));

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    AudioConverterService service;
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    const QString outputPath = tempDir.filePath(QStringLiteral("tiny669-converted.wav"));
    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 30000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));

    const qint64 outputDurationMs = readDurationMs(outputPath);
    QVERIFY2(outputDurationMs > 0,
             qPrintable(QStringLiteral("Expected non-zero output duration, got %1 ms").arg(outputDurationMs)));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::cancelsRunningConversion()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("cancel_source.wav"));
    writeSilentWavFile(sourcePath, 44100, 2, 60000);

    AudioConverterService service;
    QSignalSpy startedSpy(&service, &AudioConverterService::conversionStarted);
    QSignalSpy canceledSpy(&service, &AudioConverterService::conversionCanceled);
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);

    const QString outputPath = tempDir.filePath(QStringLiteral("cancel_output.wav"));
    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QCOMPARE(startedSpy.count(), 1);

    service.cancelConversion();

    QCOMPARE(canceledSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(service.isRunning(), false);
    QCOMPARE(service.progress(), 0.0);
    const QString cancelStatus = service.statusText();
    QVERIFY(!cancelStatus.trimmed().isEmpty());
    QVERIFY(cancelStatus.contains(QStringLiteral("canceled"), Qt::CaseInsensitive)
            || cancelStatus.contains(QStringLiteral("отмен"), Qt::CaseInsensitive));
    QCOMPARE(service.statusPresentation().value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.runtimeCanceled"));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("canceled"));
    QVERIFY(!QFileInfo::exists(outputPath));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::preservesExistingOutputOnRuntimeFailureAndCleansTemporaryFiles()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("broken_source.wav"));
    QFile brokenSource(sourcePath);
    QVERIFY(brokenSource.open(QIODevice::WriteOnly));
    QVERIFY(brokenSource.write("not-a-real-waveflux-audio-file") > 0);
    brokenSource.close();

    const QString outputPath = tempDir.filePath(QStringLiteral("existing_output.wav"));
    writeSilentWavFile(outputPath, 22050, 1, 180);
    const qint64 existingSize = QFileInfo(outputPath).size();

    AudioConverterService service;
    QSignalSpy failedSpy(&service, &AudioConverterService::conversionFailed);
    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);

    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("wav"));
    service.setOutputFile(outputPath);
    service.setOverwriteExisting(true);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 10000);
    QCOMPARE(finishedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(QFileInfo(outputPath).size(), existingSize);
    QCOMPARE(service.errorPresentation().value(QStringLiteral("messageKey")).toString(),
             QStringLiteral("audioConverter.runtimeFailedPipeline"));
    QCOMPARE(service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("runtime-failed-pipeline"));
    QVERIFY(temporaryConverterArtifacts(tempDir.path()).isEmpty());
}

void AudioConverterServiceTest::supportsNonAsciiPathsForWavAndMp3()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("wavenc")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer WAV conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourceDirPath = tempDir.filePath(QStringLiteral("тестовая папка"));
    QVERIFY(QDir().mkpath(sourceDirPath));
    const QString sourcePath = QDir(sourceDirPath).filePath(QStringLiteral("источник.wav"));
    writeSilentWavFile(sourcePath, 44100, 2, 450);

    AudioConverterService wavService;
    QSignalSpy wavFinishedSpy(&wavService, &AudioConverterService::conversionFinished);
    QSignalSpy wavFailedSpy(&wavService, &AudioConverterService::conversionFailed);
    const QString wavOutputPath = QDir(sourceDirPath).filePath(QStringLiteral("результат.wav"));
    wavService.setSourceFile(sourcePath);
    wavService.setFormat(QStringLiteral("wav"));
    wavService.setOutputFile(wavOutputPath);

    QVERIFY(wavService.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(wavFinishedSpy.count(), 1, 10000);
    QCOMPARE(wavFailedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(wavOutputPath));
    QCOMPARE(wavService.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(sourceDirPath).isEmpty());

    if (!hasFactory("lamemp3enc")) {
        return;
    }

    AudioConverterService mp3Service;
    QSignalSpy mp3FinishedSpy(&mp3Service, &AudioConverterService::conversionFinished);
    QSignalSpy mp3FailedSpy(&mp3Service, &AudioConverterService::conversionFailed);
    const QString mp3OutputPath = QDir(sourceDirPath).filePath(QStringLiteral("результат.mp3"));
    mp3Service.setSourceFile(sourcePath);
    mp3Service.setFormat(QStringLiteral("mp3"));
    mp3Service.setOutputFile(mp3OutputPath);

    QVERIFY(mp3Service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(mp3FinishedSpy.count(), 1, 10000);
    QCOMPARE(mp3FailedSpy.count(), 0);
    QVERIFY(QFileInfo::exists(mp3OutputPath));
    QCOMPARE(mp3Service.lastConversionMetrics().value(QStringLiteral("terminationKey")).toString(),
             QStringLiteral("succeeded"));
    QVERIFY(temporaryConverterArtifacts(sourceDirPath).isEmpty());
}

void AudioConverterServiceTest::addsConvertedTrackToPlaylistOnFinished()
{
    if (!hasFactory("uridecodebin")
        || !hasFactory("audioconvert")
        || !hasFactory("audioresample")
        || !hasFactory("pitch")
        || !hasFactory("capsfilter")
        || !hasFactory("opusenc")
        || !hasFactory("webmmux")
        || !hasFactory("filesink")) {
        QSKIP("Required GStreamer conversion elements are unavailable.");
    }

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString sourcePath = tempDir.filePath(QStringLiteral("playlist_source.wav"));
    writeSilentWavFile(sourcePath);

    TrackModel trackModel;
    AudioConverterService service;
    service.initialize(&trackModel);

    QSignalSpy finishedSpy(&service, &AudioConverterService::conversionFinished);
    QSignalSpy countSpy(&trackModel, &TrackModel::countChanged);

    QObject::connect(&service, &AudioConverterService::conversionFinished,
                     &trackModel, [&trackModel](const QString &outputPath) {
        trackModel.addFile(outputPath);
    });

    const QString outputPath = tempDir.filePath(QStringLiteral("playlist_output.webm"));
    service.setSourceFile(sourcePath);
    service.setFormat(QStringLiteral("webm"));
    service.setOutputFile(outputPath);

    QVERIFY(service.startConversion());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(trackModel.rowCount() == 1, 10000);
    QVERIFY(countSpy.count() >= 1);
    QCOMPARE(trackModel.getFilePath(0), outputPath);
}

void AudioConverterServiceTest::acceptsDroppedWebmFilesIntoPlaylist()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "failed to create temp dir");

    const QString webmPath = tempDir.filePath(QStringLiteral("dropped.webm"));
    QFile webmFile(webmPath);
    QVERIFY(webmFile.open(QIODevice::WriteOnly));
    QVERIFY(webmFile.write("not-empty") > 0);
    webmFile.close();

    TrackModel trackModel;
    QSignalSpy countSpy(&trackModel, &TrackModel::countChanged);

    trackModel.addUrl(QUrl::fromLocalFile(webmPath));

    QCOMPARE(trackModel.rowCount(), 1);
    QVERIFY(countSpy.count() >= 1);
    QCOMPARE(trackModel.getFilePath(0), webmPath);
}

QTEST_GUILESS_MAIN(AudioConverterServiceTest)
#include "tst_AudioConverterService.moc"
