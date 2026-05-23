#include <QAudioDevice>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

#include <gst/gst.h>

#include "AudioEngine.h"
#include "AppSettingsManager.h"
#include "playback/PlaybackBackendRouting.h"

class PlaybackBackendRoutingTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void trackerExtensions_areRecognized();
    void trackerCorpus_formatsMatchRoutingAndFilePicker();
    void preferredBackend_respectsLocality();
    void problematicStmSeek_isExplicitlyBlocked();
    void unsupportedTrackerCandidates_areRejectedBeforePlaybackStarts();
    void midiSources_areRejectedBeforePlaybackStarts();
    void audioEngine_tracksPreferredBackendOnLoad();
    void audioEngine_invalidTrackerModuleReportsOpenMptError();
    void audioEngine_exposesBackendCapabilities();
    void audioEngine_exposesCapabilityReasons();
    void audioEngine_trackerLoadRetainsPitchWhenSupported();
    void audioEngine_usesOpenMptBackendForTrackerPlayback();
    void audioEngine_trackerRatePitchChangeWhilePlaying();
    void audioEngine_trackerRepeatedSeekAndNearEofStayResponsive();
    void audioEngine_trackerRapidScrubCoalescesSeekBursts();
    void audioEngine_trackerSpectrumStartsStopsWithPlayback();
    void audioEngine_trackerReverseUnavailableDoesNotSpamErrors();
    void audioEngine_trackerEqualizerAppliesPresetAndReset();
    void audioEngine_trackerDisablesGStreamerOnlyFeatures();
    void audioEngine_repeatedTrackerOrdinaryBackendSwitchClearsTrackerState();
    void audioEngine_remoteTrackerUrlDownloadsToCacheAndUsesOpenMptBackend();
    void audioEngine_remoteTrackerDownloadCancelsSafelyOnNewLoad();
};

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
    appendFixedString(&data, "WaveFlux Engine Demo", 20);

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

QString writeModuleFixture(QTemporaryDir *dir, const QString &fileName = QStringLiteral("engine-demo.mod"))
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

bool levelsHaveEnergy(const QVariantList &levels)
{
    for (const QVariant &level : levels) {
        if (level.toDouble() > 0.001) {
            return true;
        }
    }
    return false;
}

bool levelsAreZero(const QVariantList &levels)
{
    for (const QVariant &level : levels) {
        if (level.toDouble() != 0.0) {
            return false;
        }
    }
    return true;
}

QString trackerTestDataDir()
{
#ifdef WAVEFLUX_TESTDATA_DIR
    return QString::fromUtf8(WAVEFLUX_TESTDATA_DIR) + QStringLiteral("/tracker");
#else
    return QFINDTESTDATA("testdata/tracker");
#endif
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

class HttpFixtureServer final : public QObject
{
    Q_OBJECT

public:
    struct Response {
        QByteArray body;
        int chunkSize = 0;
        int chunkDelayMs = 0;
    };

    explicit HttpFixtureServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &HttpFixtureServer::handleConnection);
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost);
    }

    QUrl urlForPath(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    void setResponse(const QString &path, const Response &response)
    {
        m_responses.insert(path, response);
    }

private:
    void handleConnection()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                m_requestBuffers[socket].append(socket->readAll());
                if (!m_requestBuffers.value(socket).contains("\r\n\r\n")) {
                    return;
                }

                const QList<QByteArray> lines = m_requestBuffers.take(socket).split('\n');
                const QList<QByteArray> requestLine =
                    !lines.isEmpty() ? lines.first().trimmed().split(' ') : QList<QByteArray>{};
                const QString path =
                    requestLine.size() >= 2 ? QString::fromLatin1(requestLine.at(1)) : QStringLiteral("/");
                const Response response = m_responses.value(path);

                const QByteArray header =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n"
                    "Connection: close\r\n\r\n";
                socket->write(header);

                if (response.chunkSize <= 0 || response.chunkDelayMs <= 0) {
                    socket->write(response.body);
                    socket->disconnectFromHost();
                    return;
                }

                auto offset = std::make_shared<int>(0);
                auto timer = new QTimer(socket);
                timer->setInterval(response.chunkDelayMs);
                connect(timer, &QTimer::timeout, socket, [socket, timer, response, offset]() {
                    if (!socket->isOpen()) {
                        timer->stop();
                        timer->deleteLater();
                        return;
                    }
                    const int remaining = response.body.size() - *offset;
                    if (remaining <= 0) {
                        timer->stop();
                        socket->disconnectFromHost();
                        timer->deleteLater();
                        return;
                    }
                    const int chunk = qMin(response.chunkSize, remaining);
                    socket->write(response.body.constData() + *offset, chunk);
                    *offset += chunk;
                });
                timer->start();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        }
    }

    QTcpServer m_server;
    QHash<QString, Response> m_responses;
    QHash<QTcpSocket *, QByteArray> m_requestBuffers;
};

} // namespace

