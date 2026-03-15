#include "XdgPortalFilePicker.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QUrl>
#include <QWindow>

namespace {
QString normalizeLocalPath(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    // Treat Windows drive-letter and UNC paths as local paths before QUrl parsing.
    // QUrl("H:/music/file.mp3") interprets "H" as a scheme and rejects the path.
    if (trimmed.size() >= 3
        && trimmed[1] == QLatin1Char(':')
        && (trimmed[2] == QLatin1Char('/') || trimmed[2] == QLatin1Char('\\'))
        && trimmed[0].isLetter()) {
        return QDir::cleanPath(trimmed);
    }

    if (trimmed.size() >= 4
        && trimmed[0] == QLatin1Char('/')
        && trimmed[2] == QLatin1Char(':')
        && (trimmed[3] == QLatin1Char('/') || trimmed[3] == QLatin1Char('\\'))
        && trimmed[1].isLetter()) {
        return QDir::cleanPath(trimmed.mid(1));
    }

    if (trimmed.startsWith(QStringLiteral("\\\\")) || trimmed.startsWith(QStringLiteral("//"))) {
        return QDir::cleanPath(trimmed);
    }

    const QUrl inputUrl(trimmed);
    if (inputUrl.isValid() && !inputUrl.scheme().isEmpty()) {
        if (!inputUrl.isLocalFile()) {
            return {};
        }
        return QDir::cleanPath(inputUrl.toLocalFile());
    }

    return QDir::cleanPath(trimmed);
}

QString buildAudioFilter(const QString &audioFilterLabel,
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

    return QStringLiteral(
               "%1 (*.mp3 *.ogg *.mp4 *.wma *.flac *.ape *.wav *.wv *.tta *.mpc *.spx *.opus *.m4a *.aac *.aiff *.alac *.xm *.s3m *.it *.mod *.cue);;"
               "%2 (*.xspf);;"
               "%3 (*)")
        .arg(audioLabel, xspfLabel, allFilesLabel);
}

QString ensureSuffixForSelectedFilter(QString path, const QString &selectedFilter)
{
    if (path.isEmpty() || QFileInfo(path).suffix().size() > 0) {
        return path;
    }

    const QString lowerFilter = selectedFilter.toLower();
    if (lowerFilter.contains(QStringLiteral("json"))) {
        path += QStringLiteral(".json");
    } else if (lowerFilter.contains(QStringLiteral("m3u"))) {
        path += QStringLiteral(".m3u");
    }

    return path;
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
    const QStringList selectedFiles = QFileDialog::getOpenFileNames(
        nullptr,
        title,
        QString(),
        buildAudioFilter(audioFilterLabel, xspfFilterLabel, allFilesFilterLabel));
    if (selectedFiles.isEmpty()) {
        return;
    }

    QVariantList urls;
    urls.reserve(selectedFiles.size());
    for (const QString &path : selectedFiles) {
        urls.append(QUrl::fromLocalFile(path));
    }
    emit openFilesSelected(urls);
}

void XdgPortalFilePicker::openFolder(const QString &title)
{
    const QString folder = QFileDialog::getExistingDirectory(
        nullptr,
        title,
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!folder.isEmpty()) {
        emit folderSelected(QUrl::fromLocalFile(folder));
    }
}

void XdgPortalFilePicker::saveFile(const QString &title, const QString &defaultName)
{
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        nullptr,
        title,
        defaultName,
        QStringLiteral("M3U playlists (*.m3u *.m3u8);;JSON playlists (*.json);;All files (*)"),
        &selectedFilter);
    path = ensureSuffixForSelectedFilter(path, selectedFilter);
    if (!path.isEmpty()) {
        emit saveFileSelected(QUrl::fromLocalFile(path));
    }
}

void XdgPortalFilePicker::openPresetFile(const QString &title)
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr,
        title,
        QString(),
        QStringLiteral("Preset files (*.json);;All files (*)"));
    if (!path.isEmpty()) {
        emit presetFileSelected(QUrl::fromLocalFile(path));
    }
}

void XdgPortalFilePicker::savePresetFile(const QString &title, const QString &defaultName)
{
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        nullptr,
        title,
        defaultName,
        QStringLiteral("Preset files (*.json);;All files (*)"),
        &selectedFilter);
    path = ensureSuffixForSelectedFilter(path, selectedFilter);
    if (!path.isEmpty()) {
        emit savePresetFileSelected(QUrl::fromLocalFile(path));
    }
}

void XdgPortalFilePicker::openImageFile(const QString &title)
{
    const QString path = QFileDialog::getOpenFileName(
        nullptr,
        title,
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif);;All files (*)"));
    if (!path.isEmpty()) {
        emit imageFileSelected(QUrl::fromLocalFile(path));
    }
}

bool XdgPortalFilePicker::openInFileManager(const QString &filePath)
{
    const auto fail = [this](const QString &message) {
        setLastError(message);
        emit pickerFailed(m_lastError);
        return false;
    };

    const QString localPath = normalizeLocalPath(filePath);
    if (localPath.isEmpty()) {
        return fail(QStringLiteral("Cannot open file manager: only local files are supported."));
    }

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists()) {
        return fail(QStringLiteral("Cannot open file manager: file not found."));
    }

#if defined(Q_OS_WIN)
    const QString nativePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    const bool started = fileInfo.isDir()
        ? QProcess::startDetached(QStringLiteral("explorer.exe"), {nativePath})
        : QProcess::startDetached(QStringLiteral("explorer.exe"),
                                  {QStringLiteral("/select,"), nativePath});
    if (started) {
        return true;
    }
#endif

    const QUrl targetUrl = QUrl::fromLocalFile(fileInfo.isDir() ? fileInfo.absoluteFilePath()
                                                                : fileInfo.absolutePath());
    if (QDesktopServices::openUrl(targetUrl)) {
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

    const QString localPath = normalizeLocalPath(filePath);
    if (localPath.isEmpty()) {
        return fail(QStringLiteral("Cannot move file to Trash: only local files are supported."));
    }

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists()) {
        return fail(QStringLiteral("Cannot move file to Trash: file not found."));
    }

    QString pathInTrash;
    if (QFile::moveToTrash(fileInfo.absoluteFilePath(), &pathInTrash)) {
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

    const QUrl parsed = QUrl::fromUserInput(url.trimmed());
    if (!parsed.isValid() || parsed.scheme().isEmpty()) {
        return fail(QStringLiteral("Cannot open URL: invalid URL."));
    }

    if (QDesktopServices::openUrl(parsed)) {
        return true;
    }

    return fail(QStringLiteral("Cannot open URL in the default browser."));
}

void XdgPortalFilePicker::onPortalRequestHandleReady(QDBusPendingCallWatcher *watcher)
{
    Q_UNUSED(watcher);
}

void XdgPortalFilePicker::onPortalResponse(uint response, const QVariantMap &results)
{
    Q_UNUSED(response);
    Q_UNUSED(results);
}

void XdgPortalFilePicker::startRequest(RequestKind kind,
                                       const QString &method,
                                       const QString &title,
                                       QVariantMap options)
{
    Q_UNUSED(kind);
    Q_UNUSED(method);
    Q_UNUSED(title);
    Q_UNUSED(options);
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
    return QString();
}

QString XdgPortalFilePicker::createHandleToken(const QString &prefix) const
{
    return prefix;
}

QVariantList XdgPortalFilePicker::extractUrls(const QVariantMap &results) const
{
    Q_UNUSED(results);
    return {};
}
