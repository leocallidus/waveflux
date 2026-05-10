#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

#include <libopenmpt/libopenmpt.hpp>

#include "playback/IPlaybackBackend.h"
#define private public
#include "playback/OpenMptPlaybackBackend.h"
#undef private
#include "playback/PlaybackBackendRouting.h"
#include "playback/TrackerPcmEngine.h"

namespace {

void appendFixedString(QByteArray *buffer, QByteArray value, int width)
{
    value.truncate(width);
    buffer->append(value);
    if (value.size() < width) {
        buffer->append(QByteArray(width - value.size(), '\0'));
    }
}

void appendBigEndianWord(QByteArray *buffer, quint16 value)
{
    buffer->append(static_cast<char>((value >> 8) & 0xff));
    buffer->append(static_cast<char>(value & 0xff));
}

QByteArray createMinimalModFile()
{
    QByteArray data;
    appendFixedString(&data, "WaveFlux MOD Demo", 20);

    for (int sampleIndex = 0; sampleIndex < 31; ++sampleIndex) {
        appendFixedString(&data, sampleIndex == 0 ? "Pulse" : QByteArray(), 22);
        appendBigEndianWord(&data, sampleIndex == 0 ? 2 : 0);
        data.append('\0');
        data.append(static_cast<char>(sampleIndex == 0 ? 64 : 0));
        appendBigEndianWord(&data, 0);
        appendBigEndianWord(&data, sampleIndex == 0 ? 1 : 0);
    }

    data.append('\1');
    data.append(static_cast<char>(127));
    data.append(QByteArray(128, '\0'));
    data.append("M.K.", 4);

    QByteArray pattern(64 * 4 * 4, '\0');
    pattern[0] = static_cast<char>(0x01);
    pattern[1] = static_cast<char>(0xac);
    pattern[2] = static_cast<char>(0x10);
    pattern[3] = static_cast<char>(0x00);
    data.append(pattern);

    data.append(QByteArray::fromHex("0040c000"));
    return data;
}

QString writeModuleFixture(QTemporaryDir *dir, const QString &fileName = QStringLiteral("demo.mod"))
{
    const QString filePath = dir->filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }

    file.write(createMinimalModFile());
    file.close();
    return filePath;
}

QString trackerTestDataDir()
{
#ifdef WAVEFLUX_TESTDATA_DIR
    return QString::fromUtf8(WAVEFLUX_TESTDATA_DIR) + QStringLiteral("/tracker");
#else
    return QFINDTESTDATA("testdata/tracker");
#endif
}

QString realModsDir()
{
    return trackerTestDataDir() + QStringLiteral("/realmods");
}

QFileInfoList supportedRealMods()
{
    QDir dir(realModsDir());
    if (!dir.exists()) {
        return {};
    }

    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    files.erase(std::remove_if(files.begin(),
                               files.end(),
                               [](const QFileInfo &info) {
                                   return !WaveFlux::isTrackerModuleExtension(info.suffix());
                               }),
                files.end());
    return files;
}

QJsonArray loadTrackerManifestFixtures()
{
    const QString manifestPath = trackerTestDataDir() + QStringLiteral("/manifest.json");
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    return document.object().value(QStringLiteral("fixtures")).toArray();
}

std::unique_ptr<openmpt::module> openModule(const QString &filePath,
                                            std::unique_ptr<std::istream> *streamOut)
{
    auto stream = std::make_unique<std::ifstream>(filePath.toStdString(), std::ios::binary);
    if (!stream->good()) {
        return {};
    }

    auto module = std::make_unique<openmpt::module>(*stream);
    *streamOut = std::move(stream);
    return module;
}

double rmsInt16(const QByteArray &buffer, qint64 bytes)
{
    const auto *samples = reinterpret_cast<const std::int16_t *>(buffer.constData());
    const qint64 sampleCount = bytes / static_cast<qint64>(sizeof(std::int16_t));
    if (sampleCount <= 0) {
        return 0.0;
    }

    double sumSquares = 0.0;
    for (qint64 i = 0; i < sampleCount; ++i) {
        const double normalized = static_cast<double>(samples[i]) / 32768.0;
        sumSquares += normalized * normalized;
    }

    return std::sqrt(sumSquares / static_cast<double>(sampleCount));
}

double renderRmsWithEqualizer(const QString &modulePath, const std::vector<double> &gainsDb)
{
    static const std::vector<double> kFrequenciesHz = {
        31.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
    };

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    if (!module) {
        return 0.0;
    }

    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);
    WaveFlux::TrackerPcmEngine engine;
    engine.setEqualizerBands(kFrequenciesHz, gainsDb);
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);

    constexpr qint64 kTargetBytes = WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 8192;
    QByteArray buffer(kTargetBytes, Qt::Uninitialized);

    qint64 bytesRead = 0;
    for (int i = 0; i < 1000 && bytesRead < kTargetBytes; ++i) {
        bytesRead += engine.readInt16(buffer.data() + bytesRead, kTargetBytes - bytesRead);
        if (bytesRead < kTargetBytes) {
            QTest::qWait(1);
        }
    }

    return rmsInt16(buffer, bytesRead);
}

struct CapturedMessages {
    QMutex mutex;
    QStringList messages;
};

CapturedMessages *g_capturedMessages = nullptr;
QtMessageHandler g_previousMessageHandler = nullptr;

void captureMessageHandler(QtMsgType type,
                           const QMessageLogContext &context,
                           const QString &message)
{
    if (g_capturedMessages && message.contains(QStringLiteral("[TrackerDiag]"))) {
        QMutexLocker locker(&g_capturedMessages->mutex);
        g_capturedMessages->messages.push_back(message);
    }

    if (g_previousMessageHandler) {
        g_previousMessageHandler(type, context, message);
    }
}

} // namespace

class OpenMptPlaybackBackendTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void load_validLocalModule_reportsMetadataAndDuration();
    void load_windowsDriveLetterPath_reportsMetadataAndDuration();
    void load_supportedTrackerExtensions_acceptValidModuleContent();
    void load_rejectsNonLocalTrackerUrl();
    void load_unavailableTrackerFile_emitsControlledError();
    void load_invalidTrackerModule_emitsError();
    void corpus_manifestFixtures_loadAndSeekWithoutAudioDevice();
    void realmods_supportedCorpus_loadsAndStressSeeksWithoutAudioDevice();
    void diagnostics_countersTrackSeekAndDecodeErrors();
    void diagnostics_exportSnapshotIncludesRuntimeBudgetFields();
    void diagnostics_traceIsOptInStructuredLog();
    void deviceLifecycle_safeStopOnDefaultOutputLossExposesStatus();
    void deviceLifecycle_sinkErrorRecordsRecoveryPolicy();
    void preloadNext_promotesPreparedModuleAndPreservesMetadata();
    void capabilities_reflectTrackerRateSupport();
    void capabilities_reflectTrackerPitchSupport();
    void pcmEngine_repeatedSeekClearsBufferAndKeepsClock();
    void pcmEngine_eosAfterBufferedFramesDrainOnlyOnce();
    void pcmEngine_snapshotTracksSubmittedAndConsumedFrames();
    void pcmEngine_snapshotExposesDspGraphStateAndEqRamp();
    void pcmEngine_playbackRateChangesClockAndSeekRemainStable();
    void pcmEngine_pitchShiftPreservesClockAndSeekRemainStable();
    void pcmEngine_reversePlaybackUsesBoundedWindowsAndSeek();
    void pcmEngine_reversePlaybackReachesEosAtZero();
    void pcmEngine_equalizerBandsChangeRenderedPcm();
    void regression_repeatedSeek500OperationsStayBounded();
    void regression_reverseShortAndLongModulesStayBounded();
    void regression_repeatedDspTogglesKeepCapabilitiesAndDiagnostics();
    void regression_dspInteractionsEqRatePitchSpectrumReverse();
    void seek_loadedModule_handlesBoundariesAndRepeatedTargets();
    void play_seek_pause_stop_lifecycle();
};

void OpenMptPlaybackBackendTest::initTestCase()
{
    qRegisterMetaType<WaveFlux::PlaybackBackendState>("WaveFlux::PlaybackBackendState");
    qRegisterMetaType<WaveFlux::PlaybackMetadata>("WaveFlux::PlaybackMetadata");
}

void OpenMptPlaybackBackendTest::load_validLocalModule_reportsMetadataAndDuration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy metadataSpy(&backend, &WaveFlux::IPlaybackBackend::metadataChanged);
    QSignalSpy durationSpy(&backend, &WaveFlux::IPlaybackBackend::durationChanged);

    backend.load(modulePath);

    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    QCOMPARE(backend.position(), 0);
    QVERIFY(backend.duration() > 0);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(metadataSpy.count(), 1);

    const WaveFlux::PlaybackMetadata metadata = backend.metadata();
    QCOMPARE(metadata.title, QStringLiteral("WaveFlux MOD Demo"));
    QVERIFY(metadata.artist.isEmpty());
    QCOMPARE(metadata.sourceFormat, QStringLiteral("mod"));
    QVERIFY(!metadata.trackerType.isEmpty());
    QVERIFY(metadata.trackerMessage.isEmpty());

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    QCOMPARE(metadata.channelCount, module->get_num_channels());
    QCOMPARE(metadata.patternCount, module->get_num_patterns());
    QCOMPARE(metadata.instrumentCount, module->get_num_instruments());
}

void OpenMptPlaybackBackendTest::load_windowsDriveLetterPath_reportsMetadataAndDuration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    const QString nativePath = QDir::toNativeSeparators(modulePath);
    if (nativePath.size() < 3 || nativePath.at(1) != QLatin1Char(':')) {
        QSKIP("Windows drive-letter path regression only applies on Windows.");
    }

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

    backend.load(nativePath);

    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    QVERIFY(backend.duration() > 0);
    QCOMPARE(errorSpy.count(), 0);
}

void OpenMptPlaybackBackendTest::load_supportedTrackerExtensions_acceptValidModuleContent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QStringList fileNames = {
        QStringLiteral("demo.669"),
        QStringLiteral("demo.amf"),
        QStringLiteral("demo.dmf"),
        QStringLiteral("demo.mod"),
        QStringLiteral("demo.xm"),
        QStringLiteral("demo.s3m"),
        QStringLiteral("demo.it")
    };

    for (const QString &fileName : fileNames) {
        const QString modulePath = writeModuleFixture(&dir, fileName);
        QVERIFY2(!modulePath.isEmpty(), qPrintable(fileName));

        WaveFlux::OpenMptPlaybackBackend backend;
        QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

        backend.load(modulePath);

        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
        QVERIFY2(backend.duration() > 0, qPrintable(fileName));
        QCOMPARE(errorSpy.count(), 0);
    }
}

void OpenMptPlaybackBackendTest::load_rejectsNonLocalTrackerUrl()
{
    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

    backend.load(QStringLiteral("https://example.com/demo.mod"));

    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Error);
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.takeFirst().constFirst().toString().contains(QStringLiteral("only local files and file:// URLs")));
}

void OpenMptPlaybackBackendTest::load_invalidTrackerModule_emitsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString invalidPath = dir.filePath(QStringLiteral("broken.mod"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalidFile.write("not a tracker module");
    invalidFile.close();

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

    backend.load(invalidPath);

    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Error);
    QCOMPARE(errorSpy.count(), 1);
}

void OpenMptPlaybackBackendTest::load_unavailableTrackerFile_emitsControlledError()
{
    const QString missingPath =
        QDir::temp().filePath(QStringLiteral("waveflux-missing-tracker-%1.mod")
                                  .arg(QDateTime::currentMSecsSinceEpoch()));
    QVERIFY(!QFileInfo::exists(missingPath));

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

    backend.load(missingPath);

    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Error);
    QCOMPARE(errorSpy.count(), 1);
    const QString errorMessage = errorSpy.constFirst().constFirst().toString();
    QVERIFY(!errorMessage.trimmed().isEmpty());
    QVERIFY(errorMessage.contains(QFileInfo(missingPath).fileName(), Qt::CaseInsensitive));
}

