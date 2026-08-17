#include "PlaylistColumnLayoutManager.h"
#include "AppSettingsManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDateTime>
#include <QLocale>
#include <algorithm>

namespace {
constexpr int kSchemaVersion = 1;
constexpr auto kNormalColumnsKey = QLatin1StringView("ui/normalPlaylistColumns_v1");
constexpr auto kCompactColumnsKey = QLatin1StringView("ui/compactPlaylistColumns_v1");
constexpr auto kCompactHeaderModeKey = QLatin1StringView("ui/compactHeaderMode_v1");

const QVector<PlaylistColumnLayoutManager::ColumnDescriptor> &initCatalog()
{
    static const QVector<PlaylistColumnLayoutManager::ColumnDescriptor> catalog = {
        {
            QStringLiteral("playlistPosition"),
            QStringLiteral("columns.playlistPosition"),
            QStringLiteral("position"),
            QStringLiteral("playlistPosition"),
            46, 32, 60, 0.0,
            QStringLiteral("right"),
            true, 0, 0,
            PlaylistColumnLayoutManager::VisibilityMode::Shown, 1,
            PlaylistColumnLayoutManager::VisibilityMode::Shown, 1
        },
        {
            QStringLiteral("trackSummary"),
            QStringLiteral("columns.trackSummary"),
            QStringLiteral("summary"),
            QStringLiteral("trackSummary"),
            220, 100, 0, 2.0,
            QStringLiteral("left"),
            true, 0, 0,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Shown, 2
        },
        {
            QStringLiteral("title"),
            QStringLiteral("columns.title"),
            QStringLiteral("text"),
            QStringLiteral("title"),
            200, 80, 0, 2.0,
            QStringLiteral("left"),
            true, 0, 0,
            PlaylistColumnLayoutManager::VisibilityMode::Shown, 2,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("artist"),
            QStringLiteral("columns.artist"),
            QStringLiteral("text"),
            QStringLiteral("artist"),
            140, 70, 0, 1.0,
            QStringLiteral("left"),
            true, 1, 480,
            PlaylistColumnLayoutManager::VisibilityMode::Automatic, 3,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("album"),
            QStringLiteral("columns.album"),
            QStringLiteral("text"),
            QStringLiteral("album"),
            140, 70, 0, 1.0,
            QStringLiteral("left"),
            true, 3, 740,
            PlaylistColumnLayoutManager::VisibilityMode::Automatic, 4,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("duration"),
            QStringLiteral("columns.duration"),
            QStringLiteral("duration"),
            QStringLiteral("duration"),
            64, 48, 90, 0.0,
            QStringLiteral("right"),
            true, 0, 0,
            PlaylistColumnLayoutManager::VisibilityMode::Shown, 5,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("bitrate"),
            QStringLiteral("columns.bitrate"),
            QStringLiteral("number"),
            QStringLiteral("bitrate"),
            74, 54, 100, 0.0,
            QStringLiteral("right"),
            true, 2, 620,
            PlaylistColumnLayoutManager::VisibilityMode::Automatic, 6,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("trackNumber"),
            QStringLiteral("columns.trackNumber"),
            QStringLiteral("trackNumber"),
            QStringLiteral("trackNumber"),
            48, 36, 70, 0.0,
            QStringLiteral("right"),
            true, 4, 500,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("year"),
            QStringLiteral("columns.year"),
            QStringLiteral("year"),
            QStringLiteral("year"),
            56, 44, 80, 0.0,
            QStringLiteral("right"),
            true, 5, 520,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("genre"),
            QStringLiteral("columns.genre"),
            QStringLiteral("text"),
            QStringLiteral("genre"),
            100, 60, 0, 1.0,
            QStringLiteral("left"),
            true, 6, 550,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("description"),
            QStringLiteral("columns.description"),
            QStringLiteral("text"),
            QStringLiteral("description"),
            160, 80, 0, 1.5,
            QStringLiteral("left"),
            true, 7, 600,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("composer"),
            QStringLiteral("columns.composer"),
            QStringLiteral("text"),
            QStringLiteral("composer"),
            120, 70, 0, 1.0,
            QStringLiteral("left"),
            true, 8, 620,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("originalArtist"),
            QStringLiteral("columns.originalArtist"),
            QStringLiteral("text"),
            QStringLiteral("originalArtist"),
            120, 70, 0, 1.0,
            QStringLiteral("left"),
            true, 9, 640,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("copyright"),
            QStringLiteral("columns.copyright"),
            QStringLiteral("text"),
            QStringLiteral("copyright"),
            140, 70, 0, 1.0,
            QStringLiteral("left"),
            true, 10, 660,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("url"),
            QStringLiteral("columns.url"),
            QStringLiteral("url"),
            QStringLiteral("url"),
            160, 80, 0, 1.5,
            QStringLiteral("left"),
            true, 11, 680,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("encoder"),
            QStringLiteral("columns.encoder"),
            QStringLiteral("text"),
            QStringLiteral("encoder"),
            110, 60, 0, 0.8,
            QStringLiteral("left"),
            true, 12, 700,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("format"),
            QStringLiteral("columns.format"),
            QStringLiteral("text"),
            QStringLiteral("format"),
            64, 48, 90, 0.0,
            QStringLiteral("center"),
            true, 13, 720,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("sampleRate"),
            QStringLiteral("columns.sampleRate"),
            QStringLiteral("number"),
            QStringLiteral("sampleRate"),
            76, 54, 100, 0.0,
            QStringLiteral("right"),
            true, 14, 740,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("bitDepth"),
            QStringLiteral("columns.bitDepth"),
            QStringLiteral("number"),
            QStringLiteral("bitDepth"),
            60, 48, 80, 0.0,
            QStringLiteral("right"),
            true, 15, 760,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("bpm"),
            QStringLiteral("columns.bpm"),
            QStringLiteral("number"),
            QStringLiteral("bpm"),
            54, 44, 80, 0.0,
            QStringLiteral("right"),
            true, 16, 780,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("channelCount"),
            QStringLiteral("columns.channelCount"),
            QStringLiteral("number"),
            QStringLiteral("channelCount"),
            64, 50, 90, 0.0,
            QStringLiteral("right"),
            true, 17, 800,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("fileName"),
            QStringLiteral("columns.fileName"),
            QStringLiteral("text"),
            QStringLiteral("fileName"),
            160, 80, 0, 1.5,
            QStringLiteral("left"),
            true, 18, 820,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("filePath"),
            QStringLiteral("columns.filePath"),
            QStringLiteral("text"),
            QStringLiteral("filePath"),
            200, 100, 0, 2.0,
            QStringLiteral("left"),
            true, 19, 840,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        },
        {
            QStringLiteral("dateAdded"),
            QStringLiteral("columns.dateAdded"),
            QStringLiteral("date"),
            QStringLiteral("dateAdded"),
            110, 80, 140, 0.0,
            QStringLiteral("left"),
            true, 20, 860,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99,
            PlaylistColumnLayoutManager::VisibilityMode::Hidden, 99
        }
    };
    return catalog;
}
} // namespace

