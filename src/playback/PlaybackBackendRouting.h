#ifndef PLAYBACKBACKENDROUTING_H
#define PLAYBACKBACKENDROUTING_H

#include <QString>
#include <QStringList>
#include <QStringView>

namespace WaveFlux {

enum class PlaybackBackendKind {
    GStreamer,
    OpenMpt
};

QStringList trackerModuleExtensions();
QStringList trackerModuleGlobPatterns();
QStringList unsupportedTrackerModuleExtensions();
bool isTrackerModuleExtension(QStringView extension);
bool isUnsupportedTrackerModuleExtension(QStringView extension);
bool isTrackerModuleSource(const QString &source);
bool isRemoteTrackerModuleSource(const QString &source);
bool isUnsupportedTrackerModuleSource(const QString &source);
bool isMidiExtension(QStringView extension);
bool isMidiSource(const QString &source);
bool isSeekBlockedForSource(const QString &source);
PlaybackBackendKind preferredPlaybackBackendForSource(const QString &source);
QString playbackBackendKindName(PlaybackBackendKind backendKind);

} // namespace WaveFlux

#endif // PLAYBACKBACKENDROUTING_H