void OpenMptPlaybackBackendTest::corpus_manifestFixtures_loadAndSeekWithoutAudioDevice()
{
    const QString corpusDir = trackerTestDataDir();
    QVERIFY2(QDir(corpusDir).exists(), qPrintable(corpusDir));

    const QJsonArray fixtures = loadTrackerManifestFixtures();
    QVERIFY(!fixtures.isEmpty());

    for (const QJsonValue &fixtureValue : fixtures) {
        const QJsonObject fixture = fixtureValue.toObject();
        const QString fileName = fixture.value(QStringLiteral("file")).toString();
        const QString filePath = corpusDir + QLatin1Char('/') + fileName;
        QVERIFY2(QFileInfo::exists(filePath), qPrintable(filePath));

        WaveFlux::OpenMptPlaybackBackend backend;
        QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);
        backend.load(filePath);

        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
        QCOMPARE(errorSpy.count(), 0);
        const WaveFlux::PlaybackMetadata metadata = backend.metadata();
        QCOMPARE(metadata.title, fixture.value(QStringLiteral("expectedTitle")).toString());
        QCOMPARE(metadata.sourceFormat, fixture.value(QStringLiteral("format")).toString());
        QVERIFY2(!metadata.trackerType.isEmpty(), qPrintable(fileName));
        QVERIFY2(!metadata.trackerMessage.contains(QStringLiteral("null"), Qt::CaseInsensitive),
                 qPrintable(fileName));
        QCOMPARE(metadata.channelCount, fixture.value(QStringLiteral("expectedChannels")).toInt());
        QCOMPARE(metadata.patternCount, fixture.value(QStringLiteral("expectedPatterns")).toInt());
        const qint64 expectedDurationMs =
            static_cast<qint64>(fixture.value(QStringLiteral("expectedDurationMs")).toDouble());
        QVERIFY2(qAbs(backend.duration() - expectedDurationMs) <= 20, qPrintable(fileName));

        backend.seek(backend.duration() / 2);
        QVERIFY2(backend.position() >= qMax<qint64>(0, backend.duration() / 2 - 250),
                 qPrintable(fileName));
        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

        std::ifstream stream(filePath.toStdString(), std::ios::binary);
        QVERIFY2(stream.is_open(), qPrintable(filePath));
        openmpt::module module(stream);
        QCOMPARE(module.get_num_channels(), fixture.value(QStringLiteral("expectedChannels")).toInt());
        QCOMPARE(module.get_num_patterns(), fixture.value(QStringLiteral("expectedPatterns")).toInt());
    }
}

void OpenMptPlaybackBackendTest::realmods_supportedCorpus_loadsAndStressSeeksWithoutAudioDevice()
{
    const QFileInfoList realModules = supportedRealMods();
    QVERIFY2(!realModules.isEmpty(), qPrintable(realModsDir()));

    for (const QFileInfo &moduleInfo : realModules) {
        WaveFlux::OpenMptPlaybackBackend backend;
        QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

        backend.load(moduleInfo.absoluteFilePath());

        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
        QCOMPARE(errorSpy.count(), 0);
        QVERIFY2(backend.duration() > 0, qPrintable(moduleInfo.fileName()));

        const WaveFlux::PlaybackMetadata metadata = backend.metadata();
        QVERIFY2(!metadata.title.trimmed().isEmpty(), qPrintable(moduleInfo.fileName()));
        QCOMPARE(metadata.sourceFormat, moduleInfo.suffix().toLower());
        QVERIFY2(!metadata.trackerType.trimmed().isEmpty(), qPrintable(moduleInfo.fileName()));
        QVERIFY2(metadata.channelCount > 0, qPrintable(moduleInfo.fileName()));
        QVERIFY2(metadata.patternCount > 0, qPrintable(moduleInfo.fileName()));

        const qint64 durationMs = backend.duration();
        for (int i = 0; i < 128; ++i) {
            const double normalized = static_cast<double>((i * 17) % 128) / 127.0;
            const qint64 requestedMs = qRound64(normalized * static_cast<double>(durationMs));
            backend.seek(requestedMs);

            const qint64 expectedMs = qBound<qint64>(0, requestedMs, durationMs);
            QVERIFY2(backend.position() >= qMax<qint64>(0, expectedMs - 300),
                     qPrintable(moduleInfo.fileName()));
            QVERIFY2(backend.position() <= durationMs, qPrintable(moduleInfo.fileName()));
            QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
        }

        backend.seek(durationMs - 1);
        QVERIFY2(backend.position() >= qMax<qint64>(0, durationMs - 350),
                 qPrintable(moduleInfo.fileName()));
        QVERIFY2(backend.position() <= durationMs, qPrintable(moduleInfo.fileName()));
        QCOMPARE(errorSpy.count(), 0);
    }
}

void OpenMptPlaybackBackendTest::diagnostics_countersTrackSeekAndDecodeErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

    WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.decodeErrors, 0);
    QCOMPARE(diagnostics.decodeStarvationCount, 0);
    QCOMPARE(diagnostics.seekCount, 0);
    QCOMPARE(diagnostics.dspReconfigurationCount, 0ULL);
    QCOMPARE(diagnostics.loadToFirstAudioFramesMs, -1);
    QCOMPARE(diagnostics.graphSnapshot.limiterStageActive, true);
    QCOMPARE(diagnostics.graphSnapshot.reverseRatePitchStageConfigured, false);

    backend.seek(1000);
    diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.seekCount, 1);
    QVERIFY(diagnostics.lastSeekLatencyUs >= 0);
    QVERIFY(diagnostics.maxSeekLatencyUs >= diagnostics.lastSeekLatencyUs);
    QVERIFY(diagnostics.graphSnapshot.lastFlushGeneration
            <= diagnostics.graphSnapshot.targetGraphGeneration);

    const QString invalidPath = dir.filePath(QStringLiteral("broken.mod"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalidFile.write("not a tracker module");
    invalidFile.close();

    backend.load(invalidPath);
    diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.decodeErrors, 1);
}