PlaylistColumnLayoutManager::PlaylistColumnLayoutManager(QObject *parent)
    : QObject(parent)
{
    m_normalColumns = defaultNormalConfig();
    m_compactColumns = defaultCompactConfig();
    loadSettings();
}

PlaylistColumnLayoutManager::~PlaylistColumnLayoutManager() = default;

const QVector<PlaylistColumnLayoutManager::ColumnDescriptor> &PlaylistColumnLayoutManager::catalogDescriptors()
{
    return initCatalog();
}

const PlaylistColumnLayoutManager::ColumnDescriptor *PlaylistColumnLayoutManager::findDescriptor(const QString &id)
{
    const auto &catalog = catalogDescriptors();
    for (const auto &desc : catalog) {
        if (desc.id == id) {
            return &desc;
        }
    }
    return nullptr;
}

QString PlaylistColumnLayoutManager::visibilityModeToString(VisibilityMode mode)
{
    switch (mode) {
    case VisibilityMode::Shown:
        return QStringLiteral("shown");
    case VisibilityMode::Automatic:
        return QStringLiteral("automatic");
    case VisibilityMode::Hidden:
    default:
        return QStringLiteral("hidden");
    }
}

PlaylistColumnLayoutManager::VisibilityMode PlaylistColumnLayoutManager::visibilityModeFromString(
    const QString &str, VisibilityMode fallback)
{
    const QString lower = str.trimmed().toLower();
    if (lower == QStringLiteral("shown")) {
        return VisibilityMode::Shown;
    }
    if (lower == QStringLiteral("automatic") || lower == QStringLiteral("auto")) {
        return VisibilityMode::Automatic;
    }
    if (lower == QStringLiteral("hidden") || lower == QStringLiteral("hide")) {
        return VisibilityMode::Hidden;
    }
    return fallback;
}

QVector<PlaylistColumnLayoutManager::ColumnConfig> PlaylistColumnLayoutManager::defaultNormalConfig()
{
    const auto &catalog = catalogDescriptors();
    QVector<ColumnConfig> config;
    config.reserve(catalog.size());

    QVector<const ColumnDescriptor *> sortedCatalog;
    for (const auto &desc : catalog) {
        sortedCatalog.push_back(&desc);
    }
    std::stable_sort(sortedCatalog.begin(), sortedCatalog.end(), [](const ColumnDescriptor *a, const ColumnDescriptor *b) {
        return a->normalDefaultOrder < b->normalDefaultOrder;
    });

    for (const auto *desc : sortedCatalog) {
        config.push_back({desc->id, desc->normalDefaultMode, desc->defaultWidth});
    }
    return config;
}

