#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "AppSettingsManager.h"
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
            QStringLiteral("test_settings_ytdlp_import_service"));
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

QByteArray readFixture(const QString &relativePath)
{
    QFile file(testDataPath(relativePath));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QString writeTextFile(const QString &directoryPath, const QString &fileName, const QByteArray &contents)
{
    const QString filePath = QDir(directoryPath).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QString();
    }
    file.write(contents);
    file.close();
    return filePath;
}

QString createFakeYtDlpScript(const QString &directoryPath, const QString &payloadPath)
{
    const QString argsLogPath = QDir(directoryPath).filePath(QStringLiteral("last-args.txt"));
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp.bat"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("@echo off\r\n");
    script.write("if \"%1\"==\"--version\" (\r\n");
    script.write("  echo 2025.04.29\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write("setlocal EnableDelayedExpansion\r\n");
    script.write(QStringLiteral("set \"PAYLOAD_PATH=%1\"\r\n").arg(QDir::toNativeSeparators(payloadPath)).toUtf8());
    script.write("set \"MODE=import\"\r\n");
    script.write("set \"OUTPUT_TEMPLATE=\"\r\n");
    script.write("set \"EXT=mp3\"\r\n");
    script.write("set \"URL=\"\r\n");
    script.write(QStringLiteral("set \"ARGS_LOG_PATH=%1\"\r\n").arg(QDir::toNativeSeparators(argsLogPath)).toUtf8());
    script.write("break > \"%ARGS_LOG_PATH%\"\r\n");
    script.write(":parse\r\n");
    script.write("if \"%~1\"==\"\" goto parsed\r\n");
    script.write(">> \"%ARGS_LOG_PATH%\" echo %~1\r\n");
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
    script.write("if \"%URL%\"==\"https://example.com/cancel-me\" ping -n 6 127.0.0.1 >nul\r\n");
    script.write("if \"%URL%\"==\"https://example.com/fail-me\" exit /b 3\r\n");
    script.write("if not \"%OUTPUT_FILE%\"==\"\" (\r\n");
    script.write("  > \"%OUTPUT_FILE%\" echo fake-audio\r\n");
    script.write(")\r\n");
    script.write("echo [download] 100%%\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp"));
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
    script.write("args_log_path=");
    script.write(QByteArray("\"") + argsLogPath.toUtf8() + QByteArray("\"\n"));
    script.write("mode=import\n");
    script.write("output_template=\n");
    script.write("ext=mp3\n");
    script.write("url=\n");
    script.write(": > \"$args_log_path\"\n");
    script.write("while [ \"$#\" -gt 0 ]; do\n");
    script.write("  printf '%s\n' \"$1\" >> \"$args_log_path\"\n");
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
    script.write("if [ \"$url\" = \"https://example.com/cancel-me\" ]; then\n");
    script.write("  sleep 2\n");
    script.write("fi\n");
    script.write("if [ \"$url\" = \"https://example.com/fail-me\" ]; then\n");
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

QString createRoutedProbeScript(const QString &directoryPath,
                                const QString &singlePayloadPath,
                                const QString &playlistPayloadPath)
{
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-router.bat"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("@echo off\r\n");
    script.write("if \"%1\"==\"--version\" (\r\n");
    script.write("  echo 2025.04.29\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write(QStringLiteral("set \"SINGLE_PAYLOAD=%1\"\r\n").arg(QDir::toNativeSeparators(singlePayloadPath)).toUtf8());
    script.write(QStringLiteral("set \"PLAYLIST_PAYLOAD=%1\"\r\n").arg(QDir::toNativeSeparators(playlistPayloadPath)).toUtf8());
    script.write("set \"MODE=import\"\r\n");
    script.write("set \"URL=\"\r\n");
    script.write(":parse\r\n");
    script.write("if \"%~1\"==\"\" goto parsed\r\n");
    script.write("if \"%~1\"==\"--dump-single-json\" set \"MODE=probe\"\r\n");
    script.write("if /I not \"%~1\"==\"--\" if /I not \"%~1:~0,2%\"==\"--\" set \"URL=%~1\"\r\n");
    script.write("shift\r\n");
    script.write("goto parse\r\n");
    script.write(":parsed\r\n");
    script.write("if \"%MODE%\"==\"probe\" (\r\n");
    script.write("  if \"%URL%\"==\"https://example.com/fail-me\" exit /b 3\r\n");
    script.write("  if \"%URL%\"==\"https://example.com/playlist\" type \"%PLAYLIST_PAYLOAD%\" & exit /b 0\r\n");
    script.write("  type \"%SINGLE_PAYLOAD%\"\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write("exit /b 0\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-router"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("#!/bin/sh\n");
    script.write("if [ \"$1\" = \"--version\" ]; then\n");
    script.write("  echo \"2025.04.29\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("mode=import\n");
    script.write("url=\n");
    script.write("while [ \"$#\" -gt 0 ]; do\n");
    script.write("  case \"$1\" in\n");
    script.write("    --dump-single-json)\n");
    script.write("      mode=probe\n");
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
    script.write("  if [ \"$url\" = \"https://example.com/fail-me\" ]; then\n");
    script.write("    exit 3\n");
    script.write("  fi\n");
    script.write("  if [ \"$url\" = \"https://example.com/playlist\" ]; then\n");
    script.write(QByteArray("    cat \"") + playlistPayloadPath.toUtf8() + QByteArray("\"\n"));
    script.write("    exit 0\n");
    script.write("  fi\n");
    script.write(QByteArray("  cat \"") + singlePayloadPath.toUtf8() + QByteArray("\"\n"));
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("exit 0\n");
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
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-probe-failure.bat"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("@echo off\r\n");
    script.write("if \"%1\"==\"--version\" (\r\n");
    script.write("  echo 2025.04.29\r\n");
    script.write("  exit /b 0\r\n");
    script.write(")\r\n");
    script.write("echo Deprecated Feature: The following options have been deprecated: --no-call-home 1>&2\r\n");
    script.write("echo Please remove them from your command/configuration to avoid future errors. 1>&2\r\n");
    script.write("echo ERROR: [youtube] broken: Sign in to confirm you're not a bot 1>&2\r\n");
    script.write("exit /b 1\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-probe-failure"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("#!/bin/sh\n");
    script.write("if [ \"$1\" = \"--version\" ]; then\n");
    script.write("  echo \"2025.04.29\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("printf '%s\\n' 'Deprecated Feature: The following options have been deprecated: --no-call-home' >&2\n");
    script.write("printf '%s\\n' 'Please remove them from your command/configuration to avoid future errors.' >&2\n");
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

QString createProbeProgressScript(const QString &directoryPath, const QString &payloadPath)
{
    const QString argsLogPath = QDir(directoryPath).filePath(QStringLiteral("probe-progress-args.txt"));
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-probe-progress.bat"));
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
    script.write(QStringLiteral("set \"ARGS_LOG_PATH=%1\"\r\n").arg(QDir::toNativeSeparators(argsLogPath)).toUtf8());
    script.write("break > \"%ARGS_LOG_PATH%\"\r\n");
    script.write(":parse\r\n");
    script.write("if \"%~1\"==\"\" goto parsed\r\n");
    script.write(">> \"%ARGS_LOG_PATH%\" echo %~1\r\n");
    script.write("shift\r\n");
    script.write("goto parse\r\n");
    script.write(":parsed\r\n");
    script.write("echo [youtube:tab] Extracting URL: https://www.youtube.com/playlist?list=pl123 1>&2\r\n");
    script.write("ping -n 2 127.0.0.1 >nul\r\n");
    script.write("type \"%PAYLOAD_PATH%\"\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-probe-progress"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    script.write("#!/bin/sh\n");
    script.write("if [ \"$1\" = \"--version\" ]; then\n");
    script.write("  echo \"2025.04.29\"\n");
    script.write("  exit 0\n");
    script.write("fi\n");
    script.write("args_log_path=");
    script.write(QByteArray("\"") + argsLogPath.toUtf8() + QByteArray("\"\n"));
    script.write(": > \"$args_log_path\"\n");
    script.write("while [ \"$#\" -gt 0 ]; do\n");
    script.write("  printf '%s\\n' \"$1\" >> \"$args_log_path\"\n");
    script.write("  shift\n");
    script.write("done\n");
    script.write("printf '%s\\n' '[youtube:tab] Extracting URL: https://www.youtube.com/playlist?list=pl123' >&2\n");
    script.write("sleep 1\n");
    script.write(QByteArray("cat \"") + payloadPath.toUtf8() + QByteArray("\"\n"));
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
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-tagged-parallel.bat"));
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
    script.write("echo [download] 25%% !TAG!\r\n");
    script.write("ping -n 3 127.0.0.1 >nul\r\n");
    script.write("if not \"%OUTPUT_FILE%\"==\"\" (\r\n");
    script.write("  > \"%OUTPUT_FILE%\" echo fake-audio\r\n");
    script.write(")\r\n");
    script.write("echo [download] 100%% !TAG!\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-tagged-parallel"));
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

QString createOutOfOrderParallelImportScript(const QString &directoryPath, const QString &payloadPath)
{
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-out-of-order-parallel.bat"));
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
    script.write("setlocal EnableDelayedExpansion\r\n");
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
    script.write("if \"%URL%\"==\"https://example.com/slow-first\" ping -n 5 127.0.0.1 >nul\r\n");
    script.write("if \"%URL%\"==\"https://example.com/fail-second\" exit /b 3\r\n");
    script.write("if \"%URL%\"==\"https://example.com/fast-third\" ping -n 2 127.0.0.1 >nul\r\n");
    script.write("if not \"%OUTPUT_FILE%\"==\"\" (\r\n");
    script.write("  > \"%OUTPUT_FILE%\" echo fake-audio\r\n");
    script.write(")\r\n");
    script.write("echo [download] 100%%\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-out-of-order-parallel"));
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
    script.write("if [ \"$url\" = \"https://example.com/slow-first\" ]; then\n");
    script.write("  sleep 2\n");
    script.write("fi\n");
    script.write("if [ \"$url\" = \"https://example.com/fail-second\" ]; then\n");
    script.write("  exit 3\n");
    script.write("fi\n");
    script.write("if [ \"$url\" = \"https://example.com/fast-third\" ]; then\n");
    script.write("  sleep 0.3\n");
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

QString createCancelableParallelImportScript(const QString &directoryPath, const QString &payloadPath)
{
#ifdef Q_OS_WIN
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-cancelable-parallel.bat"));
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
    script.write("setlocal EnableDelayedExpansion\r\n");
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
    script.write("echo [download] 25%%\r\n");
    script.write("ping -n 11 127.0.0.1 >nul\r\n");
    script.write("if not \"%OUTPUT_FILE%\"==\"\" (\r\n");
    script.write("  > \"%OUTPUT_FILE%\" echo fake-audio\r\n");
    script.write(")\r\n");
    script.write("echo [download] 100%%\r\n");
    script.close();
    return scriptPath;
#else
    const QString scriptPath = QDir(directoryPath).filePath(QStringLiteral("fake-yt-dlp-cancelable-parallel"));
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
    script.write("echo \"[download] 25%\"\n");
    script.write("sleep 10\n");
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
} // namespace

class YtDlpImportServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void normalizesSlashPrefixedWindowsOutputDirectory();
    void parsesSingleVideoFixture();
    void parsesPlaylistFixtures();
    void sourceIntakeEntryPointsShareQueueModel();
    void sourceIntakeDeduplicatesAndSeparatesInvalidInputs();
    void settingsLifecycleLoadsDefaultsPersistsHistoryAndAppliesPreset();
    void probeQueuePreservesReadySourcesAndAggregatesEntries();
    void probeFailedOrStaleSourcesOnlyReprobesSubset();
    void probeMaterializesStableJobSourceAndEntryState();
    void probeSourceUsesProcessOutput();
    void probeFailurePrefersActualErrorOverDeprecatedWarning();
    void playlistProbeUsesFlatPlaylistAndShowsProbeProgress();
    void importSingleItemUsesTitleOnlyNaming();
    void importRemovesStagingDirectoryAfterCompletion();
    void importStateExportTracksIdsAndTimestamps();
    void importQueueCompletesSequentially();
    void importSchedulerKeepsBoundedParallelWorkers();
    void parallelImportPreservesPlaylistOrderAcrossOutOfOrderCompletion();
    void parallelCancelBroadcastsToActiveAndPendingItems();
    void importRunKeepsFrozenFilesystemPlanAfterStart();
    void parallelImportAvoidsDuplicateOutputPathWrites();
    void importPartialSuccessPreservesReadyFilesAndExplainsFailures();
    void sourceQueueEditingReordersAndRemovesSourcesBeforeStart();
    void retryFailedImportsDoesNotDuplicateQueueItems();
    void importCancelAndRestartClearsStaleState();
    void importPlanningAutoRenamesConflictsAndUsesStaging();
    void retryFailedImportPreservesExistingTargetConflictSemantics();
    void previewPlanningExplainsNamingAndConflictsBeforeStart();
    void importPlanningRespectsSkipAndFailConflictPolicies();
    void finalReportCapturesNotProbedConflictBlockedAndReopens();
    void retrySelectedItemsRetriesSubsetOnly();
    void retrySelectedItemsAfterParallelRunRetriesOnlyRequestedSubset();
    void restoresPersistedDraftAfterReopen();
    void restoringDraftDoesNotReviveActiveRuntimeState();
    void clearResetsCompletedImportState();
};

void YtDlpImportServiceTest::initTestCase()
{
    configureTestSettingsIsolation();
    clearSettings();
}

void YtDlpImportServiceTest::init()
{
    clearSettings();
}

void YtDlpImportServiceTest::cleanup()
{
    clearSettings();
}

void YtDlpImportServiceTest::normalizesSlashPrefixedWindowsOutputDirectory()
{
#if defined(Q_OS_WIN)
    YtDlpImportService service;

    service.setOutputDirectory(QStringLiteral("/C:/Users/leo/Desktop"));

    QCOMPARE(service.outputDirectory(), QStringLiteral("C:/Users/leo/Desktop"));
#else
    QSKIP("Windows drive-letter path normalization only applies on Windows.");
#endif
}

void YtDlpImportServiceTest::parsesSingleVideoFixture()
{
    YtDlpImportService::ProbeResult result;
    QString error;
    QVERIFY(YtDlpImportService::parseProbeJson(
        readFixture(QStringLiteral("yt_dlp/single_video.json")),
        QStringLiteral("https://youtu.be/vid123"),
        &result,
        &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(result.isPlaylist, false);
    QCOMPARE(result.extractor, QStringLiteral("Youtube"));
    QCOMPARE(result.title, QStringLiteral("Single Track"));
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.entries.constFirst().entryId, QStringLiteral("vid123"));
    QCOMPARE(result.entries.constFirst().duration, 123);
    QCOMPARE(result.entries.constFirst().isPlayable, true);
    QCOMPARE(result.isRedirected, true);
}

void YtDlpImportServiceTest::parsesPlaylistFixtures()
{
    {
        YtDlpImportService::ProbeResult result;
        QString error;
        QVERIFY(YtDlpImportService::parseProbeJson(
            readFixture(QStringLiteral("yt_dlp/youtube_playlist.json")),
            QStringLiteral("https://www.youtube.com/playlist?list=pl123"),
            &result,
            &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result.isPlaylist, true);
        QCOMPARE(result.playlistTitle, QStringLiteral("Playlist Preview"));
        QCOMPARE(result.playlistId, QStringLiteral("pl123"));
        QCOMPARE(result.entries.size(), 2);
        QCOMPARE(result.entries.at(0).playlistIndex, 2);
        QCOMPARE(result.entries.at(1).playlistIndex, 5);
        QVERIFY(!result.hasUnavailableEntries);
    }

    {
        YtDlpImportService::ProbeResult result;
        QString error;
        QVERIFY(YtDlpImportService::parseProbeJson(
            readFixture(QStringLiteral("yt_dlp/flat_playlist.json")),
            QStringLiteral("https://www.youtube.com/playlist?list=flat123"),
            &result,
            &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result.entries.size(), 2);
        QCOMPARE(result.entries.at(0).webpageUrl,
                 QStringLiteral("https://www.youtube.com/watch?v=b1"));
        QCOMPARE(result.entries.at(1).entryId, QStringLiteral("b2"));
        QCOMPARE(result.entries.at(1).webpageUrl, QString());
        QCOMPARE(result.entries.at(1).sourceUrl,
                 QStringLiteral("https://www.youtube.com/watch?v=b2"));
        QVERIFY(result.entries.at(1).isPlayable);
    }

    {
        YtDlpImportService::ProbeResult result;
        QString error;
        QVERIFY(YtDlpImportService::parseProbeJson(
            readFixture(QStringLiteral("yt_dlp/partially_unavailable_playlist.json")),
            QStringLiteral("https://www.youtube.com/playlist?list=mixed123"),
            &result,
            &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result.entries.size(), 4);
        QVERIFY(result.hasUnavailableEntries);
        QVERIFY(result.entries.at(0).isPlayable);
        QVERIFY(!result.entries.at(1).isPlayable);
        QCOMPARE(result.entries.at(1).availability, QStringLiteral("private"));
        QVERIFY(!result.entries.at(2).isPlayable);
        QCOMPARE(result.entries.at(2).availability, QStringLiteral("unavailable"));
        QVERIFY(!result.entries.at(3).isPlayable);
        QCOMPARE(result.entries.at(3).availability, QStringLiteral("nested_playlist"));
    }
}

void YtDlpImportServiceTest::sourceIntakeEntryPointsShareQueueModel()
{
    YtDlpImportService service;

    const QVariantMap singleResult =
        service.replaceSourceUrl(QStringLiteral(" https://example.com/watch?v=one "));
    QVERIFY(singleResult.value(QStringLiteral("ok")).toBool());
    QCOMPARE(singleResult.value(QStringLiteral("acceptedCount")).toInt(), 1);
    QCOMPARE(service.sources().size(), 1);
    const QVariantMap singleSource = service.sources().constFirst().toMap();
    QCOMPARE(singleSource.value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("pending-probe"));
    QCOMPARE(singleSource.value(QStringLiteral("immutableSourceInput")).toMap().value(QStringLiteral("normalizedUrl")).toString(),
             QStringLiteral("https://example.com/watch?v=one"));

    const QVariantMap appendTextResult = service.appendSourcesFromText(
        QStringLiteral("\nhttps://example.com/playlist?list=two\n\nhttps://example.com/watch?v=three\n"));
    QVERIFY(appendTextResult.value(QStringLiteral("ok")).toBool());
    QCOMPARE(appendTextResult.value(QStringLiteral("acceptedCount")).toInt(), 2);
    QCOMPARE(appendTextResult.value(QStringLiteral("ignoredEmptyCount")).toInt(), 2);
    QCOMPARE(service.sources().size(), 3);

    const QVariantList currentSources = service.sources();
    QCOMPARE(currentSources.at(0).toMap().value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("queuePosition")).toInt(), 0);
    QCOMPARE(currentSources.at(1).toMap().value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("queuePosition")).toInt(), 1);
    QCOMPARE(currentSources.at(2).toMap().value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("queuePosition")).toInt(), 2);
    QCOMPARE(currentSources.at(1).toMap().value(QStringLiteral("immutableSourceInput")).toMap().value(QStringLiteral("normalizedUrl")).toString(),
             QStringLiteral("https://example.com/playlist?list=two"));
    QCOMPARE(currentSources.at(2).toMap().value(QStringLiteral("immutableSourceInput")).toMap().value(QStringLiteral("normalizedUrl")).toString(),
             QStringLiteral("https://example.com/watch?v=three"));

    const QVariantMap replaceListResult = service.replaceSourcesFromVariantList(
        QVariantList{QStringLiteral("https://example.com/a"),
                     QStringLiteral("https://example.com/b")});
    QVERIFY(replaceListResult.value(QStringLiteral("ok")).toBool());
    QCOMPARE(replaceListResult.value(QStringLiteral("acceptedCount")).toInt(), 2);
    QCOMPARE(service.sources().size(), 2);
    QVERIFY(service.probeResult().isEmpty());
    QVERIFY(service.items().isEmpty());

    const QVariantList replacedSources = service.sources();
    QCOMPARE(replacedSources.at(0).toMap().value(QStringLiteral("immutableSourceInput")).toMap().value(QStringLiteral("normalizedUrl")).toString(),
             QStringLiteral("https://example.com/a"));
    QCOMPARE(replacedSources.at(1).toMap().value(QStringLiteral("immutableSourceInput")).toMap().value(QStringLiteral("normalizedUrl")).toString(),
             QStringLiteral("https://example.com/b"));
}

void YtDlpImportServiceTest::sourceIntakeDeduplicatesAndSeparatesInvalidInputs()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("failing-single.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"id\":\"fail-1\","
                          "\"title\":\"Failing Track\","
                          "\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"availability\":\"public\""
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    const QVariantMap intake = service.replaceSourcesFromText(
        QStringLiteral(" \nftp://example.com/audio\nnotaurl\nhttps://example.com/watch?v=one\nhttps://example.com/watch?v=one\n"));
    QVERIFY(intake.value(QStringLiteral("ok")).toBool());
    QCOMPARE(intake.value(QStringLiteral("acceptedCount")).toInt(), 1);
    QCOMPARE(intake.value(QStringLiteral("duplicateCount")).toInt(), 1);
    QCOMPARE(intake.value(QStringLiteral("invalidCount")).toInt(), 2);
    QCOMPARE(intake.value(QStringLiteral("ignoredEmptyCount")).toInt(), 2);
    QCOMPARE(service.sources().size(), 1);

    const QVariantList invalidSources = intake.value(QStringLiteral("invalidSources")).toList();
    QCOMPARE(invalidSources.size(), 2);
    QCOMPARE(invalidSources.at(0).toMap().value(QStringLiteral("issueKey")).toString(),
             QStringLiteral("unsupported-scheme"));
    QCOMPARE(invalidSources.at(1).toMap().value(QStringLiteral("issueKey")).toString(),
             QStringLiteral("invalid-url"));

    const QVariantList duplicateSources = intake.value(QStringLiteral("duplicateSources")).toList();
    QCOMPARE(duplicateSources.size(), 1);
    QCOMPARE(duplicateSources.constFirst().toMap().value(QStringLiteral("duplicateReason")).toString(),
             QStringLiteral("same-normalized-url"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://youtu.be/vid123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QCOMPARE(service.sources().size(), 1);

    const QVariantMap canonicalDuplicate =
        service.appendSourceUrl(QStringLiteral("https://www.youtube.com/watch?v=vid123"));
    QVERIFY(canonicalDuplicate.value(QStringLiteral("ok")).toBool());
    QCOMPARE(canonicalDuplicate.value(QStringLiteral("acceptedCount")).toInt(), 0);
    QCOMPARE(canonicalDuplicate.value(QStringLiteral("duplicateCount")).toInt(), 1);
    QCOMPARE(canonicalDuplicate.value(QStringLiteral("duplicateSources")).toList().constFirst().toMap().value(QStringLiteral("duplicateReason")).toString(),
             QStringLiteral("known-canonical-source"));
    QCOMPARE(service.sources().size(), 1);
}

void YtDlpImportServiceTest::settingsLifecycleLoadsDefaultsPersistsHistoryAndAppliesPreset()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setYtDlpImportLastSettings(QVariantMap{
        {QStringLiteral("outputDirectory"), QStringLiteral("/tmp/from-settings")},
        {QStringLiteral("selectedFormat"), QStringLiteral("opus")},
        {QStringLiteral("namingPolicy"), QStringLiteral("title-only")},
        {QStringLiteral("conflictPolicy"), QStringLiteral("skip-on-conflict")},
        {QStringLiteral("parallelDownloads"), 4}
    });
    settings.setYtDlpImportRecentSources(QVariantList{
        QStringLiteral("https://example.com/history-one"),
        QStringLiteral("https://example.com/history-two")
    });
    settings.setYtDlpImportRecentOutputDirectories(QVariantList{
        QStringLiteral("/tmp/from-settings")
    });

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    QCOMPARE(service.outputDirectory(), QStringLiteral("/tmp/from-settings"));
    QCOMPARE(service.selectedFormat(), QStringLiteral("opus"));
    QCOMPARE(service.namingPolicy(), QStringLiteral("title-only"));
    QCOMPARE(service.conflictPolicy(), QStringLiteral("skip-on-conflict"));
    QCOMPARE(service.parallelDownloads(), 4);
    const QVariantList expectedRecentSources = {
        QStringLiteral("https://example.com/history-one"),
        QStringLiteral("https://example.com/history-two")
    };
    QCOMPARE(service.recentSourceUrls(), expectedRecentSources);
    const QVariantList expectedRecentOutputDirectories = {
        QStringLiteral("/tmp/from-settings")
    };
    QCOMPARE(service.recentOutputDirectories(), expectedRecentOutputDirectories);

    const QVariantMap intake =
        service.appendSourceUrl(QStringLiteral("https://example.com/new-source"));
    QVERIFY(intake.value(QStringLiteral("ok")).toBool());
    QCOMPARE(settings.ytDlpImportRecentSources().constFirst().toString(),
             QStringLiteral("https://example.com/new-source"));

    QVERIFY(service.probeSourceById(
        intake.value(QStringLiteral("acceptedSources")).toList().constFirst().toMap()
            .value(QStringLiteral("sourceId")).toString()));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QCOMPARE(settings.ytDlpImportRecentCanonicalSources().size(), 1);
    QVERIFY(!settings.ytDlpImportRecentCanonicalSources().constFirst().toString().isEmpty());

    QVERIFY(service.applySettingsPreset(QVariantMap{
        {QStringLiteral("outputDirectory"), QStringLiteral("/tmp/preset-output")},
        {QStringLiteral("selectedFormat"), QStringLiteral("mp3")},
        {QStringLiteral("namingPolicy"), QStringLiteral("auto")},
        {QStringLiteral("conflictPolicy"), QStringLiteral("fail-on-conflict")},
        {QStringLiteral("parallelDownloads"), 0}
    }));
    QCOMPARE(service.outputDirectory(), QStringLiteral("/tmp/preset-output"));
    QCOMPARE(service.selectedFormat(), QStringLiteral("mp3"));
    QCOMPARE(service.namingPolicy(), QStringLiteral("auto"));
    QCOMPARE(service.conflictPolicy(), QStringLiteral("fail-on-conflict"));
    QCOMPARE(service.parallelDownloads(), 1);
    QCOMPARE(settings.ytDlpImportLastSettings().value(QStringLiteral("outputDirectory")).toString(),
             QStringLiteral("/tmp/preset-output"));
    QCOMPARE(settings.ytDlpImportLastSettings().value(QStringLiteral("selectedFormat")).toString(),
             QStringLiteral("mp3"));
    QCOMPARE(settings.ytDlpImportLastSettings().value(QStringLiteral("namingPolicy")).toString(),
             QStringLiteral("auto"));
    QCOMPARE(settings.ytDlpImportLastSettings().value(QStringLiteral("conflictPolicy")).toString(),
             QStringLiteral("fail-on-conflict"));
    QCOMPARE(settings.ytDlpImportLastSettings().value(QStringLiteral("parallelDownloads")).toInt(),
             1);
    QCOMPARE(settings.ytDlpImportRecentOutputDirectories().constFirst().toString(),
             QStringLiteral("/tmp/preset-output"));

    const QVariantMap source =
        service.sourceById(service.sources().constLast().toMap().value(QStringLiteral("sourceId")).toString());
    QVERIFY(source.value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("isStale")).toBool());
    const QVariantMap currentPreset = service.currentSettingsPreset();
    QCOMPARE(currentPreset.value(QStringLiteral("conflictPolicy")).toString(),
             QStringLiteral("fail-on-conflict"));
    QCOMPARE(currentPreset.value(QStringLiteral("parallelDownloads")).toInt(), 1);
}

void YtDlpImportServiceTest::probeQueuePreservesReadySourcesAndAggregatesEntries()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString singlePayloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString playlistPayloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath =
        createRoutedProbeScript(tempDir.path(), singlePayloadPath, playlistPayloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    const QVariantMap intake = service.replaceSourcesFromVariantList(
        QVariantList{QStringLiteral("https://example.com/single"),
                     QStringLiteral("https://example.com/playlist")});
    QVERIFY(intake.value(QStringLiteral("ok")).toBool());
    QCOMPARE(service.sources().size(), 2);

    QVERIFY(service.probeAllSources());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);

    const QVariantList sourcesAfterFirstProbe = service.sources();
    QCOMPARE(sourcesAfterFirstProbe.size(), 2);
    QCOMPARE(sourcesAfterFirstProbe.at(0).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(sourcesAfterFirstProbe.at(1).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(sourcesAfterFirstProbe.at(0).toMap().value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("entryCount")).toInt(),
             1);
    QCOMPARE(sourcesAfterFirstProbe.at(1).toMap().value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("entryCount")).toInt(),
             2);
    QCOMPARE(service.items().size(), 3);

    const QVariantMap appendResult =
        service.appendSourceUrl(QStringLiteral("https://example.com/new-single"));
    QVERIFY(appendResult.value(QStringLiteral("ok")).toBool());
    QCOMPARE(service.sources().size(), 3);
    QCOMPARE(service.items().size(), 3);
    QCOMPARE(service.sources().at(2).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("pending-probe"));

    QVERIFY(service.probeAllSources());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);

    const QVariantList sourcesAfterSecondProbe = service.sources();
    QCOMPARE(sourcesAfterSecondProbe.size(), 3);
    QCOMPARE(sourcesAfterSecondProbe.at(0).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(sourcesAfterSecondProbe.at(1).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(sourcesAfterSecondProbe.at(2).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(service.items().size(), 4);
}

void YtDlpImportServiceTest::probeFailedOrStaleSourcesOnlyReprobesSubset()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString singlePayloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString playlistPayloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath =
        createRoutedProbeScript(tempDir.path(), singlePayloadPath, playlistPayloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    const QVariantMap intake = service.replaceSourcesFromVariantList(
        QVariantList{QStringLiteral("https://example.com/single"),
                     QStringLiteral("https://example.com/fail-me")});
    QVERIFY(intake.value(QStringLiteral("ok")).toBool());

    QVERIFY(service.probeAllSources());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);

    const QVariantList firstSources = service.sources();
    QCOMPARE(firstSources.size(), 2);
    QCOMPARE(firstSources.at(0).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(firstSources.at(1).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("probe-failed"));
    QCOMPARE(service.items().size(), 1);

    service.setSelectedFormat(QStringLiteral("opus"));
    const QVariantList staleSources = service.sources();
    QCOMPARE(staleSources.at(0).toMap().value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("isStale")).toBool(),
             true);

    QVERIFY(service.probeFailedOrStaleSources());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);

    const QVariantList reprobedSources = service.sources();
    QCOMPARE(reprobedSources.at(0).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(reprobedSources.at(0).toMap().value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("isStale")).toBool(),
             false);
    QCOMPARE(reprobedSources.at(0).toMap().value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("probeFormatSnapshot")).toString(),
             QStringLiteral("opus"));
    QCOMPARE(reprobedSources.at(1).toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("probe-failed"));
    QCOMPARE(service.items().size(), 1);
}

void YtDlpImportServiceTest::probeMaterializesStableJobSourceAndEntryState()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/playlist?list=pl123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    const QVariantMap job = service.importJob();
    QVERIFY(!job.value(QStringLiteral("jobId")).toString().trimmed().isEmpty());
    QVERIFY(job.value(QStringLiteral("createdAtMs")).toLongLong() > 0);
    QCOMPARE(job.value(QStringLiteral("startedAtMs")).toLongLong(), 0);
    QCOMPARE(job.value(QStringLiteral("finishedAtMs")).toLongLong(), 0);

    const QVariantList sources = service.sources();
    QCOMPARE(sources.size(), 1);
    const QVariantMap source = sources.constFirst().toMap();
    const QString sourceId = source.value(QStringLiteral("sourceId")).toString();
    QVERIFY(!sourceId.isEmpty());
    QCOMPARE(source.value(QStringLiteral("sourceStatus")).toString(), QStringLiteral("ready"));
    QCOMPARE(source.value(QStringLiteral("retryEligibility")).toString(),
             QStringLiteral("not-applicable"));
    QCOMPARE(source.value(QStringLiteral("runtimeState")).toMap().value(QStringLiteral("entryCount")).toInt(),
             2);
    QCOMPARE(source.value(QStringLiteral("metadataSnapshot")).toMap().value(QStringLiteral("isPlaylist")).toBool(),
             true);

    const QVariantMap exported = service.exportCurrentJobState();
    QCOMPARE(exported.value(QStringLiteral("schema")).toString(),
             QStringLiteral("waveflux.ytdlp-import.v2"));
    QCOMPARE(exported.value(QStringLiteral("jobMetadata")).toMap().value(QStringLiteral("jobId")).toString(),
             job.value(QStringLiteral("jobId")).toString());

    const QVariantList items = exported.value(QStringLiteral("items")).toList();
    QCOMPARE(items.size(), 2);
    const QVariantMap firstItem = items.at(0).toMap();
    const QVariantMap secondItem = items.at(1).toMap();
    QVERIFY(!firstItem.value(QStringLiteral("itemId")).toString().isEmpty());
    QVERIFY(!secondItem.value(QStringLiteral("itemId")).toString().isEmpty());
    QVERIFY(firstItem.value(QStringLiteral("itemId")).toString()
            != secondItem.value(QStringLiteral("itemId")).toString());
    QCOMPARE(firstItem.value(QStringLiteral("sourceId")).toString(), sourceId);
    QCOMPARE(secondItem.value(QStringLiteral("sourceId")).toString(), sourceId);
    QCOMPARE(firstItem.value(QStringLiteral("playlistIndex")).toInt(), 2);
    QCOMPARE(secondItem.value(QStringLiteral("playlistIndex")).toInt(), 5);
    QCOMPARE(firstItem.value(QStringLiteral("metadataOrder")).toInt(), 0);
    QCOMPARE(secondItem.value(QStringLiteral("metadataOrder")).toInt(), 1);

    const QString firstItemId = firstItem.value(QStringLiteral("itemId")).toString();
    QCOMPARE(service.indexOfItemId(firstItemId), 0);
    QCOMPARE(service.itemIdAt(0), firstItemId);
    QCOMPARE(service.itemById(firstItemId).value(QStringLiteral("sourceId")).toString(), sourceId);
    QCOMPARE(service.indexOfSourceId(sourceId), 0);
    QCOMPARE(service.sourceIdAt(0), sourceId);
    QCOMPARE(service.sourceById(sourceId).value(QStringLiteral("sourceId")).toString(), sourceId);
}

void YtDlpImportServiceTest::probeSourceUsesProcessOutput()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setSourceUrl(QStringLiteral("https://www.youtube.com/playlist?list=pl123"));

    QSignalSpy resultSpy(&service, &YtDlpImportService::probeResultChanged);
    QVERIFY(service.probeSource());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);
    QVERIFY(service.hasProbeResult());
    QVERIFY(resultSpy.count() > 0);
    QCOMPARE(service.probeResult().value(QStringLiteral("sourceType")).toString(),
             QStringLiteral("playlist"));
    QCOMPARE(service.probeResult().value(QStringLiteral("entryCount")).toInt(), 2);

    const QString invalidScriptPath =
#ifdef Q_OS_WIN
        QDir(tempDir.path()).filePath(QStringLiteral("fake-yt-dlp-invalid.bat"));
#else
        QDir(tempDir.path()).filePath(QStringLiteral("fake-yt-dlp-invalid"));
#endif
    QFile invalidScript(invalidScriptPath);
    QVERIFY(invalidScript.open(QIODevice::WriteOnly | QIODevice::Text));
#ifdef Q_OS_WIN
    invalidScript.write("@echo off\r\n");
    invalidScript.write("if \"%1\"==\"--version\" (\r\n");
    invalidScript.write("  echo 2025.04.29\r\n");
    invalidScript.write("  exit /b 0\r\n");
    invalidScript.write(")\r\n");
    invalidScript.write("echo {not-json}\r\n");
#else
    invalidScript.write("#!/bin/sh\n");
    invalidScript.write("if [ \"$1\" = \"--version\" ]; then\n");
    invalidScript.write("  echo \"2025.04.29\"\n");
    invalidScript.write("  exit 0\n");
    invalidScript.write("fi\n");
    invalidScript.write("echo '{not-json}'\n");
    QFile::setPermissions(invalidScriptPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
#endif
    invalidScript.close();

    settings.setYtDlpExecutablePath(invalidScriptPath);
    QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/watch?v=broken")));
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);
    QVERIFY(!service.hasProbeResult());
    QVERIFY(!service.lastError().isEmpty());
}

void YtDlpImportServiceTest::probeFailurePrefersActualErrorOverDeprecatedWarning()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scriptPath = createProbeFailureScript(tempDir.path());
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/watch?v=broken")));
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);
    QVERIFY(!service.lastError().isEmpty());
    QVERIFY(service.lastError().contains(QStringLiteral("ERROR: [youtube] broken: Sign in to confirm you're not a bot")));
    QVERIFY(!service.lastError().contains(QStringLiteral("Deprecated Feature:")));
}

