#include <QtTest>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QSettings>

#include "ThemeManager.h"
#include "UiMetrics.h"

class ThemeManagerUiMetricsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void baselineSystemFontCaptured();
    void customFontSizeBoundaries();
    void invalidCustomFontSizeHandling();
    void missingFontFamilyFallback();
    void monotonicMetricsScaleWithFontSize();
    void playlistFontMetricsCalculation();
    void resetToDefaultRestoresBaseline();
    void persistenceAndReloading();
    void signalsEmittedOnMetricsChange();
    void uiMetricsTokensAndBreakpointsScale();
};

void ThemeManagerUiMetricsTest::initTestCase()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void ThemeManagerUiMetricsTest::cleanup()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void ThemeManagerUiMetricsTest::baselineSystemFontCaptured()
{
    ThemeManager themeManager;
    QVERIFY(themeManager.baseFontPointSize() > 0.0);
    QVERIFY(themeManager.baseFontLineSpacing() > 0.0);
    QVERIFY(themeManager.effectiveFontPointSize() > 0.0);
    QVERIFY(themeManager.effectiveFontLineSpacing() > 0.0);
    QCOMPARE(themeManager.fontMetricsScale(), 1.0);
    QCOMPARE(themeManager.customFontSize(), 0);
    QCOMPARE(themeManager.customFontFamily(), QStringLiteral("Default"));
}

void ThemeManagerUiMetricsTest::customFontSizeBoundaries()
{
    ThemeManager themeManager;

    const QVector<int> testSizes = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24};
    for (int size : testSizes) {
        themeManager.setCustomFontSize(size);
        QCOMPARE(themeManager.customFontSize(), size);
        QCOMPARE(themeManager.effectiveFontPointSize(), static_cast<double>(size));
        QVERIFY(themeManager.effectiveFontLineSpacing() > 0.0);
        QVERIFY(themeManager.fontMetricsScale() >= 0.5);
        QVERIFY(themeManager.fontMetricsScale() <= 3.0);
    }
}

void ThemeManagerUiMetricsTest::invalidCustomFontSizeHandling()
{
    ThemeManager themeManager;

    // Below minimum supported size (< 8 and != 0) should clamp to 8
    themeManager.setCustomFontSize(4);
    QCOMPARE(themeManager.customFontSize(), 8);
    QCOMPARE(themeManager.effectiveFontPointSize(), 8.0);

    // Negative size should clamp to 8
    themeManager.setCustomFontSize(-5);
    QCOMPARE(themeManager.customFontSize(), 8);

    // Above maximum size (> 24) should clamp to 24
    themeManager.setCustomFontSize(36);
    QCOMPARE(themeManager.customFontSize(), 24);
    QCOMPARE(themeManager.effectiveFontPointSize(), 24.0);

    // 0 is valid for System Default
    themeManager.setCustomFontSize(0);
    QCOMPARE(themeManager.customFontSize(), 0);
    QCOMPARE(themeManager.effectiveFontPointSize(), themeManager.baseFontPointSize());
}

void ThemeManagerUiMetricsTest::missingFontFamilyFallback()
{
    ThemeManager themeManager;
    const QString nonExistentFamily = QStringLiteral("ThisFontDoesNotExistAnywhere_12345");

    themeManager.setCustomFontFamily(nonExistentFamily);
    QCOMPARE(themeManager.customFontFamily(), nonExistentFamily);
    // Effective family should fall back to baseline system font family
    QVERIFY(!themeManager.fontFamily().isEmpty());
    QVERIFY(themeManager.fontFamily() != nonExistentFamily);
    QVERIFY(themeManager.effectiveFontLineSpacing() > 0.0);
    QVERIFY(themeManager.fontMetricsScale() > 0.0);
}

void ThemeManagerUiMetricsTest::monotonicMetricsScaleWithFontSize()
{
    ThemeManager themeManager;

    themeManager.setCustomFontSize(8);
    double scale8 = themeManager.fontMetricsScale();

    themeManager.setCustomFontSize(12);
    double scale12 = themeManager.fontMetricsScale();

    themeManager.setCustomFontSize(16);
    double scale16 = themeManager.fontMetricsScale();

    themeManager.setCustomFontSize(24);
    double scale24 = themeManager.fontMetricsScale();

    QVERIFY(scale8 <= scale12);
    QVERIFY(scale12 <= scale16);
    QVERIFY(scale16 <= scale24);
    QVERIFY(scale8 < scale24);
}

void ThemeManagerUiMetricsTest::playlistFontMetricsCalculation()
{
    ThemeManager themeManager;

    QVERIFY(themeManager.playlistFontMetricsScale() > 0.0);
    QVERIFY(themeManager.playlistFontLineSpacing() > 0.0);
    QCOMPARE(themeManager.playlistFontFamily(), themeManager.fontFamily());

    const QStringList families = QFontDatabase::families();
    if (families.size() > 1) {
        QString otherFamily = families.at(0);
        if (otherFamily == themeManager.fontFamily() && families.size() > 1) {
            otherFamily = families.at(1);
        }
        themeManager.setPlaylistFontFamily(otherFamily);
        QCOMPARE(themeManager.playlistFontFamily(), otherFamily);
        QCOMPARE(themeManager.customPlaylistFontFamily(), otherFamily);
        QVERIFY(themeManager.playlistFontLineSpacing() > 0.0);
        QVERIFY(themeManager.playlistFontMetricsScale() > 0.0);
    }
}

