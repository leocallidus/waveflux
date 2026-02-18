#include "PlaylistProfilesManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDateTime>
#include <QtGlobal>
#include <algorithm>

namespace {
constexpr int kMaxPlaylistCount = 200;
}

PlaylistProfilesManager::PlaylistProfilesManager(QObject *parent)
    : QObject(parent)
{
    ensureLoaded();
}

QVariantList PlaylistProfilesManager::listPlaylists() const
{
    QVariantList result;
    if (!ensureLoaded()) {
        return result;
    }

    result.reserve(m_profiles.size());
    for (const PlaylistProfile &profile : std::as_const(m_profiles)) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), profile.id);
        item.insert(QStringLiteral("name"), profile.name);
        item.insert(QStringLiteral("trackCount"), profile.tracks.size());
        item.insert(QStringLiteral("updatedAtMs"), profile.updatedAtMs);
        result.push_back(item);
    }
    return result;
}

int PlaylistProfilesManager::savePlaylist(const QString &name,
                                          const QVariantList &snapshot,
                                          int currentIndex)
{
    if (!ensureLoaded()) {
        return -1;
    }

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Playlist name is empty"));
        return -1;
    }

    const QVariantList tracks = sanitizeSnapshot(snapshot);
    const int boundedIndex = (currentIndex >= 0 && currentIndex < tracks.size()) ? currentIndex : -1;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    int profileIndex = findProfileIndexByName(trimmedName);
    if (profileIndex < 0) {
        if (m_profiles.size() >= kMaxPlaylistCount) {
            setLastError(QStringLiteral("Playlist storage limit reached"));
            return -1;
        }
        PlaylistProfile profile;
        profile.id = m_nextId++;
        profile.name = trimmedName;
        profile.tracks = tracks;
        profile.currentIndex = boundedIndex;
        profile.updatedAtMs = nowMs;
        m_profiles.push_back(std::move(profile));
        profileIndex = m_profiles.size() - 1;
    } else {
        PlaylistProfile &profile = m_profiles[profileIndex];
        profile.name = trimmedName;
        profile.tracks = tracks;
        profile.currentIndex = boundedIndex;
        profile.updatedAtMs = nowMs;
    }

    sortProfilesByUpdateTime();
    if (!persistToDisk()) {
        return -1;
    }

    const int savedIndex = findProfileIndexByName(trimmedName);
    if (savedIndex < 0) {
        setLastError(QStringLiteral("Failed to locate saved playlist"));
        return -1;
    }
    const int playlistId = m_profiles.at(savedIndex).id;
    setLastError(QString());
    emit playlistsChanged();
    return playlistId;
}

QVariantMap PlaylistProfilesManager::loadPlaylist(int playlistId) const
{
    QVariantMap result;
    if (!ensureLoaded()) {
        return result;
    }

    const int profileIndex = findProfileIndexById(playlistId);
    if (profileIndex < 0) {
        setLastError(QStringLiteral("Playlist not found"));
        return result;
    }

    const PlaylistProfile &profile = m_profiles.at(profileIndex);
    result.insert(QStringLiteral("id"), profile.id);
    result.insert(QStringLiteral("name"), profile.name);
    result.insert(QStringLiteral("tracks"), profile.tracks);
    result.insert(QStringLiteral("currentIndex"), profile.currentIndex);
    result.insert(QStringLiteral("updatedAtMs"), profile.updatedAtMs);
    setLastError(QString());
    return result;
}

bool PlaylistProfilesManager::updatePlaylist(int playlistId,
                                             const QString &name,
                                             const QVariantList &snapshot,
                                             int currentIndex)
{
    if (!ensureLoaded()) {
        return false;
    }

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Playlist name is empty"));
        return false;
    }

    const int profileIndex = findProfileIndexById(playlistId);
    if (profileIndex < 0) {
        setLastError(QStringLiteral("Playlist not found"));
        return false;
    }

    const int duplicateIndex = findProfileIndexByName(trimmedName);
    if (duplicateIndex >= 0 && duplicateIndex != profileIndex) {
        setLastError(QStringLiteral("Playlist with this name already exists"));
        return false;
    }

    const QVariantList tracks = sanitizeSnapshot(snapshot);
    const int boundedIndex = (currentIndex >= 0 && currentIndex < tracks.size()) ? currentIndex : -1;

    PlaylistProfile &profile = m_profiles[profileIndex];
    profile.name = trimmedName;
    profile.tracks = tracks;
    profile.currentIndex = boundedIndex;
    profile.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    sortProfilesByUpdateTime();

    if (!persistToDisk()) {
        return false;
    }

    setLastError(QString());
    emit playlistsChanged();
    return true;
}