void YtDlpImportServiceTest::playlistProbeUsesFlatPlaylistAndShowsProbeProgress()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/flat_playlist.json"));
    const QString scriptPath = createProbeProgressScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    QVERIFY(service.probeSourceUrl(
        QStringLiteral("https://www.youtube.com/watch?v=seed123&list=PLTvXB6zVYulUgrKI0BKeVcIOx5WREiDA3")));
    QTRY_VERIFY_WITH_TIMEOUT(service.statusText().contains(QStringLiteral("Extracting URL")), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QCOMPARE(service.probeResult().value(QStringLiteral("sourceType")).toString(),
             QStringLiteral("playlist"));

    const QString argsLogPath = QDir(tempDir.path()).filePath(QStringLiteral("probe-progress-args.txt"));
    QFile argsLogFile(argsLogPath);
    QVERIFY(argsLogFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString argsLog = QString::fromUtf8(argsLogFile.readAll());
    QVERIFY(argsLog.contains(QStringLiteral("--flat-playlist")));
    QVERIFY(argsLog.contains(QStringLiteral("--yes-playlist")));
    QVERIFY(argsLog.contains(QStringLiteral("https://www.youtube.com/watch?v=seed123&list=PLTvXB6zVYulUgrKI0BKeVcIOx5WREiDA3")));
}

void YtDlpImportServiceTest::importSingleItemUsesTitleOnlyNaming()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    service.setSelectedFormat(QStringLiteral("mp3"));
    service.setNamingPolicy(QStringLiteral("title-only"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://youtu.be/vid123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 1);
    const QVariantMap item = items.constFirst().toMap();
    QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QVERIFY(item.value(QStringLiteral("plannedOutputFile")).toString().endsWith(
        QStringLiteral("/Single Track.mp3")));
    QVERIFY(!item.value(QStringLiteral("plannedOutputFile")).toString().contains(
        QStringLiteral("0001 - ")));

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("succeededCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("failedCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("outcomeKey")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(summary.value(QStringLiteral("orderedResultFiles")).toStringList().size(), 1);
}

void YtDlpImportServiceTest::importRemovesStagingDirectoryAfterCompletion()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    const QString outputDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads"));
    const QString stagingDirectory = QDir(outputDirectory).filePath(QStringLiteral(".waveflux-yt-dlp-staging"));

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(outputDirectory);
    service.setSelectedFormat(QStringLiteral("mp3"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://youtu.be/vid123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 1);
    QVERIFY(!QFileInfo::exists(stagingDirectory));
}

void YtDlpImportServiceTest::importStateExportTracksIdsAndTimestamps()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/playlist?list=pl123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    const QVariantMap exportedBeforeRun = service.exportCurrentJobState();
    const QVariantList exportedItemsBeforeRun = exportedBeforeRun.value(QStringLiteral("items")).toList();
    QCOMPARE(exportedItemsBeforeRun.size(), 2);
    const QString firstEntryId = exportedItemsBeforeRun.at(0).toMap().value(QStringLiteral("itemId")).toString();

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantMap job = service.importJob();
    QVERIFY(job.value(QStringLiteral("createdAtMs")).toLongLong() > 0);
    QVERIFY(job.value(QStringLiteral("startedAtMs")).toLongLong() >= job.value(QStringLiteral("createdAtMs")).toLongLong());
    QVERIFY(job.value(QStringLiteral("finishedAtMs")).toLongLong() >= job.value(QStringLiteral("startedAtMs")).toLongLong());

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("jobId")).toString(), job.value(QStringLiteral("jobId")).toString());
    QCOMPARE(summary.value(QStringLiteral("createdAtMs")).toLongLong(), job.value(QStringLiteral("createdAtMs")).toLongLong());
    QCOMPARE(summary.value(QStringLiteral("startedAtMs")).toLongLong(), job.value(QStringLiteral("startedAtMs")).toLongLong());
    QCOMPARE(summary.value(QStringLiteral("finishedAtMs")).toLongLong(), job.value(QStringLiteral("finishedAtMs")).toLongLong());

    const QVariantMap firstItem = service.itemById(firstEntryId);
    QCOMPARE(firstItem.value(QStringLiteral("itemId")).toString(), firstEntryId);
    QCOMPARE(firstItem.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QVERIFY(firstItem.value(QStringLiteral("createdAtMs")).toLongLong() > 0);
    QVERIFY(firstItem.value(QStringLiteral("updatedAtMs")).toLongLong()
            >= firstItem.value(QStringLiteral("createdAtMs")).toLongLong());
    QCOMPARE(firstItem.value(QStringLiteral("persistenceEligibility")).toString(),
             QStringLiteral("report"));

    const QVariantList sources = service.sources();
    QCOMPARE(sources.size(), 1);
    const QVariantMap source = sources.constFirst().toMap();
    QCOMPARE(source.value(QStringLiteral("sourceStatus")).toString(), QStringLiteral("completed"));
    QCOMPARE(source.value(QStringLiteral("persistenceEligibility")).toString(),
             QStringLiteral("report"));

    const QVariantMap exportedAfterRun = service.exportCurrentJobState();
    const QVariantMap exportedJob = exportedAfterRun.value(QStringLiteral("jobMetadata")).toMap();
    QCOMPARE(exportedJob.value(QStringLiteral("jobId")).toString(), job.value(QStringLiteral("jobId")).toString());
    QCOMPARE(exportedJob.value(QStringLiteral("finishedAtMs")).toLongLong(), job.value(QStringLiteral("finishedAtMs")).toLongLong());
    const QVariantList exportedItemsAfterRun = exportedAfterRun.value(QStringLiteral("items")).toList();
    QCOMPARE(exportedItemsAfterRun.size(), 2);
    QCOMPARE(exportedItemsAfterRun.at(0).toMap().value(QStringLiteral("itemId")).toString(), firstEntryId);
}

void YtDlpImportServiceTest::importQueueCompletesSequentially()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    service.setSelectedFormat(QStringLiteral("opus"));
    service.setNamingPolicy(QStringLiteral("auto"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/playlist?list=pl123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    QSignalSpy itemsSpy(&service, &YtDlpImportService::itemsChanged);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QVERIFY(itemsSpy.count() > 0);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    QCOMPARE(service.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 2);
    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 0);
    QCOMPARE(service.finalSummary().value(QStringLiteral("wasCanceled")).toBool(), false);
    QCOMPARE(service.finalSummary().value(QStringLiteral("selectedFormat")).toString(),
             QStringLiteral("opus"));
    QVERIFY(service.batchProgress() >= 1.0);
    const QStringList orderedResultFiles =
        service.finalSummary().value(QStringLiteral("orderedResultFiles")).toStringList();
    QCOMPARE(orderedResultFiles.size(), 2);

    const QVariantMap firstItem = items.at(0).toMap();
    const QVariantMap secondItem = items.at(1).toMap();
    QCOMPARE(firstItem.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(secondItem.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(firstItem.value(QStringLiteral("resultTrackInsertIndex")).toInt(), 0);
    QCOMPARE(secondItem.value(QStringLiteral("resultTrackInsertIndex")).toInt(), 1);
    QVERIFY(firstItem.value(QStringLiteral("plannedOutputFile")).toString().contains(QStringLiteral("0002 - ")));
    QVERIFY(secondItem.value(QStringLiteral("plannedOutputFile")).toString().contains(QStringLiteral("0005 - ")));
    QVERIFY(QFileInfo::exists(firstItem.value(QStringLiteral("finalOutputFile")).toString()));
    QVERIFY(QFileInfo::exists(secondItem.value(QStringLiteral("finalOutputFile")).toString()));
    QCOMPARE(orderedResultFiles.at(0), firstItem.value(QStringLiteral("finalOutputFile")).toString());
    QCOMPARE(orderedResultFiles.at(1), secondItem.value(QStringLiteral("finalOutputFile")).toString());
}

void YtDlpImportServiceTest::importSchedulerKeepsBoundedParallelWorkers()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("parallel-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"id\":\"parallel-pl\","
                          "\"title\":\"Parallel Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=parallel\","
                          "\"entries\":["
                          "{\"id\":\"slow-1\",\"title\":\"Slow One\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/tag-alpha\",\"playlist_index\":2},"
                          "{\"id\":\"slow-2\",\"title\":\"Slow Two\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/tag-beta\",\"playlist_index\":5}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createTaggedParallelImportScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    service.setSelectedFormat(QStringLiteral("opus"));
    service.setParallelDownloads(2);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=parallel")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT([&service]() {
        int runningCount = 0;
        for (const QVariant &value : service.items()) {
            if (value.toMap().value(QStringLiteral("state")).toString() == QStringLiteral("running")) {
                ++runningCount;
            }
        }
        return runningCount == 2;
    }(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(service.statusText(),
                              AppSettingsManager::translateForCurrentLanguage(QStringLiteral("ytDlpImport.importRunningActiveCount")).arg(2),
                              2000);
    QTRY_VERIFY_WITH_TIMEOUT([&service]() {
        bool sawAlpha = false;
        bool sawBeta = false;
        for (const QVariant &value : service.items()) {
            const QVariantMap item = value.toMap();
            const QString statusText = item.value(QStringLiteral("statusText")).toString();
            if (statusText.contains(QStringLiteral("alpha"))) {
                sawAlpha = true;
            }
            if (statusText.contains(QStringLiteral("beta"))) {
                sawBeta = true;
            }
        }
        return sawAlpha && sawBeta;
    }(), 2000);

    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 7000);
    QCOMPARE(service.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 2);
    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 0);
    QCOMPARE(service.finalSummary().value(QStringLiteral("wasCanceled")).toBool(), false);

    int terminalCount = 0;
    for (const QVariant &value : service.items()) {
        const QString state = value.toMap().value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("succeeded")) {
            ++terminalCount;
        }
    }
    QCOMPARE(terminalCount, 2);
}

void YtDlpImportServiceTest::parallelImportPreservesPlaylistOrderAcrossOutOfOrderCompletion()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("out-of-order-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"id\":\"ordered-pl\","
                          "\"title\":\"Ordered Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=ordered\","
                          "\"entries\":["
                          "{\"id\":\"slow-1\",\"title\":\"Slow First\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/slow-first\",\"playlist_index\":2},"
                          "{\"id\":\"fail-2\",\"title\":\"Fail Second\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/fail-second\",\"playlist_index\":5},"
                          "{\"id\":\"fast-3\",\"title\":\"Fast Third\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/fast-third\",\"playlist_index\":9}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createOutOfOrderParallelImportScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    service.setSelectedFormat(QStringLiteral("opus"));
    service.setParallelDownloads(2);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=ordered")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    QSignalSpy readySpy(&service, &YtDlpImportService::playlistImportReady);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 7000);

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("succeededCount")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("failedCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("wasCanceled")).toBool(), false);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 3);
    const QVariantMap firstItem = items.at(0).toMap();
    const QVariantMap secondItem = items.at(1).toMap();
    const QVariantMap thirdItem = items.at(2).toMap();
    QCOMPARE(firstItem.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(secondItem.value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QCOMPARE(thirdItem.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(firstItem.value(QStringLiteral("resultTrackInsertIndex")).toInt(), 0);
    QCOMPARE(secondItem.value(QStringLiteral("resultTrackInsertIndex")).toInt(), 1);
    QCOMPARE(thirdItem.value(QStringLiteral("resultTrackInsertIndex")).toInt(), 2);

    const QStringList orderedResultFiles =
        summary.value(QStringLiteral("orderedResultFiles")).toStringList();
    QCOMPARE(orderedResultFiles.size(), 2);
    QCOMPARE(orderedResultFiles.at(0), firstItem.value(QStringLiteral("finalOutputFile")).toString());
    QCOMPARE(orderedResultFiles.at(1), thirdItem.value(QStringLiteral("finalOutputFile")).toString());

    QCOMPARE(readySpy.size(), 1);
    QCOMPARE(readySpy.constFirst().constFirst().toStringList(), orderedResultFiles);
}

void YtDlpImportServiceTest::parallelCancelBroadcastsToActiveAndPendingItems()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("parallel-cancel-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"parallel-cancel\","
                          "\"title\":\"Parallel Cancel Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=parallel-cancel\","
                          "\"entries\":["
                          "{\"id\":\"cancel-a\",\"title\":\"Cancel A\","
                          "\"webpage_url\":\"https://example.com/tag-alpha\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"cancel-b\",\"title\":\"Cancel B\","
                          "\"webpage_url\":\"https://example.com/tag-beta\","
                          "\"playlist_index\":2,\"availability\":\"public\"},"
                          "{\"id\":\"cancel-c\",\"title\":\"Cancel C\","
                          "\"webpage_url\":\"https://example.com/tag-alpha-2\","
                          "\"playlist_index\":3,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createTaggedParallelImportScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setParallelDownloads(2);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=parallel-cancel")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    service.startImport();
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(service.statusText(),
                              AppSettingsManager::translateForCurrentLanguage(QStringLiteral("ytDlpImport.importRunningActiveCount")).arg(2),
                              2000);

    int runningCount = 0;
    int pendingCount = 0;
    for (const QVariant &value : service.items()) {
        const QString state = value.toMap().value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("running")) {
            ++runningCount;
        } else if (state == QStringLiteral("pending")) {
            ++pendingCount;
        }
    }
    QCOMPARE(runningCount, 2);
    QCOMPARE(pendingCount, 1);

    service.cancelImport();
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 7000);

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("wasCanceled")).toBool(), true);
    QCOMPARE(summary.value(QStringLiteral("totalCount")).toInt(), 3);
    QCOMPARE(summary.value(QStringLiteral("canceledCount")).toInt(), 3);
    QCOMPARE(summary.value(QStringLiteral("succeededCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("failedCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("skippedCount")).toInt(), 0);
    QVERIFY(summary.value(QStringLiteral("orderedResultFiles")).toStringList().isEmpty());
    QCOMPARE(service.batchProgress(), 1.0);
    QVERIFY(service.statusText().contains(
        AppSettingsManager::translateForCurrentLanguage(QStringLiteral("ytDlpImport.summaryDetailPattern"))
            .arg(0).arg(0).arg(3).arg(0)));
    QVERIFY(service.lastError().isEmpty());

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 3);
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("canceled"));
        QCOMPARE(item.value(QStringLiteral("progress")).toDouble(), 1.0);
        QVERIFY(item.value(QStringLiteral("finalOutputFile")).toString().isEmpty());
    }
}