void PlaybackBackendRoutingTest::initTestCase()
{
    int argc = 0;
    char **argv = nullptr;
    gst_init(&argc, &argv);
    QStandardPaths::setTestModeEnabled(true);
}

void PlaybackBackendRoutingTest::trackerExtensions_areRecognized()
{
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("669")));
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("amf")));
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("DMF")));
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("mod")));
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("XM")));
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("s3m")));
    QVERIFY(WaveFlux::isTrackerModuleExtension(QStringLiteral("it")));
    QVERIFY(WaveFlux::isUnsupportedTrackerModuleExtension(QStringLiteral("mptm")));
    QVERIFY(WaveFlux::isUnsupportedTrackerModuleExtension(QStringLiteral("MED")));
    QVERIFY(WaveFlux::isUnsupportedTrackerModuleExtension(QStringLiteral("umx")));
    QVERIFY(WaveFlux::isUnsupportedTrackerModuleExtension(QStringLiteral("ahx")));
    QVERIFY(!WaveFlux::isTrackerModuleExtension(QStringLiteral("mp3")));

    QCOMPARE(WaveFlux::trackerModuleExtensions(),
             QStringList({QStringLiteral("669"),
                          QStringLiteral("amf"),
                          QStringLiteral("dmf"),
                          QStringLiteral("mod"),
                          QStringLiteral("xm"),
                          QStringLiteral("s3m"),
                          QStringLiteral("it")}));
}

void PlaybackBackendRoutingTest::trackerCorpus_formatsMatchRoutingAndFilePicker()
{
    const QJsonArray fixtures = loadTrackerManifestFixtures();
    QVERIFY(!fixtures.isEmpty());

    QSet<QString> manifestFormats;
    for (const QJsonValue &fixtureValue : fixtures) {
        const QJsonObject fixture = fixtureValue.toObject();
        const QString format = fixture.value(QStringLiteral("format")).toString().trimmed().toLower();
        const QString fileName = fixture.value(QStringLiteral("file")).toString().trimmed();
        QVERIFY2(!format.isEmpty(), qPrintable(fileName));
        QVERIFY2(!fileName.isEmpty(), "Manifest fixture is missing file name.");
        manifestFormats.insert(format);
        QVERIFY2(WaveFlux::isTrackerModuleExtension(format), qPrintable(format));
    }

    const QSet<QString> routingFormats(
        WaveFlux::trackerModuleExtensions().cbegin(),
        WaveFlux::trackerModuleExtensions().cend());
    for (const QString &format : manifestFormats) {
        QVERIFY2(routingFormats.contains(format), qPrintable(format));
    }
    QVERIFY(routingFormats.contains(QStringLiteral("amf")));
    QVERIFY(routingFormats.contains(QStringLiteral("dmf")));

    const QStringList globPatterns = WaveFlux::trackerModuleGlobPatterns();
    QCOMPARE(globPatterns.size(), WaveFlux::trackerModuleExtensions().size());
    for (const QString &extension : WaveFlux::trackerModuleExtensions()) {
        QVERIFY(globPatterns.contains(QStringLiteral("*.") + extension));
    }
}

