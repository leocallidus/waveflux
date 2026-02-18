#include "XdgPortalFilePicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QStringList>
#include <QUrl>
#include <QWindow>

struct PortalFileFilterRule {
    quint32 type = 0;
    QString value;
};

using PortalFileFilterRuleList = QList<PortalFileFilterRule>;

struct PortalFileFilter {
    QString label;
    PortalFileFilterRuleList rules;
};

using PortalFileFilterList = QList<PortalFileFilter>;

Q_DECLARE_METATYPE(PortalFileFilterRule)
Q_DECLARE_METATYPE(PortalFileFilterRuleList)
Q_DECLARE_METATYPE(PortalFileFilter)
Q_DECLARE_METATYPE(PortalFileFilterList)

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFileFilterRule &rule)
{
    argument.beginStructure();
    argument << rule.type << rule.value;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFileFilterRule &rule)
{
    argument.beginStructure();
    argument >> rule.type >> rule.value;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFileFilter &filter)
{
    argument.beginStructure();
    argument << filter.label << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFileFilter &filter)
{
    argument.beginStructure();
    argument >> filter.label >> filter.rules;
    argument.endStructure();
    return argument;
}

namespace {
constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kFileChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr auto kRequestInterface = "org.freedesktop.portal.Request";
constexpr auto kFileManagerService = "org.freedesktop.FileManager1";
constexpr auto kFileManagerPath = "/org/freedesktop/FileManager1";
constexpr auto kFileManagerInterface = "org.freedesktop.FileManager1";
constexpr quint32 kFilterTypeGlobPattern = 0;

void registerPortalFileFilterMetaTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<PortalFileFilterRule>();
        qDBusRegisterMetaType<PortalFileFilterRuleList>();
        qDBusRegisterMetaType<PortalFileFilter>();
        qDBusRegisterMetaType<PortalFileFilterList>();
        return true;
    }();
    (void)registered;
}

PortalFileFilter buildPortalFilter(const QString &label, const QStringList &patterns)
{
    PortalFileFilter filter;
    filter.label = label.trimmed();
    for (const QString &pattern : patterns) {
        const QString normalizedPattern = pattern.trimmed();
        if (normalizedPattern.isEmpty()) {
            continue;
        }
        filter.rules.push_back({kFilterTypeGlobPattern, normalizedPattern});
    }
    return filter;
}

PortalFileFilterList buildOpenAudioFileFilters(const QString &audioFilterLabel,
                                               const QString &xspfFilterLabel,
                                               const QString &allFilesFilterLabel)
{
    const QString audioLabel = audioFilterLabel.trimmed().isEmpty()
        ? QStringLiteral("Audio files")
        : audioFilterLabel.trimmed();
    const QString xspfLabel = xspfFilterLabel.trimmed().isEmpty()
        ? QStringLiteral("XSPF playlists")
        : xspfFilterLabel.trimmed();
    const QString allFilesLabel = allFilesFilterLabel.trimmed().isEmpty()
        ? QStringLiteral("All files")
        : allFilesFilterLabel.trimmed();

    PortalFileFilterList filters;
    filters.push_back(buildPortalFilter(audioLabel, {
        QStringLiteral("*.mp3"), QStringLiteral("*.ogg"), QStringLiteral("*.mp4"), QStringLiteral("*.wma"),
        QStringLiteral("*.flac"), QStringLiteral("*.ape"), QStringLiteral("*.wav"), QStringLiteral("*.wv"),
        QStringLiteral("*.tta"), QStringLiteral("*.mpc"), QStringLiteral("*.spx"), QStringLiteral("*.opus"),
        QStringLiteral("*.m4a"), QStringLiteral("*.aac"), QStringLiteral("*.aiff"), QStringLiteral("*.alac"),
        QStringLiteral("*.xm"), QStringLiteral("*.s3m"), QStringLiteral("*.it"), QStringLiteral("*.mod"),
        QStringLiteral("*.cue")
    }));
    filters.push_back(buildPortalFilter(xspfLabel, {QStringLiteral("*.xspf")}));
    filters.push_back(buildPortalFilter(allFilesLabel, {QStringLiteral("*")}));
    return filters;
}
} // namespace

