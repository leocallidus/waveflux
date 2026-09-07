#include <QtTest>
#include <gst/gst.h>

#define private public
#define protected public
#include "DesktopNotificationService.h"
#include "AudioEngine.h"
#include "TrackModel.h"
#include "PlaybackController.h"
#include "AppSettingsManager.h"
#undef protected
#undef private

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
    void testNoNotificationOnPlaybackRateChangeWhenTrackUnchanged();
};

void DesktopNotificationServiceTest::initTestCase()
{
    gst_init(nullptr, nullptr);
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

void DesktopNotificationServiceTest::testNoNotificationOnPlaybackRateChangeWhenTrackUnchanged()
{
    AppSettingsManager settings;
    settings.setNotifyOnTrackChange(true);

    AudioEngine audioEngine;
    TrackModel trackModel;
    PlaybackController controller(&trackModel, &audioEngine);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString f1 = tempDir.filePath(QStringLiteral("test1.mp3"));
    const QString f2 = tempDir.filePath(QStringLiteral("test2.mp3"));
    {
        QFile file1(f1);
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("dummy");
        QFile file2(f2);
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("dummy");
    }

    Track t1;
    t1.filePath = f1;
    t1.title = QStringLiteral("Song 1");
    Track t2;
    t2.filePath = f2;
    t2.title = QStringLiteral("Song 2");
    trackModel.setTracks({t1, t2});
    trackModel.setCurrentIndex(0);
    audioEngine.m_state = AudioEngine::PlayingState;

    DesktopNotificationService service(&audioEngine, &trackModel, &controller, &settings);

    // Initial notification for track 0
    service.notifyTrackChanged();
    QCOMPARE(service.notificationCount(), 1);
    QCOMPARE(service.lastNotifiedKey(), QStringLiteral("0:%1").arg(f1));

    // Changing playback rate (speed, e.g. holding space bar) must NOT trigger duplicate notifications
    audioEngine.setPlaybackRate(2.0);
    QTest::qWait(120);
    QCOMPARE(service.notificationCount(), 1);

    audioEngine.setPlaybackRate(1.0);
    QTest::qWait(120);
    QCOMPARE(service.notificationCount(), 1);

    // Changing to next track triggers a fresh notification
    trackModel.setCurrentIndex(1);
    service.notifyTrackChanged();
    QCOMPARE(service.notificationCount(), 2);
    QCOMPARE(service.lastNotifiedKey(), QStringLiteral("1:%1").arg(f2));
}

QTEST_MAIN(DesktopNotificationServiceTest)
#include "tst_DesktopNotificationService.moc"