QVector<PlaylistColumnLayoutManager::ColumnConfig> PlaylistColumnLayoutManager::defaultCompactConfig()
{
    const auto &catalog = catalogDescriptors();
    QVector<ColumnConfig> config;
    config.reserve(catalog.size());

    QVector<const ColumnDescriptor *> sortedCatalog;
    for (const auto &desc : catalog) {
        sortedCatalog.push_back(&desc);
    }
    std::stable_sort(sortedCatalog.begin(), sortedCatalog.end(), [](const ColumnDescriptor *a, const ColumnDescriptor *b) {
        return a->compactDefaultOrder < b->compactDefaultOrder;
    });

    for (const auto *desc : sortedCatalog) {
        config.push_back({desc->id, desc->compactDefaultMode, desc->defaultWidth});
    }
    return config;
}

QVector<PlaylistColumnLayoutManager::ColumnConfig> PlaylistColumnLayoutManager::sanitizeConfig(
    const QVector<ColumnConfig> &rawConfig, const QVector<ColumnConfig> &defaultConfig)
{
    QVector<ColumnConfig> sanitized;
    QSet<QString> seenIds;

    for (const auto &item : rawConfig) {
        if (seenIds.contains(item.id)) {
            continue;
        }
        const ColumnDescriptor *desc = findDescriptor(item.id);
        if (!desc) {
            continue; // ignore unknown IDs
        }
        seenIds.insert(item.id);
        sanitized.push_back(item);
    }

    // Append any missing catalog descriptors in their default order as hidden
    for (const auto &def : defaultConfig) {
        if (!seenIds.contains(def.id)) {
            const ColumnDescriptor *desc = findDescriptor(def.id);
            int width = desc ? desc->defaultWidth : 100;
            sanitized.push_back({def.id, VisibilityMode::Hidden, width});
            seenIds.insert(def.id);
        }
    }

    return sanitized;
}

QVariantList PlaylistColumnLayoutManager::configToVariantList(const QVector<ColumnConfig> &config)
{
    QVariantList list;
    list.reserve(config.size());
    for (const auto &item : config) {
        const ColumnDescriptor *desc = findDescriptor(item.id);
        QVariantMap map;
        map.insert(QStringLiteral("id"), item.id);
        map.insert(QStringLiteral("visibility"), visibilityModeToString(item.visibility));
        map.insert(QStringLiteral("width"), item.width);
        if (desc) {
            map.insert(QStringLiteral("translationKey"), desc->translationKey);
            map.insert(QStringLiteral("valueKind"), desc->valueKind);
            map.insert(QStringLiteral("roleName"), desc->roleName);
            map.insert(QStringLiteral("defaultWidth"), desc->defaultWidth);
            map.insert(QStringLiteral("minimumWidth"), desc->minimumWidth);
            map.insert(QStringLiteral("maximumWidth"), desc->maximumWidth);
            map.insert(QStringLiteral("stretchWeight"), desc->stretchWeight);
            map.insert(QStringLiteral("alignment"), desc->alignment);
            map.insert(QStringLiteral("sortable"), desc->sortable);
            map.insert(QStringLiteral("automaticPriority"), desc->automaticPriority);
            map.insert(QStringLiteral("automaticMinWidth"), desc->automaticMinWidth);
        }
        list.push_back(map);
    }
    return list;
}

QVector<PlaylistColumnLayoutManager::ColumnConfig> PlaylistColumnLayoutManager::variantListToConfig(
    const QVariantList &list, const QVector<ColumnConfig> &fallback)
{
    QVector<ColumnConfig> raw;
    for (const auto &var : list) {
        const QVariantMap map = var.toMap();
        const QString id = map.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty()) continue;
        const QString visStr = map.value(QStringLiteral("visibility")).toString();
        const VisibilityMode mode = visibilityModeFromString(visStr, VisibilityMode::Hidden);
        const int width = map.value(QStringLiteral("width"), 0).toInt();
        raw.push_back({id, mode, width});
    }
    return sanitizeConfig(raw, fallback);
}

QVariantList PlaylistColumnLayoutManager::catalog() const
{
    QVariantList list;
    const auto &catalog = catalogDescriptors();
    list.reserve(catalog.size());
    for (const auto &desc : catalog) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), desc.id);
        map.insert(QStringLiteral("translationKey"), desc.translationKey);
        map.insert(QStringLiteral("valueKind"), desc.valueKind);
        map.insert(QStringLiteral("roleName"), desc.roleName);
        map.insert(QStringLiteral("defaultWidth"), desc.defaultWidth);
        map.insert(QStringLiteral("minimumWidth"), desc.minimumWidth);
        map.insert(QStringLiteral("maximumWidth"), desc.maximumWidth);
        map.insert(QStringLiteral("stretchWeight"), desc.stretchWeight);
        map.insert(QStringLiteral("alignment"), desc.alignment);
        map.insert(QStringLiteral("sortable"), desc.sortable);
        map.insert(QStringLiteral("automaticPriority"), desc.automaticPriority);
        map.insert(QStringLiteral("automaticMinWidth"), desc.automaticMinWidth);
        list.push_back(map);
    }
    return list;
}

