#ifndef LIBRARYREPOSITORY_H
#define LIBRARYREPOSITORY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

class QThread;

struct LibraryTrackUpsertData {
    QString filePath;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    QString format;
    int bitrate = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    QString albumArtUri;
    qint64 addedAtMs = 0;
};

struct LibraryPlaybackEventData {
    QString filePath;
    qint64 startedAtMs = 0;
    qint64 endedAtMs = 0;
    qint64 listenMs = 0;
    double completionRatio = 0.0;
    QString source;
    bool wasSkipped = false;
    bool wasCompleted = false;
    QString sessionId;
};

class LibraryRepository : public QObject
{
    Q_OBJECT

public:
    explicit LibraryRepository(QObject *parent = nullptr);
    ~LibraryRepository() override;

    void configure(bool enabled, const QString &databasePath);
    bool isEnabled() const { return m_enabled; }
    QString databasePath() const { return m_databasePath; }
    QString lastError() const { return m_lastError; }

    void enqueueUpsertTrack(const LibraryTrackUpsertData &track);
    void enqueueUpsertTracks(const QVector<LibraryTrackUpsertData> &tracks);
    void enqueueSoftDeleteTrack(const QString &filePath);
    void enqueueSoftDeleteAll();
    void enqueueReconcileFolderScan(const QString &folderPath, const QStringList &presentFilePaths);
    void enqueuePlaybackEvents(const QVector<LibraryPlaybackEventData> &events);
    bool writePlaybackEventsBlocking(const QVector<LibraryPlaybackEventData> &events);

signals:
    void errorOccurred(const QString &operation, const QString &message);

private:
    class Worker;

    bool canEnqueue() const;
    void reportError(const QString &operation, const QString &message);

    Worker *m_worker = nullptr;
    QThread *m_thread = nullptr;
    bool m_enabled = false;
    QString m_databasePath;
    QString m_lastError;
};

#endif // LIBRARYREPOSITORY_H
