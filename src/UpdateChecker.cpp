#include "UpdateChecker.h"

#include "AppSettingsManager.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr int kRequestTimeoutMs = 15000;
constexpr int kStartupCheckDelayMs = 30000;
constexpr qint64 kAutoCheckIntervalSeconds = 24LL * 60LL * 60LL;
constexpr qint64 kReminderDelaySeconds = 24LL * 60LL * 60LL;
constexpr int kMaxReleaseNotesLength = 4000;

const QUrl kDefaultLatestReleaseUrl(QStringLiteral("https://api.github.com/repos/leocallidus/waveflux/releases/latest"));
const QUrl kDefaultReleasesUrl(QStringLiteral("https://api.github.com/repos/leocallidus/waveflux/releases"));

QString githubApiErrorMessage(const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (document.isObject()) {
        const QString message = document.object().value(QStringLiteral("message")).toString().trimmed();
        if (!message.isEmpty()) {
            return message;
        }
    }
    return QString();
}
} // namespace

UpdateChecker::UpdateChecker(AppSettingsManager *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_latestReleaseUrl(kDefaultLatestReleaseUrl)
    , m_releasesUrl(kDefaultReleasesUrl)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(kRequestTimeoutMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        QNetworkReply *reply = m_currentReply;
        if (!reply) {
            return;
        }
        const bool manual = m_currentCheckManual;
        m_currentReply.clear();
        m_currentCheckManual = false;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
        setChecking(false);
        if (manual) {
            setLastError(QStringLiteral("Update check timed out."));
            emit checkFailed(m_lastError);
        }
    });

    m_startupCheckTimer.setSingleShot(true);
    m_startupCheckTimer.setInterval(kStartupCheckDelayMs);
    connect(&m_startupCheckTimer, &QTimer::timeout, this, [this]() {
        checkNow(false);
    });
}

QString UpdateChecker::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

void UpdateChecker::checkNow(bool manual)
{
    if (m_checking) {
        if (manual) {
            setLastError(QStringLiteral("Update check is already running."));
        }
        return;
    }

    if (!manual && !autoCheckAllowed()) {
        return;
    }

    startRequest(manual);
}

void UpdateChecker::deferCurrentUpdate()
{
    if (!m_settings || m_latestTag.isEmpty()) {
        return;
    }

    m_settings->setUpdateReminderDeferredUntil(
        QDateTime::currentDateTimeUtc().addSecs(kReminderDelaySeconds));
}

void UpdateChecker::skipCurrentVersion()
{
    if (!m_settings || m_latestTag.isEmpty()) {
        return;
    }

    m_settings->setSkippedUpdateTag(m_latestTag);
}

void UpdateChecker::openReleasePage()
{
    if (!isSafeReleaseUrl(m_releaseUrl)) {
        return;
    }

    QDesktopServices::openUrl(QUrl(m_releaseUrl));
}

void UpdateChecker::scheduleStartupCheck()
{
    if (m_startupCheckTimer.isActive() || !m_settings || !m_settings->autoCheckUpdates()) {
        return;
    }

    m_startupCheckTimer.start();
}

#ifdef WAVEFLUX_UPDATECHECKER_TESTING
void UpdateChecker::setApiUrlsForTesting(const QUrl &latestReleaseUrl, const QUrl &releasesUrl)
{
    m_latestReleaseUrl = latestReleaseUrl;
    m_releasesUrl = releasesUrl;
}
#endif

void UpdateChecker::startRequest(bool manual)
{
    if (manual && m_startupCheckTimer.isActive()) {
        m_startupCheckTimer.stop();
    }

    clearResult();
    m_currentCheckManual = manual;
    if (manual) {
        setLastError(QString());
    }
    setChecking(true);

    QNetworkRequest request = buildRequest();
    QNetworkReply *reply = m_network.get(request);
    m_currentReply = reply;
    m_timeoutTimer.start();

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        finishRequest(reply);
    });
}