void YtDlpImportServiceTest::importRunKeepsFrozenFilesystemPlanAfterStart()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("frozen-plan-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"id\":\"frozen-plan\","
                          "\"title\":\"Frozen Plan Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=frozen-plan\","
                          "\"entries\":["
                          "{\"id\":\"slow-1\",\"title\":\"Slow One\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/tag-alpha\",\"playlist_index\":2},"
                          "{\"id\":\"slow-2\",\"title\":\"Slow Two\",\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/tag-beta\",\"playlist_index\":5}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createTaggedParallelImportScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    const QString initialOutputDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads-initial"));
    const QString changedOutputDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads-changed"));
    QVERIFY(QDir().mkpath(initialOutputDirectory));
    QVERIFY(QDir().mkpath(changedOutputDirectory));

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(initialOutputDirectory);
    service.setSelectedFormat(QStringLiteral("mp3"));
    service.setParallelDownloads(1);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=frozen-plan")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    const QVariantList plannedItems = service.items();
    QCOMPARE(plannedItems.size(), 2);
    const QString firstPlannedOutput = plannedItems.at(0).toMap().value(QStringLiteral("plannedOutputFile")).toString();
    const QString secondPlannedOutput = plannedItems.at(1).toMap().value(QStringLiteral("plannedOutputFile")).toString();
    QVERIFY(firstPlannedOutput.endsWith(QStringLiteral(".mp3")));
    QVERIFY(secondPlannedOutput.endsWith(QStringLiteral(".mp3")));
    QVERIFY(firstPlannedOutput.startsWith(initialOutputDirectory));
    QVERIFY(secondPlannedOutput.startsWith(initialOutputDirectory));

    service.startImport();
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT([&service]() {
        for (const QVariant &value : service.items()) {
            const QVariantMap item = value.toMap();
            if (item.value(QStringLiteral("state")).toString() == QStringLiteral("running")
                && item.value(QStringLiteral("statusText")).toString().contains(QStringLiteral("alpha"))) {
                return true;
            }
        }
        return false;
    }(), 3000);

    service.setOutputDirectory(changedOutputDirectory);
    service.setSelectedFormat(QStringLiteral("opus"));

    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 7000);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
        const QString finalOutputFile = item.value(QStringLiteral("finalOutputFile")).toString();
        const QString plannedOutputFile = item.value(QStringLiteral("plannedOutputFile")).toString();
        const QString stagingDirectory = item.value(QStringLiteral("stagingDirectory")).toString();
        const QString stagingOutputFile = item.value(QStringLiteral("stagingOutputFile")).toString();
        QVERIFY(finalOutputFile.startsWith(initialOutputDirectory));
        QVERIFY(plannedOutputFile.startsWith(initialOutputDirectory));
        QVERIFY(finalOutputFile.endsWith(QStringLiteral(".mp3")));
        QVERIFY(plannedOutputFile.endsWith(QStringLiteral(".mp3")));
        QVERIFY(QFileInfo::exists(finalOutputFile));
        QVERIFY(stagingDirectory.isEmpty());
        QVERIFY(stagingOutputFile.isEmpty());
    }

    QCOMPARE(service.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 2);
    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 0);
    QVERIFY(service.finalSummary().value(QStringLiteral("orderedResultFiles")).toStringList()
                .constFirst()
                .startsWith(initialOutputDirectory));
    QVERIFY(QDir(changedOutputDirectory).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
}