QVariantList PlaylistColumnLayoutManager::normalColumns() const
{
    return configToVariantList(m_normalColumns);
}

QVariantList PlaylistColumnLayoutManager::compactColumns() const
{
    return configToVariantList(m_compactColumns);
}

bool PlaylistColumnLayoutManager::normalHasVisibleColumns() const
{
    return hasVisibleColumns(QStringLiteral("normal"));
}

bool PlaylistColumnLayoutManager::compactHasVisibleColumns() const
{
    return hasVisibleColumns(QStringLiteral("compact"));
}

bool PlaylistColumnLayoutManager::compactIsExactDefault() const
{
    return isExactDefaultLayout(QStringLiteral("compact"));
}

QVariantList PlaylistColumnLayoutManager::columnsForSkin(const QString &skin) const
{
    if (skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0) {
        return compactColumns();
    }
    return normalColumns();
}

void PlaylistColumnLayoutManager::setColumnsForSkin(const QString &skin, const QVariantList &columns)
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const QVector<ColumnConfig> fallback = isCompact ? defaultCompactConfig() : defaultNormalConfig();
    const QVector<ColumnConfig> updated = variantListToConfig(columns, fallback);

    if (isCompact) {
        if (m_compactColumns != updated) {
            m_compactColumns = updated;
            m_effectiveColumnsCache.clear();
            saveSettings();
            ++m_layoutRevision;
            emit layoutRevisionChanged();
            emit compactColumnsChanged();
            emit layoutChanged(QStringLiteral("compact"));
        }
    } else {
        if (m_normalColumns != updated) {
            m_normalColumns = updated;
            m_effectiveColumnsCache.clear();
            saveSettings();
            ++m_layoutRevision;
            emit layoutRevisionChanged();
            emit normalColumnsChanged();
            emit layoutChanged(QStringLiteral("normal"));
        }
    }
}

void PlaylistColumnLayoutManager::setColumnVisibility(const QString &skin, const QString &columnId, const QString &visibility)
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    QVector<ColumnConfig> &target = isCompact ? m_compactColumns : m_normalColumns;
    const VisibilityMode newMode = visibilityModeFromString(visibility, VisibilityMode::Hidden);

    bool changed = false;
    for (auto &item : target) {
        if (item.id == columnId) {
            if (item.visibility != newMode) {
                item.visibility = newMode;
                changed = true;
            }
            break;
        }
    }

    if (changed) {
        m_effectiveColumnsCache.clear();
        saveSettings();
        ++m_layoutRevision;
        emit layoutRevisionChanged();
        if (isCompact) {
            emit compactColumnsChanged();
            emit layoutChanged(QStringLiteral("compact"));
        } else {
            emit normalColumnsChanged();
            emit layoutChanged(QStringLiteral("normal"));
        }
    }
}

void PlaylistColumnLayoutManager::moveColumn(const QString &skin, int fromIndex, int toIndex)
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    QVector<ColumnConfig> &target = isCompact ? m_compactColumns : m_normalColumns;

    if (fromIndex < 0 || fromIndex >= target.size() || toIndex < 0 || toIndex >= target.size() || fromIndex == toIndex) {
        return;
    }

    const ColumnConfig item = target.takeAt(fromIndex);
    target.insert(toIndex, item);

    m_effectiveColumnsCache.clear();
    saveSettings();
    ++m_layoutRevision;
    emit layoutRevisionChanged();
    if (isCompact) {
        emit compactColumnsChanged();
        emit layoutChanged(QStringLiteral("compact"));
    } else {
        emit normalColumnsChanged();
        emit layoutChanged(QStringLiteral("normal"));
    }
}

void PlaylistColumnLayoutManager::copySkinLayout(const QString &sourceSkin, const QString &targetSkin)
{
    const bool srcCompact = sourceSkin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const bool tgtCompact = targetSkin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    if (srcCompact == tgtCompact) return;

    const QVector<ColumnConfig> &source = srcCompact ? m_compactColumns : m_normalColumns;
    if (tgtCompact) {
        m_compactColumns = source;
        m_effectiveColumnsCache.clear();
        saveSettings();
        ++m_layoutRevision;
        emit layoutRevisionChanged();
        emit compactColumnsChanged();
        emit layoutChanged(QStringLiteral("compact"));
    } else {
        m_normalColumns = source;
        m_effectiveColumnsCache.clear();
        saveSettings();
        ++m_layoutRevision;
        emit layoutRevisionChanged();
        emit normalColumnsChanged();
        emit layoutChanged(QStringLiteral("normal"));
    }
}

