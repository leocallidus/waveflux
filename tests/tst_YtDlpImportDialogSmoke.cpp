#include <QtTest>

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QTemporaryDir>

#include "AppSettingsManager.h"
#include "ThemeManager.h"
#include "YtDlpImportService.h"
#include "AppSettingsManager.h"

namespace {
void clearSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void configureTestSettingsIsolation()
{
    const QString settingsDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("test_settings_ytdlp_import_dialog_smoke"));
    QDir().mkpath(settingsDir);
    qputenv("XDG_CONFIG_HOME", settingsDir.toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir);
}

QString testDataPath(const QString &relativePath)
{
    return QDir(QStringLiteral(WAVEFLUX_TESTDATA_DIR)).filePath(relativePath);
}

QString createFakeYtDlpScript(const QString &directoryPath,
                              const QString &payloadPath,
                              bool failOnFailUrl = true)
{
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(
        failOnFailUrl ? QStringLiteral("fake-yt-dlp-fail.bat")
                      : QStringLiteral("fake-yt-dlp-ok.bat"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("@echo off\r\n");
    script.write("if \"%1\"==\"--version\" (\r\n");
    script.write("  echo 2025.04.29\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write(QStringLiteral("set \"PAYLOAD_PATH=%1\"\r\n").arg(QDir::toNativeSeparators(payloadPath)).toUtf8());
    script.write("set \"MODE=import\"\r\n");
    script.write("set \"OUTPUT_TEMPLATE=\"\r\n");
    script.write("set \"EXT=mp3\"\r\n");
    script.write("set \"URL=\"\r\n");
    script.write(QStringLiteral("set \"FAIL_ON_FAIL_URL=%1\"\r\n").arg(failOnFailUrl ? 1 : 0).toUtf8());
    script.write(":parse\r\n");
    script.write("if \"%~1\"==\"\" goto parsed\r\n");
    script.write("if \"%~1\"==\"--dump-single-json\" set \"MODE=probe\"\r\n");
    script.write("if \"%~1\"==\"--audio-format\" (\r\n");
    script.write("  shift\r\n");
    script.write("  set \"EXT=%~1\"\r\n");
    script.write(") else if \"%~1\"==\"--output\" (\r\n");
    script.write("  shift\r\n");
    script.write("  set \"OUTPUT_TEMPLATE=%~1\"\r\n");
    script.write(") else if /I not \"%~1\"==\"--\" if /I not \"%~1:~0,2%\"==\"--\" (\r\n");
    script.write("  set \"URL=%~1\"\r\n");
    script.write(")\r\n");
    script.write("shift\r\n");
    script.write("goto parse\r\n");
    script.write(":parsed\r\n");
    script.write("if \"%MODE%\"==\"probe\" (\r\n");
    script.write("  type \"%PAYLOAD_PATH%\"\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write("if not \"%OUTPUT_TEMPLATE%\"==\"\" set \"OUTPUT_FILE=%OUTPUT_TEMPLATE:%%(ext)s=%EXT%%\"\r\n");
    script.write("echo [download] 25%%\r\n");
    script.write("if \"%FAIL_ON_FAIL_URL%\"==\"1\" if \"%URL%\"==\"https://example.com/fail-me\" exit /b 3\r\n");
    script.write("if not \"%OUTPUT_FILE%\"==\"\" (\r\n");
    script.write("  > \"%OUTPUT_FILE%\" echo fake-audio\r\n");
    script.write(")\r\n");
    script.write("echo [download] 100%%\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(
        failOnFailUrl ? QStringLiteral("fake-yt-dlp-fail")
                      : QStringLiteral("fake-yt-dlp-ok"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("#!/bin/sh\n");
    script.write("if [ \"$1\" = \"--version\" ]; then\n");
    script.write("  echo \"2025.04.29\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("payload_path=");
    script.write(QByteArray("\"") + payloadPath.toUtf8() + QByteArray("\"\n"));
    script.write("mode=import\n");
    script.write("output_template=\n");
    script.write("ext=mp3\n");
    script.write("url=\n");
    script.write(QStringLiteral("fail_on_fail_url=%1\n").arg(failOnFailUrl ? 1 : 0).toUtf8());
    script.write("while [ \"$#\" -gt 0 ]; do\n");
    script.write("  case \"$1\" in\n");
    script.write("    --dump-single-json)\n");
    script.write("      mode=probe\n");
    script.write("      ;;\n");
    script.write("    --audio-format)\n");
    script.write("      shift\n");
    script.write("      ext=\"$1\"\n");
    script.write("      ;;\n");
    script.write("    --output)\n");
    script.write("      shift\n");
    script.write("      output_template=\"$1\"\n");
    script.write("      ;;\n");
    script.write("    --)\n");
    script.write("      ;;\n");
    script.write("    --*)\n");
    script.write("      ;;\n");
    script.write("    *)\n");
    script.write("      url=\"$1\"\n");
    script.write("      ;;\n");
    script.write("  esac\n");
    script.write("  shift\n");
    script.write("done\n");
    script.write("if [ \"$mode\" = \"probe\" ]; then\n");
    script.write("  cat \"$payload_path\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("output_file=$(printf '%s' \"$output_template\" | sed \"s/%(ext)s/$ext/g\")\n");
    script.write("echo \"[download] 25%\"\n");
    script.write("if printf '%s' \"$*\" | grep -q \"https://example.com/cancel-me\"; then\n");
    script.write("  sleep 2\n");
    script.write("fi\n");
    script.write("if [ \"$fail_on_fail_url\" = \"1\" ] && [ \"$url\" = \"https://example.com/fail-me\" ]; then\n");
    script.write("  exit 3\n");
    script.write("fi\n");
    script.write("if [ -n \"$output_file\" ]; then\n");
    script.write("  mkdir -p \"$(dirname \"$output_file\")\"\n");
    script.write("  printf 'fake-audio' > \"$output_file\"\n");
    script.write("fi\n");
    script.write("echo \"[download] 100%\"\n");
    script.close();
    QFile::setPermissions(scriptPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
    return scriptPath;
#endif
}

QString createProbeFailureScript(const QString &directoryPath)
{
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(
        QStringLiteral("fake-yt-dlp-probe-failure.bat"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("@echo off\r\n");
    script.write("if \"%1\"==\"--version\" (\r\n");
    script.write("  echo 2025.04.29\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write("echo ERROR: [youtube] broken: Sign in to confirm you're not a bot 1>&2\r\n");
    script.write("exit /b 1\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(
        QStringLiteral("fake-yt-dlp-probe-failure"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("#!/bin/sh\n");
    script.write("if [ \"$1\" = \"--version\" ]; then\n");
    script.write("  echo \"2025.04.29\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("printf '%s\\n' \"ERROR: [youtube] broken: Sign in to confirm you're not a bot\" >&2\n");
    script.write("exit 1\n");
    script.close();
    QFile::setPermissions(scriptPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
    return scriptPath;
#endif
}

QString createTaggedParallelImportScript(const QString &directoryPath, const QString &payloadPath)
{
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(
        QStringLiteral("fake-yt-dlp-tagged-parallel.bat"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("@echo off\r\n");
    script.write("if \"%1\"==\"--version\" (\r\n");
    script.write("  echo 2025.04.29\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write(QStringLiteral("set \"PAYLOAD_PATH=%1\"\r\n").arg(QDir::toNativeSeparators(payloadPath)).toUtf8());
    script.write("set \"MODE=import\"\r\n");
    script.write("set \"OUTPUT_TEMPLATE=\"\r\n");
    script.write("set \"EXT=mp3\"\r\n");
    script.write("set \"URL=\"\r\n");
    script.write(":parse\r\n");
    script.write("if \"%~1\"==\"\" goto parsed\r\n");
    script.write("if \"%~1\"==\"--dump-single-json\" set \"MODE=probe\"\r\n");
    script.write("if \"%~1\"==\"--audio-format\" (\r\n");
    script.write("  shift\r\n");
    script.write("  set \"EXT=%~1\"\r\n");
    script.write(") else if \"%~1\"==\"--output\" (\r\n");
    script.write("  shift\r\n");
    script.write("  set \"OUTPUT_TEMPLATE=%~1\"\r\n");
    script.write(") else if /I not \"%~1\"==\"--\" if /I not \"%~1:~0,2%\"==\"--\" (\r\n");
    script.write("  set \"URL=%~1\"\r\n");
    script.write(")\r\n");
    script.write("shift\r\n");
    script.write("goto parse\r\n");
    script.write(":parsed\r\n");
    script.write("if \"%MODE%\"==\"probe\" (\r\n");
    script.write("  type \"%PAYLOAD_PATH%\"\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write("if not \"%OUTPUT_TEMPLATE%\"==\"\" set \"OUTPUT_FILE=%OUTPUT_TEMPLATE:%%(ext)s=%EXT%%\"\r\n");
    script.write("set \"TAG=alpha\"\r\n");
    script.write("if \"%URL%\"==\"https://example.com/tag-beta\" set \"TAG=beta\"\r\n");
    script.write("echo [download] 25%% %TAG%\r\n");
    script.write("ping -n 3 127.0.0.1 >nul\r\n");
    script.write("if not \"%OUTPUT_FILE%\"==\"\" (\r\n");
    script.write("  > \"%OUTPUT_FILE%\" echo fake-audio\r\n");
    script.write(")\r\n");
    script.write("echo [download] 100%% %TAG%\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(
        QStringLiteral("fake-yt-dlp-tagged-parallel"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("#!/bin/sh\n");
    script.write("if [ \"$1\" = \"--version\" ]; then\n");
    script.write("  echo \"2025.04.29\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("payload_path=");
    script.write(QByteArray("\"") + payloadPath.toUtf8() + QByteArray("\"\n"));
    script.write("mode=import\n");
    script.write("output_template=\n");
    script.write("ext=mp3\n");
    script.write("url=\n");
    script.write("while [ \"$#\" -gt 0 ]; do\n");
    script.write("  case \"$1\" in\n");
    script.write("    --dump-single-json)\n");
    script.write("      mode=probe\n");
    script.write("      ;;\n");
    script.write("    --audio-format)\n");
    script.write("      shift\n");
    script.write("      ext=\"$1\"\n");
    script.write("      ;;\n");
    script.write("    --output)\n");
    script.write("      shift\n");
    script.write("      output_template=\"$1\"\n");
    script.write("      ;;\n");
    script.write("    --)\n");
    script.write("      ;;\n");
    script.write("    --*)\n");
    script.write("      ;;\n");
    script.write("    *)\n");
    script.write("      url=\"$1\"\n");
    script.write("      ;;\n");
    script.write("  esac\n");
    script.write("  shift\n");
    script.write("done\n");
    script.write("if [ \"$mode\" = \"probe\" ]; then\n");
    script.write("  cat \"$payload_path\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("output_file=$(printf '%s' \"$output_template\" | sed \"s/%(ext)s/$ext/g\")\n");
    script.write("tag=alpha\n");
    script.write("if [ \"$url\" = \"https://example.com/tag-beta\" ]; then\n");
    script.write("  tag=beta\n");
    script.write("fi\n");
    script.write("echo \"[download] 25% $tag\"\n");
    script.write("sleep 1\n");
    script.write("if [ -n \"$output_file\" ]; then\n");
    script.write("  mkdir -p \"$(dirname \"$output_file\")\"\n");
    script.write("  printf 'fake-audio' > \"$output_file\"\n");
    script.write("fi\n");
    script.write("echo \"[download] 100% $tag\"\n");
    script.close();
    QFile::setPermissions(scriptPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
    return scriptPath;
#endif
}

QString qmlDirPath()
{
    const QDir testDataDir(QStringLiteral(WAVEFLUX_TESTDATA_DIR));
    return QDir(testDataDir.absoluteFilePath(QStringLiteral(".."))).absoluteFilePath(
        QStringLiteral("../qml"));
}

QString componentErrorString(const QQmlComponent &component)
{
    QStringList messages;
    const QList<QQmlError> errors = component.errors();
    messages.reserve(errors.size());
    for (const QQmlError &error : errors) {
        messages.push_back(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}
} // namespace

class YtDlpImportDialogSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void dialogLoadsAndOpens();
    void dialogTracksServiceStateTransitions();
    void dialogShowsParallelRuntimeProgress();
    void dialogHandlesPartialFailureAndRetryFollowUp();
    void dialogHidesAndReopensActiveSession();
    void dialogRestoresDraftAfterReopen();
    void dialogExposesParallelDownloadsSettingAndRestoresIt();
    void dialogOpensErrorDialogForProbeFailure();
};

void YtDlpImportDialogSmokeTest::initTestCase()
{
    configureTestSettingsIsolation();
    clearSettings();
}

void YtDlpImportDialogSmokeTest::init()
{
    clearSettings();
}

void YtDlpImportDialogSmokeTest::cleanup()
{
    clearSettings();
}

void YtDlpImportDialogSmokeTest::dialogLoadsAndOpens()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;
    YtDlpImportService service;
    service.setAppSettingsManager(&appSettings);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &service);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QCOMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("idle"));
    QCOMPARE(dialogObject->property("modal").toBool(), false);
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
}

void YtDlpImportDialogSmokeTest::dialogTracksServiceStateTransitions()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(scriptPath);
    appSettings.setFfmpegExecutablePath(scriptPath);

    ThemeManager themeManager;
    YtDlpImportService service;
    service.setAppSettingsManager(&appSettings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &service);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QCOMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("idle"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://youtu.be/vid123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("ready"));

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("running"));

    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("succeeded"));
    QVERIFY(dialogObject->property("hasFinalSummary").toBool());

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "requestClear"));
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("idle"));
    QVERIFY(!dialogObject->property("hasFinalSummary").toBool());
}

void YtDlpImportDialogSmokeTest::dialogShowsParallelRuntimeProgress()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        QDir(tempDir.path()).filePath(QStringLiteral("parallel-playlist.json"));
    QFile payload(payloadPath);
    QVERIFY(payload.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    payload.write(QByteArrayLiteral(
        "{"
        "\"_type\":\"playlist\","
        "\"id\":\"dialog-parallel\","
        "\"title\":\"Dialog Parallel Playlist\","
        "\"extractor_key\":\"YoutubeTab\","
        "\"webpage_url\":\"https://example.com/playlist?list=dialog-parallel\","
        "\"entries\":["
        "{\"id\":\"tag-alpha\",\"title\":\"Alpha Entry\","
        "\"webpage_url\":\"https://example.com/tag-alpha\","
        "\"playlist_index\":2,\"availability\":\"public\"},"
        "{\"id\":\"tag-beta\",\"title\":\"Beta Entry\","
        "\"webpage_url\":\"https://example.com/tag-beta\","
        "\"playlist_index\":5,\"availability\":\"public\"}"
        "]"
        "}"));
    payload.close();

    const QString scriptPath = createTaggedParallelImportScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(scriptPath);
    appSettings.setFfmpegExecutablePath(scriptPath);

    ThemeManager themeManager;
    YtDlpImportService service;
    service.setAppSettingsManager(&appSettings);
    service.setParallelDownloads(2);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &service);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=dialog-parallel")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("ready"));

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("running"));
    QTRY_COMPARE(dialogObject->property("runningQueueItems").toList().size(), 2);

    QObject *activeDownloadsColumn =
        dialogObject->findChild<QObject *>(QStringLiteral("activeDownloadsColumn"));
    QVERIFY(activeDownloadsColumn);
    QTRY_VERIFY(activeDownloadsColumn->property("visible").toBool());

    QObject *batchProgressBar =
        dialogObject->findChild<QObject *>(QStringLiteral("batchProgressBar"));
    QVERIFY(batchProgressBar);
    QTRY_VERIFY(batchProgressBar->property("value").toDouble() > 0.0);
    QVERIFY(batchProgressBar->property("value").toDouble() < 1.0);

    QObject *batchStatusLabel =
        dialogObject->findChild<QObject *>(QStringLiteral("batchStatusLabel"));
    QVERIFY(batchStatusLabel);
    QTRY_COMPARE(batchStatusLabel->property("text").toString(),
                 AppSettingsManager::translateForCurrentLanguage(QStringLiteral("ytDlpImport.importRunningActiveCount")).arg(2));

    QTRY_VERIFY_WITH_TIMEOUT([dialogObject]() {
        bool sawAlpha = false;
        bool sawBeta = false;
        const QVariantList runningItems = dialogObject->property("runningQueueItems").toList();
        for (const QVariant &value : runningItems) {
            const QString statusText =
                value.toMap().value(QStringLiteral("statusText")).toString();
            if (statusText.contains(QStringLiteral("alpha"))) {
                sawAlpha = true;
            }
            if (statusText.contains(QStringLiteral("beta"))) {
                sawBeta = true;
            }
        }
        return sawAlpha && sawBeta;
    }(), 3000);

    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("succeeded"));
}

void YtDlpImportDialogSmokeTest::dialogHandlesPartialFailureAndRetryFollowUp()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        QDir(tempDir.path()).filePath(QStringLiteral("partial-playlist.json"));
    QFile payload(payloadPath);
    QVERIFY(payload.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    payload.write(QByteArrayLiteral(
        "{"
        "\"_type\":\"playlist\","
        "\"id\":\"dialog-partial\","
        "\"title\":\"Dialog Partial Playlist\","
        "\"extractor_key\":\"YoutubeTab\","
        "\"webpage_url\":\"https://example.com/playlist?list=dialog-partial\","
        "\"entries\":["
        "{\"id\":\"ok-a\",\"title\":\"First OK\","
        "\"webpage_url\":\"https://example.com/watch?v=ok-a\","
        "\"playlist_index\":1,\"availability\":\"public\"},"
        "{\"id\":\"fail-b\",\"title\":\"Second Fail\","
        "\"webpage_url\":\"https://example.com/fail-me\","
        "\"playlist_index\":2,\"availability\":\"public\"}"
        "]"
        "}"));
    payload.close();

    const QString failingScriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath, true);
    const QString retryScriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath, false);
    QVERIFY(!failingScriptPath.isEmpty());
    QVERIFY(!retryScriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(failingScriptPath);
    appSettings.setFfmpegExecutablePath(failingScriptPath);

    ThemeManager themeManager;
    YtDlpImportService service;
    service.setAppSettingsManager(&appSettings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &service);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=dialog-partial")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("ready"));

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QVERIFY(service.finalSummary().value(QStringLiteral("failedCount")).toInt() > 0);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("succeeded"));

    QString failedItemId;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("state")).toString() == QStringLiteral("failed")) {
            failedItemId = item.value(QStringLiteral("itemId")).toString();
            break;
        }
    }
    QVERIFY(!failedItemId.isEmpty());

    appSettings.setYtDlpExecutablePath(retryScriptPath);
    appSettings.setFfmpegExecutablePath(retryScriptPath);
    dialogObject->setProperty("selectedReportItemIds", QVariantList{failedItemId});
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "retrySelectedReportItems"));
    QVERIFY(service.finalSummary().isEmpty());
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("ready"));

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QCOMPARE(service.finalSummary().value(QStringLiteral("totalCount")).toInt(), 1);
    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 0);
    QCOMPARE(service.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 1);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("succeeded"));
}