void YtDlpImportServiceTest::parallelImportAvoidsDuplicateOutputPathWrites()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("parallel-duplicate-output-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"parallel-duplicate-output\","
                          "\"title\":\"Parallel Duplicate Output\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=parallel-duplicate-output\","
                          "\"entries\":["
                          "{\"id\":\"dup-a\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/tag-alpha\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"dup-b\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/tag-beta\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createTaggedParallelImportScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    const QString downloadDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads"));
    QVERIFY(QDir().mkpath(downloadDirectory));

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(downloadDirectory);
    service.setSelectedFormat(QStringLiteral("opus"));
    service.setNamingPolicy(QStringLiteral("title-only"));
    service.setParallelDownloads(2);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=parallel-duplicate-output")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    const QVariantList plannedItems = service.items();
    QCOMPARE(plannedItems.size(), 2);
    const QString firstPlannedPath =
        plannedItems.at(0).toMap().value(QStringLiteral("plannedOutputFile")).toString();
    const QString secondPlannedPath =
        plannedItems.at(1).toMap().value(QStringLiteral("plannedOutputFile")).toString();
    QVERIFY(firstPlannedPath.endsWith(QStringLiteral("Duplicate Track.opus")));
    QVERIFY(secondPlannedPath.endsWith(QStringLiteral("Duplicate Track (2).opus")));
    QVERIFY(firstPlannedPath != secondPlannedPath);

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 7000);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    QSet<QString> finalOutputPaths;
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
        const QString finalOutputFile = item.value(QStringLiteral("finalOutputFile")).toString();
        QVERIFY(!finalOutputFile.isEmpty());
        QVERIFY(QFileInfo::exists(finalOutputFile));
        QVERIFY(!finalOutputPaths.contains(finalOutputFile));
        finalOutputPaths.insert(finalOutputFile);
    }
    QCOMPARE(finalOutputPaths.size(), 2);

    const QStringList orderedResultFiles =
        service.finalSummary().value(QStringLiteral("orderedResultFiles")).toStringList();
    QCOMPARE(orderedResultFiles.size(), 2);
    QCOMPARE(QSet<QString>(orderedResultFiles.begin(), orderedResultFiles.end()).size(), 2);
}

