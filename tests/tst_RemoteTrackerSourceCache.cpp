#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include "playback/RemoteTrackerSourceCache.h"

namespace {

void appendFixedString(QByteArray *buffer, QByteArray value, int width)
{
    value.truncate(width);
    buffer->append(value);
    if (value.size() < width) {
        buffer->append(QByteArray(width - value.size(), '\0'));
    }
}

void appendBigEndianWord(QByteArray *buffer, quint16 value)
{
    buffer->append(static_cast<char>((value >> 8) & 0xff));
    buffer->append(static_cast<char>(value & 0xff));
}

QByteArray createMinimalModFile()
{
    QByteArray data;
    appendFixedString(&data, "WaveFlux Remote Demo", 20);

    for (int sampleIndex = 0; sampleIndex < 31; ++sampleIndex) {
        appendFixedString(&data, sampleIndex == 0 ? "Pulse" : QByteArray(), 22);
        appendBigEndianWord(&data, sampleIndex == 0 ? 2 : 0);
        data.append('\0');
        data.append(static_cast<char>(sampleIndex == 0 ? 64 : 0));
        appendBigEndianWord(&data, 0);
        appendBigEndianWord(&data, sampleIndex == 0 ? 1 : 0);
    }

    data.append('\1');
    data.append(static_cast<char>(127));
    data.append(QByteArray(128, '\0'));
    data.append("M.K.", 4);

    QByteArray pattern(64 * 4 * 4, '\0');
    pattern[0] = static_cast<char>(0x01);
    pattern[1] = static_cast<char>(0xac);
    pattern[2] = static_cast<char>(0x10);
    pattern[3] = static_cast<char>(0x00);
    data.append(pattern);
    data.append(QByteArray::fromHex("0040c000"));
    return data;
}

class HttpFixtureServer final : public QObject
{
    Q_OBJECT

public:
    struct Response {
        QByteArray body;
        int statusCode = 200;
        int chunkSize = 0;
        int chunkDelayMs = 0;
        qint64 declaredLength = -1;
    };

    explicit HttpFixtureServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &HttpFixtureServer::handleConnection);
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost);
    }

    QUrl urlForPath(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    void setResponse(const QString &path, const Response &response)
    {
        m_responses.insert(path, response);
    }

    int requestCount(const QString &path) const
    {
        return m_requestCounts.value(path, 0);
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

                const QList<QByteArray> lines =
                    m_requestBuffers.take(socket).split('\n');
                if (lines.isEmpty()) {
                    socket->disconnectFromHost();
                    return;
                }

                const QList<QByteArray> requestLine =
                    lines.first().trimmed().split(' ');
                const QString path =
                    requestLine.size() >= 2 ? QString::fromLatin1(requestLine.at(1)) : QStringLiteral("/");
                m_requestCounts[path] += 1;

                const Response response = m_responses.value(path);
                const qint64 contentLength =
                    response.declaredLength >= 0 ? response.declaredLength : response.body.size();
                const QByteArray header =
                    "HTTP/1.1 " + QByteArray::number(response.statusCode) + " OK\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Content-Length: " + QByteArray::number(contentLength) + "\r\n"
                    "Connection: close\r\n\r\n";
                socket->write(header);

                if (response.chunkSize <= 0 || response.chunkDelayMs <= 0) {
                    socket->write(response.body);
                    socket->disconnectFromHost();
                    return;
                }

                auto offset = std::make_shared<int>(0);
                auto timer = new QTimer(socket);
                timer->setInterval(response.chunkDelayMs);
                connect(timer, &QTimer::timeout, socket, [socket, timer, response, offset]() {
                    if (!socket->isOpen()) {
                        timer->stop();
                        timer->deleteLater();
                        return;
                    }
                    const int remaining = response.body.size() - *offset;
                    if (remaining <= 0) {
                        timer->stop();
                        socket->disconnectFromHost();
                        timer->deleteLater();
                        return;
                    }
                    const int chunk = qMin(response.chunkSize, remaining);
                    socket->write(response.body.constData() + *offset, chunk);
                    *offset += chunk;
                });
                timer->start();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        }
    }

    QTcpServer m_server;
    QHash<QString, Response> m_responses;
    QHash<QString, int> m_requestCounts;
    QHash<QTcpSocket *, QByteArray> m_requestBuffers;
};