bool PlaylistProfilesManager::renamePlaylist(int playlistId, const QString &newName)
{
    if (!ensureLoaded()) {
        return false;
    }

    const QString trimmedName = newName.trimmed();
    if (trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Playlist name is empty"));
        return false;
    }

    const int profileIndex = findProfileIndexById(playlistId);
    if (profileIndex < 0) {
        setLastError(QStringLiteral("Playlist not found"));
        return false;
    }

    const int duplicateIndex = findProfileIndexByName(trimmedName);
    if (duplicateIndex >= 0 && duplicateIndex != profileIndex) {
        setLastError(QStringLiteral("Playlist with this name already exists"));
        return false;
    }

    PlaylistProfile &profile = m_profiles[profileIndex];
    profile.name = trimmedName;
    profile.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    sortProfilesByUpdateTime();

    if (!persistToDisk()) {
        return false;
    }

    setLastError(QString());
    emit playlistsChanged();
    return true;
}

int PlaylistProfilesManager::duplicatePlaylist(int playlistId, const QString &newName)
{
    if (!ensureLoaded()) {
        return -1;
    }

    const int sourceIndex = findProfileIndexById(playlistId);
    if (sourceIndex < 0) {
        setLastError(QStringLiteral("Playlist not found"));
        return -1;
    }
    if (m_profiles.size() >= kMaxPlaylistCount) {
        setLastError(QStringLiteral("Playlist storage limit reached"));
        return -1;
    }

    const PlaylistProfile &source = m_profiles.at(sourceIndex);
    QString candidateName = newName.trimmed();
    if (candidateName.isEmpty()) {
        candidateName = source.name + QStringLiteral(" (copy)");
    }

    QString uniqueName = candidateName;
    int suffix = 2;
    while (findProfileIndexByName(uniqueName) >= 0) {
        uniqueName = QStringLiteral("%1 (%2)").arg(candidateName).arg(suffix);
        ++suffix;
    }

    PlaylistProfile profile;
    profile.id = m_nextId++;
    profile.name = uniqueName;
    profile.tracks = source.tracks;
    profile.currentIndex = source.currentIndex;
    profile.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_profiles.push_back(std::move(profile));
    sortProfilesByUpdateTime();

    if (!persistToDisk()) {
        return -1;
    }

    const int duplicatedIndex = findProfileIndexByName(uniqueName);
    if (duplicatedIndex < 0) {
        setLastError(QStringLiteral("Failed to locate duplicated playlist"));
        return -1;
    }

    setLastError(QString());
    emit playlistsChanged();
    return m_profiles.at(duplicatedIndex).id;
}

bool PlaylistProfilesManager::deletePlaylist(int playlistId)
{
    if (!ensureLoaded()) {
        return false;
    }

    const int profileIndex = findProfileIndexById(playlistId);
    if (profileIndex < 0) {
        setLastError(QStringLiteral("Playlist not found"));
        return false;
    }

    m_profiles.removeAt(profileIndex);
    if (!persistToDisk()) {
        return false;
    }

    setLastError(QString());
    emit playlistsChanged();
    return true;
}

bool PlaylistProfilesManager::ensureLoaded() const
{
    if (m_loaded) {
        return true;
    }
    return loadFromDisk();
}