void PlaybackBackendRoutingTest::preferredBackend_respectsLocality()
{
    QVERIFY(WaveFlux::isTrackerModuleSource(QStringLiteral("/music/test.mod")));
    QVERIFY(WaveFlux::isTrackerModuleSource(QStringLiteral("/music/test.669")));
    QVERIFY(WaveFlux::isTrackerModuleSource(QStringLiteral("/music/test.amf")));
    QVERIFY(WaveFlux::isTrackerModuleSource(QStringLiteral("file:///music/test.xm")));
    QVERIFY(WaveFlux::isTrackerModuleSource(QStringLiteral("file:///music/test.dmf")));
    QVERIFY(!WaveFlux::isTrackerModuleSource(QStringLiteral("https://example.com/test.it")));
    QVERIFY(!WaveFlux::isTrackerModuleSource(QStringLiteral("https://example.com/test.mp3")));
    QVERIFY(WaveFlux::isUnsupportedTrackerModuleSource(QStringLiteral("/music/test.med")));
    QVERIFY(WaveFlux::isUnsupportedTrackerModuleSource(QStringLiteral("https://example.com/test.umx")));
    QVERIFY(WaveFlux::isMidiExtension(QStringLiteral("mid")));
    QVERIFY(WaveFlux::isMidiExtension(QStringLiteral("MIDI")));
    QVERIFY(WaveFlux::isMidiSource(QStringLiteral("/music/test.mid")));
    QVERIFY(WaveFlux::isMidiSource(QStringLiteral("file:///music/test.midi")));
    QVERIFY(WaveFlux::isMidiSource(QStringLiteral("https://example.com/test.mid")));

    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("/music/test.mod")),
             WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("/music/test.669")),
             WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("/music/test.amf")),
             WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("file:///music/test.it")),
             WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("file:///music/test.dmf")),
             WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("https://example.com/test.it")),
             WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(WaveFlux::preferredPlaybackBackendForSource(QStringLiteral("/music/test.flac")),
             WaveFlux::PlaybackBackendKind::GStreamer);
}

void PlaybackBackendRoutingTest::problematicStmSeek_isExplicitlyBlocked()
{
    const QString stmPath =
        trackerTestDataDir() + QStringLiteral("/realmods/arcanoid.stm");
    QVERIFY2(QFileInfo::exists(stmPath), qPrintable(stmPath));

    QVERIFY(WaveFlux::isSeekBlockedForSource(stmPath));
    QVERIFY(WaveFlux::isSeekBlockedForSource(QStringLiteral("file://") + stmPath));
    QVERIFY(!WaveFlux::isSeekBlockedForSource(QStringLiteral("/music/test.it")));
    QVERIFY(!WaveFlux::isSeekBlockedForSource(QStringLiteral("https://example.com/test.stm")));
}

