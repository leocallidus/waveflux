#include "LrcParser.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStringDecoder>
#include <algorithm>

namespace WaveFlux::Lyrics {

bool LrcParser::parseTimestamp(const QString &tag, qint64 &outMs)
{
    // Matches mm:ss, mm:ss.xx, mm:ss.xxx, mm:ss:xx
    const QStringList colonParts = tag.split(u':');
    if (colonParts.size() < 2) {
        return false;
    }

    bool ok = false;
    const qint64 minutes = colonParts[0].trimmed().toLongLong(&ok);
    if (!ok || minutes < 0) {
        return false;
    }

    QString secPart = colonParts[1].trimmed();
    QString fracPart;

    if (colonParts.size() == 3) {
        // Form: mm:ss:xx
        secPart = colonParts[1].trimmed();
        fracPart = colonParts[2].trimmed();
    } else {
        const int dotIdx = secPart.indexOf(u'.');
        if (dotIdx >= 0) {
            fracPart = secPart.mid(dotIdx + 1);
            secPart = secPart.left(dotIdx);
        }
    }

    const qint64 seconds = secPart.toLongLong(&ok);
    if (!ok || seconds < 0 || seconds >= 60) {
        return false;
    }

    qint64 fractionMs = 0;
    if (!fracPart.isEmpty()) {
        if (fracPart.length() == 1) {
            fractionMs = fracPart.toLongLong(&ok) * 100;
        } else if (fracPart.length() == 2) {
            fractionMs = fracPart.toLongLong(&ok) * 10;
        } else {
            fractionMs = fracPart.left(3).toLongLong(&ok);
            // If less than 3 digits after left(3)
            if (fracPart.left(3).length() == 1) fractionMs *= 100;
            else if (fracPart.left(3).length() == 2) fractionMs *= 10;
        }
        if (!ok) {
            fractionMs = 0;
        }
    }

    outMs = (minutes * 60 * 1000) + (seconds * 1000) + fractionMs;
    return true;
}

QString LrcParser::stripEnhancedTags(const QString &line)
{
    // Strip word timestamps like <00:12.34>
    static const QRegularExpression wordTsRegex(QStringLiteral(R"(<[0-9]{1,2}:[0-9]{2}(?:[\.:][0-9]{1,3})?>)"));
    return QString(line).remove(wordTsRegex);
}

ParsedLyrics LrcParser::parse(const QByteArray &data)
{
    ParsedLyrics res;
    if (data.isEmpty()) {
        res.error = QStringLiteral("Empty data");
        return res;
    }
    if (data.size() > kMaxLrcFileSize) {
        res.error = QStringLiteral("Payload exceeds maximum size (256 KiB)");
        return res;
    }

    res.contentHash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());

    QString text;
    if (data.startsWith("\xEF\xBB\xBF")) {
        text = QString::fromUtf8(data.constData() + 3, data.size() - 3);
    } else if (data.startsWith("\xFF\xFE")) {
        auto decoder = QStringDecoder(QStringDecoder::Utf16LE);
        text = decoder(data.mid(2));
    } else if (data.startsWith("\xFE\xFF")) {
        auto decoder = QStringDecoder(QStringDecoder::Utf16BE);
        text = decoder(data.mid(2));
    } else {
        // Quick binary check
        if (data.contains('\0')) {
            res.error = QStringLiteral("Binary data detected");
            return res;
        }
        text = QString::fromUtf8(data);
    }

    static const QRegularExpression metaRegex(QStringLiteral(R"(^\[([a-zA-Z]+)\s*:\s*(.*)\]$)"));
    static const QRegularExpression tsRegex(QStringLiteral(R"(\[(\d{1,2}:\d{2}(?:[\.:]\d{1,3})?)\])"));

    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = text.split(QLatin1Char('\n'));

    std::vector<CueGroup> rawCues;

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        // Check if metadata tag
        const auto metaMatch = metaRegex.match(line);
        if (metaMatch.hasMatch()) {
            const QString tag = metaMatch.captured(1).toLower();
            const QString val = metaMatch.captured(2).trimmed();
            if (tag == QLatin1String("offset")) {
                bool ok = false;
                const qint64 off = val.toLongLong(&ok);
                if (ok) {
                    res.offsetMs = off;
                }
            } else if (tag == QLatin1String("ti") || tag == QLatin1String("title")) {
                res.title = val;
            } else if (tag == QLatin1String("ar") || tag == QLatin1String("artist")) {
                res.artist = val;
            } else if (tag == QLatin1String("al") || tag == QLatin1String("album")) {
                res.album = val;
            }
            continue;
        }

        // Search for timestamp tags
        QRegularExpressionMatchIterator it = tsRegex.globalMatch(line);
        QList<qint64> timestamps;
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            qint64 ms = 0;
            if (parseTimestamp(m.captured(1), ms)) {
                timestamps.append(ms);
            }
        }

        if (!timestamps.isEmpty()) {
            // Strip out timestamp tags from line to get text
            QString cueText = line;
            cueText.remove(tsRegex);
            cueText = stripEnhancedTags(cueText).trimmed();

            const bool isBlank = cueText.isEmpty();
            for (qint64 ms : timestamps) {
                if (rawCues.size() >= kMaxLrcCues) {
                    break;
                }
                CueGroup cg;
                cg.rawCueMs = ms;
                cg.effectiveCueMs = std::max<qint64>(0, ms - res.offsetMs);
                cg.text = cueText;
                cg.isBlank = isBlank;
                rawCues.push_back(cg);
            }
        }
    }

    if (!rawCues.empty()) {
        // Sort stably by rawCueMs
        std::stable_sort(rawCues.begin(), rawCues.end(), [](const CueGroup &a, const CueGroup &b) {
            return a.rawCueMs < b.rawCueMs;
        });

        // Consolidate identical timestamps
        std::vector<CueGroup> consolidated;
        consolidated.reserve(rawCues.size());
        for (const auto &cue : rawCues) {
            if (!consolidated.empty() && consolidated.back().rawCueMs == cue.rawCueMs) {
                if (consolidated.back().isBlank && !cue.isBlank) {
                    consolidated.back() = cue;
                } else if (!consolidated.back().isBlank && !cue.isBlank) {
                    consolidated.back().text += QStringLiteral(" / ") + cue.text;
                }
            } else {
                consolidated.push_back(cue);
            }
        }

        res.cues = std::move(consolidated);
        res.kind = LyricsKind::Synced;
        res.valid = true;

        // Also build plainText representation for fallback or copying
        QStringList fullPlain;
        for (const auto &c : res.cues) {
            if (!c.isBlank) {
                fullPlain.append(c.text);
            }
        }
        res.plainText = fullPlain.join(QLatin1Char('\n'));
    } else if (!text.trimmed().isEmpty()) {
        return parsePlainText(text);
    } else {
        res.valid = false;
        res.error = QStringLiteral("No lyrics content found");
    }

    return res;
}

ParsedLyrics LrcParser::parsePlainText(const QString &text)
{
    ParsedLyrics res;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        res.error = QStringLiteral("Empty text");
        return res;
    }

    const QByteArray data = trimmed.toUtf8();
    res.contentHash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());

    const QString lower = trimmed.toLower();
    if (lower == QLatin1String("[instrumental]") || lower == QLatin1String("(instrumental)") || lower == QLatin1String("instrumental")) {
        res.kind = LyricsKind::Instrumental;
    } else {
        res.kind = LyricsKind::Plain;
    }
    res.plainText = trimmed;
    res.valid = true;
    return res;
}

} // namespace WaveFlux::Lyrics