void PlaylistColumnLayoutManager::resetSkin(const QString &skin)
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    if (isCompact) {
        m_compactColumns = defaultCompactConfig();
        m_compactHeaderMode = QStringLiteral("automatic");
        m_effectiveColumnsCache.clear();
        saveSettings();
        ++m_layoutRevision;
        emit layoutRevisionChanged();
        emit compactColumnsChanged();
        emit compactHeaderModeChanged();
        emit layoutChanged(QStringLiteral("compact"));
    } else {
        m_normalColumns = defaultNormalConfig();
        m_effectiveColumnsCache.clear();
        saveSettings();
        ++m_layoutRevision;
        emit layoutRevisionChanged();
        emit normalColumnsChanged();
        emit layoutChanged(QStringLiteral("normal"));
    }
}

void PlaylistColumnLayoutManager::resetAllSkins()
{
    m_normalColumns = defaultNormalConfig();
    m_compactColumns = defaultCompactConfig();
    m_compactHeaderMode = QStringLiteral("automatic");
    m_effectiveColumnsCache.clear();
    saveSettings();
    ++m_layoutRevision;
    emit layoutRevisionChanged();
    emit normalColumnsChanged();
    emit compactColumnsChanged();
    emit compactHeaderModeChanged();
    emit layoutChanged(QStringLiteral("normal"));
    emit layoutChanged(QStringLiteral("compact"));
}

void PlaylistColumnLayoutManager::setCompactHeaderMode(const QString &mode)
{
    const QString trimmed = mode.trimmed();
    if (m_compactHeaderMode == trimmed) return;
    if (trimmed == QStringLiteral("alwaysShown") || trimmed == QStringLiteral("alwaysHidden") || trimmed == QStringLiteral("automatic")) {
        m_compactHeaderMode = trimmed;
    } else {
        m_compactHeaderMode = QStringLiteral("automatic");
    }
    saveSettings();
    ++m_layoutRevision;
    emit layoutRevisionChanged();
    emit compactHeaderModeChanged();
}

bool PlaylistColumnLayoutManager::hasVisibleColumns(const QString &skin) const
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const QVector<ColumnConfig> &cols = isCompact ? m_compactColumns : m_normalColumns;
    for (const auto &item : cols) {
        if (item.visibility != VisibilityMode::Hidden) {
            return true;
        }
    }
    return false;
}

bool PlaylistColumnLayoutManager::isExactDefaultLayout(const QString &skin) const
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const QVector<ColumnConfig> &current = isCompact ? m_compactColumns : m_normalColumns;
    const QVector<ColumnConfig> expected = isCompact ? defaultCompactConfig() : defaultNormalConfig();

    if (current.size() != expected.size()) return false;
    for (int i = 0; i < current.size(); ++i) {
        if (current.at(i).id != expected.at(i).id || current.at(i).visibility != expected.at(i).visibility) {
            return false;
        }
    }
    return true;
}

bool PlaylistColumnLayoutManager::isColumnVisible(const QString &skin, const QString &columnId) const
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const QVector<ColumnConfig> &cols = isCompact ? m_compactColumns : m_normalColumns;
    for (const auto &item : cols) {
        if (item.id == columnId) {
            return item.visibility != VisibilityMode::Hidden;
        }
    }
    return false;
}

void PlaylistColumnLayoutManager::toggleColumnVisibility(const QString &skin, const QString &columnId)
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    QVector<ColumnConfig> &target = isCompact ? m_compactColumns : m_normalColumns;

    for (auto &item : target) {
        if (item.id == columnId) {
            const VisibilityMode newMode = (item.visibility == VisibilityMode::Hidden)
                                               ? VisibilityMode::Shown
                                               : VisibilityMode::Hidden;
            if (item.visibility != newMode) {
                item.visibility = newMode;
                m_effectiveColumnsCache.clear();
                saveSettings();
                ++m_layoutRevision;
                emit layoutRevisionChanged();
                if (isCompact) {
                    emit compactColumnsChanged();
                    emit layoutChanged(QStringLiteral("compact"));
                } else {
                    emit normalColumnsChanged();
                    emit layoutChanged(QStringLiteral("normal"));
                }
            }
            break;
        }
    }
}

int PlaylistColumnLayoutManager::widthBucket(const QString &skin, qreal availableWidth) const
{
    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const QVector<ColumnConfig> &config = isCompact ? m_compactColumns : m_normalColumns;

    QVector<int> thresholds;
    thresholds.reserve(config.size() + 1);
    thresholds.push_back(0);

    for (const auto &item : config) {
        if (item.visibility == VisibilityMode::Automatic) {
            const ColumnDescriptor *desc = findDescriptor(item.id);
            if (desc && desc->automaticMinWidth > 0) {
                thresholds.push_back(desc->automaticMinWidth);
            }
        }
    }

    std::sort(thresholds.begin(), thresholds.end());
    thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());

    int selectedBucket = 0;
    for (int t : thresholds) {
        if (availableWidth >= t) {
            selectedBucket = t;
        } else {
            break;
        }
    }
    return selectedBucket;
}