XdgPortalFilePicker::XdgPortalFilePicker(QObject *parent)
    : QObject(parent)
{
}

void XdgPortalFilePicker::setMainWindow(QWindow *window)
{
    m_mainWindow = window;
}

void XdgPortalFilePicker::openAudioFiles(const QString &title,
                                         const QString &audioFilterLabel,
                                         const QString &xspfFilterLabel,
                                         const QString &allFilesFilterLabel)
{
    registerPortalFileFilterMetaTypes();

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), createHandleToken(QStringLiteral("open")));
    options.insert(QStringLiteral("multiple"), true);
    options.insert(QStringLiteral("modal"), true);
    const PortalFileFilterList filters = buildOpenAudioFileFilters(audioFilterLabel,
                                                                   xspfFilterLabel,
                                                                   allFilesFilterLabel);
    options.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
    if (!filters.isEmpty()) {
        options.insert(QStringLiteral("current_filter"), QVariant::fromValue(filters.constFirst()));
    }

    startRequest(RequestKind::OpenFiles, QStringLiteral("OpenFile"), title, options);
}

void XdgPortalFilePicker::openFolder(const QString &title)
{
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), createHandleToken(QStringLiteral("folder")));
    options.insert(QStringLiteral("directory"), true);
    options.insert(QStringLiteral("modal"), true);

    startRequest(RequestKind::OpenFolder, QStringLiteral("OpenFile"), title, options);
}

void XdgPortalFilePicker::saveFile(const QString &title, const QString &defaultName)
{
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), createHandleToken(QStringLiteral("save")));
    options.insert(QStringLiteral("current_name"), defaultName);
    options.insert(QStringLiteral("modal"), true);

    startRequest(RequestKind::SaveFile, QStringLiteral("SaveFile"), title, options);
}

void XdgPortalFilePicker::openPresetFile(const QString &title)
{
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), createHandleToken(QStringLiteral("preset_open")));
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("modal"), true);

    startRequest(RequestKind::OpenPresetFile, QStringLiteral("OpenFile"), title, options);
}

void XdgPortalFilePicker::savePresetFile(const QString &title, const QString &defaultName)
{
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), createHandleToken(QStringLiteral("preset_save")));
    options.insert(QStringLiteral("current_name"), defaultName);
    options.insert(QStringLiteral("modal"), true);

    startRequest(RequestKind::SavePresetFile, QStringLiteral("SaveFile"), title, options);
}

void XdgPortalFilePicker::openImageFile(const QString &title)
{
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), createHandleToken(QStringLiteral("image_open")));
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("modal"), true);

    startRequest(RequestKind::OpenImageFile, QStringLiteral("OpenFile"), title, options);
}

bool XdgPortalFilePicker::openInFileManager(const QString &filePath)
{
    const auto fail = [this](const QString &message) {
        setLastError(message);
        emit pickerFailed(m_lastError);
        return false;
    };

    if (filePath.trimmed().isEmpty()) {
        return fail(QStringLiteral("Cannot open file manager: empty file path."));
    }

    QString localPath = filePath;
    const QUrl inputUrl(filePath);
    if (inputUrl.isValid() && !inputUrl.scheme().isEmpty()) {
        if (!inputUrl.isLocalFile()) {
            return fail(QStringLiteral("Cannot open file manager: only local files are supported."));
        }
        localPath = inputUrl.toLocalFile();
    }

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists()) {
        return fail(QStringLiteral("Cannot open file manager: file not found."));
    }

    const QString normalizedPath = fileInfo.canonicalFilePath().isEmpty()
        ? fileInfo.absoluteFilePath()
        : fileInfo.canonicalFilePath();
    const QUrl fileUrl = QUrl::fromLocalFile(normalizedPath);

    QDBusInterface fileManager(kFileManagerService,
                               kFileManagerPath,
                               kFileManagerInterface,
                               QDBusConnection::sessionBus(),
                               this);
    if (fileManager.isValid()) {
        QDBusReply<void> reply = fileManager.call(QStringLiteral("ShowItems"),
                                                  QStringList{fileUrl.toString()},
                                                  QString());
        if (reply.isValid()) {
            return true;
        }
    }

    const QUrl directoryUrl = QUrl::fromLocalFile(fileInfo.absolutePath());
    if (QDesktopServices::openUrl(directoryUrl)) {
        return true;
    }

    if (QProcess::startDetached(QStringLiteral("xdg-open"), {fileInfo.absolutePath()})) {
        return true;
    }

    return fail(QStringLiteral("Cannot open file manager for the selected file."));
}

