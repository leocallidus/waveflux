#include "XspfPlaylistParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QXmlStreamReader>
#include <utility>

namespace {
constexpr QStringView kXspfNamespace = u"http://xspf.org/ns/0/";

bool isLikelyWindowsAbsolutePath(const QString &path)
{
    return path.size() >= 3
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('\\') || path.at(2) == QLatin1Char('/'));
}

bool isXspfElement(const QXmlStreamReader &xml, QLatin1StringView localName)
{
    if (xml.name() != localName) {
        return false;
    }

    const QStringView ns = xml.namespaceUri();
    return ns.isEmpty() || ns == kXspfNamespace;
}

void appendWarning(QStringList *warnings, const QString &message)
{
    if (warnings) {
        warnings->push_back(message);
    }
}

void setFatalError(XspfParseError *error, const QString &message, qint64 line, qint64 column)
{
    if (!error) {
        return;
    }

    error->message = message;
    error->line = line;
    error->column = column;
}

QString resolveLocationValue(const QString &locationText, const QDir &playlistDir)
{
    const QString normalized = locationText.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    if (isLikelyWindowsAbsolutePath(normalized) || QDir::isAbsolutePath(normalized)) {
        return QDir::cleanPath(normalized);
    }

    const QUrl url(normalized);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (url.isLocalFile()) {
            const QString localPath = url.toLocalFile().trimmed();
            if (!localPath.isEmpty()) {
                return QDir::cleanPath(localPath);
            }
            return {};
        }

        // Keep absolute URI intact (http/https/...)
        return normalized;
    }

    return QDir::cleanPath(playlistDir.filePath(normalized));
}

void parseTrackElement(QXmlStreamReader &xml,
                       const QDir &playlistDir,
                       int trackNumber,
                       QVector<XspfTrackEntry> *entries,
                       QStringList *warnings)
{
    XspfTrackEntry entry;
    const qint64 trackLine = xml.lineNumber();

    while (xml.readNextStartElement()) {
        if (isXspfElement(xml, QLatin1StringView("location"))) {
            const QString locationText = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            const QString resolved = resolveLocationValue(locationText, playlistDir);
            if (resolved.isEmpty()) {
                appendWarning(warnings,
                              QStringLiteral("track %1 (line %2): empty or invalid location")
                                  .arg(trackNumber)
                                  .arg(trackLine));
            } else {
                entry.source = resolved;
            }
            continue;
        }

        if (isXspfElement(xml, QLatin1StringView("title"))) {
            entry.title = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            continue;
        }

        if (isXspfElement(xml, QLatin1StringView("creator"))) {
            entry.creator = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            continue;
        }

        if (isXspfElement(xml, QLatin1StringView("album"))) {
            entry.album = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            continue;
        }

        if (isXspfElement(xml, QLatin1StringView("duration"))) {
            const QString durationText = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            bool ok = false;
            const qint64 durationValue = durationText.toLongLong(&ok);
            if (ok && durationValue >= 0) {
                entry.durationMs = durationValue;
            } else if (!durationText.isEmpty()) {
                appendWarning(warnings,
                              QStringLiteral("track %1 (line %2): invalid duration '%3'")
                                  .arg(trackNumber)
                                  .arg(trackLine)
                                  .arg(durationText));
            }
            continue;
        }

        xml.skipCurrentElement();
    }

    if (entry.source.isEmpty()) {
        appendWarning(warnings,
                      QStringLiteral("track %1 (line %2): skipped because location is missing")
                          .arg(trackNumber)
                          .arg(trackLine));
        return;
    }

    entries->push_back(std::move(entry));
}

void parseTrackListElement(QXmlStreamReader &xml,
                           const QDir &playlistDir,
                           QVector<XspfTrackEntry> *entries,
                           QStringList *warnings)
{
    int trackNumber = 0;
    while (xml.readNextStartElement()) {
        if (isXspfElement(xml, QLatin1StringView("track"))) {
            ++trackNumber;
            parseTrackElement(xml, playlistDir, trackNumber, entries, warnings);
        } else {
            xml.skipCurrentElement();
        }
    }
}

void parsePlaylistElement(QXmlStreamReader &xml,
                          const QDir &playlistDir,
                          QVector<XspfTrackEntry> *entries,
                          QStringList *warnings)
{
    while (xml.readNextStartElement()) {
        if (isXspfElement(xml, QLatin1StringView("trackList"))) {
            parseTrackListElement(xml, playlistDir, entries, warnings);
        } else {
            xml.skipCurrentElement();
        }
    }
}
} // namespace

bool XspfPlaylistParser::parseFile(const QString &xspfPath,
                                   QVector<XspfTrackEntry> *entries,
                                   QStringList *warnings,
                                   XspfParseError *error)
{
    if (!entries) {
        return false;
    }

    entries->clear();
    if (warnings) {
        warnings->clear();
    }
    if (error) {
        *error = {};
    }

    const QString normalizedPath = xspfPath.trimmed();
    if (normalizedPath.isEmpty()) {
        setFatalError(error, QStringLiteral("XSPF path is empty."), -1, -1);
        return false;
    }

    QFile file(normalizedPath);
    if (!file.exists()) {
        setFatalError(error, QStringLiteral("XSPF file does not exist."), -1, -1);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setFatalError(error, QStringLiteral("Failed to open XSPF file."), -1, -1);
        return false;
    }

    const QFileInfo playlistInfo(normalizedPath);
    const QDir playlistDir = playlistInfo.absoluteDir();

    QXmlStreamReader xml(&file);
    bool foundPlaylist = false;

    while (xml.readNextStartElement()) {
        if (isXspfElement(xml, QLatin1StringView("playlist"))) {
            foundPlaylist = true;
            parsePlaylistElement(xml, playlistDir, entries, warnings);
            break;
        }
        xml.skipCurrentElement();
    }

    if (xml.hasError()) {
        entries->clear();
        setFatalError(error, xml.errorString(), xml.lineNumber(), xml.columnNumber());
        return false;
    }

    if (!foundPlaylist) {
        entries->clear();
        setFatalError(error, QStringLiteral("No <playlist> root element found."), -1, -1);
        return false;
    }

    return true;
}