void YtDlpImportServiceTest::importPartialSuccessPreservesReadyFilesAndExplainsFailures()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("partial-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"partial123\","
                          "\"title\":\"Partial Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=partial123\","
                          "\"entries\":["
                          "{\"id\":\"ok-a\",\"title\":\"First OK\","
                          "\"webpage_url\":\"https://example.com/watch?v=ok-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"fail-b\",\"title\":\"Second Fail\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=partial123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    QSignalSpy readySpy(&service, &YtDlpImportService::playlistImportReady);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("outcomeKey")).toString(), QStringLiteral("partial-failed"));
    QCOMPARE(summary.value(QStringLiteral("succeededCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("failedCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("canceledCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("skippedCount")).toInt(), 0);
    QCOMPARE(summary.value(QStringLiteral("hasPartialSuccess")).toBool(), true);

    const QStringList orderedResultFiles =
        summary.value(QStringLiteral("orderedResultFiles")).toStringList();
    QCOMPARE(orderedResultFiles.size(), 1);
    QVERIFY(QFileInfo::exists(orderedResultFiles.constFirst()));

    const QVariantList problemItems = summary.value(QStringLiteral("problemItems")).toList();
    QCOMPARE(problemItems.size(), 1);
    const QVariantMap problem = problemItems.constFirst().toMap();
    QCOMPARE(problem.value(QStringLiteral("title")).toString(), QStringLiteral("Second Fail"));
    QCOMPARE(problem.value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QVERIFY(!problem.value(QStringLiteral("message")).toString().trimmed().isEmpty());
    QVERIFY(!problem.value(QStringLiteral("errorCategoryLabel")).toString().trimmed().isEmpty());

    QCOMPARE(readySpy.count(), 1);
    const QList<QVariant> readyArgs = readySpy.takeFirst();
    QCOMPARE(readyArgs.constFirst().toStringList(), orderedResultFiles);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
}

void YtDlpImportServiceTest::sourceQueueEditingReordersAndRemovesSourcesBeforeStart()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString singlePayloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString playlistPayloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath =
        createRoutedProbeScript(tempDir.path(), singlePayloadPath, playlistPayloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);

    QVERIFY(service.replaceSourcesFromVariantList(
                QVariantList{QStringLiteral("https://example.com/single"),
                             QStringLiteral("https://example.com/playlist"),
                             QStringLiteral("https://example.com/fail-me")})
                .value(QStringLiteral("ok")).toBool());
    QVERIFY(service.probeAllSources());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isProbing(), 5000);

    const QString firstSourceId = service.sourceIdAt(0);
    const QString secondSourceId = service.sourceIdAt(1);
    const QString thirdSourceId = service.sourceIdAt(2);
    QVERIFY(service.canMoveSourceDown(firstSourceId));
    QVERIFY(service.moveSourceDown(firstSourceId));
    QCOMPARE(service.sourceIdAt(1), firstSourceId);
    QCOMPARE(service.sourceIdAt(0), secondSourceId);

    QVERIFY(service.canRemoveSource(thirdSourceId));
    QVERIFY(service.removeSourceById(thirdSourceId));
    QCOMPARE(service.sources().size(), 2);
    QCOMPARE(service.clearFailedProbes(), 0);

    const QVariantList sources = service.sources();
    QCOMPARE(sources.at(0).toMap().value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("queuePosition")).toInt(), 0);
    QCOMPARE(sources.at(1).toMap().value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("queuePosition")).toInt(), 1);
    QCOMPARE(service.items().size(), 3);
}

void YtDlpImportServiceTest::retryFailedImportsDoesNotDuplicateQueueItems()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("partial-playlist-retry.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"retry123\","
                          "\"title\":\"Retry Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=retry123\","
                          "\"entries\":["
                          "{\"id\":\"ok-a\",\"title\":\"First OK\","
                          "\"webpage_url\":\"https://example.com/watch?v=ok-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"fail-b\",\"title\":\"Second Fail\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=retry123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantList firstRunItems = service.items();
    QCOMPARE(firstRunItems.size(), 2);
    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 1);

    const int retriedCount = service.retryFailedImports();
    QCOMPARE(retriedCount, 1);
    QCOMPARE(service.items().size(), 2);

    QVariantMap failedItem;
    QVariantMap succeededItem;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("title")).toString() == QStringLiteral("Second Fail")) {
            failedItem = item;
        } else if (item.value(QStringLiteral("title")).toString() == QStringLiteral("First OK")) {
            succeededItem = item;
        }
    }
    QVERIFY(!failedItem.isEmpty());
    QVERIFY(!succeededItem.isEmpty());
    QCOMPARE(failedItem.value(QStringLiteral("retryCount")).toInt(), 1);
    QVERIFY(!failedItem.value(QStringLiteral("finalResultState")).toMap().value(QStringLiteral("attemptHistory")).toList().isEmpty());

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantMap retrySummary = service.finalSummary();
    QCOMPARE(retrySummary.value(QStringLiteral("totalCount")).toInt(), 1);
    QCOMPARE(retrySummary.value(QStringLiteral("failedCount")).toInt(), 1);
    QCOMPARE(retrySummary.value(QStringLiteral("succeededCount")).toInt(), 0);
}