void YtDlpImportDialogSmokeTest::dialogHidesAndReopensActiveSession()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(scriptPath);
    appSettings.setFfmpegExecutablePath(scriptPath);

    ThemeManager themeManager;
    YtDlpImportService service;
    service.setAppSettingsManager(&appSettings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &service);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/cancel-me")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("running"));

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "requestHideSession"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
    QVERIFY(service.isRunning());

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("running"));

    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("succeeded"));
}

void YtDlpImportDialogSmokeTest::dialogRestoresDraftAfterReopen()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(scriptPath);
    appSettings.setFfmpegExecutablePath(scriptPath);

    {
        YtDlpImportService originalService;
        originalService.setAppSettingsManager(&appSettings);
        originalService.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
        QVERIFY(originalService.probeSourceUrl(QStringLiteral("https://youtu.be/vid123")));
        QTRY_VERIFY_WITH_TIMEOUT(originalService.hasProbeResult(), 5000);
        QVERIFY(!appSettings.ytDlpImportDraft().isEmpty());
    }

    ThemeManager themeManager;
    YtDlpImportService restoredService;
    restoredService.setAppSettingsManager(&appSettings);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &restoredService);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QCOMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("ready"));
    QCOMPARE(dialogObject->property("hasQueueItems").toBool(), true);
    QCOMPARE(restoredService.sources().size(), 1);
    QCOMPARE(restoredService.items().size(), 1);
}

