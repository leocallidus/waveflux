#ifndef PEAKSCACHEMANAGER_H
#define PEAKSCACHEMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <optional>

/**
 * @brief Thread-safe disk cache for waveform peaks.
 *
 * Cache key = canonical filePath + mtime + fileSize + analyzerVersion.
 * Each entry is stored as a separate file under
 *   QStandardPaths::CacheLocation / "peaks" /  <sha256-hex>.peaks
 * in a compact binary format with magic, format version, and integrity metadata.
 *
 * Features:
 *  - Atomic writes via QSaveFile (no partial/corrupt files on crash).
 *  - LRU eviction: file access time is touched on read; oldest files are
 *    evicted first when total cache size exceeds the configured limit.
 *  - Thread safety: all public methods are guarded by a QMutex, so the class
 *    can be called from the UI thread and QtConcurrent workers alike.
 *  - Versioned binary format: if the on-disk format changes in the future,
 *    old entries are silently discarded (format version mismatch).
 */
class PeaksCacheManager : public QObject
{
    Q_OBJECT

public:
    /// Current version of the analysis algorithm.
    /// Bump this whenever the extraction logic in WaveformProvider changes
    /// in a way that produces different peak values for the same input.
    static constexpr quint32 kAnalyzerVersion = 2;

    explicit PeaksCacheManager(QObject *parent = nullptr);
    ~PeaksCacheManager() override = default;

    // ── Lookup ──────────────────────────────────────────────────────
    /// Returns cached peaks for @p filePath if a valid entry exists,
    /// or std::nullopt on cache miss / stale entry / corrupt file.
    std::optional<QVector<float>> lookup(const QString &filePath) const;

    // ── Store ───────────────────────────────────────────────────────
    /// Persists @p peaks for @p filePath atomically.
    /// Triggers background eviction when the cache grows beyond the limit.
    void store(const QString &filePath, const QVector<float> &peaks);

    // ── Maintenance ─────────────────────────────────────────────────
    /// Removes every *.peaks file from the cache directory.
    void clear();

    /// Maximum total cache size in bytes (default 512 MiB).
    void setMaxCacheSize(qint64 bytes);
    qint64 maxCacheSize() const;

private:
    /// On-disk binary format version (independent of analyzerVersion).
    static constexpr quint32 kFormatVersion = 1;
    static constexpr quint32 kMagic = 0x57465043; // "WFPC" – WaveFlux Peaks Cache
    static constexpr qint64  kDefaultMaxCacheBytes = 512LL * 1024 * 1024;

    struct FileIdentity {
        QString canonicalPath;
        qint64  lastModifiedMs = 0; // msecs since epoch
        qint64  fileSize = 0;
    };

    static FileIdentity identityOf(const QString &filePath);
    static QByteArray   cacheKey(const FileIdentity &id);
    QString             cachePath(const QByteArray &hexHash) const;
    QString             cacheDir() const;

    void ensureCacheDir() const;
    void evictIfNeeded();

    mutable QMutex m_mutex;
    qint64 m_maxCacheBytes = kDefaultMaxCacheBytes;
};

#endif // PEAKSCACHEMANAGER_H
