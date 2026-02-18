#include "PlaylistExportService.h"

#include "TrackModel.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

PlaylistExportService::PlaylistExportService(QObject *parent)
    : QObject(parent)
{
}

void PlaylistExportService::initialize(TrackModel *trackModel)
{
    m_trackModel = trackModel;
}

bool PlaylistExportService::exportToFile(const QUrl &targetUrl)
{
    if (!m_trackModel) {
        setLastError(QStringLiteral("Playlist export service is not initialized."));
        emit exportCompleted(false, m_lastError);
        return false;
    }

    return exportTracksToFile(m_trackModel->tracks(), targetUrl);
}

bool PlaylistExportService::exportSelectedToFile(const QUrl &targetUrl, const QStringList &filePaths)
{
    if (!m_trackModel) {
        setLastError(QStringLiteral("Playlist export service is not initialized."));
        emit exportCompleted(false, m_lastError);
        return false;
    }

    if (filePaths.isEmpty()) {
        setLastError(QStringLiteral("No tracks selected."));
        emit exportCompleted(false, m_lastError);
        return false;
    }

    QHash<QString, Track> tracksByPath;
    tracksByPath.reserve(m_trackModel->tracks().size());
    for (const Track &track : m_trackModel->tracks()) {
        tracksByPath.insert(track.filePath, track);
    }

    QVector<Track> selectedTracks;
    selectedTracks.reserve(filePaths.size());
    QSet<QString> seenPaths;
    seenPaths.reserve(filePaths.size());

    for (const QString &filePath : filePaths) {
        if (filePath.isEmpty() || seenPaths.contains(filePath)) {
            continue;
        }
        const auto it = tracksByPath.constFind(filePath);
        if (it == tracksByPath.constEnd()) {
            continue;
        }
        selectedTracks.push_back(it.value());
        seenPaths.insert(filePath);
    }

    return exportTracksToFile(selectedTracks, targetUrl);
}

bool PlaylistExportService::exportTracksToFile(const QVector<Track> &tracks, const QUrl &targetUrl)
{
    if (tracks.isEmpty()) {
        setLastError(QStringLiteral("Playlist is empty."));
        emit exportCompleted(false, m_lastError);
        return false;
    }

    QString filePath = targetUrl.isLocalFile() ? targetUrl.toLocalFile() : targetUrl.toString();
    if (filePath.isEmpty()) {
        setLastError(QStringLiteral("No output path selected."));
        emit exportCompleted(false, m_lastError);
        return false;
    }

    const ExportFormat format = detectFormat(filePath);
    filePath = ensureExtension(filePath, format);

    const QString content = format == ExportFormat::JSON
                                ? generateJSON(tracks)
                                : generateM3U(tracks);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setLastError(QStringLiteral("Failed to open output file for writing."));
        emit exportCompleted(false, m_lastError);
        return false;
    }

    file.write(content.toUtf8());
    file.close();

    setLastError(QString());
    emit exportCompleted(true, QStringLiteral("Playlist exported to %1").arg(filePath));
    return true;
}

PlaylistExportService::ExportFormat PlaylistExportService::detectFormat(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("json")) {
        return ExportFormat::JSON;
    }
    return ExportFormat::M3U;
}

QString PlaylistExportService::ensureExtension(const QString &path, ExportFormat format)
{
    QFileInfo info(path);
    const QString suffix = info.suffix().toLower();

    if (format == ExportFormat::JSON) {
        if (suffix == QStringLiteral("json")) {
            return path;
        }
        return path + QStringLiteral(".json");
    }

    if (suffix == QStringLiteral("m3u") || suffix == QStringLiteral("m3u8")) {
        return path;
    }
    return path + QStringLiteral(".m3u");
}

QString PlaylistExportService::generateM3U(const QVector<Track> &tracks)
{
    QStringList lines;
    lines.reserve(tracks.size() * 3 + 1);
    lines << QStringLiteral("#EXTM3U");

    for (const Track &track : tracks) {
        const int durationSec = track.duration > 0 ? static_cast<int>(track.duration / 1000) : 0;
        QString display = track.title.isEmpty() ? QFileInfo(track.filePath).completeBaseName() : track.title;
        if (!track.artist.isEmpty()) {
            display = track.artist + QStringLiteral(" - ") + display;
        }

        lines << QStringLiteral("#EXTINF:%1,%2").arg(durationSec).arg(display);
        lines << track.filePath;
        lines << QString();
    }

    return lines.join(QLatin1Char('\n'));
}

QString PlaylistExportService::generateJSON(const QVector<Track> &tracks)
{
    QJsonArray tracksArray;
    for (const Track &track : tracks) {
        QJsonObject obj;
        obj["path"] = track.filePath;
        obj["title"] = track.title;
        obj["artist"] = track.artist;
        obj["album"] = track.album;
        obj["duration"] = static_cast<double>(track.duration);
        obj["format"] = track.format;
        obj["bitrate"] = track.bitrate;
        obj["sampleRate"] = track.sampleRate;
        obj["bitDepth"] = track.bitDepth;
        obj["bpm"] = track.bpm;
        obj["addedAt"] = static_cast<double>(track.addedAt);
        tracksArray.append(obj);
    }

    QJsonObject root;
    root["version"] = 1;
    root["generator"] = QStringLiteral("WaveFlux");
    root["created"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root["trackCount"] = tracks.size();
    root["tracks"] = tracksArray;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void PlaylistExportService::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}