void PlaybackBackendRoutingTest::unsupportedTrackerCandidates_areRejectedBeforePlaybackStarts()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString regularPath = dir.filePath(QStringLiteral("baseline.flac"));
    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();

    const QString unsupportedPath = dir.filePath(QStringLiteral("candidate.med"));
    QFile unsupportedFile(unsupportedPath);
    QVERIFY(unsupportedFile.open(QIODevice::WriteOnly));
    unsupportedFile.write("not yet in WaveFlux matrix");
    unsupportedFile.close();

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadFile(regularPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(engine.currentFile(), regularPath);

    engine.loadFile(unsupportedPath);

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.at(0).at(0).toString().contains(
        AppSettingsManager::translateForCurrentLanguage(QStringLiteral("error.trackerUnsupported")).arg(QString()).trimmed()));
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(engine.currentFile(), regularPath);

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::midiSources_areRejectedBeforePlaybackStarts()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString regularPath = dir.filePath(QStringLiteral("baseline.flac"));
    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();

    const QString midiPath = dir.filePath(QStringLiteral("unsupported.mid"));
    QFile midiFile(midiPath);
    QVERIFY(midiFile.open(QIODevice::WriteOnly));
    midiFile.write("MThd");
    midiFile.close();

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadFile(regularPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(engine.currentFile(), regularPath);

    engine.loadFile(midiPath);

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.at(0).at(0).toString(),
             AppSettingsManager::translateForCurrentLanguage(QStringLiteral("error.midiUnsupported")).arg(QStringLiteral("unsupported.mid")));
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(engine.currentFile(), regularPath);

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_tracksPreferredBackendOnLoad()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString trackerPath = dir.filePath(QStringLiteral("demo.it"));
    const QString regularPath = dir.filePath(QStringLiteral("demo.flac"));

    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly));
    trackerFile.close();

    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();

    AudioEngine engine;

    engine.loadFile(trackerPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(engine.currentFile(), trackerPath);

    engine.loadFile(regularPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(engine.currentFile(), regularPath);

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_remoteTrackerUrlDownloadsToCacheAndUsesOpenMptBackend()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    HttpFixtureServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/remote.mod"), {.body = createMinimalModFile()});

    AudioEngine engine;
    QSignalSpy downloadSpy(&engine, &AudioEngine::remoteTrackerDownloadChanged);

    const QUrl remoteUrl = server.urlForPath(QStringLiteral("/remote.mod"));
    engine.loadUrl(remoteUrl);

    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(engine.currentFile(), remoteUrl.toString());
    QVERIFY(engine.remoteSourcesAvailable());
    QVERIFY(engine.remoteTrackerDownloadActive());

    QTRY_VERIFY(!engine.remoteTrackerDownloadActive());
    QVERIFY(downloadSpy.count() > 0);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(engine.currentFile(), remoteUrl.toString());
    QVERIFY(engine.remoteTrackerDownloadStatus().isEmpty());

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_remoteTrackerDownloadCancelsSafelyOnNewLoad()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    HttpFixtureServer server;
    QVERIFY(server.listen());
    HttpFixtureServer::Response response;
    response.body = createMinimalModFile() + QByteArray(16384, '\x33');
    response.chunkSize = 256;
    response.chunkDelayMs = 20;
    server.setResponse(QStringLiteral("/slow.mod"), response);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString regularPath = dir.filePath(QStringLiteral("regular.flac"));
    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadUrl(server.urlForPath(QStringLiteral("/slow.mod")));
    QTRY_VERIFY(engine.remoteTrackerDownloadActive());

    engine.loadFile(regularPath);

    QTRY_VERIFY(!engine.remoteTrackerDownloadActive());
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QCOMPARE(engine.currentFile(), regularPath);
    QCOMPARE(errorSpy.count(), 0);

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_invalidTrackerModuleReportsOpenMptError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString invalidPath = dir.filePath(QStringLiteral("broken.mod"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalidFile.write("not a tracker module");
    invalidFile.close();

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadFile(invalidPath);

    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(engine.currentFile(), invalidPath);
    QCOMPARE(engine.state(), AudioEngine::ErrorState);
    QCOMPARE(errorSpy.count(), 1);
}

void PlaybackBackendRoutingTest::audioEngine_exposesBackendCapabilities()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString trackerPath = dir.filePath(QStringLiteral("capabilities.mod"));
    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly));
    trackerFile.close();

    const QString regularPath = dir.filePath(QStringLiteral("capabilities.flac"));
    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();

    AudioEngine engine;
    engine.loadFile(trackerPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QVERIFY(engine.seekAvailable());
    QVERIFY(engine.waveformAvailable());
    QVERIFY(engine.spectrumAvailable());
    QVERIFY(engine.equalizerAvailable());
    QVERIFY(!engine.reverseAvailable());
    QVERIFY(engine.gaplessAvailable());
    QCOMPARE(engine.rateAvailable(), false);
    QCOMPARE(engine.pitchAvailable(), false);
    QVERIFY(!engine.rateWithPitchChangeAvailable());
    QCOMPARE(engine.timeStretchAvailable(), false);
    QCOMPARE(engine.pitchShiftAvailable(), false);
    QVERIFY(engine.remoteSourcesAvailable());

    const QVariantMap trackerCapabilities = engine.playbackCapabilities();
    QCOMPARE(trackerCapabilities.value(QStringLiteral("seek")).toBool(), true);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("waveform")).toBool(), true);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("spectrum")).toBool(), true);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("equalizer")).toBool(), true);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("reverse")).toBool(), false);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("gapless")).toBool(), true);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("rate")).toBool(), false);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("pitch")).toBool(), false);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("rateWithPitchChange")).toBool(), false);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("timeStretch")).toBool(), false);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("pitchShift")).toBool(), false);
    QCOMPARE(trackerCapabilities.value(QStringLiteral("remoteSources")).toBool(), true);

    engine.loadFile(regularPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
    QVERIFY(engine.seekAvailable());
    QVERIFY(engine.waveformAvailable());
    QCOMPARE(engine.spectrumAvailable(), engine.playbackCapabilities().value(QStringLiteral("spectrum")).toBool());
    QCOMPARE(engine.equalizerAvailable(), engine.playbackCapabilities().value(QStringLiteral("equalizer")).toBool());
    QVERIFY(engine.reverseAvailable());
    QVERIFY(!engine.gaplessAvailable());
    QVERIFY(engine.rateAvailable());
    QCOMPARE(engine.pitchAvailable(), engine.playbackCapabilities().value(QStringLiteral("pitch")).toBool());
    QCOMPARE(engine.rateWithPitchChangeAvailable(),
             engine.playbackCapabilities().value(QStringLiteral("rateWithPitchChange")).toBool());
    QCOMPARE(engine.timeStretchAvailable(),
             engine.playbackCapabilities().value(QStringLiteral("timeStretch")).toBool());
    QCOMPARE(engine.pitchShiftAvailable(),
             engine.playbackCapabilities().value(QStringLiteral("pitchShift")).toBool());
    QVERIFY(engine.remoteSourcesAvailable());
    QVERIFY(engine.rateWithPitchChangeAvailable());
    QVERIFY(!engine.timeStretchAvailable());
    QCOMPARE(engine.pitchShiftAvailable(), engine.pitchAvailable());

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_exposesCapabilityReasons()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString trackerPath = dir.filePath(QStringLiteral("capability-reasons.mod"));
    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly));
    trackerFile.close();

    const QString regularPath = dir.filePath(QStringLiteral("capability-reasons.flac"));
    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();

    AudioEngine engine;

    engine.loadFile(trackerPath);
    const QVariantMap trackerReasons = engine.playbackCapabilityReasons();
    QCOMPARE(trackerReasons.value(QStringLiteral("reverse")).toString(),
             QStringLiteral("settings.reversePlaybackUnavailableDescription"));
    if (engine.rateAvailable()) {
        QVERIFY(!trackerReasons.contains(QStringLiteral("rate")));
    } else {
        QCOMPARE(trackerReasons.value(QStringLiteral("rate")).toString(),
                 QStringLiteral("settings.speedUnavailableDescription"));
    }
    if (engine.pitchAvailable()) {
        QVERIFY(!trackerReasons.contains(QStringLiteral("pitch")));
    } else {
        QCOMPARE(trackerReasons.value(QStringLiteral("pitch")).toString(),
                 QStringLiteral("settings.pitchUnavailableDescription"));
    }
    QCOMPARE(trackerReasons.value(QStringLiteral("audioQualityProfile")).toString(),
             QStringLiteral("settings.audioQualityProfileUnavailableDescription"));
    QVERIFY(!trackerReasons.contains(QStringLiteral("equalizer")));

    engine.loadFile(regularPath);
    const QVariantMap regularReasons = engine.playbackCapabilityReasons();
    QVERIFY(!regularReasons.contains(QStringLiteral("reverse")));
    QVERIFY(!regularReasons.contains(QStringLiteral("rate")));
    QVERIFY(!regularReasons.contains(QStringLiteral("pitch")));
    QVERIFY(!regularReasons.contains(QStringLiteral("audioQualityProfile")));

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_trackerLoadRetainsPitchWhenSupported()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString trackerPath = dir.filePath(QStringLiteral("pitch-policy.mod"));
    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly));
    trackerFile.close();

    AudioEngine engine;
    engine.setPlaybackRate(1.5);
    engine.setPitchSemitones(3);

    engine.loadFile(trackerPath);

    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(engine.rateAvailable(), false);
    QCOMPARE(engine.timeStretchAvailable(), false);
    QCOMPARE(engine.pitchAvailable(), false);
    QCOMPARE(engine.pitchShiftAvailable(), false);
    QCOMPARE(engine.playbackRate(), 1.0);
    QCOMPARE(engine.pitchSemitones(), 0);

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_usesOpenMptBackendForTrackerPlayback()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir);
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    QSignalSpy stateSpy(&engine, &AudioEngine::stateChanged);
    QSignalSpy durationSpy(&engine, &AudioEngine::durationChanged);

    engine.loadFile(modulePath);

    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QCOMPARE(engine.currentFile(), modulePath);
    QCOMPARE(engine.title(), QStringLiteral("WaveFlux Engine Demo"));
    QVERIFY(engine.trackerMetadataAvailable());
    QVERIFY(!engine.trackerType().isEmpty());
    QVERIFY(engine.trackerMessage().isEmpty());
    QCOMPARE(engine.trackerChannelCount(), 4);
    QCOMPARE(engine.trackerPatternCount(), 1);
    QCOMPARE(engine.trackerInstrumentCount(), 0);
    const QVariantMap trackerDiagnostics = engine.trackerDiagnosticsSnapshot();
    QCOMPARE(trackerDiagnostics.value(QStringLiteral("component")).toString(),
             QStringLiteral("OpenMptPlaybackBackend"));
    QVERIFY(trackerDiagnostics.contains(QStringLiteral("audio")));
    QVERIFY(trackerDiagnostics.contains(QStringLiteral("graph")));
    const QVariantMap audioEngineSnapshot =
        trackerDiagnostics.value(QStringLiteral("audioEngine")).toMap();
    QCOMPARE(audioEngineSnapshot.value(QStringLiteral("currentFile")).toString(), modulePath);
    QVERIFY(engine.duration() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(engine.position() > 0, 3000);

    engine.seek(1000);
    QTRY_VERIFY_WITH_TIMEOUT(engine.position() >= 900, 3000);

    engine.pause();
    QCOMPARE(engine.state(), AudioEngine::PausedState);

    engine.play();
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);

    engine.stop();
    QCOMPARE(engine.state(), AudioEngine::StoppedState);
    QCOMPARE(engine.position(), 0);
    QVERIFY(durationSpy.count() >= 1);
    QVERIFY(stateSpy.count() >= 4);
}