bool XdgPortalFilePicker::moveFileToTrash(const QString &filePath)
{
    const auto fail = [this](const QString &message) {
        setLastError(message);
        emit pickerFailed(m_lastError);
        return false;
    };

    if (filePath.trimmed().isEmpty()) {
        return fail(QStringLiteral("Cannot move file to Trash: empty file path."));
    }

    QString localPath = filePath;
    const QUrl inputUrl(filePath);
    if (inputUrl.isValid() && !inputUrl.scheme().isEmpty()) {
        if (!inputUrl.isLocalFile()) {
            return fail(QStringLiteral("Cannot move file to Trash: only local files are supported."));
        }
        localPath = inputUrl.toLocalFile();
    }

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists()) {
        return fail(QStringLiteral("Cannot move file to Trash: file not found."));
    }

    const QString normalizedPath = fileInfo.canonicalFilePath().isEmpty()
        ? fileInfo.absoluteFilePath()
        : fileInfo.canonicalFilePath();

    QString pathInTrash;
    if (QFile::moveToTrash(normalizedPath, &pathInTrash)) {
        return true;
    }

    return fail(QStringLiteral("Cannot move file to Trash. Check permissions and Trash support for this disk."));
}

bool XdgPortalFilePicker::openExternalUrl(const QString &url)
{
    const auto fail = [this](const QString &message) {
        setLastError(message);
        emit pickerFailed(m_lastError);
        return false;
    };

    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        return fail(QStringLiteral("Cannot open URL: empty value."));
    }

    const QUrl parsed = QUrl::fromUserInput(trimmed);
    if (!parsed.isValid() || parsed.scheme().isEmpty()) {
        return fail(QStringLiteral("Cannot open URL: invalid URL."));
    }

#if defined(Q_OS_LINUX)
    if (QProcess::startDetached(QStringLiteral("xdg-open"), QStringList{parsed.toString()})) {
        return true;
    }
#endif

    if (QDesktopServices::openUrl(parsed)) {
        return true;
    }

    return fail(QStringLiteral("Cannot open URL in the default browser."));
}

void XdgPortalFilePicker::startRequest(RequestKind kind,
                                       const QString &method,
                                       const QString &title,
                                       QVariantMap options)
{
    if (m_activeKind != RequestKind::None) {
        setLastError(QStringLiteral("A file picker request is already in progress."));
        emit pickerFailed(m_lastError);
        return;
    }

    QDBusInterface portal(kPortalService,
                          kPortalPath,
                          kFileChooserInterface,
                          QDBusConnection::sessionBus(),
                          this);
    if (!portal.isValid()) {
        setLastError(QStringLiteral("XDG portal is unavailable: %1")
                         .arg(portal.lastError().message()));
        emit pickerFailed(m_lastError);
        return;
    }

    m_activeKind = kind;
    const QString parentId = parentWindowIdentifier();
    QDBusPendingCall pending = portal.asyncCall(method, parentId, title, options);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &XdgPortalFilePicker::onPortalRequestHandleReady);
}