QString cacheDirPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/tracker-remote");
}

} // namespace

class RemoteTrackerSourceCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(cacheDirPath()).removeRecursively();
    }

    void cleanup()
    {
        QDir(cacheDirPath()).removeRecursively();
    }

    void downloadsAndReusesCachedModule()
    {
        HttpFixtureServer server;
        QVERIFY(server.listen());
        server.setResponse(QStringLiteral("/demo.mod"), {.body = createMinimalModFile()});

        WaveFlux::RemoteTrackerSourceCache cache;
        QSignalSpy finishedSpy(&cache, &WaveFlux::RemoteTrackerSourceCache::finished);
        QSignalSpy failedSpy(&cache, &WaveFlux::RemoteTrackerSourceCache::failed);

        const QUrl url = server.urlForPath(QStringLiteral("/demo.mod"));
        cache.materialize(url);

        QTRY_COMPARE(finishedSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(server.requestCount(QStringLiteral("/demo.mod")), 1);

        const QList<QVariant> firstResult = finishedSpy.takeFirst();
        const QString firstPath = firstResult.at(1).toString();
        const QString firstCacheKey = firstResult.at(2).toString();
        QVERIFY(QFileInfo::exists(firstPath));
        QVERIFY(!firstCacheKey.isEmpty());

        cache.materialize(url);

        QTRY_COMPARE(finishedSpy.count(), 1);
        QCOMPARE(server.requestCount(QStringLiteral("/demo.mod")), 1);
        const QList<QVariant> secondResult = finishedSpy.takeFirst();
        QCOMPARE(secondResult.at(1).toString(), firstPath);
        QCOMPARE(secondResult.at(2).toString(), firstCacheKey);
    }

    void rejectsOversizedModule()
    {
        HttpFixtureServer server;
        QVERIFY(server.listen());
        HttpFixtureServer::Response response;
        response.body = createMinimalModFile();
        response.declaredLength = 512;
        server.setResponse(QStringLiteral("/huge.mod"), response);

        WaveFlux::RemoteTrackerSourceCache cache(nullptr, 128);
        QSignalSpy failedSpy(&cache, &WaveFlux::RemoteTrackerSourceCache::failed);

        cache.materialize(server.urlForPath(QStringLiteral("/huge.mod")));

        QTRY_COMPARE(failedSpy.count(), 1);
        QVERIFY(failedSpy.takeFirst().at(1).toString().contains(QStringLiteral("cache limit")));
    }

    void cancelStopsActiveDownloadAndRemovesPartialFile()
    {
        HttpFixtureServer server;
        QVERIFY(server.listen());

        HttpFixtureServer::Response response;
        response.body = createMinimalModFile() + QByteArray(16384, '\x7f');
        response.chunkSize = 256;
        response.chunkDelayMs = 20;
        server.setResponse(QStringLiteral("/slow.mod"), response);

        WaveFlux::RemoteTrackerSourceCache cache;
        QSignalSpy canceledSpy(&cache, &WaveFlux::RemoteTrackerSourceCache::canceled);
        QSignalSpy finishedSpy(&cache, &WaveFlux::RemoteTrackerSourceCache::finished);

        cache.materialize(server.urlForPath(QStringLiteral("/slow.mod")));

        QTRY_VERIFY(cache.isActive());
        cache.cancel();

        QTRY_COMPARE(canceledSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);

        const QDir dir(cacheDirPath());
        const QStringList partialFiles = dir.entryList(QStringList{QStringLiteral("*.part")},
                                                       QDir::Files | QDir::NoDotAndDotDot);
        QVERIFY(partialFiles.isEmpty());
    }
};

QTEST_MAIN(RemoteTrackerSourceCacheTest)
#include "tst_RemoteTrackerSourceCache.moc"