void OpenMptPlaybackBackendTest::diagnostics_exportSnapshotIncludesRuntimeBudgetFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("diag-export.mod"));
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

    backend.setReversePlayback(true);
    backend.setReversePlayback(false);
    backend.setEqualizerBands({100.0, 1000.0}, {1.0, -1.0});
    backend.setPlaybackRate(1.25);
    backend.setPitchSemitones(2);

    const WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.dspReconfigurationCount, 1ULL);
    QCOMPARE(diagnostics.reverseActivationCount, 0ULL);
    QVERIFY(diagnostics.lastDspReconfigurationLatencyUs >= 0);
    QVERIFY(diagnostics.maxDspReconfigurationLatencyUs >= diagnostics.lastDspReconfigurationLatencyUs);
    QCOMPARE(diagnostics.rateActivationCount, 0ULL);
    QCOMPARE(diagnostics.pitchActivationCount, 0ULL);

    const QVariantMap snapshot = backend.diagnosticsExportSnapshot();
    QCOMPARE(snapshot.value(QStringLiteral("component")).toString(),
             QStringLiteral("OpenMptPlaybackBackend"));
    QCOMPARE(snapshot.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QVERIFY(snapshot.contains(QStringLiteral("audio")));
    QVERIFY(snapshot.contains(QStringLiteral("device")));
    QVERIFY(snapshot.contains(QStringLiteral("dsp")));
    QVERIFY(snapshot.contains(QStringLiteral("gapless")));
    QVERIFY(snapshot.contains(QStringLiteral("graph")));
    QCOMPARE(snapshot.value(QStringLiteral("graphOrder")).toList().size(), 6);

    const QVariantMap dsp = snapshot.value(QStringLiteral("dsp")).toMap();
    QCOMPARE(dsp.value(QStringLiteral("dspReconfigurationCount")).toULongLong(), 1ULL);
    QCOMPARE(dsp.value(QStringLiteral("reverseActivationCount")).toULongLong(), 0ULL);
    QVERIFY(dsp.value(QStringLiteral("maxDspReconfigurationLatencyUs")).toLongLong()
            >= dsp.value(QStringLiteral("lastDspReconfigurationLatencyUs")).toLongLong());

    const QVariantMap audio = snapshot.value(QStringLiteral("audio")).toMap();
    QCOMPARE(audio.value(QStringLiteral("decodeStarvationCount")).toULongLong(), 0ULL);
    QCOMPARE(audio.value(QStringLiteral("decodeErrors")).toULongLong(), 0ULL);

    const QVariantMap graph = snapshot.value(QStringLiteral("graph")).toMap();
    QCOMPARE(graph.value(QStringLiteral("limiterStageActive")).toBool(), true);
    QCOMPARE(graph.value(QStringLiteral("equalizerStageActive")).toBool(), true);
}

void OpenMptPlaybackBackendTest::diagnostics_traceIsOptInStructuredLog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString invalidPath = dir.filePath(QStringLiteral("broken.mod"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalidFile.write("not a tracker module");
    invalidFile.close();

    CapturedMessages captured;
    g_capturedMessages = &captured;
    g_previousMessageHandler = qInstallMessageHandler(captureMessageHandler);
    qputenv("WAVEFLUX_TRACKER_DIAG", "1");

    {
        WaveFlux::OpenMptPlaybackBackend backend;
        backend.load(invalidPath);
    }

    qunsetenv("WAVEFLUX_TRACKER_DIAG");
    qInstallMessageHandler(g_previousMessageHandler);
    g_previousMessageHandler = nullptr;
    g_capturedMessages = nullptr;

    QMutexLocker locker(&captured.mutex);
    QVERIFY(!captured.messages.isEmpty());
    const QString joinedMessages = captured.messages.join(QLatin1Char('\n'));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"component\":\"OpenMptPlaybackBackend\"")));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"event\":\"error\"")));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"decodeErrors\":1")));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"audioDeviceLossCount\"")));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"lastAudioDeviceStatus\"")));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"graphOrder\":[\"decode\",\"seekResetBoundary\",\"reverseRatePitch\",\"equalizer\",\"spectrumTap\",\"outputClipGuard\"]")));
    QVERIFY(joinedMessages.contains(QStringLiteral("\"limiterStageActive\":true")));
}

void OpenMptPlaybackBackendTest::deviceLifecycle_safeStopOnDefaultOutputLossExposesStatus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("device-loss.mod"));
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    backend.setState(WaveFlux::PlaybackBackendState::Playing, true);

    backend.handleDefaultAudioOutputChanged(QAudioDevice());

    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Paused);
    QCOMPARE(errorSpy.count(), 1);
    const QString message = errorSpy.takeFirst().at(0).toString();
    QVERIFY(message.contains(QStringLiteral("Default audio output device")));

    const WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.audioDeviceLossCount, 1ULL);
    QCOMPARE(diagnostics.audioSafeStopCount, 1ULL);
    QVERIFY(diagnostics.lastAudioDeviceStatus.contains(QStringLiteral("Default audio output device")));
}

void OpenMptPlaybackBackendTest::deviceLifecycle_sinkErrorRecordsRecoveryPolicy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("sink-error.mod"));
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    backend.setState(WaveFlux::PlaybackBackendState::Playing, true);

    backend.recoverFromAudioSinkError(QStringLiteral("Injected tracker sink error."));

    const WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.audioSinkErrorCount, 1ULL);
    QVERIFY(diagnostics.audioRecoveryAttemptCount + diagnostics.audioSafeStopCount >= 1ULL);
    QVERIFY(diagnostics.lastAudioDeviceStatus.contains(QStringLiteral("Injected tracker sink error"))
            || diagnostics.lastAudioDeviceStatus.contains(QStringLiteral("recovered")));
    if (backend.state() == WaveFlux::PlaybackBackendState::Playing) {
        QVERIFY(diagnostics.audioRecoverySuccessCount > 0);
    } else {
        QVERIFY(backend.state() == WaveFlux::PlaybackBackendState::Paused
                || backend.state() == WaveFlux::PlaybackBackendState::Error);
    }
}

