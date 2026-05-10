#include "playback/PlaybackBackendRouting.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

namespace {
bool isLikelyWindowsAbsolutePath(const QString &path)
{
    return path.size() >= 3
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('\\') || path.at(2) == QLatin1Char('/'));
}

QString localPathFromAudioSource(const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    if (QDir::isAbsolutePath(normalized) || isLikelyWindowsAbsolutePath(normalized)) {
        return QDir::cleanPath(normalized);
    }

    const QUrl parsed(normalized);
    if (parsed.isValid() && parsed.isLocalFile()) {
        return QDir::cleanPath(parsed.toLocalFile());
    }

    if (parsed.isValid() && !parsed.scheme().isEmpty()) {
        return {};
    }

    return QDir::cleanPath(normalized);
}

const QSet<QString> &trackerModuleExtensionSet()
{
    static const QSet<QString> extensions = {
        QStringLiteral("669"),
        QStringLiteral("amf"),
        QStringLiteral("dmf"),
        QStringLiteral("mod"),
        QStringLiteral("xm"),
        QStringLiteral("s3m"),
        QStringLiteral("it")
    };
    return extensions;
}

const QSet<QString> &unsupportedTrackerModuleExtensionSet()
{
    static const QSet<QString> extensions = {
        QStringLiteral("mptm"),
        QStringLiteral("med"),
        QStringLiteral("umx"),
        QStringLiteral("ahx")
    };
    return extensions;
}

const QSet<QString> &midiExtensionSet()
{
    static const QSet<QString> extensions = {
        QStringLiteral("mid"),
        QStringLiteral("midi")
    };
    return extensions;
}
} // namespace

namespace WaveFlux {

QStringList trackerModuleExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("669"),
        QStringLiteral("amf"),
        QStringLiteral("dmf"),
        QStringLiteral("mod"),
        QStringLiteral("xm"),
        QStringLiteral("s3m"),
        QStringLiteral("it")
    };
    return extensions;
}

QStringList unsupportedTrackerModuleExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("mptm"),
        QStringLiteral("med"),
        QStringLiteral("umx"),
        QStringLiteral("ahx")
    };
    return extensions;
}

QStringList trackerModuleGlobPatterns()
{
    static const QStringList patterns = {
        QStringLiteral("*.669"),
        QStringLiteral("*.amf"),
        QStringLiteral("*.dmf"),
        QStringLiteral("*.mod"),
        QStringLiteral("*.xm"),
        QStringLiteral("*.s3m"),
        QStringLiteral("*.it")
    };
    return patterns;
}

bool isTrackerModuleExtension(QStringView extension)
{
    return trackerModuleExtensionSet().contains(extension.toString().trimmed().toLower());
}

bool isUnsupportedTrackerModuleExtension(QStringView extension)
{
    return unsupportedTrackerModuleExtensionSet().contains(
        extension.toString().trimmed().toLower());
}

bool isMidiExtension(QStringView extension)
{
    return midiExtensionSet().contains(extension.toString().trimmed().toLower());
}

bool isTrackerModuleSource(const QString &source)
{
    const QString localPath = localPathFromAudioSource(source);
    if (localPath.isEmpty()) {
        return false;
    }

    return isTrackerModuleExtension(QFileInfo(localPath).suffix());
}

bool isRemoteTrackerModuleSource(const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    const QUrl parsed(normalized);
    if (!parsed.isValid() || parsed.isLocalFile()) {
        return false;
    }

    const QString scheme = parsed.scheme().trimmed().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return false;
    }

    return isTrackerModuleExtension(QFileInfo(parsed.path()).suffix());
}

bool isUnsupportedTrackerModuleSource(const QString &source)
{
    const QString localPath = localPathFromAudioSource(source);
    if (!localPath.isEmpty()) {
        return isUnsupportedTrackerModuleExtension(QFileInfo(localPath).suffix());
    }

    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    const QUrl parsed(normalized);
    if (!parsed.isValid()) {
        return false;
    }

    return isUnsupportedTrackerModuleExtension(QFileInfo(parsed.path()).suffix());
}

bool isMidiSource(const QString &source)
{
    const QString localPath = localPathFromAudioSource(source);
    if (!localPath.isEmpty()) {
        return isMidiExtension(QFileInfo(localPath).suffix());
    }

    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    const QUrl parsed(normalized);
    if (!parsed.isValid()) {
        return false;
    }

    return isMidiExtension(QFileInfo(parsed.path()).suffix());
}

bool isSeekBlockedForSource(const QString &source)
{
    const QString localPath = localPathFromAudioSource(source);
    if (localPath.isEmpty()) {
        return false;
    }

    const QString extension = QFileInfo(localPath).suffix().trimmed().toLower();
    return extension == QStringLiteral("stm");
}

PlaybackBackendKind preferredPlaybackBackendForSource(const QString &source)
{
    if (isTrackerModuleSource(source)) {
        return PlaybackBackendKind::OpenMpt;
    }

    return PlaybackBackendKind::GStreamer;
}

QString playbackBackendKindName(PlaybackBackendKind backendKind)
{
    switch (backendKind) {
    case PlaybackBackendKind::OpenMpt:
        return QStringLiteral("openmpt");
    case PlaybackBackendKind::GStreamer:
    default:
        return QStringLiteral("gstreamer");
    }
}

} // namespace WaveFlux