QVariantList PlaylistColumnLayoutManager::effectiveVisibleColumns(const QString &skin, qreal availableWidth) const
{
    const QString cacheKey = QStringLiteral("%1_%2_%3")
                                 .arg(skin)
                                 .arg(static_cast<int>(availableWidth))
                                 .arg(m_layoutRevision);
    auto cacheIt = m_effectiveColumnsCache.constFind(cacheKey);
    if (cacheIt != m_effectiveColumnsCache.constEnd()) {
        return *cacheIt;
    }

    const bool isCompact = skin.compare(QStringLiteral("compact"), Qt::CaseInsensitive) == 0;
    const QVector<ColumnConfig> &config = isCompact ? m_compactColumns : m_normalColumns;

    QVector<const ColumnConfig *> candidateColumns;
    for (const auto &item : config) {
        if (item.visibility == VisibilityMode::Shown) {
            candidateColumns.push_back(&item);
        } else if (item.visibility == VisibilityMode::Automatic) {
            const ColumnDescriptor *desc = findDescriptor(item.id);
            const int minWidth = desc ? desc->automaticMinWidth : 500;
            if (availableWidth <= 0 || availableWidth >= minWidth) {
                candidateColumns.push_back(&item);
            }
        }
    }

    if (candidateColumns.isEmpty()) {
        const ColumnDescriptor *desc = findDescriptor(QStringLiteral("trackSummary"));
        if (!desc) desc = findDescriptor(QStringLiteral("title"));
        QVariantMap map;
        map.insert(QStringLiteral("id"), desc ? desc->id : QStringLiteral("trackSummary"));
        map.insert(QStringLiteral("visibility"), QStringLiteral("shown"));
        if (desc) {
            map.insert(QStringLiteral("translationKey"), desc->translationKey);
            map.insert(QStringLiteral("valueKind"), desc->valueKind);
            map.insert(QStringLiteral("roleName"), desc->roleName);
            map.insert(QStringLiteral("defaultWidth"), desc->defaultWidth);
            map.insert(QStringLiteral("minimumWidth"), desc->minimumWidth);
            map.insert(QStringLiteral("maximumWidth"), desc->maximumWidth);
            map.insert(QStringLiteral("stretchWeight"), desc->stretchWeight);
            map.insert(QStringLiteral("alignment"), desc->alignment);
            map.insert(QStringLiteral("sortable"), desc->sortable);
            const int fallbackWidth = availableWidth > 0 ? static_cast<int>(availableWidth) : desc->defaultWidth;
            map.insert(QStringLiteral("computedWidth"), fallbackWidth);
            map.insert(QStringLiteral("width"), fallbackWidth);
        }
        QVariantList fallbackList = {map};
        m_effectiveColumnsCache.insert(cacheKey, fallbackList);
        return fallbackList;
    }

    // Now calculate dynamic widths and build result list
    // 1. Calculate base/minimum widths and total stretch weight
    qreal fixedWidthSum = 0.0;
    qreal totalStretchWeight = 0.0;

    for (const auto *item : candidateColumns) {
        const ColumnDescriptor *desc = findDescriptor(item->id);
        const int minW = desc ? desc->minimumWidth : 40;
        const qreal weight = desc ? desc->stretchWeight : 0.0;
        if (weight > 0.0) {
            totalStretchWeight += weight;
            fixedWidthSum += minW;
        } else {
            const int defW = desc ? desc->defaultWidth : minW;
            fixedWidthSum += defW;
        }
    }

    const qreal remainingWidth = (availableWidth > 0) ? qMax(0.0, availableWidth - fixedWidthSum) : 0.0;

    QVariantList result;
    result.reserve(candidateColumns.size());

    for (const auto *item : candidateColumns) {
        const ColumnDescriptor *desc = findDescriptor(item->id);
        QVariantMap map;
        map.insert(QStringLiteral("id"), item->id);
        map.insert(QStringLiteral("visibility"), visibilityModeToString(item->visibility));
        if (desc) {
            map.insert(QStringLiteral("translationKey"), desc->translationKey);
            map.insert(QStringLiteral("valueKind"), desc->valueKind);
            map.insert(QStringLiteral("roleName"), desc->roleName);
            map.insert(QStringLiteral("defaultWidth"), desc->defaultWidth);
            map.insert(QStringLiteral("minimumWidth"), desc->minimumWidth);
            map.insert(QStringLiteral("maximumWidth"), desc->maximumWidth);
            map.insert(QStringLiteral("stretchWeight"), desc->stretchWeight);
            map.insert(QStringLiteral("alignment"), desc->alignment);
            map.insert(QStringLiteral("sortable"), desc->sortable);

            int computedWidth = desc->defaultWidth;
            if (desc->stretchWeight > 0.0 && totalStretchWeight > 0.0) {
                computedWidth = desc->minimumWidth + static_cast<int>(remainingWidth * (desc->stretchWeight / totalStretchWeight));
            }
            if (desc->minimumWidth > 0 && computedWidth < desc->minimumWidth) {
                computedWidth = desc->minimumWidth;
            }
            if (desc->maximumWidth > 0 && computedWidth > desc->maximumWidth) {
                computedWidth = desc->maximumWidth;
            }
            map.insert(QStringLiteral("computedWidth"), computedWidth);
            map.insert(QStringLiteral("width"), computedWidth);
        } else {
            map.insert(QStringLiteral("computedWidth"), 100);
            map.insert(QStringLiteral("width"), 100);
        }
        result.push_back(map);
    }

    m_effectiveColumnsCache.insert(cacheKey, result);
    return result;
}