void OpenMptPlaybackBackendTest::preloadNext_promotesPreparedModuleAndPreservesMetadata()
{
    const QString firstPath = trackerTestDataDir() + QStringLiteral("/tiny.mod");
    const QString nextPath = trackerTestDataDir() + QStringLiteral("/tiny.xm");
    QVERIFY2(QFileInfo::exists(firstPath), qPrintable(firstPath));
    QVERIFY2(QFileInfo::exists(nextPath), qPrintable(nextPath));

    WaveFlux::OpenMptPlaybackBackend backend;
    backend.load(firstPath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    QCOMPARE(backend.metadata().sourceFormat, QStringLiteral("mod"));

    QVERIFY(backend.preloadNext(nextPath));
    QVERIFY(backend.hasPreloadedNext());
    WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.preloadedNextCount, 1ULL);
    QVERIFY(diagnostics.lastPreloadLatencyUs >= 0);
    QVERIFY(diagnostics.maxPreloadLatencyUs >= diagnostics.lastPreloadLatencyUs);

    backend.load(nextPath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    QCOMPARE(backend.metadata().sourceFormat, QStringLiteral("xm"));
    QVERIFY(backend.duration() > 0);
    diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.promotedPreloadedNextCount, 1ULL);
    QVERIFY(diagnostics.lastGaplessTransitionLatencyUs >= 0);
    QVERIFY(diagnostics.maxGaplessTransitionLatencyUs >= diagnostics.lastGaplessTransitionLatencyUs);
    QVERIFY(!backend.hasPreloadedNext());
}

void OpenMptPlaybackBackendTest::capabilities_reflectTrackerRateSupport()
{
    WaveFlux::OpenMptPlaybackBackend backend;
    const WaveFlux::PlaybackBackendCapabilities capabilities = backend.capabilities();

    QCOMPARE(capabilities.rate, false);
    QCOMPARE(capabilities.timeStretch, false);
    QCOMPARE(capabilities.pitch, false);
    QCOMPARE(capabilities.pitchShift, false);
    QCOMPARE(capabilities.reverse, false);
    QCOMPARE(capabilities.gapless, true);
    QCOMPARE(capabilities.rateWithPitchChange, false);
}

void OpenMptPlaybackBackendTest::capabilities_reflectTrackerPitchSupport()
{
    WaveFlux::OpenMptPlaybackBackend backend;
    const WaveFlux::PlaybackBackendCapabilities capabilities = backend.capabilities();

    QCOMPARE(capabilities.pitch, false);
    QCOMPARE(capabilities.pitchShift, false);
    QCOMPARE(capabilities.rateWithPitchChange, false);
}

void OpenMptPlaybackBackendTest::pcmEngine_repeatedSeekClearsBufferAndKeepsClock()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);

    WaveFlux::TrackerPcmEngine engine;
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);
    QTRY_VERIFY_WITH_TIMEOUT(engine.availableOutputBytes() > 0, 1000);

    QByteArray firstBuffer(4096, Qt::Uninitialized);
    QVERIFY(engine.readInt16(firstBuffer.data(), firstBuffer.size()) > 0);
    QVERIFY(engine.positionMs() > 0);

    for (int i = 0; i < 40; ++i) {
        const qint64 targetMs = (i % 2 == 0) ? 0 : qMin<qint64>(durationMs - 500, 1800);
        const qint64 actualMs = engine.seek(targetMs);
        QVERIFY(actualMs >= qMax<qint64>(0, targetMs - 250));
        QVERIFY(engine.positionMs() >= qMax<qint64>(0, actualMs - 1));
        QVERIFY(engine.positionMs() <= durationMs);
    }

    QTRY_VERIFY_WITH_TIMEOUT(engine.availableOutputBytes() > 0, 1000);
    QByteArray secondBuffer(4096, Qt::Uninitialized);
    QVERIFY(engine.readInt16(secondBuffer.data(), secondBuffer.size()) > 0);
    QVERIFY(engine.positionMs() > 0);
}

void OpenMptPlaybackBackendTest::pcmEngine_eosAfterBufferedFramesDrainOnlyOnce()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/stress-short-silent.mod");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);

    WaveFlux::TrackerPcmEngine engine;
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);

    QByteArray buffer(8192, Qt::Uninitialized);
    int zeroReadsAfterEos = 0;
    for (int i = 0; i < 2000 && zeroReadsAfterEos < 2; ++i) {
        const qint64 bytes = engine.readInt16(buffer.data(), buffer.size());
        if (bytes == 0) {
            if (engine.isEndOfStream()) {
                ++zeroReadsAfterEos;
            } else {
                QTest::qWait(1);
            }
        }
    }

    QVERIFY(engine.isEndOfStream());
    QCOMPARE(engine.readInt16(buffer.data(), buffer.size()), 0);
    QCOMPARE(engine.readInt16(buffer.data(), buffer.size()), 0);
    QCOMPARE(engine.positionMs(), durationMs);
}

void OpenMptPlaybackBackendTest::pcmEngine_snapshotTracksSubmittedAndConsumedFrames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);

    WaveFlux::TrackerPcmEngine engine;
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);
    QTRY_VERIFY_WITH_TIMEOUT(engine.snapshot().submittedFrames > 0, 1000);

    const WaveFlux::TrackerPcmEngineSnapshot before = engine.snapshot();
    QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 2048, Qt::Uninitialized);
    const qint64 bytes = engine.readInt16(buffer.data(), buffer.size());
    QVERIFY(bytes > 0);

    const WaveFlux::TrackerPcmEngineSnapshot after = engine.snapshot();
    QVERIFY(after.submittedFrames >= before.submittedFrames);
    QVERIFY(after.consumedFrames > before.consumedFrames);
    QVERIFY(after.positionMs > before.positionMs);
    QVERIFY(after.decodePositionMs >= after.positionMs);
    QCOMPARE(after.playbackRate, 1.0);
    QCOMPARE(after.pitchSemitones, 0);
    QCOMPARE(after.reversePlayback, false);
    QCOMPARE(after.reverseRatePitchStageConfigured, false);
    QCOMPARE(after.limiterStageActive, true);
    QVERIFY(after.targetGraphGeneration >= after.activeGraphGeneration);
}