void UpdateChecker::finishRequest(QNetworkReply *reply)
{
    if (!reply || reply != m_currentReply) {
        if (reply) {
            reply->deleteLater();
        }
        return;
    }

    m_timeoutTimer.stop();
    const QByteArray payload = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool manual = m_currentCheckManual;

    const QVariant etagHeader = reply->header(QNetworkRequest::ETagHeader);
    if (etagHeader.isValid() && m_settings) {
        const QString etag = etagHeader.toString().trimmed();
        if (!etag.isEmpty()) {
            m_settings->setUpdateCheckerEtag(etag);
        }
    }

    if (statusCode == 304) {
        if (m_settings) {
            const QDateTime checkedAt = QDateTime::currentDateTimeUtc();
            m_settings->setLastUpdateCheckAt(checkedAt);
            if (!manual) {
                m_settings->setLastAutomaticUpdateCheckAt(checkedAt);
            }
        }
        completeRequest();
        emit noUpdateAvailable();
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString apiMessage = githubApiErrorMessage(payload);
        const QString message = !apiMessage.isEmpty() ? apiMessage : reply->errorString();
        reply->deleteLater();
        if (manual) {
            failRequest(message);
        } else {
            completeRequest();
        }
        return;
    }

    if (statusCode < 200 || statusCode >= 300) {
        const QString apiMessage = githubApiErrorMessage(payload);
        const QString message = !apiMessage.isEmpty()
            ? apiMessage
            : QStringLiteral("GitHub Releases returned HTTP %1.").arg(statusCode);
        reply->deleteLater();
        if (manual) {
            failRequest(message);
        } else {
            completeRequest();
        }
        return;
    }

    ReleaseInfo release;
    QString errorMessage;
    if (!parseResponse(payload, &release, &errorMessage)) {
        reply->deleteLater();
        if (manual) {
            failRequest(errorMessage);
        } else {
            completeRequest();
        }
        return;
    }

    if (m_settings) {
        const QDateTime checkedAt = QDateTime::currentDateTimeUtc();
        m_settings->setLastUpdateCheckAt(checkedAt);
        if (!manual) {
            m_settings->setLastAutomaticUpdateCheckAt(checkedAt);
        }
    }

    applyReleaseResult(release);
    completeRequest();
    reply->deleteLater();
}

void UpdateChecker::failRequest(const QString &message)
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply.clear();
    }
    m_timeoutTimer.stop();
    setLastError(message.isEmpty() ? QStringLiteral("Update check failed.") : message);
    setChecking(false);
    emit checkFailed(m_lastError);
}

void UpdateChecker::completeRequest()
{
    m_currentReply.clear();
    m_currentCheckManual = false;
    setChecking(false);
}

void UpdateChecker::clearResult()
{
    if (!m_updateAvailable && m_latestVersion.isEmpty() && m_latestTag.isEmpty()
        && m_releaseName.isEmpty() && m_releaseNotes.isEmpty() && m_releaseUrl.isEmpty()
        && !m_publishedAt.isValid()) {
        return;
    }

    m_updateAvailable = false;
    m_latestVersion.clear();
    m_latestTag.clear();
    m_releaseName.clear();
    m_releaseNotes.clear();
    m_releaseUrl.clear();
    m_publishedAt = QDateTime();
    emit resultChanged();
}

void UpdateChecker::setChecking(bool checking)
{
    if (m_checking == checking) {
        return;
    }

    m_checking = checking;
    emit checkingChanged();
}

void UpdateChecker::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit errorChanged();
}

bool UpdateChecker::autoCheckAllowed() const
{
    if (!m_settings || !m_settings->autoCheckUpdates()) {
        return false;
    }

    const QDateTime lastCheck = m_settings->lastAutomaticUpdateCheckAt();
    if (lastCheck.isValid()
        && lastCheck.secsTo(QDateTime::currentDateTimeUtc()) < kAutoCheckIntervalSeconds) {
        return false;
    }

    return true;
}

QNetworkRequest UpdateChecker::buildRequest() const
{
    QUrl url = (m_settings && m_settings->includePrereleaseUpdates()) ? m_releasesUrl : m_latestReleaseUrl;
    if (url == m_releasesUrl) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("10"));
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("WaveFlux/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (m_settings && !m_settings->includePrereleaseUpdates()) {
        const QString etag = m_settings->updateCheckerEtag();
        if (!etag.isEmpty()) {
            request.setRawHeader("If-None-Match", etag.toUtf8());
        }
    }

    return request;
}

bool UpdateChecker::parseResponse(const QByteArray &payload,
                                  ReleaseInfo *release,
                                  QString *errorMessage) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GitHub Releases response is not valid JSON: %1.")
                                .arg(parseError.errorString());
        }
        return false;
    }

    if (document.isObject()) {
        ReleaseInfo parsed;
        if (!parseReleaseObject(document.object(), &parsed, errorMessage)) {
            return false;
        }
        if (!shouldUseRelease(parsed)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("GitHub Releases response did not contain a usable release.");
            }
            return false;
        }
        if (release) {
            *release = parsed;
        }
        return true;
    }

    if (document.isArray()) {
        const QJsonArray releases = document.array();
        for (const QJsonValue &value : releases) {
            if (!value.isObject()) {
                continue;
            }
            ReleaseInfo candidate;
            QString candidateError;
            if (!parseReleaseObject(value.toObject(), &candidate, &candidateError)) {
                continue;
            }
            if (shouldUseRelease(candidate)) {
                if (release) {
                    *release = candidate;
                }
                return true;
            }
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("GitHub Releases response did not contain a usable release.");
        }
        return false;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("GitHub Releases response has an unexpected shape.");
    }
    return false;
}

