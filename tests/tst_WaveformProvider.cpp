#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "PeaksCacheManager.h"
#include "WaveformItem.h"
#include "WaveformProvider.h"

namespace {

QString trackerTestDataDir()
{
#ifdef WAVEFLUX_TESTDATA_DIR
    return QString::fromUtf8(WAVEFLUX_TESTDATA_DIR) + QStringLiteral("/tracker");
#else
    return QFINDTESTDATA("testdata/tracker");
#endif
}

QString fixturePath(const QString &fileName)
{
    return trackerTestDataDir() + QLatin1Char('/') + fileName;
}

} // namespace

class WaveformProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void trackerRenderStoresCacheAndCacheHitIsSynchronous();
    void cancelStopsInFlightTrackerRender();
    void placeholderStatesDifferentiateUnsupportedFailedAndSilent();
    void loadingPlaceholderAnimationAdvancesSmoothly();
};

void WaveformProviderTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void WaveformProviderTest::trackerRenderStoresCacheAndCacheHitIsSynchronous()
{
    const QString modulePath = fixturePath(QStringLiteral("tiny.mod"));
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    PeaksCacheManager cache;
    cache.clear();

    WaveformProvider firstProvider;
    QSignalSpy firstPeaksSpy(&firstProvider, &WaveformProvider::peaksReady);

    firstProvider.loadFile(modulePath);

    QTRY_VERIFY(!firstProvider.isLoading());
    QVERIFY(firstProvider.sampleCount() > 0);
    QCOMPARE(firstProvider.placeholderState(), QString());
    QVERIFY(firstPeaksSpy.count() >= 2);

    const auto cached = cache.lookup(modulePath);
    QVERIFY(cached.has_value());
    QVERIFY(!cached->isEmpty());

    WaveformProvider secondProvider;
    QSignalSpy loadingSpy(&secondProvider, &WaveformProvider::loadingChanged);

    secondProvider.loadFile(modulePath);

    QCOMPARE(secondProvider.isLoading(), false);
    QVERIFY(secondProvider.sampleCount() > 0);
    QCOMPARE(secondProvider.placeholderState(), QString());
    QCOMPARE(loadingSpy.count(), 0);
}

void WaveformProviderTest::cancelStopsInFlightTrackerRender()
{
    const QString modulePath = fixturePath(QStringLiteral("stress-long-loopish.mod"));
    QVERIFY2(QFileInfo::exists(modulePath), qPrintable(modulePath));

    PeaksCacheManager cache;
    cache.clear();

    WaveformProvider provider;
    provider.loadFile(modulePath);

    QTRY_VERIFY(provider.isLoading());
    provider.cancel();

    QCOMPARE(provider.isLoading(), false);
}

void WaveformProviderTest::placeholderStatesDifferentiateUnsupportedFailedAndSilent()
{
    WaveformProvider provider;
    QSignalSpy errorSpy(&provider, &WaveformProvider::error);

    provider.loadFile(QStringLiteral("https://example.com/demo.mod"));
    QCOMPARE(provider.isLoading(), false);
    QCOMPARE(provider.sampleCount(), 0);
    QCOMPARE(provider.placeholderState(), QStringLiteral("unsupported"));
    QCOMPARE(errorSpy.count(), 0);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString brokenPath = dir.filePath(QStringLiteral("broken.mod"));
    QFile brokenFile(brokenPath);
    QVERIFY(brokenFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    brokenFile.write("not a tracker module");
    brokenFile.close();

    provider.loadFile(brokenPath);
    QTRY_COMPARE(provider.isLoading(), false);
    QCOMPARE(provider.sampleCount(), 0);
    QCOMPARE(provider.placeholderState(), QStringLiteral("failed"));
    QCOMPARE(errorSpy.count(), 1);

    provider.loadFile(fixturePath(QStringLiteral("stress-short-silent.mod")));
    QTRY_COMPARE(provider.isLoading(), false);
    QCOMPARE(provider.sampleCount(), 0);
    QCOMPARE(provider.placeholderState(), QStringLiteral("empty"));
    QCOMPARE(errorSpy.count(), 1);
}

void WaveformProviderTest::loadingPlaceholderAnimationAdvancesSmoothly()
{
    WaveformItem item;
    item.setWidth(480);
    item.setHeight(120);
    item.setBackgroundColor(QColor(QStringLiteral("#101820")));
    item.setWaveformColor(QColor(QStringLiteral("#7aa7c7")));
    item.setProgressColor(QColor(QStringLiteral("#3daee9")));

    QCOMPARE(item.loadingAnimationRunning(), false);
    item.setLoading(true);
    QCOMPARE(item.loadingAnimationRunning(), true);
    QCOMPARE(item.displayedGenerationProgress(), 0.0);

    item.setGenerationProgress(0.75);
    QTRY_VERIFY_WITH_TIMEOUT(item.displayedGenerationProgress() > 0.0, 250);
    QVERIFY(item.displayedGenerationProgress() < item.generationProgress());

    QImage firstFrame(480, 120, QImage::Format_ARGB32_Premultiplied);
    firstFrame.fill(Qt::transparent);
    {
        QPainter painter(&firstFrame);
        item.paint(&painter);
    }

    QTest::qWait(80);
    QVERIFY(item.displayedGenerationProgress() <= item.generationProgress());

    QImage secondFrame(480, 120, QImage::Format_ARGB32_Premultiplied);
    secondFrame.fill(Qt::transparent);
    {
        QPainter painter(&secondFrame);
        item.paint(&painter);
    }
    QVERIFY(firstFrame != secondFrame);

    // Replacing a request with a fresh zero-progress job must not leave the
    // preceding track's progress visible while it eases backwards.
    item.setGenerationProgress(0.0);
    QCOMPARE(item.displayedGenerationProgress(), 0.0);

    item.setGenerationProgress(0.4);
    QTRY_VERIFY_WITH_TIMEOUT(item.displayedGenerationProgress() > 0.0, 250);
    item.setLoading(false);
    QCOMPARE(item.loadingAnimationRunning(), false);
    QCOMPARE(item.displayedGenerationProgress(), item.generationProgress());
}

QTEST_MAIN(WaveformProviderTest)

#include "tst_WaveformProvider.moc"
