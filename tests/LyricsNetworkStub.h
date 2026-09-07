#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QPointer>
#include <deque>
#include <cstring>

struct LyricsResponse {
    QByteArray body;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    bool hold = false;
};

class LyricsReply final : public QNetworkReply {
public:
    LyricsReply(const QNetworkRequest &request, const LyricsResponse &response, QObject *parent)
        : QNetworkReply(parent), m_body(response.body)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        if (response.status == 429) setRawHeader("Retry-After", "60");
        open(QIODevice::ReadOnly);
        if (!response.hold) {
            QTimer::singleShot(0, this, [this, response]() {
                if (isFinished()) return;
                if (response.error != NoError) setError(response.error, QStringLiteral("Simulated network failure"));
                emit readyRead();
                if (isFinished()) return;
                setFinished(true);
                emit finished();
            });
        }
    }
    void abort() override
    {
        if (isFinished()) return;
        setError(OperationCanceledError, QStringLiteral("Canceled"));
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 maximum) override
    {
        const qint64 length = std::min(maximum, static_cast<qint64>(m_body.size()) - m_offset);
        if (length <= 0) return -1;
        std::memcpy(buffer, m_body.constData() + m_offset, static_cast<size_t>(length));
        m_offset += length;
        return length;
    }
private:
    QByteArray m_body;
    qint64 m_offset = 0;
};

class LyricsNetworkStub final : public QNetworkAccessManager {
public:
    std::deque<LyricsResponse> responses;
    QList<QNetworkRequest> requests;
    QPointer<LyricsReply> lastReply;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request);
        LyricsResponse response;
        if (responses.empty()) response.hold = true;
        else { response = responses.front(); responses.pop_front(); }
        lastReply = new LyricsReply(request, response, this);
        return lastReply;
    }
};