void OpenMptPlaybackBackendTest::pcmEngine_snapshotExposesDspGraphStateAndEqRamp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("graph.mod"));
    QVERIFY(!modulePath.isEmpty());

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);

    WaveFlux::TrackerPcmEngine engine;
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);
    QTRY_VERIFY_WITH_TIMEOUT(engine.snapshot().submittedFrames > 0, 1000);

    const std::vector<double> frequenciesHz = {
        31.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
    };
    const std::vector<double> flatGains(10, 0.0);
    const std::vector<double> boostedGains(10, 12.0);

    engine.setEqualizerBands(frequenciesHz, flatGains);
    engine.setEqualizerBands(frequenciesHz, boostedGains);

    const WaveFlux::TrackerPcmEngineSnapshot beforeRead = engine.snapshot();
    QCOMPARE(beforeRead.equalizerStageActive, true);
    QCOMPARE(beforeRead.equalizerBandCount, 10);
    QCOMPARE(beforeRead.pendingEqualizerBandCount, 10);
    QVERIFY(beforeRead.equalizerRampBlocksRemaining > 0);
    QVERIFY(beforeRead.targetGraphGeneration > 0);
    QVERIFY(beforeRead.targetGraphGeneration >= beforeRead.activeGraphGeneration);
    QCOMPARE(beforeRead.limiterStageActive, true);

    QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 1024, Qt::Uninitialized);
    QVERIFY(engine.readInt16(buffer.data(), buffer.size()) > 0);
    QTRY_VERIFY_WITH_TIMEOUT(engine.snapshot().activeGraphGeneration == engine.snapshot().targetGraphGeneration,
                             1000);

    const WaveFlux::TrackerPcmEngineSnapshot afterRead = engine.snapshot();
    QVERIFY(afterRead.equalizerRampBlocksRemaining < beforeRead.equalizerRampBlocksRemaining);
    QVERIFY(afterRead.lastFlushGeneration <= afterRead.targetGraphGeneration);
}

void OpenMptPlaybackBackendTest::pcmEngine_playbackRateChangesClockAndSeekRemainStable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("rate.mod"));
    QVERIFY(!modulePath.isEmpty());

    std::unique_ptr<std::istream> neutralStream;
    std::unique_ptr<openmpt::module> neutralModule = openModule(modulePath, &neutralStream);
    QVERIFY(neutralModule != nullptr);
    const qint64 durationMs = qRound64(neutralModule->get_duration_seconds() * 1000.0);

    auto neutralEngine = std::make_unique<WaveFlux::TrackerPcmEngine>();
    neutralEngine->load(std::move(neutralModule), std::move(neutralStream), nullptr, durationMs);
    neutralEngine->setPlaybackRate(1.0);

    std::unique_ptr<std::istream> fastStream;
    std::unique_ptr<openmpt::module> fastModule = openModule(modulePath, &fastStream);
    QVERIFY(fastModule != nullptr);

    auto fastEngine = std::make_unique<WaveFlux::TrackerPcmEngine>();
    fastEngine->load(std::move(fastModule), std::move(fastStream), nullptr, durationMs);
    fastEngine->setPlaybackRate(1.5);

    QTRY_VERIFY_WITH_TIMEOUT(neutralEngine->availableOutputBytes() > 0, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(fastEngine->availableOutputBytes() > 0, 1000);

    QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 4096, Qt::Uninitialized);
    QVERIFY(neutralEngine->readInt16(buffer.data(), buffer.size()) > 0);
    QVERIFY(fastEngine->readInt16(buffer.data(), buffer.size()) > 0);
    QTRY_VERIFY_WITH_TIMEOUT(neutralEngine->availableOutputBytes() > 0, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(fastEngine->availableOutputBytes() > 0, 1000);
    QVERIFY(neutralEngine->readInt16(buffer.data(), buffer.size()) > 0);
    QVERIFY(fastEngine->readInt16(buffer.data(), buffer.size()) > 0);

    const qint64 neutralPositionMs = neutralEngine->positionMs();
    const qint64 fastPositionMs = fastEngine->positionMs();
    QCOMPARE(fastPositionMs, neutralPositionMs);
    QCOMPARE(fastEngine->snapshot().playbackRate, 1.0);

    fastEngine->seek(1500);
    QTRY_VERIFY_WITH_TIMEOUT(fastEngine->availableOutputBytes() > 0, 1000);
    QVERIFY(fastEngine->readInt16(buffer.data(), buffer.size()) > 0);
    const qint64 seekedPositionMs = fastEngine->positionMs();
    QVERIFY(seekedPositionMs >= 1450);
    QVERIFY(seekedPositionMs <= fastEngine->durationMs());
}

void OpenMptPlaybackBackendTest::pcmEngine_pitchShiftPreservesClockAndSeekRemainStable()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/tiny.xm");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    std::unique_ptr<std::istream> neutralStream;
    std::unique_ptr<openmpt::module> neutralModule = openModule(modulePath, &neutralStream);
    QVERIFY(neutralModule != nullptr);
    const qint64 durationMs = qRound64(neutralModule->get_duration_seconds() * 1000.0);

    auto neutralEngine = std::make_unique<WaveFlux::TrackerPcmEngine>();
    neutralEngine->load(std::move(neutralModule), std::move(neutralStream), nullptr, durationMs);

    std::unique_ptr<std::istream> pitchedStream;
    std::unique_ptr<openmpt::module> pitchedModule = openModule(modulePath, &pitchedStream);
    QVERIFY(pitchedModule != nullptr);

    auto pitchedEngine = std::make_unique<WaveFlux::TrackerPcmEngine>();
    pitchedEngine->load(std::move(pitchedModule), std::move(pitchedStream), nullptr, durationMs);
    pitchedEngine->setPitchSemitones(4);

    QTRY_VERIFY_WITH_TIMEOUT(neutralEngine->availableOutputBytes() > 0, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(pitchedEngine->availableOutputBytes() > 0, 1000);

    QByteArray neutralBuffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 4096, Qt::Uninitialized);
    QByteArray pitchedBuffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 4096, Qt::Uninitialized);

    QVERIFY(neutralEngine->readInt16(neutralBuffer.data(), neutralBuffer.size()) > 0);
    QVERIFY(pitchedEngine->readInt16(pitchedBuffer.data(), pitchedBuffer.size()) > 0);
    QTRY_VERIFY_WITH_TIMEOUT(neutralEngine->availableOutputBytes() > 0, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(pitchedEngine->availableOutputBytes() > 0, 1000);
    QVERIFY(neutralEngine->readInt16(neutralBuffer.data(), neutralBuffer.size()) > 0);
    QVERIFY(pitchedEngine->readInt16(pitchedBuffer.data(), pitchedBuffer.size()) > 0);

    const qint64 neutralPositionMs = neutralEngine->positionMs();
    const qint64 pitchedPositionMs = pitchedEngine->positionMs();
    QCOMPARE(pitchedPositionMs, neutralPositionMs);
    QCOMPARE(pitchedEngine->snapshot().pitchSemitones, 0);

    pitchedEngine->setPlaybackRate(1.5);
    pitchedEngine->seek(1500);
    QTRY_VERIFY_WITH_TIMEOUT(pitchedEngine->availableOutputBytes() > 0, 1000);
    QVERIFY(pitchedEngine->readInt16(pitchedBuffer.data(), pitchedBuffer.size()) > 0);
    const qint64 seekedPositionMs = pitchedEngine->positionMs();
    QVERIFY(seekedPositionMs >= 1450);
    QVERIFY(seekedPositionMs <= pitchedEngine->durationMs());

}