void PlaybackBackendRoutingTest::audioEngine_trackerRatePitchChangeWhilePlaying()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("rate-pitch-live.mod"));
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);
    engine.loadFile(modulePath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);

    engine.setPlaybackRate(1.25);
    QCOMPARE(engine.playbackRate(), 1.0);
    QCOMPARE(engine.rateAvailable(), false);
    QCOMPARE(engine.state(), AudioEngine::PlayingState);

    engine.setPitchSemitones(2);
    QCOMPARE(engine.pitchSemitones(), 0);
    QCOMPARE(engine.pitchAvailable(), false);
    QCOMPARE(engine.state(), AudioEngine::PlayingState);
    QTRY_VERIFY_WITH_TIMEOUT(engine.position() > 0, 3000);

    const QVariantMap diagnostics = engine.trackerDiagnosticsSnapshot();
    const QVariantMap dsp = diagnostics.value(QStringLiteral("dsp")).toMap();
    QCOMPARE(dsp.value(QStringLiteral("rateActivationCount")).toULongLong(), 0ULL);
    QCOMPARE(dsp.value(QStringLiteral("pitchActivationCount")).toULongLong(), 0ULL);
    QCOMPARE(errorSpy.count(), 0);
}

void PlaybackBackendRoutingTest::audioEngine_trackerRepeatedSeekAndNearEofStayResponsive()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("scrub-demo.mod"));
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);
    engine.loadFile(modulePath);

    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);
    QVERIFY(engine.duration() > 2500);

    const qint64 durationMs = engine.duration();
    const auto waveformTargetMs = [durationMs](double normalized) {
        const double clamped = std::clamp(normalized, 0.0, 1.0);
        return qRound64(clamped * static_cast<double>(durationMs));
    };

    const QList<qint64> anchorTargets = {
        waveformTargetMs(0.0),
        waveformTargetMs(0.5),
        qMax<qint64>(0, durationMs - 240)
    };

    for (qint64 targetMs : anchorTargets) {
        engine.seekWithSource(targetMs, QStringLiteral("test.waveform_anchor_seek"));
        const qint64 expectedMs = qMin(targetMs, qMax<qint64>(0, durationMs - 120));
        QTRY_VERIFY_WITH_TIMEOUT(engine.position() >= qMax<qint64>(0, expectedMs - 180), 3000);
        QVERIFY(engine.position() <= durationMs);
        QCOMPARE(engine.state(), AudioEngine::PlayingState);
    }

    for (int i = 0; i < 24; ++i) {
        const double normalized = static_cast<double>((i * 7) % 24) / 23.0;
        const qint64 targetMs = waveformTargetMs(normalized);
        engine.seekWithSource(targetMs, QStringLiteral("test.waveform_drag_scrub"));
        const qint64 expectedMs = qMin(targetMs, qMax<qint64>(0, durationMs - 120));
        QTRY_VERIFY_WITH_TIMEOUT(engine.position() >= qMax<qint64>(0, expectedMs - 220), 3000);
        QVERIFY(engine.position() <= durationMs);
        QCOMPARE(engine.state(), AudioEngine::PlayingState);
    }

    engine.seekWithSource(durationMs - 1, QStringLiteral("test.near_eof_seek"));
    QTRY_VERIFY_WITH_TIMEOUT(engine.position() >= qMax<qint64>(0, durationMs - 400), 3000);
    QVERIFY(engine.position() <= durationMs);
    QCOMPARE(engine.state(), AudioEngine::PlayingState);
    QCOMPARE(errorSpy.count(), 0);
}

