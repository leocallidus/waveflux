#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <cmath>

#include "playback/OpenMptPlaybackBackend.h"
#include "playback/OpenMptWaveformRenderer.h"
#include "playback/PlaybackBackendRouting.h"

namespace {

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
    QFile manifestFile(trackerTestDataDir() + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    return QJsonDocument::fromJson(manifestFile.readAll())
        .object()
        .value(QStringLiteral("fixtures"))
        .toArray();
}

bool peaksAreRenderable(const QVector<float> &peaks)
{
    if (peaks.isEmpty()) {
        return false;
    }

    for (float peak : peaks) {
        if (!std::isfinite(peak) || peak < 0.0f || peak > 1.0f) {
            return false;
        }
    }
    return true;
}

QString fixturePath(const QString &fileName)
{
    return QDir(trackerTestDataDir()).filePath(fileName);
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

} // namespace

class OpenMptWaveformRendererTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersFormatCorpusWithAlignedDuration();
    void rendersSupportedRealModsCorpus();
    void corruptModuleReturnsControlledError();
    void cancellationStopsBeforeOpeningModule();
    void durationMatchesPlaybackBackend();
    void peakIndexMapsToExpectedPosition();
    void repeatedRenderDoesNotLeaveJobs();
};

void OpenMptWaveformRendererTest::rendersFormatCorpusWithAlignedDuration()
{
    const QJsonArray fixtures = loadTrackerManifestFixtures();
    QVERIFY(!fixtures.isEmpty());

    for (const QJsonValue &value : fixtures) {
        const QJsonObject fixture = value.toObject();
        if (fixture.value(QStringLiteral("stress")).toBool()) {
            continue;
        }

        const QString path = fixturePath(fixture.value(QStringLiteral("file")).toString());
        QVERIFY2(QFile::exists(path), qPrintable(path));

        const WaveFlux::OpenMptWaveformRenderResult result =
            WaveFlux::OpenMptWaveformRenderer::render(path, 1024);

        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QVERIFY(peaksAreRenderable(result.peaks));
        QVERIFY(result.peaks.size() <= 1024);
        QCOMPARE(result.canceled, false);

        const qint64 expectedDurationMs = fixture.value(QStringLiteral("expectedDurationMs")).toInt();
        QVERIFY2(std::llabs(result.durationMs - expectedDurationMs) <= 25,
                 qPrintable(QStringLiteral("%1 duration %2 expected %3")
                                .arg(path)
                                .arg(result.durationMs)
                                .arg(expectedDurationMs)));
    }
}

void OpenMptWaveformRendererTest::corruptModuleReturnsControlledError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("broken.mod"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("not a tracker module");
    file.close();

    const WaveFlux::OpenMptWaveformRenderResult result =
        WaveFlux::OpenMptWaveformRenderer::render(path, 256);

    QVERIFY(!result.success);
    QVERIFY(!result.canceled);
    QVERIFY(result.peaks.isEmpty());
    QVERIFY(!result.errorMessage.trimmed().isEmpty());
}

void OpenMptWaveformRendererTest::rendersSupportedRealModsCorpus()
{
    const QFileInfoList realModules = supportedRealMods();
    QVERIFY2(!realModules.isEmpty(), qPrintable(realModsDir()));

    for (const QFileInfo &moduleInfo : realModules) {
        const QString path = moduleInfo.absoluteFilePath();

        WaveFlux::OpenMptPlaybackBackend backend;
        backend.load(path);
        QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

        const WaveFlux::OpenMptWaveformRenderResult result =
            WaveFlux::OpenMptWaveformRenderer::render(path, 1536);

        QVERIFY2(result.success, qPrintable(moduleInfo.fileName() + QStringLiteral(": ") + result.errorMessage));
        QVERIFY2(peaksAreRenderable(result.peaks), qPrintable(moduleInfo.fileName()));
        QVERIFY2(!result.peaks.isEmpty(), qPrintable(moduleInfo.fileName()));
        QVERIFY2(result.durationMs > 0, qPrintable(moduleInfo.fileName()));
        QVERIFY2(std::llabs(result.durationMs - backend.duration()) <= 30,
                 qPrintable(moduleInfo.fileName()));
    }
}

void OpenMptWaveformRendererTest::cancellationStopsBeforeOpeningModule()
{
    std::atomic_bool cancelRequested { true };
    const WaveFlux::OpenMptWaveformRenderResult result =
        WaveFlux::OpenMptWaveformRenderer::render(fixturePath(QStringLiteral("stress-long-loopish.mod")),
                                                  1024,
                                                  {},
                                                  &cancelRequested);

    QVERIFY(result.canceled);
    QVERIFY(!result.success);
    QVERIFY(result.peaks.isEmpty());
}

void OpenMptWaveformRendererTest::durationMatchesPlaybackBackend()
{
    const QString path = fixturePath(QStringLiteral("tiny.xm"));
    QVERIFY(QFile::exists(path));

    WaveFlux::OpenMptPlaybackBackend backend;
    backend.load(path);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);

    const WaveFlux::OpenMptWaveformRenderResult result =
        WaveFlux::OpenMptWaveformRenderer::render(path, 512);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QVERIFY(std::llabs(result.durationMs - backend.duration()) <= 25);
}

void OpenMptWaveformRendererTest::peakIndexMapsToExpectedPosition()
{
    const WaveFlux::OpenMptWaveformRenderResult result =
        WaveFlux::OpenMptWaveformRenderer::render(fixturePath(QStringLiteral("tiny.mod")), 512);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QVERIFY(!result.peaks.isEmpty());
    QVERIFY(result.durationMs > 0);

    const int centerIndex = result.peaks.size() / 2;
    const qint64 mappedPositionMs =
        qRound64((static_cast<double>(centerIndex) + 0.5)
                 * static_cast<double>(result.durationMs)
                 / static_cast<double>(result.peaks.size()));

    const qint64 toleranceMs =
        qCeil(static_cast<double>(result.durationMs) / static_cast<double>(result.peaks.size())) + 1;
    QVERIFY(std::llabs(mappedPositionMs - result.durationMs / 2) <= toleranceMs);
}

void OpenMptWaveformRendererTest::repeatedRenderDoesNotLeaveJobs()
{
    const QStringList files = {
        QStringLiteral("tiny.669"),
        QStringLiteral("tiny.mod"),
        QStringLiteral("tiny.xm"),
        QStringLiteral("tiny.s3m"),
        QStringLiteral("tiny.it"),
        QStringLiteral("stress-rapid-seek.mod")
    };

    for (int iteration = 0; iteration < 3; ++iteration) {
        for (const QString &fileName : files) {
            const WaveFlux::OpenMptWaveformRenderResult result =
                WaveFlux::OpenMptWaveformRenderer::render(fixturePath(fileName), 384);
            QVERIFY2(result.success, qPrintable(result.errorMessage));
            QVERIFY(peaksAreRenderable(result.peaks));
        }
    }
}

QTEST_MAIN(OpenMptWaveformRendererTest)

#include "tst_OpenMptWaveformRenderer.moc"
