#ifndef PLAYLISTPROFILESMANAGER_H
#define PLAYLISTPROFILESMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QVector>

class PlaylistProfilesManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit PlaylistProfilesManager(QObject *parent = nullptr);

    QString lastError() const { return m_lastError; }

    Q_INVOKABLE QVariantList listPlaylists() const;
    Q_INVOKABLE int savePlaylist(const QString &name,
                                 const QVariantList &snapshot,
                                 int currentIndex = -1);
    Q_INVOKABLE QVariantMap loadPlaylist(int playlistId) const;
    Q_INVOKABLE bool updatePlaylist(int playlistId,
                                    const QString &name,
                                    const QVariantList &snapshot,
                                    int currentIndex = -1);
    Q_INVOKABLE bool renamePlaylist(int playlistId, const QString &newName);
    Q_INVOKABLE int duplicatePlaylist(int playlistId, const QString &newName = QString());
    Q_INVOKABLE bool deletePlaylist(int playlistId);

signals:
    void playlistsChanged();
    void lastErrorChanged();

private:
    struct PlaylistProfile {
        int id = -1;
        QString name;
        QVariantList tracks;
        int currentIndex = -1;
        qint64 updatedAtMs = 0;
    };

    bool ensureLoaded() const;
    bool loadFromDisk() const;
    bool persistToDisk() const;
    QString storagePath() const;
    int findProfileIndexById(int playlistId) const;
    int findProfileIndexByName(const QString &name) const;
    void sortProfilesByUpdateTime() const;
    static QVariantList sanitizeSnapshot(const QVariantList &snapshot);
    void setLastError(const QString &error) const;

    mutable bool m_loaded = false;
    mutable QVector<PlaylistProfile> m_profiles;
    mutable int m_nextId = 1;
    mutable QString m_lastError;

    static constexpr int kStorageVersion = 1;
};

#endif // PLAYLISTPROFILESMANAGER_H
