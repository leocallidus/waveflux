#include "CueSheetParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringDecoder>
#include <QStringList>

namespace {
void skipSpaces(const QString &line, int *cursor)
{
    if (!cursor) {
        return;
    }

    while (*cursor < line.size() && line.at(*cursor).isSpace()) {
        ++(*cursor);
    }
}

QString readToken(const QString &line, int *cursor, bool *closedQuote = nullptr)
{
    if (!cursor) {
        if (closedQuote) {
            *closedQuote = true;
        }
        return {};
    }

    if (closedQuote) {
        *closedQuote = true;
    }

    skipSpaces(line, cursor);
    if (*cursor >= line.size()) {
        return {};
    }

    if (line.at(*cursor) != QLatin1Char('"')) {
        const int start = *cursor;
        while (*cursor < line.size() && !line.at(*cursor).isSpace()) {
            ++(*cursor);
        }
        return line.mid(start, *cursor - start).trimmed();
    }

    ++(*cursor);
    QString token;
    bool closed = false;
    while (*cursor < line.size()) {
        const QChar ch = line.at(*cursor);
        if (ch == QLatin1Char('"')) {
            if ((*cursor + 1) < line.size() && line.at(*cursor + 1) == QLatin1Char('"')) {
                token.append(QLatin1Char('"'));
                *cursor += 2;
                continue;
            }

            ++(*cursor);
            closed = true;
            break;
        }

        token.append(ch);
        ++(*cursor);
    }

    if (closedQuote) {
        *closedQuote = closed;
    }
    return token.trimmed();
}

QString readCommandValue(const QString &line, int cursor)
{
    skipSpaces(line, &cursor);
    if (cursor >= line.size()) {
        return {};
    }

    if (line.at(cursor) == QLatin1Char('"')) {
        bool closedQuote = true;
        const QString value = readToken(line, &cursor, &closedQuote);
        return value.trimmed();
    }

    return line.mid(cursor).trimmed();
}

QString parseTextPayload(const QString &line, int cursor, QStringList *diagnostics, int lineNumber)
{
    bool closedQuote = true;
    const QString value = readToken(line, &cursor, &closedQuote);
    if (!closedQuote) {
        diagnostics->push_back(QStringLiteral("line %1: unclosed quoted value").arg(lineNumber));
    }
    return value.trimmed();
}

QString decodeCueContent(const QByteArray &rawData)
{
    if (rawData.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(rawData.mid(3));
    }

    if (rawData.size() >= 2) {
        const unsigned char first = static_cast<unsigned char>(rawData.at(0));
        const unsigned char second = static_cast<unsigned char>(rawData.at(1));
        if (first == 0xFF && second == 0xFE) {
            QStringDecoder decoder(QStringDecoder::Utf16LE);
            return decoder.decode(rawData.sliced(2));
        }
        if (first == 0xFE && second == 0xFF) {
            QStringDecoder decoder(QStringDecoder::Utf16BE);
            return decoder.decode(rawData.sliced(2));
        }
    }

    const QString utf8 = QString::fromUtf8(rawData);
    const qsizetype utf8ReplacementCount = utf8.count(QChar::ReplacementCharacter);
    if (utf8ReplacementCount == 0) {
        return utf8;
    }

    const QString local8Bit = QString::fromLocal8Bit(rawData);
    const qsizetype localReplacementCount = local8Bit.count(QChar::ReplacementCharacter);
    if (localReplacementCount < utf8ReplacementCount) {
        return local8Bit;
    }

    return utf8;
}

QString resolveSourceFilePath(const QString &sourceToken, const QDir &cueDir)
{
    const QString normalized = sourceToken.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    if (QDir::isAbsolutePath(normalized)) {
        return QDir::cleanPath(normalized);
    }

    if (normalized.size() >= 3 && normalized.at(1) == QLatin1Char(':')
        && (normalized.at(2) == QLatin1Char('\\') || normalized.at(2) == QLatin1Char('/'))) {
        return QDir::cleanPath(normalized);
    }

    return QDir::cleanPath(cueDir.filePath(normalized));
}

qint64 parseCueTimeMs(const QString &value)
{
    const QStringList parts = value.trimmed().split(QLatin1Char(':'));
    if (parts.size() != 3) {
        return -1;
    }

    bool okMinutes = false;
    bool okSeconds = false;
    bool okFrames = false;
    const int minutes = parts.at(0).toInt(&okMinutes);
    const int seconds = parts.at(1).toInt(&okSeconds);
    const int frames = parts.at(2).toInt(&okFrames);
    if (!okMinutes || !okSeconds || !okFrames || minutes < 0 || seconds < 0 || seconds >= 60 || frames < 0 || frames >= 75) {
        return -1;
    }

    const qint64 totalFrames = static_cast<qint64>(minutes) * 60 * 75
        + static_cast<qint64>(seconds) * 75
        + static_cast<qint64>(frames);
    return (totalFrames * 1000) / 75;
}

struct PendingTrack {
    int number = 0;
    bool isAudioTrack = true;
    int lineNumber = 0;
    QString sourceFilePath;
    QString title;
    QString performer;
    QString songwriter;
    qint64 index00Ms = -1;
    qint64 index01Ms = -1;