void OpenMptPlaybackBackendTest::pcmEngine_reversePlaybackUsesBoundedWindowsAndSeek()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/stress-long-loopish.mod");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);
    QVERIFY(durationMs > 4000);

    WaveFlux::TrackerPcmEngine engine;
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);
    const qint64 startMs = engine.seek(qMin<qint64>(durationMs - 500, 3200));
    engine.setReversePlayback(true);

    QTRY_VERIFY_WITH_TIMEOUT(engine.availableOutputBytes() > 0, 1000);
    const WaveFlux::TrackerPcmEngineSnapshot beforeRead = engine.snapshot();
    QCOMPARE(beforeRead.reversePlayback, false);
    QCOMPARE(beforeRead.reverseRatePitchStageConfigured, false);
    QVERIFY(beforeRead.positionMs >= startMs);
    QVERIFY(beforeRead.bufferedFrames <= static_cast<quint64>(WaveFlux::TrackerPcmEngine::kSampleRate * 2));

    QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 4096, Qt::Uninitialized);
    QVERIFY(engine.readInt16(buffer.data(), buffer.size()) > 0);
    const qint64 afterReadPositionMs = engine.positionMs();
    QVERIFY(afterReadPositionMs >= beforeRead.positionMs);
    QVERIFY(afterReadPositionMs >= 0);

    const qint64 seekTargetMs = 1800;
    const qint64 actualSeekMs = engine.seek(seekTargetMs);
    QVERIFY(actualSeekMs >= seekTargetMs - 250);
    QTRY_VERIFY_WITH_TIMEOUT(engine.availableOutputBytes() > 0, 1000);
    QVERIFY(engine.readInt16(buffer.data(), buffer.size()) > 0);
    QVERIFY(engine.positionMs() >= actualSeekMs);
    QVERIFY(engine.positionMs() >= 0);
}

void OpenMptPlaybackBackendTest::pcmEngine_reversePlaybackReachesEosAtZero()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/tiny.mod");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
    QVERIFY(module != nullptr);
    const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);

    WaveFlux::TrackerPcmEngine engine;
    engine.load(std::move(module), std::move(stream), nullptr, durationMs);
    engine.seek(120);
    engine.setReversePlayback(true);

    QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 2048, Qt::Uninitialized);
    for (int i = 0; i < 2000 && !engine.isEndOfStream(); ++i) {
        if (engine.readInt16(buffer.data(), buffer.size()) == 0) {
            QTest::qWait(1);
        }
    }

    QVERIFY(engine.isEndOfStream());
    QCOMPARE(engine.snapshot().reversePlayback, false);
    QVERIFY(engine.positionMs() >= 120);
    QCOMPARE(engine.readInt16(buffer.data(), buffer.size()), 0);
}

void OpenMptPlaybackBackendTest::pcmEngine_equalizerBandsChangeRenderedPcm()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("eq-render.mod"));
    QVERIFY(!modulePath.isEmpty());

    const std::vector<double> flatGains(10, 0.0);
    const std::vector<double> boostedGains(10, 12.0);
    const std::vector<double> cutGains(10, -24.0);

    const double flatRms = renderRmsWithEqualizer(modulePath, flatGains);
    const double boostedRms = renderRmsWithEqualizer(modulePath, boostedGains);
    const double cutRms = renderRmsWithEqualizer(modulePath, cutGains);

    QVERIFY(flatRms > 0.0001);
    QVERIFY(boostedRms > flatRms * 1.05);
    QVERIFY(cutRms < flatRms * 0.95);
}

void OpenMptPlaybackBackendTest::regression_repeatedSeek500OperationsStayBounded()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/stress-rapid-seek.mod");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);
    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    QVERIFY(backend.duration() > 1000);

    const qint64 durationMs = backend.duration();
    for (int i = 0; i < 520; ++i) {
        const qint64 targetMs = (static_cast<qint64>(i) * 173) % qMax<qint64>(1, durationMs);
        backend.seek(targetMs);
        QVERIFY2(backend.position() >= qMax<qint64>(0, targetMs - 300), qPrintable(QString::number(i)));
        QVERIFY2(backend.position() <= durationMs, qPrintable(QString::number(i)));
        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    }

    const WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.seekCount, 520ULL);
    QVERIFY(diagnostics.maxSeekLatencyUs >= diagnostics.lastSeekLatencyUs);
    QCOMPARE(diagnostics.decodeErrors, 0ULL);
    QCOMPARE(errorSpy.count(), 0);
}

void OpenMptPlaybackBackendTest::regression_reverseShortAndLongModulesStayBounded()
{
    const QStringList modulePaths = {
        trackerTestDataDir() + QStringLiteral("/tiny.mod"),
        trackerTestDataDir() + QStringLiteral("/stress-long-loopish.mod")
    };

    for (const QString &modulePath : modulePaths) {
        QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));
        std::unique_ptr<std::istream> stream;
        std::unique_ptr<openmpt::module> module = openModule(modulePath, &stream);
        QVERIFY2(module != nullptr, qPrintable(modulePath));
        const qint64 durationMs = qRound64(module->get_duration_seconds() * 1000.0);
        QVERIFY2(durationMs > 0, qPrintable(modulePath));

        WaveFlux::TrackerPcmEngine engine;
        engine.load(std::move(module), std::move(stream), nullptr, durationMs);
        engine.seek(qMin<qint64>(durationMs, qMax<qint64>(120, durationMs / 2)));
        engine.setReversePlayback(true);
        QTRY_VERIFY_WITH_TIMEOUT(engine.availableOutputBytes() > 0, 1000);

        const WaveFlux::TrackerPcmEngineSnapshot before = engine.snapshot();
        QCOMPARE(before.reversePlayback, false);
        QVERIFY2(before.bufferedFrames <= static_cast<quint64>(WaveFlux::TrackerPcmEngine::kSampleRate * 2),
                 qPrintable(modulePath));

        QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 2048, Qt::Uninitialized);
        const qint64 bytes = engine.readInt16(buffer.data(), buffer.size());
        QVERIFY2(bytes > 0, qPrintable(modulePath));
        QVERIFY2(engine.positionMs() >= before.positionMs, qPrintable(modulePath));
        QVERIFY2(engine.positionMs() >= 0, qPrintable(modulePath));
    }
}