QVariantMap PlaylistColumnLayoutManager::columnDescriptor(const QString &columnId) const
{
    auto it = m_columnDescriptorCache.constFind(columnId);
    if (it != m_columnDescriptorCache.constEnd()) {
        return *it;
    }

    const ColumnDescriptor *desc = findDescriptor(columnId);
    if (!desc) return {};

    QVariantMap map;
    map.insert(QStringLiteral("id"), desc->id);
    map.insert(QStringLiteral("translationKey"), desc->translationKey);
    map.insert(QStringLiteral("valueKind"), desc->valueKind);
    map.insert(QStringLiteral("roleName"), desc->roleName);
    map.insert(QStringLiteral("defaultWidth"), desc->defaultWidth);
    map.insert(QStringLiteral("minimumWidth"), desc->minimumWidth);
    map.insert(QStringLiteral("maximumWidth"), desc->maximumWidth);
    map.insert(QStringLiteral("stretchWeight"), desc->stretchWeight);
    map.insert(QStringLiteral("alignment"), desc->alignment);
    map.insert(QStringLiteral("sortable"), desc->sortable);
    map.insert(QStringLiteral("automaticPriority"), desc->automaticPriority);
    map.insert(QStringLiteral("automaticMinWidth"), desc->automaticMinWidth);

    m_columnDescriptorCache.insert(columnId, map);
    return map;
}

QString PlaylistColumnLayoutManager::alignString(const QString &columnId) const
{
    const ColumnDescriptor *desc = findDescriptor(columnId);
    return desc ? desc->alignment : QStringLiteral("left");
}

bool PlaylistColumnLayoutManager::isUrlSchemeAllowed(const QString &url) const
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) return false;
    const QUrl parsed(trimmed);
    if (!parsed.isValid()) return false;
    const QString scheme = parsed.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