void XdgPortalFilePicker::onPortalRequestHandleReady(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QDBusObjectPath> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        setLastError(QStringLiteral("Failed to open XDG portal file picker: %1")
                         .arg(reply.error().message()));
        emit pickerFailed(m_lastError);
        finishRequest();
        return;
    }

    m_activeRequestPath = reply.value().path();
    if (m_activeRequestPath.isEmpty()) {
        setLastError(QStringLiteral("XDG portal returned an invalid request handle."));
        emit pickerFailed(m_lastError);
        finishRequest();
        return;
    }

    const bool connected = QDBusConnection::sessionBus().connect(
        kPortalService,
        m_activeRequestPath,
        kRequestInterface,
        QStringLiteral("Response"),
        this,
        SLOT(onPortalResponse(uint,QVariantMap)));

    if (!connected) {
        setLastError(QStringLiteral("Failed to subscribe to XDG portal response."));
        emit pickerFailed(m_lastError);
        finishRequest();
    }
}

void XdgPortalFilePicker::onPortalResponse(uint response, const QVariantMap &results)
{
    const RequestKind kind = m_activeKind;
    const QString requestPath = m_activeRequestPath;

    finishRequest();

    if (!requestPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            kPortalService,
            requestPath,
            kRequestInterface,
            QStringLiteral("Response"),
            this,
            SLOT(onPortalResponse(uint,QVariantMap)));
    }

    if (response != 0) {
        // 1 = cancelled by user, 2 = other error. Keep silent for cancel.
        if (response == 2) {
            setLastError(QStringLiteral("XDG portal file picker returned an error."));
            emit pickerFailed(m_lastError);
        }
        return;
    }

    const QVariantList urls = extractUrls(results);
    if (urls.isEmpty()) {
        return;
    }

    switch (kind) {
    case RequestKind::OpenFiles:
        emit openFilesSelected(urls);
        break;
    case RequestKind::OpenFolder:
        emit folderSelected(urls.constFirst().toUrl());
        break;
    case RequestKind::SaveFile:
        emit saveFileSelected(urls.constFirst().toUrl());
        break;
    case RequestKind::OpenPresetFile:
        emit presetFileSelected(urls.constFirst().toUrl());
        break;
    case RequestKind::SavePresetFile:
        emit savePresetFileSelected(urls.constFirst().toUrl());
        break;
    case RequestKind::OpenImageFile:
        emit imageFileSelected(urls.constFirst().toUrl());
        break;
    case RequestKind::None:
        break;
    }
}

void XdgPortalFilePicker::finishRequest()
{
    m_activeKind = RequestKind::None;
    m_activeRequestPath.clear();
}

void XdgPortalFilePicker::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

QString XdgPortalFilePicker::parentWindowIdentifier() const
{
    if (!m_mainWindow || !m_mainWindow->isVisible()) {
        return QString();
    }

    // For Wayland, obtaining a valid parent handle requires xdg-foreign integration.
    // Passing an empty parent still works, but without transient parenting.
    return QString();
}

QString XdgPortalFilePicker::createHandleToken(const QString &prefix) const
{
    const qint64 millis = QDateTime::currentMSecsSinceEpoch();
    const quint32 random = QRandomGenerator::global()->generate();
    return QStringLiteral("%1_%2_%3").arg(prefix).arg(millis).arg(random);
}

QVariantList XdgPortalFilePicker::extractUrls(const QVariantMap &results) const
{
    QVariantList parsed;

    auto appendUri = [&parsed](const QString &uri) {
        const QUrl url(uri);
        if (url.isValid()) {
            parsed.append(url);
        }
    };

    const QVariant urisValue = results.value(QStringLiteral("uris"));
    if (urisValue.canConvert<QStringList>()) {
        const QStringList uris = urisValue.toStringList();
        for (const QString &uri : uris) {
            appendUri(uri);
        }
    } else if (urisValue.typeId() == QMetaType::QVariantList) {
        const QVariantList uris = urisValue.toList();
        for (const QVariant &uriValue : uris) {
            appendUri(uriValue.toString());
        }
    }

    if (parsed.isEmpty()) {
        const QString singleUri = results.value(QStringLiteral("uri")).toString();
        if (!singleUri.isEmpty()) {
            appendUri(singleUri);
        }
    }

    return parsed;
}
