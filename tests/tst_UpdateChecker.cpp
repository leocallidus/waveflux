#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "AppSettingsManager.h"
#include "UpdateChecker.h"

namespace {

void clearSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

QByteArray releaseJson(const QString &tag,
                       bool prerelease = false,
                       const QString &name = QString(),
                       const QString &body = QStringLiteral("- Fixed playback\n- Improved updates"))
{
    QJsonObject object;
    object.insert(QStringLiteral("tag_name"), tag);
    object.insert(QStringLiteral("name"), name.isEmpty() ? QStringLiteral("WaveFlux %1").arg(tag) : name);
    object.insert(QStringLiteral("body"), body);
    object.insert(QStringLiteral("html_url"),
                  QStringLiteral("https://github.com/leocallidus/waveflux/releases/tag/%1").arg(tag));
    object.insert(QStringLiteral("published_at"), QStringLiteral("2026-05-22T08:15:30Z"));
    object.insert(QStringLiteral("prerelease"), prerelease);
    object.insert(QStringLiteral("draft"), false);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray releasesJson(const QList<QByteArray> &objects)
{
    QJsonArray array;
    for (const QByteArray &objectJson : objects) {
        array.append(QJsonDocument::fromJson(objectJson).object());
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

class HttpTestServer final : public QObject
{
    Q_OBJECT

public:
    struct Response {
        int status = 200;
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers;
    };

    explicit HttpTestServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &HttpTestServer::handleConnection);
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost);
    }

    QUrl url(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    void setResponse(const QString &path, const Response &response)
    {
        m_responses.insert(path, response);
    }

    int requestCount() const
    {
        return m_requestCount;
    }

    QString lastRequestPath() const
    {
        return m_lastRequestPath;
    }

    QByteArray lastRequest() const
    {
        return m_lastRequest;
    }

private:
    void handleConnection()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                m_requestBuffers[socket].append(socket->readAll());
                if (!m_requestBuffers.value(socket).contains("\r\n\r\n")) {
                    return;
                }

                const QByteArray request = m_requestBuffers.take(socket);
                const QList<QByteArray> lines = request.split('\n');
                const QList<QByteArray> requestLine =
                    !lines.isEmpty() ? lines.first().trimmed().split(' ') : QList<QByteArray>{};
                const QString path =
                    requestLine.size() >= 2 ? QString::fromLatin1(requestLine.at(1)) : QStringLiteral("/");
                const Response response = m_responses.value(path);

                ++m_requestCount;
                m_lastRequestPath = path;
                m_lastRequest = request;

                QByteArray header = "HTTP/1.1 " + QByteArray::number(response.status) + " ";
                header += response.status == 304 ? "Not Modified" : "OK";
                header += "\r\nContent-Type: application/json\r\nConnection: close\r\n";
                for (const auto &pair : response.headers) {
                    header += pair.first + ": " + pair.second + "\r\n";
                }
                header += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n\r\n";

                socket->write(header);
                socket->write(response.body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }

    QTcpServer m_server;
    QHash<QString, Response> m_responses;
    QHash<QTcpSocket *, QByteArray> m_requestBuffers;
    int m_requestCount = 0;
    QString m_lastRequestPath;
    QByteArray m_lastRequest;
};

} // namespace

class UpdateCheckerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void comparesVersions_data();
    void comparesVersions();
    void ignoresPrereleaseByDefault();
    void skipSuppressesAutomaticUpdateButNotManualCheck();
    void deferSuppressesAutomaticUpdateButNotManualCheck();
    void manualCheckDoesNotConsumeAutomaticInterval();
    void parsesGithubJsonAndPersistsMetadata();
    void notModifiedIsNotAnError();
};

void UpdateCheckerTest::initTestCase()
{
    const QString settingsDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test_update_settings"));
    QDir().mkpath(settingsDir);
    qputenv("XDG_CONFIG_HOME", settingsDir.toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir);
    QCoreApplication::setApplicationVersion(QStringLiteral("1.3.0"));
    clearSettings();
}

void UpdateCheckerTest::init()
{
    clearSettings();
    QCoreApplication::setApplicationVersion(QStringLiteral("1.3.0"));
}

void UpdateCheckerTest::cleanup()
{
    clearSettings();
}

void UpdateCheckerTest::comparesVersions_data()
{
    QTest::addColumn<QString>("currentVersion");
    QTest::addColumn<QString>("availableTag");
    QTest::addColumn<bool>("updateExpected");

    QTest::newRow("v1.3 equals 1.3.0") << QStringLiteral("1.3.0") << QStringLiteral("v1.3") << false;
    QTest::newRow("1.3.1 newer than 1.3.0") << QStringLiteral("1.3.0") << QStringLiteral("1.3.1") << true;
    QTest::newRow("2.0 newer than 1.9.9") << QStringLiteral("1.9.9") << QStringLiteral("2.0") << true;
}

void UpdateCheckerTest::comparesVersions()
{
    QFETCH(QString, currentVersion);
    QFETCH(QString, availableTag);
    QFETCH(bool, updateExpected);

    QCoreApplication::setApplicationVersion(currentVersion);

    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest"),
                       HttpTestServer::Response{200, releaseJson(availableTag)});

    AppSettingsManager settings;
    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateFound);
    QSignalSpy noUpdateSpy(&checker, &UpdateChecker::noUpdateAvailable);

    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);

    QCOMPARE(checker.updateAvailable(), updateExpected);
    QCOMPARE(updateSpy.count(), updateExpected ? 1 : 0);
    QCOMPARE(noUpdateSpy.count(), updateExpected ? 0 : 1);
}