void YtDlpImportServiceTest::importCancelAndRestartClearsStaleState()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("probe.json"),
                      QByteArrayLiteral(
                          "{\"id\":\"cancel123\",\"title\":\"Cancelable Track\","
                          "\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/cancel-me\"}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/watch?v=cancel")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 2000);
    service.cancelImport();
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QCOMPARE(service.finalSummary().value(QStringLiteral("wasCanceled")).toBool(), true);
    QCOMPARE(service.items().size(), 1);
    QCOMPARE(service.items().constFirst().toMap().value(QStringLiteral("state")).toString(),
             QStringLiteral("canceled"));

    QVERIFY(!writeTextFile(tempDir.path(),
                           QStringLiteral("probe.json"),
                           QByteArrayLiteral(
                               "{\"id\":\"ok123\",\"title\":\"Restarted Track\","
                               "\"extractor_key\":\"Youtube\","
                               "\"webpage_url\":\"https://example.com/ok-me\"}"))
                 .isEmpty());

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/watch?v=ok")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    QCOMPARE(service.finalSummary().value(QStringLiteral("wasCanceled")).toBool(), false);
    QCOMPARE(service.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 1);
    QCOMPARE(service.finalSummary().value(QStringLiteral("canceledCount")).toInt(), 0);
    QCOMPARE(service.items().size(), 1);
    const QVariantMap restartedItem = service.items().constFirst().toMap();
    QCOMPARE(restartedItem.value(QStringLiteral("state")).toString(), QStringLiteral("succeeded"));
    QVERIFY(restartedItem.value(QStringLiteral("finalOutputFile")).toString().contains(
        QStringLiteral("Restarted Track.mp3")));
}

void YtDlpImportServiceTest::importPlanningAutoRenamesConflictsAndUsesStaging()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("duplicate-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"dup123\","
                          "\"title\":\"Duplicate Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=dup123\","
                          "\"entries\":["
                          "{\"id\":\"dup-a\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=dup-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"dup-b\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=dup-b\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    const QString downloadDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads"));
    QVERIFY(QDir().mkpath(downloadDirectory));

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(downloadDirectory);
    service.setSelectedFormat(QStringLiteral("opus"));
    service.setNamingPolicy(QStringLiteral("title-only"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=dup123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    const QVariantList plannedItems = service.items();
    QCOMPARE(plannedItems.size(), 2);

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantList items = service.items();
    QCOMPARE(items.size(), 2);

    QVariantMap conflictedItem;
    for (const QVariant &value : items) {
        const QVariantMap candidate = value.toMap();
        const QVariantMap conflict = candidate.value(QStringLiteral("conflictResolution")).toMap();
        if (conflict.value(QStringLiteral("hadConflict")).toBool()) {
            conflictedItem = candidate;
            break;
        }
    }
    QVERIFY(!conflictedItem.isEmpty());

    const QVariantMap firstConflict = conflictedItem.value(QStringLiteral("conflictResolution")).toMap();
    const QVariantMap firstPreview = conflictedItem.value(QStringLiteral("previewDiagnostics")).toMap();
    const QVariantMap formatPlan = firstPreview.value(QStringLiteral("formatPlan")).toMap();
    QCOMPARE(firstConflict.value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("auto-renamed"));
    QCOMPARE(firstConflict.value(QStringLiteral("collisionRuleKey")).toString(),
             QStringLiteral("queue-conflict"));
    QVERIFY(conflictedItem.value(QStringLiteral("plannedOutputFile")).toString().contains(
        QStringLiteral("Duplicate Track (2).opus")));
    QCOMPARE(firstPreview.value(QStringLiteral("finalizationStrategyKey")).toString(),
             QStringLiteral("temp-commit"));
    QCOMPARE(formatPlan.value(QStringLiteral("audioFormatArgument")).toString(),
             QStringLiteral("opus"));

    const QString argsLogPath = QDir(tempDir.path()).filePath(QStringLiteral("last-args.txt"));
    QFile argsLogFile(argsLogPath);
    QVERIFY(argsLogFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString argsLog = QString::fromUtf8(argsLogFile.readAll());
    QVERIFY(argsLog.contains(QStringLiteral("--extract-audio")));
    QVERIFY(argsLog.contains(QStringLiteral("--audio-format")));
    QVERIFY(argsLog.contains(QStringLiteral("--audio-quality")));
    QVERIFY(argsLog.contains(QStringLiteral("0")));
    const QDir stagingRoot(QDir(downloadDirectory).filePath(QStringLiteral(".waveflux-yt-dlp-staging")));
    if (stagingRoot.exists()) {
        QVERIFY(stagingRoot.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty());
    }
}

void YtDlpImportServiceTest::retryFailedImportPreservesExistingTargetConflictSemantics()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("retry-existing-target.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"retry-existing-target\","
                          "\"title\":\"Retry Existing Target\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=retry-existing-target\","
                          "\"entries\":["
                          "{\"id\":\"dup-a\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=ok-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"dup-b\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());

    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    const QString downloadDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads"));
    QVERIFY(QDir().mkpath(downloadDirectory));

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(downloadDirectory);
    service.setSelectedFormat(QStringLiteral("mp3"));
    service.setNamingPolicy(QStringLiteral("title-only"));
    service.setConflictPolicy(QStringLiteral("auto-rename"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=retry-existing-target")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    QVariantMap succeededItem;
    QVariantMap failedItem;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("state")).toString() == QStringLiteral("succeeded")) {
            succeededItem = item;
        } else if (item.value(QStringLiteral("state")).toString() == QStringLiteral("failed")) {
            failedItem = item;
        }
    }
    QVERIFY(!succeededItem.isEmpty());
    QVERIFY(!failedItem.isEmpty());
    const QString firstRunSucceededPath = succeededItem.value(QStringLiteral("finalOutputFile")).toString();
    QVERIFY(QFileInfo::exists(firstRunSucceededPath));
    QCOMPARE(failedItem.value(QStringLiteral("conflictResolution")).toMap().value(QStringLiteral("collisionRuleKey")).toString(),
             QStringLiteral("queue-conflict"));
    QVERIFY(failedItem.value(QStringLiteral("plannedOutputFile")).toString().contains(
        QStringLiteral("Duplicate Track (2).mp3")));

    QCOMPARE(service.retryFailedImports(), 1);

    QVariantMap retryItem;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("title")).toString() == QStringLiteral("Duplicate Track")
            && item.value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("includeInNextRun")).toBool()) {
            retryItem = item;
            break;
        }
    }
    QVERIFY(!retryItem.isEmpty());
    QCOMPARE(retryItem.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
    QVERIFY(QFileInfo::exists(firstRunSucceededPath));

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QCOMPARE(service.finalSummary().value(QStringLiteral("totalCount")).toInt(), 1);
    QCOMPARE(service.finalSummary().value(QStringLiteral("failedCount")).toInt(), 1);
    QVERIFY(QFileInfo::exists(firstRunSucceededPath));

    QVariantMap retriedFailedItem;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("state")).toString() == QStringLiteral("failed")) {
            retriedFailedItem = item;
            break;
        }
    }
    QVERIFY(!retriedFailedItem.isEmpty());
    QCOMPARE(retriedFailedItem.value(QStringLiteral("conflictResolution")).toMap().value(QStringLiteral("collisionRuleKey")).toString(),
             QStringLiteral("existing-target"));
    QCOMPARE(retriedFailedItem.value(QStringLiteral("conflictResolution")).toMap().value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("auto-renamed"));
    QVERIFY(retriedFailedItem.value(QStringLiteral("plannedOutputFile")).toString().contains(
        QStringLiteral("Duplicate Track (2).mp3")));
    QVERIFY(QFileInfo::exists(firstRunSucceededPath));
}