void OpenMptPlaybackBackendTest::regression_repeatedDspTogglesKeepCapabilitiesAndDiagnostics()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/tiny.xm");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);
    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

    const std::vector<double> frequenciesHz = {
        31.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
    };
    const std::vector<double> flatGains(10, 0.0);
    const std::vector<double> boostedGains(10, 6.0);

    for (int i = 0; i < 64; ++i) {
        backend.setReversePlayback((i % 2) != 0);
        backend.setEqualizerBands(frequenciesHz, (i % 3 == 0) ? boostedGains : flatGains);
        backend.setPlaybackRate(1.25);
        backend.setPitchSemitones(2);
        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
        QVERIFY(backend.position() >= 0);
        QVERIFY(backend.position() <= backend.duration());
    }

    const WaveFlux::PlaybackBackendCapabilities capabilities = backend.capabilities();
    QCOMPARE(capabilities.reverse, false);
    QCOMPARE(capabilities.equalizer, true);
    QCOMPARE(capabilities.spectrum, true);
    QCOMPARE(capabilities.rateWithPitchChange, false);
    QCOMPARE(capabilities.rate, false);
    QCOMPARE(capabilities.pitch, false);

    const WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.reverseActivationCount, 0ULL);
    QVERIFY(diagnostics.dspReconfigurationCount >= 1ULL);
    QVERIFY(diagnostics.maxDspReconfigurationLatencyUs >= diagnostics.lastDspReconfigurationLatencyUs);
    QCOMPARE(diagnostics.decodeErrors, 0ULL);
    QCOMPARE(errorSpy.count(), 0);
}

void OpenMptPlaybackBackendTest::regression_dspInteractionsEqRatePitchSpectrumReverse()
{
    const QString modulePath = trackerTestDataDir() + QStringLiteral("/tiny.xm");
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy pcmTapSpy(&backend, &WaveFlux::OpenMptPlaybackBackend::pcmFramesReady);
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);
    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

    const std::vector<double> frequenciesHz = {
        31.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
    };
    const std::vector<double> boostedGains(10, 4.0);
    backend.setEqualizerBands(frequenciesHz, boostedGains);
    backend.setPlaybackRate(1.25);
    backend.setPitchSemitones(2);
    backend.setPcmTapEnabled(true);

    QByteArray buffer(WaveFlux::TrackerPcmEngine::kOutputBytesPerFrame * 4096, Qt::Uninitialized);
    for (int i = 0; i < 20 && pcmTapSpy.count() == 0; ++i) {
        if (backend.readAudioData(buffer.data(), buffer.size()) <= 0) {
            QTest::qWait(1);
        }
    }
    QVERIFY(pcmTapSpy.count() > 0);

    WaveFlux::OpenMptPlaybackDiagnostics diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.graphSnapshot.equalizerStageActive, true);
    QCOMPARE(diagnostics.graphSnapshot.playbackRate, 1.0);
    QCOMPARE(diagnostics.graphSnapshot.pitchSemitones, 0);

    backend.setReversePlayback(true);
    backend.seek(qMin<qint64>(backend.duration(), 1200));
    QTRY_VERIFY_WITH_TIMEOUT(backend.m_pcmEngine.availableOutputBytes() > 0, 1000);
    QVERIFY(backend.readAudioData(buffer.data(), buffer.size()) > 0);
    diagnostics = backend.diagnosticsSnapshot();
    QCOMPARE(diagnostics.graphSnapshot.reversePlayback, false);
    QCOMPARE(diagnostics.graphSnapshot.equalizerStageActive, true);
    QCOMPARE(diagnostics.decodeErrors, 0ULL);
    QCOMPARE(errorSpy.count(), 0);
}

void OpenMptPlaybackBackendTest::seek_loadedModule_handlesBoundariesAndRepeatedTargets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy errorSpy(&backend, &WaveFlux::IPlaybackBackend::error);

    backend.load(modulePath);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    const qint64 durationMs = backend.duration();
    QVERIFY(durationMs > 2500);

    backend.seek(0);
    QCOMPARE(backend.position(), 0);

    const QList<qint64> targets = {
        durationMs / 2,
        0,
        qMax<qint64>(0, durationMs - 1),
        300,
        durationMs + 10000,
        -1000,
        qMin<qint64>(durationMs, 1200)
    };

    for (qint64 targetMs : targets) {
        backend.seek(targetMs);
        const qint64 expectedMin = qBound<qint64>(0, targetMs, durationMs);
        QVERIFY(backend.position() >= qMax<qint64>(0, expectedMin - 250));
        QVERIFY(backend.position() <= durationMs);
        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    }

    QCOMPARE(errorSpy.count(), 0);
}

void OpenMptPlaybackBackendTest::play_seek_pause_stop_lifecycle()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    WaveFlux::OpenMptPlaybackBackend backend;
    QSignalSpy stateSpy(&backend, &WaveFlux::IPlaybackBackend::stateChanged);
    backend.load(QUrl::fromLocalFile(modulePath).toString());

    backend.play();
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Playing);
    QTRY_VERIFY_WITH_TIMEOUT(backend.position() > 0, 3000);

    backend.seek(1000);
    QTRY_VERIFY_WITH_TIMEOUT(backend.position() >= 900, 3000);

    backend.pause();
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Paused);

    backend.play();
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Playing);

    backend.stop();
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Stopped);
    QCOMPARE(backend.position(), 0);
    QVERIFY(stateSpy.count() >= 4);
}

QTEST_MAIN(OpenMptPlaybackBackendTest)

#include "tst_OpenMptPlaybackBackend.moc"