void UpdateCheckerTest::ignoresPrereleaseByDefault()
{
    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest-array"),
                       HttpTestServer::Response{
                           200,
                           releasesJson({releaseJson(QStringLiteral("v1.3.0-beta.1"), true),
                                         releaseJson(QStringLiteral("v1.3.0"), false)})
                       });

    AppSettingsManager settings;
    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest-array")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateFound);
    QSignalSpy noUpdateSpy(&checker, &UpdateChecker::noUpdateAvailable);
    QSignalSpy failedSpy(&checker, &UpdateChecker::checkFailed);

    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);

    QCOMPARE(checker.updateAvailable(), false);
    QCOMPARE(updateSpy.count(), 0);
    QCOMPARE(noUpdateSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
}

void UpdateCheckerTest::skipSuppressesAutomaticUpdateButNotManualCheck()
{
    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest"),
                       HttpTestServer::Response{200, releaseJson(QStringLiteral("v1.3.0"))});

    AppSettingsManager settings;
    settings.setSkippedUpdateTag(QStringLiteral("v1.3.0"));

    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateFound);

    checker.checkNow(false);
    QTRY_COMPARE(checker.checking(), false);
    QCOMPARE(checker.updateAvailable(), true);
    QCOMPARE(updateSpy.count(), 0);

    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);
    QCOMPARE(checker.updateAvailable(), true);
    QCOMPARE(updateSpy.count(), 1);
}

void UpdateCheckerTest::deferSuppressesAutomaticUpdateButNotManualCheck()
{
    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest"),
                       HttpTestServer::Response{200, releaseJson(QStringLiteral("v1.3.0"))});

    AppSettingsManager settings;
    settings.setUpdateReminderDeferredUntil(QDateTime::currentDateTimeUtc().addSecs(3600));

    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateFound);

    checker.checkNow(false);
    QTRY_COMPARE(checker.checking(), false);
    QCOMPARE(checker.updateAvailable(), true);
    QCOMPARE(updateSpy.count(), 0);

    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);
    QCOMPARE(checker.updateAvailable(), true);
    QCOMPARE(updateSpy.count(), 1);
}

void UpdateCheckerTest::manualCheckDoesNotConsumeAutomaticInterval()
{
    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest"),
                       HttpTestServer::Response{200, releaseJson(QStringLiteral("v1.3.0"))});

    AppSettingsManager settings;
    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateFound);

    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);
    QCOMPARE(updateSpy.count(), 1);
    QVERIFY(settings.lastUpdateCheckAt().isValid());
    QVERIFY(!settings.lastAutomaticUpdateCheckAt().isValid());

    checker.checkNow(false);
    QTRY_COMPARE(checker.checking(), false);
    QCOMPARE(updateSpy.count(), 2);
    QVERIFY(settings.lastAutomaticUpdateCheckAt().isValid());
}

void UpdateCheckerTest::parsesGithubJsonAndPersistsMetadata()
{
    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest"),
                       HttpTestServer::Response{
                           200,
                           releaseJson(QStringLiteral("v1.3.0"),
                                       false,
                                       QStringLiteral("WaveFlux 1.3"),
                                       QStringLiteral("<b>unsafe</b>\n- New updater")),
                           {{QByteArrayLiteral("ETag"), QByteArrayLiteral("\"etag-1\"")}}
                       });

    AppSettingsManager settings;
    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy updateSpy(&checker, &UpdateChecker::updateFound);
    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);

    QCOMPARE(updateSpy.count(), 1);
    QCOMPARE(checker.updateAvailable(), true);
    QCOMPARE(checker.latestVersion(), QStringLiteral("1.3.0"));
    QCOMPARE(checker.latestTag(), QStringLiteral("v1.3.0"));
    QCOMPARE(checker.releaseName(), QStringLiteral("WaveFlux 1.3"));
    QVERIFY(checker.releaseNotes().contains(QStringLiteral("- New updater")));
    QVERIFY(!checker.releaseNotes().contains(QStringLiteral("<b>")));
    QCOMPARE(checker.releaseUrl(), QStringLiteral("https://github.com/leocallidus/waveflux/releases/tag/v1.3.0"));
    QVERIFY(checker.publishedAt().isValid());
    QCOMPARE(settings.updateCheckerEtag(), QStringLiteral("\"etag-1\""));
    QVERIFY(settings.lastUpdateCheckAt().isValid());
}

void UpdateCheckerTest::notModifiedIsNotAnError()
{
    HttpTestServer server;
    QVERIFY(server.listen());
    server.setResponse(QStringLiteral("/latest"),
                       HttpTestServer::Response{
                           304,
                           QByteArray(),
                           {{QByteArrayLiteral("ETag"), QByteArrayLiteral("\"etag-2\"")}}
                       });

    AppSettingsManager settings;
    settings.setUpdateCheckerEtag(QStringLiteral("\"etag-old\""));

    UpdateChecker checker(&settings);
    checker.setApiUrlsForTesting(server.url(QStringLiteral("/latest")),
                                 server.url(QStringLiteral("/releases")));

    QSignalSpy noUpdateSpy(&checker, &UpdateChecker::noUpdateAvailable);
    QSignalSpy failedSpy(&checker, &UpdateChecker::checkFailed);

    checker.checkNow(true);
    QTRY_COMPARE(checker.checking(), false);

    QCOMPARE(noUpdateSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(checker.lastError(), QString());
    QCOMPARE(checker.updateAvailable(), false);
    QCOMPARE(settings.updateCheckerEtag(), QStringLiteral("\"etag-2\""));
    QVERIFY(settings.lastUpdateCheckAt().isValid());
    QVERIFY(server.lastRequest().contains("If-None-Match: \"etag-old\""));
}

QTEST_MAIN(UpdateCheckerTest)

#include "tst_UpdateChecker.moc"