void YtDlpImportDialogSmokeTest::dialogExposesParallelDownloadsSettingAndRestoresIt()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(scriptPath);
    appSettings.setFfmpegExecutablePath(scriptPath);

    ThemeManager themeManager;
    {
        YtDlpImportService originalService;
        originalService.setAppSettingsManager(&appSettings);
        originalService.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
        engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
        engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &originalService);

        const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
        const QByteArray wrapper = QStringLiteral(
                                       "import QtQuick\n"
                                       "import QtQuick.Controls\n"
                                       "import \"%1\" as WaveFluxQml\n"
                                       "ApplicationWindow {\n"
                                       "    width: 960\n"
                                       "    height: 720\n"
                                       "    visible: true\n"
                                       "    WaveFluxQml.YtDlpImportDialog {\n"
                                       "        id: dialog\n"
                                       "        objectName: \"ytDlpDialog\"\n"
                                       "    }\n"
                                       "}\n")
                                       .arg(qmlImportUrl)
                                       .toUtf8();

        QQmlComponent component(&engine);
        component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
        QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

        QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
        QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
        QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
        QVERIFY(dialogObject);

        QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
        QTRY_VERIFY(dialogObject->property("visible").toBool());

        QVERIFY(originalService.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=parallel-setting")));
        QTRY_VERIFY_WITH_TIMEOUT(originalService.hasProbeResult(), 5000);
        QTRY_COMPARE(dialogObject->property("dialogState").toString(), QStringLiteral("ready"));

        QObject *parallelDownloadsComboBox =
            dialogObject->findChild<QObject *>(QStringLiteral("parallelDownloadsComboBox"));
        QVERIFY(parallelDownloadsComboBox);
        QCOMPARE(originalService.parallelDownloads(), 1);
        QCOMPARE(parallelDownloadsComboBox->property("currentIndex").toInt(), 0);

        parallelDownloadsComboBox->setProperty("currentIndex", 2);
        QTRY_COMPARE(originalService.parallelDownloads(), 4);
        QCOMPARE(parallelDownloadsComboBox->property("currentIndex").toInt(), 2);

        QObject *parallelDownloadsHintLabel =
            dialogObject->findChild<QObject *>(QStringLiteral("parallelDownloadsHintLabel"));
        QVERIFY(parallelDownloadsHintLabel);
        QVERIFY(parallelDownloadsHintLabel->property("text").toString().contains(
            QStringLiteral("rate limit"), Qt::CaseInsensitive));

        QObject *summaryQueueModeValueLabel =
            dialogObject->findChild<QObject *>(QStringLiteral("summaryQueueModeValueLabel"));
        QVERIFY(summaryQueueModeValueLabel);
        QVERIFY(summaryQueueModeValueLabel->property("text").toString().contains(QStringLiteral("4")));

        QVERIFY(!appSettings.ytDlpImportDraft().isEmpty());
    }

    YtDlpImportService restoredService;
    restoredService.setAppSettingsManager(&appSettings);

    QQmlEngine restoredEngine;
    restoredEngine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    restoredEngine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    restoredEngine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &restoredService);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent restoredComponent(&restoredEngine);
    restoredComponent.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(restoredComponent.isReady(), qPrintable(componentErrorString(restoredComponent)));

    QScopedPointer<QObject> restoredWindowObject(restoredComponent.create(restoredEngine.rootContext()));
    QVERIFY2(restoredWindowObject, qPrintable(componentErrorString(restoredComponent)));
    QObject *restoredDialogObject =
        restoredWindowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(restoredDialogObject);

    QVERIFY(QMetaObject::invokeMethod(restoredDialogObject, "open"));
    QTRY_VERIFY(restoredDialogObject->property("visible").toBool());
    QCOMPARE(restoredDialogObject->property("dialogState").toString(), QStringLiteral("ready"));
    QCOMPARE(restoredService.parallelDownloads(), 4);

    QObject *restoredParallelDownloadsComboBox =
        restoredDialogObject->findChild<QObject *>(QStringLiteral("parallelDownloadsComboBox"));
    QVERIFY(restoredParallelDownloadsComboBox);
    QCOMPARE(restoredParallelDownloadsComboBox->property("currentIndex").toInt(), 2);

    QObject *restoredSummaryQueueModeValueLabel =
        restoredDialogObject->findChild<QObject *>(QStringLiteral("summaryQueueModeValueLabel"));
    QVERIFY(restoredSummaryQueueModeValueLabel);
    QVERIFY(restoredSummaryQueueModeValueLabel->property("text").toString().contains(QStringLiteral("4")));
}

void YtDlpImportDialogSmokeTest::dialogOpensErrorDialogForProbeFailure()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createProbeFailureScript(tempDir.path());
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager appSettings;
    appSettings.setYtDlpExecutablePath(scriptPath);

    ThemeManager themeManager;
    YtDlpImportService service;
    service.setAppSettingsManager(&appSettings);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ytDlpImportService"), &service);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFluxQml\n"
                                   "ApplicationWindow {\n"
                                   "    width: 960\n"
                                   "    height: 720\n"
                                   "    visible: true\n"
                                   "    WaveFluxQml.YtDlpImportDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"ytDlpDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("ytDlpDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/watch?v=broken")));
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);

    QObject *errorDialog = dialogObject->findChild<QObject *>(QStringLiteral("errorDialog"));
    QVERIFY(errorDialog);
    QTRY_VERIFY(errorDialog->property("visible").toBool());
    QVERIFY(!service.lastError().trimmed().isEmpty());
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    YtDlpImportDialogSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_YtDlpImportDialogSmoke.moc"
