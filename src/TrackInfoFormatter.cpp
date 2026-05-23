#include "TrackInfoFormatter.h"

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace WaveFlux {
namespace {

struct Node;
using NodeList = QList<Node>;

struct Node {
    enum class Type {
        Literal,
        Placeholder,
        Conditional
    };

    Type type = Type::Literal;
    QString text;
    QChar placeholder;
    QList<NodeList> alternatives;
};

struct RenderResult {
    QString text;
    bool available = true;
};

QString trimInsignificantZeros(QString value)
{
    while (value.contains(QLatin1Char('.')) && value.endsWith(QLatin1Char('0'))) {
        value.chop(1);
    }
    if (value.endsWith(QLatin1Char('.'))) {
        value.chop(1);
    }
    return value;
}

QString formatDurationHhMmSs(qint64 ms)
{
    if (ms < 0) {
        return {};
    }

    const qint64 totalSeconds = ms / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatPlaybackTime(qint64 ms)
{
    if (ms < 0) {
        return {};
    }

    const qint64 totalTenths = ms / 100;
    const qint64 totalSeconds = totalTenths / 10;
    const qint64 tenths = totalTenths % 10;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(tenths);
    }

    return QStringLiteral("%1:%2.%3")
        .arg(minutes)
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(tenths);
}

QString formatSignedPlaybackDelta(qint64 ms)
{
    if (ms == std::numeric_limits<qint64>::min()) {
        return {};
    }

    const QChar sign = ms >= 0 ? QLatin1Char('+') : QLatin1Char('-');
    return QString(sign) + formatPlaybackTime(std::llabs(ms));
}

bool nonEmpty(const QString &value)
{
    return !value.trimmed().isEmpty();
}

RenderResult placeholderValue(QChar token,
                              const TrackInfoFormatter::TrackInfoContext &track,
                              TrackInfoFormatter::RenderContext renderContext)
{
    using RenderContext = TrackInfoFormatter::RenderContext;

    auto unavailable = []() -> RenderResult {
        return {QString(), false};
    };
    auto stringValue = [](const QString &value) -> RenderResult {
        return {value, nonEmpty(value)};
    };
    auto positiveIntValue = [](int value) -> RenderResult {
        return {value > 0 ? QString::number(value) : QString(), value > 0};
    };

    switch (token.unicode()) {
    case 'a':
        return stringValue(track.artist);
    case 't':
        return stringValue(track.title);
    case 'A':
        return stringValue(track.album);
    case 'c':
        return stringValue(track.comment);
    case 'g':
        return stringValue(track.genre);
    case 'y':
        return stringValue(track.year);
    case 'n':
        return stringValue(track.trackNumber);
    case 'i':
        if (renderContext == RenderContext::Playlist && track.playlistIndex >= 0) {
            return {QString::number(track.playlistIndex + 1), true};
        }
        return unavailable();
    case 'T':
        if (renderContext == RenderContext::WaveformOverlay && track.positionMs >= 0) {
            return {formatPlaybackTime(track.positionMs), true};
        }
        return unavailable();
    case 'r':
        if (renderContext == RenderContext::WaveformOverlay
            && track.durationMs >= 0
            && track.positionMs >= 0) {
            return {formatPlaybackTime(qMax<qint64>(0, track.durationMs - track.positionMs)), true};
        }
        return unavailable();
    case 'C':
        if (renderContext == RenderContext::WaveformTooltip && track.hoverPositionMs >= 0) {
            return {formatPlaybackTime(track.hoverPositionMs), true};
        }
        return unavailable();
    case 'o':
        if (renderContext == RenderContext::WaveformTooltip
            && track.hoverPositionMs >= 0
            && track.positionMs >= 0) {
            return {formatSignedPlaybackDelta(track.hoverPositionMs - track.positionMs), true};
        }
        return unavailable();
    case 'd':
        return {formatDurationHhMmSs(track.durationMs), track.durationMs >= 0};
    case 'D':
        if (track.durationMs >= 0) {
            return {QString::number(track.durationMs / 1000), true};
        }
        return unavailable();
    case 'L':
        return {formatDurationHhMmSs(track.playlistDurationMs), track.playlistDurationMs >= 0};
    case 'b':
        return positiveIntValue(track.bitDepth);
    case 'B':
        return positiveIntValue(track.bitrateKbps);
    case 's':
        if (track.sampleRateHz > 0) {
            return {trimInsignificantZeros(QString::number(track.sampleRateHz / 1000.0, 'f', 1)), true};
        }
        return unavailable();
    case 'H':
        return positiveIntValue(track.channelCount);
    case 'M':
        return positiveIntValue(track.bpm);
    case 'f': {
        const QFileInfo info(track.filePath);
        return stringValue(info.completeBaseName());
    }
    case 'F': {
        const QFileInfo info(track.filePath);
        return stringValue(info.fileName());
    }
    case 'p':
        return stringValue(track.filePath);
    case 'P': {
        const QFileInfo info(track.filePath);
        return stringValue(QDir::toNativeSeparators(info.absolutePath()));
    }
    case 'N': {
        const QFileInfo info(track.filePath);
        return stringValue(info.absoluteDir().dirName());
    }
    case 'e': {
        const QFileInfo info(track.filePath);
        return stringValue(info.suffix().toLower());
    }
    case 'E': {
        const QFileInfo info(track.filePath);
        return stringValue(info.suffix().toUpper());
    }
    case 'v':
        return stringValue(track.appVersion);
    default:
        return unavailable();
    }
}

class Parser
{
public:
    explicit Parser(QString format)
        : m_format(std::move(format))
    {
    }

    NodeList parse()
    {
        return parseSequence({});
    }

private:
    NodeList parseSequence(const QString &terminators)
    {
        NodeList nodes;
        QString literal;

        auto flushLiteral = [&]() {
            if (literal.isEmpty()) {
                return;
            }
            Node node;
            node.type = Node::Type::Literal;
            node.text = literal;
            nodes.push_back(node);
            literal.clear();
        };

        while (m_pos < m_format.size()) {
            const QChar ch = m_format.at(m_pos);
            if (terminators.contains(ch)) {
                break;
            }

            if (ch == QLatin1Char('\\')) {
                ++m_pos;
                if (m_pos >= m_format.size()) {
                    literal.append(QLatin1Char('\\'));
                    break;
                }

                const QChar escaped = m_format.at(m_pos);
                if (escaped == QLatin1Char('{')
                    || escaped == QLatin1Char('}')
                    || escaped == QLatin1Char('|')
                    || escaped == QLatin1Char('%')) {
                    literal.append(escaped);
                } else {
                    literal.append(QLatin1Char('\\'));
                    literal.append(escaped);
                }
                ++m_pos;
                continue;
            }

            if (ch == QLatin1Char('%')) {
                flushLiteral();
                ++m_pos;
                if (m_pos >= m_format.size()) {
                    literal.append(QLatin1Char('%'));
                    break;
                }
                Node node;
                node.type = Node::Type::Placeholder;
                node.placeholder = m_format.at(m_pos);
                nodes.push_back(node);
                ++m_pos;
                continue;
            }

            if (ch == QLatin1Char('{')) {
                flushLiteral();
                nodes.push_back(parseConditional());
                continue;
            }

            literal.append(ch);
            ++m_pos;
        }

        flushLiteral();
        return nodes;
    }

    Node parseConditional()
    {
        Node node;
        node.type = Node::Type::Conditional;
        ++m_pos;

        while (m_pos < m_format.size()) {
            node.alternatives.push_back(parseSequence(QStringLiteral("|}")));

            if (m_pos >= m_format.size()) {
                break;
            }

            const QChar terminator = m_format.at(m_pos);
            ++m_pos;
            if (terminator == QLatin1Char('}')) {
                break;
            }
        }

        if (node.alternatives.isEmpty()) {
            node.alternatives.push_back({});
        }
        return node;
    }

    QString m_format;
    int m_pos = 0;
};

RenderResult renderNodes(const NodeList &nodes,
                         const TrackInfoFormatter::TrackInfoContext &track,
                         TrackInfoFormatter::RenderContext renderContext,
                         bool requireDirectPlaceholders)
{
    RenderResult result;

    for (const Node &node : nodes) {
        switch (node.type) {
        case Node::Type::Literal:
            result.text += node.text;
            break;
        case Node::Type::Placeholder: {
            const RenderResult value = placeholderValue(node.placeholder, track, renderContext);
            if (!value.available) {
                if (requireDirectPlaceholders) {
                    result.available = false;
                }
                continue;
            }
            result.text += value.text;
            break;
        }
        case Node::Type::Conditional: {
            for (const NodeList &alternative : node.alternatives) {
                const RenderResult branch = renderNodes(alternative, track, renderContext, true);
                if (!branch.available || branch.text.isEmpty()) {
                    continue;
                }
                result.text += branch.text;
                break;
            }
            break;
        }
        }
    }

    return result;
}

} // namespace

QString TrackInfoFormatter::render(const QString &format,
                                   const TrackInfoContext &track,
                                   RenderContext renderContext)
{
    Parser parser(format);
    return renderNodes(parser.parse(), track, renderContext, false).text;
}

} // namespace WaveFlux
