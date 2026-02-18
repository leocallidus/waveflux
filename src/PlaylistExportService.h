#ifndef PLAYLISTEXPORTSERVICE_H
#define PLAYLISTEXPORTSERVICE_H

#include <QObject>
#include <QStringList>
#include <QUrl>

#include "TrackModel.h"

class PlaylistExportService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit PlaylistExportService(QObject *parent = nullptr);

    void initialize(TrackModel *trackModel);
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE bool exportToFile(const QUrl &targetUrl);
    Q_INVOKABLE bool exportSelectedToFile(const QUrl &targetUrl, const QStringList &filePaths);

signals:
    void lastErrorChanged();
    void exportCompleted(bool success, const QString &message);

private:
    enum class ExportFormat {
        M3U,
        JSON
    };

    static ExportFormat detectFormat(const QString &path);
    static QString ensureExtension(const QString &path, ExportFormat format);
    static QString generateM3U(const QVector<Track> &tracks);
    static QString generateJSON(const QVector<Track> &tracks);
    bool exportTracksToFile(const QVector<Track> &tracks, const QUrl &targetUrl);
    void setLastError(const QString &error);

    TrackModel *m_trackModel = nullptr;
    QString m_lastError;
};

#endif // PLAYLISTEXPORTSERVICE_H
