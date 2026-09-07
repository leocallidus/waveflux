#pragma once

#include "LyricsTypes.h"
#include <QAbstractListModel>
#include <vector>

namespace WaveFlux::Lyrics {

class LyricsLineModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool isSynced READ isSynced NOTIFY isSyncedChanged)

public:
    enum LineRoles {
        TextRole = Qt::UserRole + 1,
        TimestampMsRole,
        RawTimestampMsRole,
        IsCurrentRole,
        IsPastRole,
        IsBlankRole,
        CanSeekRole
    };
    Q_ENUM(LineRoles)

    explicit LyricsLineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const { return m_currentIndex; }
    int count() const { return static_cast<int>(m_lines.size()); }
    bool isSynced() const { return m_isSynced; }

    void setCues(const std::vector<CueGroup> &cues, qint64 lrcOffsetMs, qint64 userDelayMs);
    void setPlainText(const QString &text);
    void clear();

    void setActiveLine(int index);
    void setPlaybackProgress(int activeIndex, int progressIndex);
    void updateTiming(qint64 lrcOffsetMs, qint64 userDelayMs);
    qint64 cueTimestampAt(int index) const;

signals:
    void currentIndexChanged();
    void countChanged();
    void isSyncedChanged();

private:
    struct LineItem {
        qint64 rawCueMs = -1;
        qint64 effectiveCueMs = -1;
        QString text;
        bool isBlank = false;
        bool canSeek = false;
    };

    std::vector<LineItem> m_lines;
    int m_currentIndex = -1;
    int m_progressIndex = -1;
    bool m_isSynced = false;
};

} // namespace WaveFlux::Lyrics
