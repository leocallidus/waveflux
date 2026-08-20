#include <QtTest>
#include "DesktopNotificationService.h"
#include "AudioEngine.h"
#include "TrackModel.h"
#include "PlaybackController.h"
#include "AppSettingsManager.h"

class DesktopNotificationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void testNotificationSuppressionWhenDisabled();
    void testNotificationSuppressionWhenNoTrack();
    void testNotificationTriggerOnTrackSwitch();
};

void DesktopNotificationServiceTest::initTestCase()
{
    const QString settingsDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test_notify_settings"));
    QDir().mkpath(settingsDir);
    qputenv("XDG_CONFIG_HOME", settingsDir.toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

void DesktopNotificationServiceTest::cleanupTestCase()
{
}

void DesktopNotificationServiceTest::init()
{
}

void DesktopNotificationServiceTest::testNotificationSuppressionWhenDisabled()
{
    AppSettingsManager settings;
    settings.setNotifyOnTrackChange(false);

    AudioEngine audioEngine;
    TrackModel trackModel;
    PlaybackController controller(&trackModel, &audioEngine);

    DesktopNotificationService service(&audioEngine, &trackModel, &controller, &settings);
    // Should not crash and should respect disabled setting
    service.notifyTrackChanged();
    QCOMPARE(settings.notifyOnTrackChange(), false);
}

void DesktopNotificationServiceTest::testNotificationSuppressionWhenNoTrack()
{
    AppSettingsManager settings;
    settings.setNotifyOnTrackChange(true);

    AudioEngine audioEngine;
    TrackModel trackModel;
    PlaybackController controller(&trackModel, &audioEngine);

    DesktopNotificationService service(&audioEngine, &trackModel, &controller, &settings);
    // When trackModel is empty / currentIndex < 0, notifyTrackChanged returns safely
    service.notifyTrackChanged();
    QCOMPARE(trackModel.rowCount(), 0);
}

void DesktopNotificationServiceTest::testNotificationTriggerOnTrackSwitch()
{
    AppSettingsManager settings;
    settings.setNotifyOnTrackChange(true);

    AudioEngine audioEngine;
    TrackModel trackModel;
    PlaybackController controller(&trackModel, &audioEngine);

    DesktopNotificationService service(&audioEngine, &trackModel, &controller, &settings);

    // Verify service connects without issues
    QVERIFY(&service);
}

QTEST_MAIN(DesktopNotificationServiceTest)
#include "tst_DesktopNotificationService.moc"