void YtDlpImportServiceTest::previewPlanningExplainsNamingAndConflictsBeforeStart()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("named-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"named123\","
                          "\"title\":\"Source Capsule\","
                          "\"playlist_title\":\"Source Capsule\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=named123\","
                          "\"entries\":["
                          "{\"id\":\"named-a\",\"title\":\"Episode One\","
                          "\"webpage_url\":\"https://example.com/watch?v=named-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"named-b\",\"title\":\"Episode One\","
                          "\"webpage_url\":\"https://example.com/watch?v=named-b\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    const QString downloadDirectory = QDir(tempDir.path()).filePath(QStringLiteral("downloads"));
    QVERIFY(QDir().mkpath(downloadDirectory));
    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(downloadDirectory);
    service.setNamingPolicy(QStringLiteral("source-title-entry-title"));
    service.setConflictPolicy(QStringLiteral("skip-on-conflict"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=named123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

    const QVariantList plannedItems = service.items();
    QCOMPARE(plannedItems.size(), 2);
    for (const QVariant &value : plannedItems) {
        const QVariantMap item = value.toMap();
        const QVariantMap diagnostics = item.value(QStringLiteral("previewDiagnostics")).toMap();
        const QVariantMap conflict = item.value(QStringLiteral("conflictResolution")).toMap();
        QVERIFY(item.value(QStringLiteral("plannedOutputFile")).toString().contains(
            QStringLiteral("Source Capsule - Episode One.mp3")));
        QCOMPARE(diagnostics.value(QStringLiteral("appliedNamingPolicy")).toString(),
                 QStringLiteral("source-title-entry-title"));
        QCOMPARE(diagnostics.value(QStringLiteral("conflictPolicy")).toString(),
                 QStringLiteral("skip-on-conflict"));
        QVERIFY(conflict.contains(QStringLiteral("resolutionKey")));
    }

    const QVariantMap firstItem = plannedItems.constFirst().toMap();
    const QVariantMap secondItem = plannedItems.constLast().toMap();
    QCOMPARE(firstItem.value(QStringLiteral("previewDiagnostics")).toMap().value(QStringLiteral("collisionRuleKey")).toString(),
             QStringLiteral("none"));
    QCOMPARE(firstItem.value(QStringLiteral("previewDiagnostics")).toMap().value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("planned"));
    QCOMPARE(secondItem.value(QStringLiteral("previewDiagnostics")).toMap().value(QStringLiteral("collisionRuleKey")).toString(),
             QStringLiteral("queue-conflict"));
    QCOMPARE(secondItem.value(QStringLiteral("previewDiagnostics")).toMap().value(QStringLiteral("resolutionKey")).toString(),
             QStringLiteral("skip-on-conflict"));
}

void YtDlpImportServiceTest::importPlanningRespectsSkipAndFailConflictPolicies()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("duplicate-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"dup-stage7\","
                          "\"title\":\"Duplicate Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=dup-stage7\","
                          "\"entries\":["
                          "{\"id\":\"dup-a\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=dup-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"dup-b\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=dup-b\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    {
        const QString skipDirectory = QDir(tempDir.path()).filePath(QStringLiteral("skip-downloads"));
        QVERIFY(QDir().mkpath(skipDirectory));

        YtDlpImportService skipService;
        skipService.setAppSettingsManager(&settings);
        skipService.setOutputDirectory(skipDirectory);
        skipService.setNamingPolicy(QStringLiteral("title-only"));
        skipService.setConflictPolicy(QStringLiteral("skip-on-conflict"));

        QVERIFY(skipService.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=dup-stage7")));
        QTRY_VERIFY_WITH_TIMEOUT(skipService.hasProbeResult(), 5000);
        QVERIFY(skipService.startImport());
        QTRY_VERIFY_WITH_TIMEOUT(!skipService.isRunning(), 5000);

        QCOMPARE(skipService.finalSummary().value(QStringLiteral("skippedCount")).toInt(), 1);
        QCOMPARE(skipService.finalSummary().value(QStringLiteral("failedCount")).toInt(), 0);
        QCOMPARE(skipService.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 1);

        QVariantMap skippedItem;
        for (const QVariant &value : skipService.items()) {
            const QVariantMap item = value.toMap();
            if (item.value(QStringLiteral("state")).toString() == QStringLiteral("skipped")) {
                skippedItem = item;
                break;
            }
        }
        QVERIFY(!skippedItem.isEmpty());
        QCOMPARE(skippedItem.value(QStringLiteral("failureType")).toString(),
                 QStringLiteral("output"));
        QCOMPARE(skippedItem.value(QStringLiteral("conflictResolution")).toMap().value(QStringLiteral("resolutionKey")).toString(),
                 QStringLiteral("skip-on-conflict"));
        QCOMPARE(skipService.finalSummary().value(QStringLiteral("problemItems")).toList().constFirst().toMap()
                     .value(QStringLiteral("errorCategory")).toString(),
                 QStringLiteral("output"));
    }

    {
        const QString failDirectory = QDir(tempDir.path()).filePath(QStringLiteral("fail-downloads"));
        QVERIFY(QDir().mkpath(failDirectory));

        YtDlpImportService failService;
        failService.setAppSettingsManager(&settings);
        failService.setOutputDirectory(failDirectory);
        failService.setNamingPolicy(QStringLiteral("title-only"));
        failService.setConflictPolicy(QStringLiteral("fail-on-conflict"));

        QVERIFY(failService.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=dup-stage7")));
        QTRY_VERIFY_WITH_TIMEOUT(failService.hasProbeResult(), 5000);
        QVERIFY(failService.startImport());
        QTRY_VERIFY_WITH_TIMEOUT(!failService.isRunning(), 5000);

        QCOMPARE(failService.finalSummary().value(QStringLiteral("failedCount")).toInt(), 1);
        QCOMPARE(failService.finalSummary().value(QStringLiteral("skippedCount")).toInt(), 0);
        QCOMPARE(failService.finalSummary().value(QStringLiteral("succeededCount")).toInt(), 1);

        QVariantMap failedItem;
        for (const QVariant &value : failService.items()) {
            const QVariantMap item = value.toMap();
            if (item.value(QStringLiteral("state")).toString() == QStringLiteral("failed")) {
                failedItem = item;
                break;
            }
        }
        QVERIFY(!failedItem.isEmpty());
        QCOMPARE(failedItem.value(QStringLiteral("retryEligibility")).toString(),
                 QStringLiteral("allowed"));
        QCOMPARE(failedItem.value(QStringLiteral("failureType")).toString(),
                 QStringLiteral("output"));
        QCOMPARE(failedItem.value(QStringLiteral("conflictResolution")).toMap().value(QStringLiteral("resolutionKey")).toString(),
                 QStringLiteral("fail-on-conflict"));
    }
}

void YtDlpImportServiceTest::finalReportCapturesNotProbedConflictBlockedAndReopens()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("duplicate-playlist.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"report-stage8\","
                          "\"title\":\"Duplicate Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=report-stage8\","
                          "\"entries\":["
                          "{\"id\":\"dup-a\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=dup-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"dup-b\",\"title\":\"Duplicate Track\","
                          "\"webpage_url\":\"https://example.com/watch?v=dup-b\","
                          "\"playlist_index\":2,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    service.setNamingPolicy(QStringLiteral("title-only"));
    service.setConflictPolicy(QStringLiteral("skip-on-conflict"));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=report-stage8")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    const QVariantMap appendResult =
        service.appendSourceUrl(QStringLiteral("https://example.com/pending-source"));
    QVERIFY(appendResult.value(QStringLiteral("ok")).toBool());

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantMap summary = service.finalSummary();
    QCOMPARE(summary.value(QStringLiteral("notProbedCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("conflictBlockedCount")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("importedCount")).toInt(), 1);
    QVERIFY(!summary.value(QStringLiteral("jobId")).toString().trimmed().isEmpty());
    QVERIFY(!service.completedReports().isEmpty());
    QCOMPARE(service.latestCompletedReport().value(QStringLiteral("jobId")).toString(),
             summary.value(QStringLiteral("jobId")).toString());

    bool sawSourceProblem = false;
    for (const QVariant &value : summary.value(QStringLiteral("problemItems")).toList()) {
        const QVariantMap problem = value.toMap();
        if (problem.value(QStringLiteral("reportKind")).toString() == QStringLiteral("source")) {
            sawSourceProblem = true;
            QCOMPARE(problem.value(QStringLiteral("state")).toString(),
                     QStringLiteral("not-probed"));
        }
    }
    QVERIFY(sawSourceProblem);

    const QString reportText = service.currentReportText();
    QVERIFY(reportText.contains(QStringLiteral("Not probed: 1")));
    QVERIFY(reportText.contains(QStringLiteral("Conflict blocked: 1")));

    const QString reportJobId = summary.value(QStringLiteral("jobId")).toString();
    service.clear();
    QVERIFY(service.finalSummary().isEmpty());
    QVERIFY(!service.completedReports().isEmpty());
    QVERIFY(service.reopenCompletedReport(reportJobId));
    QCOMPARE(service.finalSummary().value(QStringLiteral("jobId")).toString(), reportJobId);
    QCOMPARE(service.finalSummary().value(QStringLiteral("notProbedCount")).toInt(), 1);
}

void YtDlpImportServiceTest::retrySelectedItemsRetriesSubsetOnly()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("retry-subset.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"retry-subset\","
                          "\"title\":\"Retry Subset Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=retry-subset\","
                          "\"entries\":["
                          "{\"id\":\"ok-a\",\"title\":\"First OK\","
                          "\"webpage_url\":\"https://example.com/watch?v=ok-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"fail-b\",\"title\":\"Second Fail\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":2,\"availability\":\"public\"},"
                          "{\"id\":\"fail-c\",\"title\":\"Third Fail\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":3,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=retry-subset")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    QVariantList retryIds;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("title")).toString() == QStringLiteral("Second Fail")) {
            retryIds.push_back(item.value(QStringLiteral("itemId")).toString());
        }
    }
    QCOMPARE(retryIds.size(), 1);
    QCOMPARE(service.retrySelectedItemsById(retryIds), 1);
    QVERIFY(service.finalSummary().isEmpty());

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantMap retrySummary = service.finalSummary();
    QCOMPARE(retrySummary.value(QStringLiteral("totalCount")).toInt(), 1);
    QCOMPARE(retrySummary.value(QStringLiteral("failedCount")).toInt(), 1);
    QCOMPARE(retrySummary.value(QStringLiteral("succeededCount")).toInt(), 0);
}

void YtDlpImportServiceTest::retrySelectedItemsAfterParallelRunRetriesOnlyRequestedSubset()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("retry-subset-parallel.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"_type\":\"playlist\","
                          "\"id\":\"retry-subset-parallel\","
                          "\"title\":\"Retry Subset Parallel Playlist\","
                          "\"extractor_key\":\"YoutubeTab\","
                          "\"webpage_url\":\"https://example.com/playlist?list=retry-subset-parallel\","
                          "\"entries\":["
                          "{\"id\":\"ok-a\",\"title\":\"First OK\","
                          "\"webpage_url\":\"https://example.com/watch?v=ok-a\","
                          "\"playlist_index\":1,\"availability\":\"public\"},"
                          "{\"id\":\"fail-b\",\"title\":\"Second Fail\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":2,\"availability\":\"public\"},"
                          "{\"id\":\"fail-c\",\"title\":\"Third Fail\","
                          "\"webpage_url\":\"https://example.com/fail-me\","
                          "\"playlist_index\":3,\"availability\":\"public\"}"
                          "]"
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    service.setParallelDownloads(2);

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/playlist?list=retry-subset-parallel")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 7000);

    QVariantList retryIds;
    QString unselectedFailedItemId;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("title")).toString() == QStringLiteral("Second Fail")) {
            retryIds.push_back(item.value(QStringLiteral("itemId")).toString());
        } else if (item.value(QStringLiteral("title")).toString() == QStringLiteral("Third Fail")) {
            unselectedFailedItemId = item.value(QStringLiteral("itemId")).toString();
        }
    }
    QCOMPARE(retryIds.size(), 1);
    QVERIFY(!unselectedFailedItemId.isEmpty());

    QCOMPARE(service.retrySelectedItemsById(retryIds), 1);
    QVERIFY(service.finalSummary().isEmpty());

    bool sawSelectedPending = false;
    bool sawUnselectedExcluded = false;
    for (const QVariant &value : service.items()) {
        const QVariantMap item = value.toMap();
        const QString itemId = item.value(QStringLiteral("itemId")).toString();
        const QVariantMap queueMetadata = item.value(QStringLiteral("queueMetadata")).toMap();
        if (itemId == retryIds.constFirst().toString()) {
            QCOMPARE(item.value(QStringLiteral("state")).toString(), QStringLiteral("pending"));
            QCOMPARE(queueMetadata.value(QStringLiteral("includeInNextRun")).toBool(), true);
            sawSelectedPending = true;
        } else if (itemId == unselectedFailedItemId) {
            QCOMPARE(queueMetadata.value(QStringLiteral("includeInNextRun")).toBool(), false);
            sawUnselectedExcluded = true;
        }
    }
    QVERIFY(sawSelectedPending);
    QVERIFY(sawUnselectedExcluded);

    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);

    const QVariantMap retrySummary = service.finalSummary();
    QCOMPARE(retrySummary.value(QStringLiteral("totalCount")).toInt(), 1);
    QCOMPARE(retrySummary.value(QStringLiteral("failedCount")).toInt(), 1);
    QCOMPARE(retrySummary.value(QStringLiteral("succeededCount")).toInt(), 0);
}

void YtDlpImportServiceTest::restoresPersistedDraftAfterReopen()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/youtube_playlist.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);

    {
        YtDlpImportService service;
        service.setAppSettingsManager(&settings);
        service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
        service.setSelectedFormat(QStringLiteral("opus"));
        service.setNamingPolicy(QStringLiteral("source-title-entry-title"));
        service.setConflictPolicy(QStringLiteral("skip-on-conflict"));
        service.setParallelDownloads(4);

        QVERIFY(service.probeSourceUrl(QStringLiteral("https://www.youtube.com/playlist?list=pl123")));
        QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);

        const QVariantMap persistedDraft = settings.ytDlpImportDraft();
        QCOMPARE(persistedDraft.value(QStringLiteral("schema")).toString(),
                 QStringLiteral("waveflux.ytdlp-import.v2"));
        QCOMPARE(persistedDraft.value(QStringLiteral("sources")).toList().size(), 1);
        QCOMPARE(persistedDraft.value(QStringLiteral("items")).toList().size(), 2);
    }

    YtDlpImportService restored;
    restored.setAppSettingsManager(&settings);

    QCOMPARE(restored.outputDirectory(),
             QDir(tempDir.path()).filePath(QStringLiteral("downloads")));
    QCOMPARE(restored.selectedFormat(), QStringLiteral("opus"));
    QCOMPARE(restored.namingPolicy(), QStringLiteral("source-title-entry-title"));
    QCOMPARE(restored.conflictPolicy(), QStringLiteral("skip-on-conflict"));
    QCOMPARE(restored.parallelDownloads(), 4);
    QVERIFY(!restored.importJob().value(QStringLiteral("jobId")).toString().trimmed().isEmpty());
    QCOMPARE(restored.importJob().value(QStringLiteral("startedAtMs")).toLongLong(), 0);
    QCOMPARE(restored.importJob().value(QStringLiteral("finishedAtMs")).toLongLong(), 0);
    QCOMPARE(restored.sources().size(), 1);
    QCOMPARE(restored.items().size(), 2);
    QCOMPARE(restored.sources().constFirst().toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(restored.sources().constFirst().toMap().value(QStringLiteral("runtimeState")).toMap()
                 .value(QStringLiteral("isStale")).toBool(),
             false);
    QCOMPARE(restored.items().constFirst().toMap().value(QStringLiteral("state")).toString(),
             QStringLiteral("pending"));
    QVERIFY(restored.finalSummary().isEmpty());
    QVERIFY(!restored.isRunning());
    QVERIFY(!restored.isProbing());

    restored.clear();
    QCOMPARE(settings.ytDlpImportDraft(), QVariantMap());
}