void PlaybackBackendRoutingTest::audioEngine_trackerRapidScrubCoalescesSeekBursts()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("rapid-scrub.mod"));
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);
    engine.loadFile(modulePath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);

    const qint64 durationMs = engine.duration();
    QVERIFY(durationMs > 1000);

    for (int i = 0; i < 80; ++i) {
        const double normalized = static_cast<double>((i * 13) % 80) / 79.0;
        const qint64 targetMs = qRound64(normalized * static_cast<double>(durationMs));
        engine.seekWithSource(targetMs, QStringLiteral("test.rapid_scrub_drag"));
    }

    QTRY_VERIFY_WITH_TIMEOUT(engine.position() >= 0, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);

    const QVariantMap diagnostics = engine.trackerDiagnosticsSnapshot();
    const QVariantMap seek = diagnostics.value(QStringLiteral("seek")).toMap();
    QVERIFY(seek.value(QStringLiteral("seekCount")).toULongLong() < 20ULL);
    QCOMPARE(errorSpy.count(), 0);
}

void PlaybackBackendRoutingTest::audioEngine_trackerSpectrumStartsStopsWithPlayback()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("spectrum-demo.mod"));
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    QSignalSpy spectrumSpy(&engine, &AudioEngine::spectrumLevelsChanged);
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadFile(modulePath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QVERIFY(engine.spectrumAvailable());
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState, 3000);

    engine.setSpectrumEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(levelsHaveEnergy(engine.spectrumLevels()), 3000);

    engine.seekWithSource(qMin<qint64>(1000, qMax<qint64>(0, engine.duration() - 240)),
                          QStringLiteral("test.spectrum_seek_reset"));
    QVERIFY(levelsAreZero(engine.spectrumLevels()));
    QTRY_VERIFY_WITH_TIMEOUT(levelsHaveEnergy(engine.spectrumLevels()), 3000);

    engine.pause();
    QTRY_VERIFY_WITH_TIMEOUT(levelsAreZero(engine.spectrumLevels()), 3000);
    const int pausedSignalCount = spectrumSpy.count();
    QTest::qWait(180);
    QCOMPARE(spectrumSpy.count(), pausedSignalCount);

    engine.play();
    QTRY_VERIFY_WITH_TIMEOUT(levelsHaveEnergy(engine.spectrumLevels()), 3000);

    engine.stop();
    QTRY_VERIFY_WITH_TIMEOUT(levelsAreZero(engine.spectrumLevels()), 1000);
    const int stoppedSignalCount = spectrumSpy.count();
    QTest::qWait(180);
    QCOMPARE(spectrumSpy.count(), stoppedSignalCount);
    QCOMPARE(errorSpy.count(), 0);
}

