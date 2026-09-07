#include "LyricsLineModel.h"
#include <algorithm>

namespace WaveFlux::Lyrics {

LyricsLineModel::LyricsLineModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LyricsLineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_lines.size());
}

QVariant LyricsLineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_lines.size())) {
        return QVariant();
    }

    const auto &item = m_lines[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return item.text;
    case TimestampMsRole:
        return item.effectiveCueMs;
    case RawTimestampMsRole:
        return item.rawCueMs;
    case IsCurrentRole:
        return index.row() == m_currentIndex;
    case IsPastRole:
        return index.row() < m_progressIndex;
    case IsBlankRole:
        return item.isBlank;
    case CanSeekRole:
        return item.canSeek;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LyricsLineModel::roleNames() const
{
    return {
        { TextRole, "text" },
        { TimestampMsRole, "timestampMs" },
        { RawTimestampMsRole, "rawTimestampMs" },
        { IsCurrentRole, "isCurrent" },
        { IsPastRole, "isPast" },
        { IsBlankRole, "isBlank" },
        { CanSeekRole, "canSeek" }
    };
}

void LyricsLineModel::setCues(const std::vector<CueGroup> &cues, qint64 lrcOffsetMs, qint64 userDelayMs)
{
    beginResetModel();
    m_lines.clear();
    m_lines.reserve(cues.size());
    for (const auto &c : cues) {
        LineItem item;
        item.rawCueMs = c.rawCueMs;
        item.effectiveCueMs = std::max<qint64>(0, c.rawCueMs - lrcOffsetMs + userDelayMs);
        item.text = c.text;
        item.isBlank = c.isBlank;
        item.canSeek = true;
        m_lines.push_back(item);
    }
    m_currentIndex = -1;
    m_progressIndex = -1;
    m_isSynced = true;
    endResetModel();

    emit countChanged();
    emit isSyncedChanged();
    emit currentIndexChanged();
}

void LyricsLineModel::setPlainText(const QString &text)
{
    beginResetModel();
    m_lines.clear();
    const QStringList rawLines = text.split(QLatin1Char('\n'));
    m_lines.reserve(rawLines.size());
    for (const QString &l : rawLines) {
        LineItem item;
        item.rawCueMs = -1;
        item.effectiveCueMs = -1;
        item.text = l;
        item.isBlank = l.trimmed().isEmpty();
        item.canSeek = false;
        m_lines.push_back(item);
    }
    m_currentIndex = -1;
    m_progressIndex = -1;
    m_isSynced = false;
    endResetModel();

    emit countChanged();
    emit isSyncedChanged();
    emit currentIndexChanged();
}

void LyricsLineModel::clear()
{
    if (m_lines.empty() && m_currentIndex == -1 && !m_isSynced) {
        return;
    }
    beginResetModel();
    m_lines.clear();
    m_currentIndex = -1;
    m_progressIndex = -1;
    m_isSynced = false;
    endResetModel();

    emit countChanged();
    emit isSyncedChanged();
    emit currentIndexChanged();
}

void LyricsLineModel::setActiveLine(int index)
{
    setPlaybackProgress(index, index);
}

void LyricsLineModel::setPlaybackProgress(int activeIndex, int progressIndex)
{
    activeIndex = activeIndex >= 0 && activeIndex < count() ? activeIndex : -1;
    progressIndex = std::clamp(progressIndex, -1, count());
    if (m_currentIndex == activeIndex && m_progressIndex == progressIndex) {
        return;
    }

    const int oldIndex = m_currentIndex;
    const int firstChanged = std::max(0, std::min({oldIndex, activeIndex, m_progressIndex, progressIndex}));
    const int lastChanged = std::min(count() - 1, std::max({oldIndex, activeIndex, m_progressIndex, progressIndex}));
    m_currentIndex = activeIndex;
    m_progressIndex = progressIndex;
    if (firstChanged <= lastChanged) {
        emit dataChanged(index(firstChanged), index(lastChanged), {IsCurrentRole, IsPastRole});
    }

    if (oldIndex != m_currentIndex) {
        emit currentIndexChanged();
    }
}

void LyricsLineModel::updateTiming(qint64 lrcOffsetMs, qint64 userDelayMs)
{
    if (m_lines.empty() || !m_isSynced) {
        return;
    }

    for (auto &item : m_lines) {
        if (item.rawCueMs >= 0) {
            item.effectiveCueMs = std::max<qint64>(0, item.rawCueMs - lrcOffsetMs + userDelayMs);
        }
    }

    emit dataChanged(index(0), index(static_cast<int>(m_lines.size()) - 1), {TimestampMsRole});
}

qint64 LyricsLineModel::cueTimestampAt(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_lines.size())) {
        return m_lines[index].effectiveCueMs;
    }
    return -1;
}

} // namespace WaveFlux::Lyrics