    qint64 startMs() const
    {
        return index01Ms >= 0 ? index01Ms : index00Ms;
    }
};
} // namespace

bool CueSheetParser::parseFile(const QString &cuePath,
                               QVector<CueTrackSegment> *segments,
                               QString *errorMessage)
{
    if (!segments) {
        return false;
    }

    QFile file(cuePath);
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CUE file does not exist");
        }
        segments->clear();
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open CUE file");
        }
        segments->clear();
        return false;
    }

    const QString content = decodeCueContent(file.readAll());
    file.close();
    QVector<PendingTrack> pending;
    PendingTrack currentTrack;
    bool hasCurrentTrack = false;
    auto flushCurrentTrack = [&pending, &currentTrack, &hasCurrentTrack]() {
        if (!hasCurrentTrack) {
            return;
        }
        if (currentTrack.number > 0) {
            pending.push_back(currentTrack);
        }
        currentTrack = {};
        hasCurrentTrack = false;
    };

    const QFileInfo cueInfo(cuePath);
    const QDir cueDir = cueInfo.absoluteDir();
    const QString cueSheetAbsolutePath = cueInfo.absoluteFilePath();

    QString globalTitle;
    QString remAlbumTitle;
    QString globalPerformer;
    QString globalSongwriter;
    QString currentSourceFilePath;
    QStringList diagnostics;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int lineNumber = lineIndex + 1;
        QString rawLine = lines.at(lineIndex).trimmed();
        if (rawLine.isEmpty()) {
            continue;
        }

        int cursor = 0;
        const QString command = readToken(rawLine, &cursor).toUpper();
        if (command.isEmpty()) {
            continue;
        }

        if (command == QStringLiteral("REM")) {
            const QString remKey = readToken(rawLine, &cursor).toUpper();
            const QString remValue = readCommandValue(rawLine, cursor);
            if (remValue.isEmpty()) {
                continue;
            }

            if (remKey == QStringLiteral("ALBUM") && !hasCurrentTrack && globalTitle.trimmed().isEmpty()) {
                remAlbumTitle = remValue;
            } else if (remKey == QStringLiteral("PERFORMER") && !hasCurrentTrack && globalPerformer.trimmed().isEmpty()) {
                globalPerformer = remValue;
            } else if (remKey == QStringLiteral("SONGWRITER") && !hasCurrentTrack && globalSongwriter.trimmed().isEmpty()) {
                globalSongwriter = remValue;
            }
            continue;
        }

        if (command == QStringLiteral("FILE")) {
            flushCurrentTrack();
            const QString sourceToken = parseTextPayload(rawLine, cursor, &diagnostics, lineNumber);
            if (sourceToken.isEmpty()) {
                diagnostics.push_back(QStringLiteral("line %1: FILE command missing source path").arg(lineNumber));
                currentSourceFilePath.clear();
            } else {
                currentSourceFilePath = resolveSourceFilePath(sourceToken, cueDir);
            }
            continue;
        }

        if (command == QStringLiteral("TRACK")) {
            flushCurrentTrack();
            currentTrack = {};
            hasCurrentTrack = true;
            currentTrack.sourceFilePath = currentSourceFilePath;
            currentTrack.lineNumber = lineNumber;

            const QString trackNumberToken = readToken(rawLine, &cursor);
            bool okTrack = false;
            const int trackNumber = trackNumberToken.toInt(&okTrack);
            if (!okTrack || trackNumber <= 0) {
                diagnostics.push_back(QStringLiteral("line %1: invalid TRACK number '%2'")
                                          .arg(lineNumber)
                                          .arg(trackNumberToken));
                currentTrack.number = 0;
            } else {
                currentTrack.number = trackNumber;
            }

            const QString trackType = readToken(rawLine, &cursor).toUpper().trimmed();
            if (!trackType.isEmpty() && trackType != QStringLiteral("AUDIO")) {
                currentTrack.isAudioTrack = false;
            }

            if (currentTrack.sourceFilePath.isEmpty()) {
                diagnostics.push_back(QStringLiteral("line %1: TRACK declared before FILE").arg(lineNumber));
            }
            continue;
        }

        if (command == QStringLiteral("TITLE")) {
            const QString title = readCommandValue(rawLine, cursor);
            if (hasCurrentTrack && currentTrack.number > 0) {
                currentTrack.title = title;
            } else {
                globalTitle = title;
            }
            continue;
        }

        if (command == QStringLiteral("PERFORMER")) {
            const QString performer = readCommandValue(rawLine, cursor);
            if (hasCurrentTrack && currentTrack.number > 0) {
                currentTrack.performer = performer;
            } else {
                globalPerformer = performer;
            }
            continue;
        }

        if (command == QStringLiteral("SONGWRITER")) {
            const QString songwriter = readCommandValue(rawLine, cursor);
            if (hasCurrentTrack && currentTrack.number > 0) {
                currentTrack.songwriter = songwriter;
            } else {
                globalSongwriter = songwriter;
            }
            continue;
        }

        if (command == QStringLiteral("INDEX")) {
            if (!hasCurrentTrack || currentTrack.number <= 0) {
                continue;
            }

            const QString indexToken = readToken(rawLine, &cursor);
            bool okIndex = false;
            const int indexNumber = indexToken.toInt(&okIndex);
            if (!okIndex || indexNumber < 0) {
                diagnostics.push_back(QStringLiteral("line %1: invalid INDEX token '%2'")
                                          .arg(lineNumber)
                                          .arg(indexToken));
                continue;
            }

            const QString timeToken = readToken(rawLine, &cursor).trimmed();
            if (timeToken.isEmpty()) {
                diagnostics.push_back(QStringLiteral("line %1: missing INDEX time").arg(lineNumber));
                continue;
            }

            const qint64 parsedMs = parseCueTimeMs(timeToken);
            if (parsedMs < 0) {
                diagnostics.push_back(QStringLiteral("line %1: invalid INDEX time '%2'")
                                          .arg(lineNumber)
                                          .arg(timeToken));
                continue;
            }

            if (indexNumber == 1) {
                currentTrack.index01Ms = parsedMs;
            } else if (indexNumber == 0 && currentTrack.index01Ms < 0) {
                currentTrack.index00Ms = parsedMs;
            }
            continue;
        }
    }

    flushCurrentTrack();

    const QString normalizedGlobalTitle = globalTitle.trimmed();
    const QString normalizedRemAlbum = remAlbumTitle.trimmed();
    const QString normalizedGlobalPerformer = globalPerformer.trimmed();
    const QString normalizedGlobalSongwriter = globalSongwriter.trimmed();

    segments->clear();
    segments->reserve(pending.size());
    QHash<QString, bool> sourceFileExistsCache;
    QHash<QString, qint64> lastAcceptedStartBySource;

    for (int i = 0; i < pending.size(); ++i) {
        const PendingTrack &node = pending.at(i);
        const qint64 nodeStartMs = node.startMs();

        if (node.number <= 0 || !node.isAudioTrack) {
            continue;
        }
        if (node.sourceFilePath.isEmpty()) {
            diagnostics.push_back(QStringLiteral("line %1: TRACK %2 has no source FILE")
                                      .arg(node.lineNumber)
                                      .arg(node.number, 2, 10, QLatin1Char('0')));
            continue;
        }
        if (nodeStartMs < 0) {
            diagnostics.push_back(QStringLiteral("line %1: TRACK %2 missing INDEX 01/00")
                                      .arg(node.lineNumber)
                                      .arg(node.number, 2, 10, QLatin1Char('0')));
            continue;
        }

        if (!sourceFileExistsCache.contains(node.sourceFilePath)) {
            sourceFileExistsCache.insert(node.sourceFilePath, QFileInfo::exists(node.sourceFilePath));
        }
        if (!sourceFileExistsCache.value(node.sourceFilePath)) {
            diagnostics.push_back(QStringLiteral("line %1: source file not found '%2'")
                                      .arg(node.lineNumber)
                                      .arg(node.sourceFilePath));
            continue;
        }

        if (lastAcceptedStartBySource.contains(node.sourceFilePath)
            && nodeStartMs <= lastAcceptedStartBySource.value(node.sourceFilePath)) {
            diagnostics.push_back(QStringLiteral("line %1: non-increasing start time for TRACK %2")
                                      .arg(node.lineNumber)
                                      .arg(node.number, 2, 10, QLatin1Char('0')));
            continue;
        }
        lastAcceptedStartBySource.insert(node.sourceFilePath, nodeStartMs);

        CueTrackSegment segment;
        segment.sourceFilePath = node.sourceFilePath;
        segment.cueSheetPath = cueSheetAbsolutePath;
        segment.trackNumber = node.number;
        segment.startMs = nodeStartMs;
        segment.title = node.title.trimmed();
        segment.performer = node.performer.trimmed();
        if (segment.performer.isEmpty()) {
            segment.performer = node.songwriter.trimmed();
        }
        segment.album = normalizedGlobalTitle.isEmpty() ? normalizedRemAlbum : normalizedGlobalTitle;
        if (segment.performer.isEmpty()) {
            segment.performer = normalizedGlobalPerformer.isEmpty() ? normalizedGlobalSongwriter : normalizedGlobalPerformer;
        }
        if (segment.title.isEmpty()) {
            segment.title = QStringLiteral("Track %1")
                                .arg(segment.trackNumber, 2, 10, QLatin1Char('0'));
        }
        segment.endMs = -1;

        for (int j = i + 1; j < pending.size(); ++j) {
            if (pending.at(j).sourceFilePath != node.sourceFilePath) {
                continue;
            }
            const qint64 nextStartMs = pending.at(j).startMs();
            if (nextStartMs > nodeStartMs) {
                segment.endMs = nextStartMs;
                break;
            }
        }

        segments->push_back(std::move(segment));
    }

    if (segments->isEmpty()) {
        if (errorMessage) {
            QString message = QStringLiteral("No playable tracks found in CUE");
            if (!diagnostics.isEmpty()) {
                message.append(QStringLiteral(": %1").arg(diagnostics.constFirst()));
            }
            *errorMessage = std::move(message);
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}