bool UpdateChecker::parseReleaseObject(const QJsonObject &object,
                                       ReleaseInfo *release,
                                       QString *errorMessage) const
{
    ReleaseInfo parsed;
    parsed.tag = object.value(QStringLiteral("tag_name")).toString().trimmed();
    parsed.version = normalizeVersionText(parsed.tag);
    parsed.name = object.value(QStringLiteral("name")).toString().trimmed();
    parsed.notes = plainReleaseNotes(object.value(QStringLiteral("body")).toString());
    parsed.url = object.value(QStringLiteral("html_url")).toString().trimmed();
    parsed.publishedAt =
        QDateTime::fromString(object.value(QStringLiteral("published_at")).toString(),
                              Qt::ISODate).toUTC();
    parsed.prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
    parsed.draft = object.value(QStringLiteral("draft")).toBool(false);

    if (parsed.tag.isEmpty() || parsed.version.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GitHub release does not contain a parseable version tag.");
        }
        return false;
    }
    if (!isSafeReleaseUrl(parsed.url)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GitHub release URL is not safe.");
        }
        return false;
    }

    if (release) {
        *release = parsed;
    }
    return true;
}

bool UpdateChecker::shouldUseRelease(const ReleaseInfo &release) const
{
    if (release.draft) {
        return false;
    }
    if (release.prerelease && (!m_settings || !m_settings->includePrereleaseUpdates())) {
        return false;
    }
    return true;
}

void UpdateChecker::applyReleaseResult(const ReleaseInfo &release)
{
    const VersionRelation relation = compareVersions(release.version, currentVersion());
    if (relation != VersionRelation::Newer) {
        emit noUpdateAvailable();
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const bool manual = m_currentCheckManual;
    const bool suppressedBySkip = !manual && m_settings && release.tag == m_settings->skippedUpdateTag();
    const bool suppressedByDefer = !manual && m_settings && m_settings->updateReminderDeferredUntil().isValid()
        && m_settings->updateReminderDeferredUntil() > now;

    m_updateAvailable = true;
    m_latestVersion = release.version;
    m_latestTag = release.tag;
    m_releaseName = release.name;
    m_releaseNotes = release.notes;
    m_releaseUrl = release.url;
    m_publishedAt = release.publishedAt;
    emit resultChanged();

    if (!suppressedBySkip && !suppressedByDefer) {
        emit updateFound();
    }
}

bool UpdateChecker::isSafeReleaseUrl(const QString &url) const
{
    const QUrl parsed(url);
    return parsed.isValid()
        && parsed.scheme() == QStringLiteral("https")
        && parsed.host() == QStringLiteral("github.com")
        && parsed.path().startsWith(QStringLiteral("/leocallidus/waveflux/releases/"));
}

QString UpdateChecker::normalizeVersionText(const QString &tagOrVersion)
{
    QString value = tagOrVersion.trimmed();
    if (value.startsWith(QLatin1Char('v')) || value.startsWith(QLatin1Char('V'))) {
        value.remove(0, 1);
    }

    const QRegularExpression matchVersion(QStringLiteral(R"((\d+(?:\.\d+){0,3}))"));
    const QRegularExpressionMatch match = matchVersion.match(value);
    return match.hasMatch() ? match.captured(1) : QString();
}

UpdateChecker::VersionRelation UpdateChecker::compareVersions(const QString &available,
                                                              const QString &current)
{
    const QString normalizedAvailable = normalizeVersionText(available);
    const QString normalizedCurrent = normalizeVersionText(current);
    if (normalizedAvailable.isEmpty() || normalizedCurrent.isEmpty()) {
        return VersionRelation::Unknown;
    }

    const QStringList availableParts = normalizedAvailable.split(QLatin1Char('.'));
    const QStringList currentParts = normalizedCurrent.split(QLatin1Char('.'));
    const int partCount = qMax(availableParts.size(), currentParts.size());
    for (int i = 0; i < partCount; ++i) {
        bool availableOk = true;
        bool currentOk = true;
        const int availablePart = i < availableParts.size() ? availableParts.at(i).toInt(&availableOk) : 0;
        const int currentPart = i < currentParts.size() ? currentParts.at(i).toInt(&currentOk) : 0;
        if (!availableOk || !currentOk) {
            return VersionRelation::Unknown;
        }
        if (availablePart > currentPart) {
            return VersionRelation::Newer;
        }
        if (availablePart < currentPart) {
            return VersionRelation::Older;
        }
    }
    return VersionRelation::Equal;
}

QString UpdateChecker::plainReleaseNotes(const QString &notes)
{
    QString text = notes;
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text.replace(QRegularExpression(QStringLiteral("[\r\n]{3,}")), QStringLiteral("\n\n"));
    text = text.trimmed();
    if (text.size() > kMaxReleaseNotesLength) {
        text = text.left(kMaxReleaseNotesLength).trimmed() + QStringLiteral("...");
    }
    return text;
}