void PlaybackBackendRoutingTest::audioEngine_trackerReverseUnavailableDoesNotSpamErrors()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString trackerPath = dir.filePath(QStringLiteral("reverse-policy.mod"));
    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly));
    trackerFile.close();

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadFile(trackerPath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QVERIFY(!engine.reverseAvailable());
    QCOMPARE(engine.reversePlayback(), false);

    engine.setReversePlayback(true);
    QCOMPARE(engine.reversePlayback(), false);
    engine.setPlaybackRate(1.25);
    QCOMPARE(engine.playbackRate(), 1.0);
    engine.setPitchSemitones(2);
    QCOMPARE(engine.pitchSemitones(), 0);
    QCOMPARE(errorSpy.count(), 0);

    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

void PlaybackBackendRoutingTest::audioEngine_trackerEqualizerAppliesPresetAndReset()
{
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        QSKIP("Default audio output device is not available in this environment.");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("eq-policy.mod"));
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    QSignalSpy gainSpy(&engine, &AudioEngine::equalizerBandGainsChanged);
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.loadFile(modulePath);
    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QTRY_VERIFY_WITH_TIMEOUT(engine.state() == AudioEngine::PlayingState
                                 || engine.state() == AudioEngine::ReadyState,
                             3000);
    QVERIFY(engine.equalizerAvailable());

    const QVariantList defaultGains = engine.equalizerBandGains();
    QVariantList preset = defaultGains;
    const QList<double> presetValues = {6.0, 3.0, 0.0, -3.0, -6.0, -3.0, 0.0, 3.0, 6.0, 0.0};
    for (int i = 0; i < preset.size() && i < presetValues.size(); ++i) {
        preset[i] = presetValues.at(i);
    }

    engine.setEqualizerBandGains(preset);
    QCOMPARE(engine.equalizerBandGains(), preset);

    engine.resetEqualizerBands();
    QCOMPARE(engine.equalizerBandGains(), defaultGains);
    QVERIFY(gainSpy.count() >= 2);
    QCOMPARE(errorSpy.count(), 0);
}