QString PlaylistColumnLayoutManager::formatValue(
    const QString &columnId, const QVariant &value, const QVariantMap &extra) const
{
    const ColumnDescriptor *desc = findDescriptor(columnId);
    if (!desc) {
        return value.toString().trimmed();
    }

    if (desc->valueKind == QStringLiteral("position")) {
        int pos = value.toInt();
        if (pos <= 0) {
            pos = extra.value(QStringLiteral("sourceIndex")).toInt() + 1;
        }
        return pos > 0 ? QString::number(pos) : QString();
    }

    if (desc->valueKind == QStringLiteral("duration")) {
        qint64 ms = value.toLongLong();
        if (ms <= 0) return QString();
        const qint64 totalSeconds = ms / 1000;
        const qint64 seconds = totalSeconds % 60;
        const qint64 minutes = (totalSeconds / 60) % 60;
        const qint64 hours = totalSeconds / 3600;
        if (hours > 0) {
            return QStringLiteral("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'));
        }
        return QStringLiteral("%1:%2")
            .arg(minutes)
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    if (desc->valueKind == QStringLiteral("number")) {
        if (columnId == QStringLiteral("bitrate")) {
            const int br = value.toInt();
            return br > 0 ? QStringLiteral("%1 kbps").arg(br) : QString();
        }
        if (columnId == QStringLiteral("sampleRate")) {
            const int sr = value.toInt();
            if (sr <= 0) return QString();
            if (sr % 1000 == 0) {
                return QStringLiteral("%1 kHz").arg(sr / 1000);
            }
            return QStringLiteral("%1 kHz").arg(sr / 1000.0, 0, 'f', 1);
        }
        if (columnId == QStringLiteral("bitDepth")) {
            const int bd = value.toInt();
            return bd > 0 ? QStringLiteral("%1-bit").arg(bd) : QString();
        }
        if (columnId == QStringLiteral("bpm")) {
            const int bpm = value.toInt();
            return bpm > 0 ? QStringLiteral("%1 BPM").arg(bpm) : QString();
        }
        if (columnId == QStringLiteral("channelCount")) {
            const int ch = value.toInt();
            if (ch <= 0) return QString();
            if (ch == 1) return QStringLiteral("Mono");
            if (ch == 2) return QStringLiteral("Stereo");
            if (ch == 6) return QStringLiteral("5.1");
            if (ch == 8) return QStringLiteral("7.1");
            return QStringLiteral("%1 ch").arg(ch);
        }
        const int num = value.toInt();
        return num > 0 ? QString::number(num) : QString();
    }

    if (desc->valueKind == QStringLiteral("date")) {
        const qint64 ms = value.toLongLong();
        if (ms <= 0) return QString();
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
        return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }

    if (desc->valueKind == QStringLiteral("summary")) {
        const QString summary = value.toString().trimmed();
        if (!summary.isEmpty()) return summary;
        const QString artist = extra.value(QStringLiteral("artist")).toString().trimmed();
        const QString title = extra.value(QStringLiteral("title")).toString().trimmed();
        if (!title.isEmpty()) {
            if (!artist.isEmpty()) {
                return artist + QStringLiteral(" - ") + title;
            }
            return title;
        }
        return extra.value(QStringLiteral("displayName")).toString().trimmed();
    }

    if (desc->valueKind == QStringLiteral("url")) {
        const QString urlStr = value.toString().trimmed();
        if (isUrlSchemeAllowed(urlStr)) {
            return urlStr;
        }
        return QString();
    }

    return value.toString().trimmed();
}

void PlaylistColumnLayoutManager::loadSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));

    // Normal skin
    const QString normalJsonStr = settings.value(kNormalColumnsKey).toString();
    if (!normalJsonStr.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(normalJsonStr.toUtf8());
        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            const int ver = root.value(QStringLiteral("schemaVersion")).toInt();
            if (ver == kSchemaVersion) {
                const QJsonArray arr = root.value(QStringLiteral("columns")).toArray();
                QVector<ColumnConfig> loaded;
                for (const auto &val : arr) {
                    const QJsonObject obj = val.toObject();
                    const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
                    const QString visStr = obj.value(QStringLiteral("visibility")).toString();
                    const VisibilityMode vis = visibilityModeFromString(visStr, VisibilityMode::Hidden);
                    const int width = obj.value(QStringLiteral("width")).toInt(0);
                    if (!id.isEmpty()) {
                        loaded.push_back({id, vis, width});
                    }
                }
                m_normalColumns = sanitizeConfig(loaded, defaultNormalConfig());
            }
        }
    }

    // Compact skin
    const QString compactJsonStr = settings.value(kCompactColumnsKey).toString();
    if (!compactJsonStr.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(compactJsonStr.toUtf8());
        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            const int ver = root.value(QStringLiteral("schemaVersion")).toInt();
            if (ver == kSchemaVersion) {
                const QString headerMode = root.value(QStringLiteral("compactHeaderMode")).toString();
                if (!headerMode.isEmpty()) {
                    m_compactHeaderMode = headerMode;
                }
                const QJsonArray arr = root.value(QStringLiteral("columns")).toArray();
                QVector<ColumnConfig> loaded;
                for (const auto &val : arr) {
                    const QJsonObject obj = val.toObject();
                    const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
                    const QString visStr = obj.value(QStringLiteral("visibility")).toString();
                    const VisibilityMode vis = visibilityModeFromString(visStr, VisibilityMode::Hidden);
                    const int width = obj.value(QStringLiteral("width")).toInt(0);
                    if (!id.isEmpty()) {
                        loaded.push_back({id, vis, width});
                    }
                }
                m_compactColumns = sanitizeConfig(loaded, defaultCompactConfig());
            }
        }
    } else {
        const QString legacyHeaderMode = settings.value(kCompactHeaderModeKey).toString();
        if (!legacyHeaderMode.isEmpty()) {
            m_compactHeaderMode = legacyHeaderMode;
        }
    }
    m_effectiveColumnsCache.clear();
}

void PlaylistColumnLayoutManager::saveSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));

    // Normal skin
    {
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
        QJsonArray arr;
        for (const auto &item : m_normalColumns) {
            QJsonObject obj;
            obj.insert(QStringLiteral("id"), item.id);
            obj.insert(QStringLiteral("visibility"), visibilityModeToString(item.visibility));
            if (item.width > 0) {
                obj.insert(QStringLiteral("width"), item.width);
            }
            arr.append(obj);
        }
        root.insert(QStringLiteral("columns"), arr);
        settings.setValue(kNormalColumnsKey,
                          QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    }

    // Compact skin
    {
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
        root.insert(QStringLiteral("compactHeaderMode"), m_compactHeaderMode);
        QJsonArray arr;
        for (const auto &item : m_compactColumns) {
            QJsonObject obj;
            obj.insert(QStringLiteral("id"), item.id);
            obj.insert(QStringLiteral("visibility"), visibilityModeToString(item.visibility));
            if (item.width > 0) {
                obj.insert(QStringLiteral("width"), item.width);
            }
            arr.append(obj);
        }
        root.insert(QStringLiteral("columns"), arr);
        settings.setValue(kCompactColumnsKey,
                          QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    }

    settings.sync();
}
