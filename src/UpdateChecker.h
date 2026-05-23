#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QObject>
#include <QUrl>

class AppSettingsManager;
class QNetworkReply;

class UpdateChecker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY resultChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY resultChanged)
    Q_PROPERTY(QString latestTag READ latestTag NOTIFY resultChanged)
    Q_PROPERTY(QString releaseName READ releaseName NOTIFY resultChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY resultChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY resultChanged)
    Q_PROPERTY(QDateTime publishedAt READ publishedAt NOTIFY resultChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
    explicit UpdateChecker(AppSettingsManager *settings, QObject *parent = nullptr);

    bool checking() const { return m_checking; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString currentVersion() const;
    QString latestVersion() const { return m_latestVersion; }
    QString latestTag() const { return m_latestTag; }
    QString releaseName() const { return m_releaseName; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString releaseUrl() const { return m_releaseUrl; }
    QDateTime publishedAt() const { return m_publishedAt; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void checkNow(bool manual);
    Q_INVOKABLE void deferCurrentUpdate();
    Q_INVOKABLE void skipCurrentVersion();
    Q_INVOKABLE void openReleasePage();

    void scheduleStartupCheck();

#ifdef WAVEFLUX_UPDATECHECKER_TESTING
    void setApiUrlsForTesting(const QUrl &latestReleaseUrl, const QUrl &releasesUrl);
#endif

signals:
    void checkingChanged();
    void resultChanged();
    void errorChanged();
    void updateFound();
    void noUpdateAvailable();
    void checkFailed(const QString &message);

private:
    struct ReleaseInfo {
        QString tag;
        QString version;
        QString name;
        QString notes;
        QString url;
        QDateTime publishedAt;
        bool prerelease = false;
        bool draft = false;
    };

    enum class VersionRelation {
        Older,
        Equal,
        Newer,
        Unknown
    };

    void startRequest(bool manual);
    void finishRequest(QNetworkReply *reply);
    void failRequest(const QString &message);
    void completeRequest();
    void clearResult();
    void setChecking(bool checking);
    void setLastError(const QString &message);
    bool autoCheckAllowed() const;
    QNetworkRequest buildRequest() const;
    bool parseResponse(const QByteArray &payload, ReleaseInfo *release, QString *errorMessage) const;
    bool parseReleaseObject(const QJsonObject &object, ReleaseInfo *release, QString *errorMessage) const;
    bool shouldUseRelease(const ReleaseInfo &release) const;
    void applyReleaseResult(const ReleaseInfo &release);
    bool isSafeReleaseUrl(const QString &url) const;

    static QString normalizeVersionText(const QString &tagOrVersion);
    static VersionRelation compareVersions(const QString &available, const QString &current);
    static QString plainReleaseNotes(const QString &notes);

    AppSettingsManager *m_settings = nullptr;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_currentReply;
    QTimer m_timeoutTimer;
    QTimer m_startupCheckTimer;
    bool m_checking = false;
    bool m_currentCheckManual = false;
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QString m_latestTag;
    QString m_releaseName;
    QString m_releaseNotes;
    QString m_releaseUrl;
    QDateTime m_publishedAt;
    QString m_lastError;
    QUrl m_latestReleaseUrl;
    QUrl m_releasesUrl;
};

#endif // UPDATECHECKER_H