void PlaybackBackendRoutingTest::audioEngine_trackerDisablesGStreamerOnlyFeatures()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString modulePath = writeModuleFixture(&dir, QStringLiteral("feature-policy.mod"));
    QVERIFY(!modulePath.isEmpty());

    AudioEngine engine;
    engine.setPlaybackRate(1.5);
    engine.setReversePlayback(true);
    engine.setPitchSemitones(3);
    engine.setAudioQualityProfile(QStringLiteral("hifi"));

    engine.loadFile(modulePath);

    QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
    QTRY_VERIFY_WITH_TIMEOUT(engine.currentBackendKind() == WaveFlux::PlaybackBackendKind::OpenMpt,
                             3000);
    QCOMPARE(engine.reversePlayback(), false);
    QCOMPARE(engine.audioQualityProfile(), QStringLiteral("standard"));
    QCOMPARE(engine.equalizerAvailable(), true);
    QCOMPARE(engine.pitchAvailable(), false);
    QCOMPARE(engine.rateWithPitchChangeAvailable(), false);
    QCOMPARE(engine.pitchShiftAvailable(), false);
    QCOMPARE(engine.reverseAvailable(), false);
    QCOMPARE(engine.spectrumAvailable(), true);

    QCOMPARE(engine.playbackRate(), 1.0);
    QCOMPARE(engine.pitchSemitones(), 0);
    QCOMPARE(engine.rateAvailable(), false);
    QCOMPARE(engine.timeStretchAvailable(), false);

    const QVariantList defaultGains = engine.equalizerBandGains();
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    engine.setReversePlayback(true);
    QCOMPARE(engine.reversePlayback(), false);

    engine.setPlaybackRate(1.25);
    QCOMPARE(engine.playbackRate(), 1.0);

    engine.setPitchSemitones(2);
    QCOMPARE(engine.pitchSemitones(), 0);

    engine.setAudioQualityProfile(QStringLiteral("hifi"));
    QCOMPARE(engine.audioQualityProfile(), QStringLiteral("standard"));

    engine.setSpectrumEnabled(true);
    engine.setSpectrumEnabled(false);

    engine.setEqualizerBandGain(0, 6.0);
    QVERIFY(engine.equalizerBandGains().at(0).toDouble() > 5.9);
    engine.resetEqualizerBands();
    QCOMPARE(engine.equalizerBandGains(), defaultGains);

    QVariantList boostedGains = defaultGains;
    boostedGains[0] = 6.0;
    engine.setEqualizerBandGains(boostedGains);
    QCOMPARE(engine.equalizerBandGains(), boostedGains);

    QCOMPARE(errorSpy.count(), 0);
}

void PlaybackBackendRoutingTest::audioEngine_repeatedTrackerOrdinaryBackendSwitchClearsTrackerState()
{
    qputenv("WAVEFLUX_SKIP_PIPELINE_LOAD", "1");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString trackerPath = dir.filePath(QStringLiteral("switch-loop.mod"));
    QFile trackerFile(trackerPath);
    QVERIFY(trackerFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    trackerFile.write(createMinimalModFile());
    trackerFile.close();

    const QString regularPath = dir.filePath(QStringLiteral("switch-loop.flac"));
    QFile regularFile(regularPath);
    QVERIFY(regularFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    regularFile.close();

    AudioEngine engine;
    QSignalSpy errorSpy(&engine, &AudioEngine::error);

    for (int i = 0; i < 80; ++i) {
        engine.loadFile(trackerPath);
        QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::OpenMpt);
        QCOMPARE(engine.currentFile(), trackerPath);
        QCOMPARE(engine.trackerMetadataAvailable(), false);
        QVERIFY(engine.trackerDiagnosticsSnapshot().contains(QStringLiteral("component")));

        engine.loadFile(regularPath);
        QCOMPARE(engine.currentBackendKind(), WaveFlux::PlaybackBackendKind::GStreamer);
        QCOMPARE(engine.currentFile(), regularPath);
        QCOMPARE(engine.trackerMetadataAvailable(), false);
        QVERIFY(engine.trackerType().isEmpty());
        QVERIFY(engine.trackerDiagnosticsSnapshot().isEmpty());
    }

    QCOMPARE(errorSpy.count(), 0);
    qunsetenv("WAVEFLUX_SKIP_PIPELINE_LOAD");
}

QTEST_MAIN(PlaybackBackendRoutingTest)

#include "tst_PlaybackBackendRouting.moc"