bool PlaylistProfilesManager::loadFromDisk() const
{
    m_profiles.clear();
    m_nextId = 1;

    QFile file(storagePath());
    if (!file.exists()) {
        m_loaded = true;
        setLastError(QString());
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setLastError(QStringLiteral("Failed to open playlists storage"));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setLastError(QStringLiteral("Failed to parse playlists storage"));
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != kStorageVersion) {
        m_loaded = true;
        setLastError(QString());
        return true;
    }

    const QJsonArray playlistsArray = root.value(QStringLiteral("playlists")).toArray();
    int maxId = 0;
    for (const QJsonValue &value : playlistsArray) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const int id = object.value(QStringLiteral("id")).toInt(-1);
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        if (id <= 0 || name.isEmpty()) {
            continue;
        }

        QVariantList tracks;
        const QJsonArray tracksArray = object.value(QStringLiteral("tracks")).toArray();
        tracks.reserve(tracksArray.size());
        for (const QJsonValue &trackValue : tracksArray) {
            if (!trackValue.isObject()) {
                continue;
            }
            QVariantMap trackMap = trackValue.toObject().toVariantMap();
            QString filePath = trackMap.value(QStringLiteral("filePath")).toString().trimmed();
            if (filePath.isEmpty()) {
                filePath = trackMap.value(QStringLiteral("path")).toString().trimmed();
            }
            if (filePath.isEmpty()) {
                continue;
            }
            trackMap.insert(QStringLiteral("filePath"), filePath);
            tracks.push_back(trackMap);
        }

        PlaylistProfile profile;
        profile.id = id;
        profile.name = name;
        profile.tracks = std::move(tracks);
        profile.currentIndex = object.value(QStringLiteral("currentIndex")).toInt(-1);
        profile.updatedAtMs = static_cast<qint64>(
            object.value(QStringLiteral("updatedAtMs")).toDouble(0.0));
        if (profile.currentIndex < 0 || profile.currentIndex >= profile.tracks.size()) {
            profile.currentIndex = -1;
        }
        m_profiles.push_back(std::move(profile));
        maxId = qMax(maxId, id);
    }

    const int storedNextId = root.value(QStringLiteral("nextId")).toInt(maxId + 1);
    m_nextId = qMax(maxId + 1, storedNextId);
    sortProfilesByUpdateTime();
    m_loaded = true;
    setLastError(QString());
    return true;
}

bool PlaylistProfilesManager::persistToDisk() const
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), kStorageVersion);
    root.insert(QStringLiteral("nextId"), qMax(1, m_nextId));

    QJsonArray playlistsArray;
    for (const PlaylistProfile &profile : std::as_const(m_profiles)) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), profile.id);
        object.insert(QStringLiteral("name"), profile.name);
        object.insert(QStringLiteral("currentIndex"), profile.currentIndex);
        object.insert(QStringLiteral("updatedAtMs"), static_cast<double>(profile.updatedAtMs));

        QJsonArray tracksArray;
        for (const QVariant &trackValue : profile.tracks) {
            const QVariantMap trackMap = trackValue.toMap();
            if (trackMap.isEmpty()) {
                continue;
            }
            tracksArray.push_back(QJsonObject::fromVariantMap(trackMap));
        }
        object.insert(QStringLiteral("tracks"), tracksArray);
        playlistsArray.push_back(object);
    }
    root.insert(QStringLiteral("playlists"), playlistsArray);

    const QString path = storagePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setLastError(QStringLiteral("Failed to open playlists storage for writing"));
        return false;
    }

    const QJsonDocument document(root);
    file.write(document.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setLastError(QStringLiteral("Failed to persist playlists storage"));
        return false;
    }

    return true;
}

QString PlaylistProfilesManager::storagePath() const
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + QStringLiteral("/playlist_profiles.json");
}

int PlaylistProfilesManager::findProfileIndexById(int playlistId) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == playlistId) {
            return i;
        }
    }
    return -1;
}

int PlaylistProfilesManager::findProfileIndexByName(const QString &name) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (QString::compare(m_profiles.at(i).name, name, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

void PlaylistProfilesManager::sortProfilesByUpdateTime() const
{
    std::sort(m_profiles.begin(), m_profiles.end(), [](const PlaylistProfile &a, const PlaylistProfile &b) {
        if (a.updatedAtMs == b.updatedAtMs) {
            return a.id > b.id;
        }
        return a.updatedAtMs > b.updatedAtMs;
    });
}

QVariantList PlaylistProfilesManager::sanitizeSnapshot(const QVariantList &snapshot)
{
    QVariantList tracks;
    tracks.reserve(snapshot.size());
    for (const QVariant &value : snapshot) {
        QVariantMap trackMap = value.toMap();
        QString filePath = trackMap.value(QStringLiteral("filePath")).toString().trimmed();
        if (filePath.isEmpty()) {
            filePath = trackMap.value(QStringLiteral("path")).toString().trimmed();
        }
        if (filePath.isEmpty()) {
            continue;
        }
        trackMap.insert(QStringLiteral("filePath"), filePath);
        tracks.push_back(trackMap);
    }
    return tracks;
}

void PlaylistProfilesManager::setLastError(const QString &error) const
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit const_cast<PlaylistProfilesManager *>(this)->lastErrorChanged();
}