void YtDlpImportServiceTest::restoringDraftDoesNotReviveActiveRuntimeState()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath =
        writeTextFile(tempDir.path(),
                      QStringLiteral("slow-single.json"),
                      QByteArrayLiteral(
                          "{"
                          "\"id\":\"slow-1\","
                          "\"title\":\"Slow Track\","
                          "\"extractor_key\":\"Youtube\","
                          "\"webpage_url\":\"https://example.com/cancel-me\","
                          "\"availability\":\"public\""
                          "}"));
    QVERIFY(!payloadPath.isEmpty());
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://example.com/watch?v=slow-1")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(service.isRunning(), 5000);

    const QVariantMap persistedDraft = settings.ytDlpImportDraft();
    QVERIFY(!persistedDraft.isEmpty());
    QCOMPARE(persistedDraft.value(QStringLiteral("jobMetadata")).toMap()
                 .value(QStringLiteral("startedAtMs")).toLongLong(),
             0);

    YtDlpImportService restored;
    restored.setAppSettingsManager(&settings);

    QVERIFY(!restored.isRunning());
    QVERIFY(!restored.isProbing());
    QVERIFY(restored.finalSummary().isEmpty());
    QCOMPARE(restored.importJob().value(QStringLiteral("startedAtMs")).toLongLong(), 0);
    QCOMPARE(restored.importJob().value(QStringLiteral("finishedAtMs")).toLongLong(), 0);
    QCOMPARE(restored.sources().size(), 1);
    QCOMPARE(restored.sources().constFirst().toMap().value(QStringLiteral("sourceStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(restored.items().size(), 1);
    QCOMPARE(restored.items().constFirst().toMap().value(QStringLiteral("state")).toString(),
             QStringLiteral("pending"));
    QCOMPARE(restored.items().constFirst().toMap().value(QStringLiteral("progress")).toDouble(), 0.0);
    QCOMPARE(restored.items().constFirst().toMap().value(QStringLiteral("finalOutputFile")).toString(),
             QString());

    service.cancelImport();
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
}

void YtDlpImportServiceTest::clearResetsCompletedImportState()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString payloadPath = testDataPath(QStringLiteral("yt_dlp/single_video.json"));
    const QString scriptPath = createFakeYtDlpScript(tempDir.path(), payloadPath);
    QVERIFY(!scriptPath.isEmpty());

    AppSettingsManager settings;
    settings.setYtDlpExecutablePath(scriptPath);
    settings.setFfmpegExecutablePath(scriptPath);

    YtDlpImportService service;
    service.setAppSettingsManager(&settings);
    service.setOutputDirectory(QDir(tempDir.path()).filePath(QStringLiteral("downloads")));

    QVERIFY(service.probeSourceUrl(QStringLiteral("https://youtu.be/vid123")));
    QTRY_VERIFY_WITH_TIMEOUT(service.hasProbeResult(), 5000);
    QVERIFY(service.startImport());
    QTRY_VERIFY_WITH_TIMEOUT(!service.isRunning(), 5000);
    QVERIFY(!service.finalSummary().isEmpty());
    QVERIFY(!service.items().isEmpty());

    service.clear();

    QVERIFY(!service.hasProbeResult());
    QVERIFY(service.items().isEmpty());
    QVERIFY(service.finalSummary().isEmpty());
    QVERIFY(service.lastError().isEmpty());
    QVERIFY(service.statusText().isEmpty());
    QCOMPARE(service.batchProgress(), 0.0);
}

QTEST_MAIN(YtDlpImportServiceTest)
#include "tst_YtDlpImportService.moc"