void ThemeManagerUiMetricsTest::resetToDefaultRestoresBaseline()
{
    ThemeManager themeManager;

    themeManager.setCustomFontSize(20);
    themeManager.setCustomFontFamily(QStringLiteral("NonExistent_XYZ"));
    themeManager.setPlaylistFontFamily(QStringLiteral("NonExistent_PL"));

    QCOMPARE(themeManager.customFontSize(), 20);
    QVERIFY(themeManager.fontMetricsScale() > 1.0);

    themeManager.resetToDefault();

    QCOMPARE(themeManager.customFontSize(), 0);
    QCOMPARE(themeManager.customFontFamily(), QStringLiteral("Default"));
    QCOMPARE(themeManager.customPlaylistFontFamily(), QStringLiteral("Default"));
    QCOMPARE(themeManager.playlistFontFamily(), themeManager.fontFamily());
    QCOMPARE(themeManager.effectiveFontPointSize(), themeManager.baseFontPointSize());
    QCOMPARE(themeManager.fontMetricsScale(), 1.0);
}

void ThemeManagerUiMetricsTest::persistenceAndReloading()
{
    {
        ThemeManager manager1;
        manager1.setCustomFontSize(16);
        const QStringList families = QFontDatabase::families();
        if (!families.isEmpty()) {
            manager1.setCustomFontFamily(families.first());
        }
    }

    {
        ThemeManager manager2;
        QCOMPARE(manager2.customFontSize(), 16);
        QCOMPARE(manager2.effectiveFontPointSize(), 16.0);
        const QStringList families = QFontDatabase::families();
        if (!families.isEmpty()) {
            QCOMPARE(manager2.customFontFamily(), families.first());
        }
    }
}

void ThemeManagerUiMetricsTest::signalsEmittedOnMetricsChange()
{
    ThemeManager themeManager;

    QSignalSpy sizeSpy(&themeManager, &ThemeManager::customFontSizeChanged);
    QSignalSpy scaleSpy(&themeManager, &ThemeManager::fontMetricsScaleChanged);
    QSignalSpy effectiveSizeSpy(&themeManager, &ThemeManager::effectiveFontPointSizeChanged);
    QSignalSpy lineSpacingSpy(&themeManager, &ThemeManager::effectiveFontLineSpacingChanged);

    themeManager.setCustomFontSize(18);

    QCOMPARE(sizeSpy.count(), 1);
    QCOMPARE(scaleSpy.count(), 1);
    QCOMPARE(effectiveSizeSpy.count(), 1);
    QCOMPARE(lineSpacingSpy.count(), 1);
}

void ThemeManagerUiMetricsTest::uiMetricsTokensAndBreakpointsScale()
{
    ThemeManager themeManager;
    UiMetrics uiMetrics(&themeManager);

    QSignalSpy metricsSpy(&uiMetrics, &UiMetrics::metricsChanged);

    // Initial baseline tokens
    QCOMPARE(uiMetrics.fontScale(), 1.0);
    QVERIFY(uiMetrics.bodyPointSize() > 0.0);
    QVERIFY(uiMetrics.captionPointSize() < uiMetrics.bodyPointSize());
    QVERIFY(uiMetrics.titlePointSize() > uiMetrics.bodyPointSize());
    QVERIFY(uiMetrics.spaceXS() > 0);
    QVERIFY(uiMetrics.spaceS() > uiMetrics.spaceXS());
    QVERIFY(uiMetrics.spaceM() > uiMetrics.spaceS());
    QVERIFY(uiMetrics.spaceL() > uiMetrics.spaceM());
    QVERIFY(uiMetrics.controlHeightNormal() >= 32);
    QVERIFY(uiMetrics.playlistRowHeight() >= 28);

    const int initialBreakpoint = uiMetrics.breakpoint(800);
    QCOMPARE(initialBreakpoint, 800);

    // Scale up font size
    themeManager.setCustomFontSize(20);
    QVERIFY(metricsSpy.count() > 0);
    QVERIFY(uiMetrics.fontScale() > 1.0);
    QVERIFY(uiMetrics.bodyPointSize() == 20.0);
    QVERIFY(uiMetrics.titlePointSize() > 20.0);
    QVERIFY(uiMetrics.controlHeightNormal() > 32);
    QVERIFY(uiMetrics.playlistRowHeight() > 28);
    QVERIFY(uiMetrics.breakpoint(800) > initialBreakpoint);
}

QTEST_MAIN(ThemeManagerUiMetricsTest)
#include "tst_ThemeManagerUiMetrics.moc"
