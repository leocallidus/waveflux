#include "PeaksCacheManager.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <utime.h> // POSIX – touch access time for LRU

// ─── helpers ────────────────────────────────────────────────────────
namespace {
constexpr quint32 kMaxCachedPeakCount = 65536;
constexpr float kMaxExpectedPeakValue = 1.0f;

bool isValidPeakValue(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= kMaxExpectedPeakValue;
}
} // namespace

PeaksCacheManager::FileIdentity PeaksCacheManager::identityOf(const QString &filePath)
{
    const QFileInfo fi(filePath);
    return {
        fi.canonicalFilePath(),
        fi.lastModified().toMSecsSinceEpoch(),
        fi.size(),
    };
}

QByteArray PeaksCacheManager::cacheKey(const FileIdentity &id)
{
    // SHA-256( canonicalPath | mtime | size | analyzerVersion )
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(id.canonicalPath.toUtf8());
    hash.addData(QByteArray::number(id.lastModifiedMs));
    hash.addData(QByteArray::number(id.fileSize));
    hash.addData(QByteArray::number(kAnalyzerVersion));
    return hash.result().toHex();
}

QString PeaksCacheManager::cacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/peaks");
}

QString PeaksCacheManager::cachePath(const QByteArray &hexHash) const
{
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(hexHash) + QStringLiteral(".peaks");
}

void PeaksCacheManager::ensureCacheDir() const
{
    QDir().mkpath(cacheDir());
}

// ─── public API ─────────────────────────────────────────────────────

PeaksCacheManager::PeaksCacheManager(QObject *parent)
    : QObject(parent)
{
}

void PeaksCacheManager::setMaxCacheSize(qint64 bytes)
{
    QMutexLocker lock(&m_mutex);
    m_maxCacheBytes = bytes;
}

qint64 PeaksCacheManager::maxCacheSize() const
{
    QMutexLocker lock(&m_mutex);
    return m_maxCacheBytes;
}

// ── Lookup ──────────────────────────────────────────────────────────

std::optional<QVector<float>> PeaksCacheManager::lookup(const QString &filePath) const
{
    QMutexLocker lock(&m_mutex);

    const FileIdentity id = identityOf(filePath);
    if (id.canonicalPath.isEmpty()) {
        return std::nullopt; // file does not exist or is unresolvable
    }

    const QByteArray hex = cacheKey(id);
    const QString path   = cachePath(hex);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt; // cache miss
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_5);
    in.setByteOrder(QDataStream::LittleEndian);

    // ── header ──
    quint32 magic = 0, formatVer = 0, analyzerVer = 0;
    in >> magic >> formatVer >> analyzerVer;
    if (in.status() != QDataStream::Ok
        || magic != kMagic
        || formatVer != kFormatVersion
        || analyzerVer != kAnalyzerVersion) {
        qDebug() << "PeaksCacheManager: stale/corrupt entry, removing" << path;
        file.close();
        QFile::remove(path);
        return std::nullopt;
    }

    // ── stored identity (for double-check) ──
    QString storedPath;
    qint64  storedMtime = 0, storedSize = 0;
    in >> storedPath >> storedMtime >> storedSize;
    if (in.status() != QDataStream::Ok
        || storedPath != id.canonicalPath
        || storedMtime != id.lastModifiedMs
        || storedSize != id.fileSize) {
        qDebug() << "PeaksCacheManager: identity mismatch, removing" << path;
        file.close();
        QFile::remove(path);
        return std::nullopt;
    }

    // ── peaks payload ──
    quint32 count = 0;
    in >> count;
    if (in.status() != QDataStream::Ok || count == 0 || count > kMaxCachedPeakCount) {
        file.close();
        QFile::remove(path);
        return std::nullopt;
    }

    QVector<float> peaks(static_cast<int>(count));
    for (quint32 i = 0; i < count; ++i) {
        in >> peaks[static_cast<int>(i)];
    }
    if (in.status() != QDataStream::Ok) {
        file.close();
        QFile::remove(path);
        return std::nullopt;
    }

    for (const float peak : std::as_const(peaks)) {
        if (!isValidPeakValue(peak)) {
            qWarning() << "PeaksCacheManager: invalid peak payload, removing" << path;
            file.close();
            QFile::remove(path);
            return std::nullopt;
        }
    }

    file.close();

    // Touch access time so LRU eviction keeps recently-used files.
    // utimensat would be more precise, but utime is simpler and sufficient.
    ::utime(path.toLocal8Bit().constData(), nullptr);

    qDebug() << "PeaksCacheManager: cache hit for" << id.canonicalPath
             << "(" << count << "peaks)";
    return peaks;
}

// ── Store ───────────────────────────────────────────────────────────

void PeaksCacheManager::store(const QString &filePath,
                              const QVector<float> &peaks)
{
    if (peaks.isEmpty()) {
        return;
    }
    if (peaks.size() > static_cast<int>(kMaxCachedPeakCount)) {
        qWarning() << "PeaksCacheManager: too many peaks, skipping cache write for" << filePath;
        return;
    }
    for (const float peak : peaks) {
        if (!isValidPeakValue(peak)) {
            qWarning() << "PeaksCacheManager: refusing to cache invalid peak payload for" << filePath;
            return;
        }
    }

    QMutexLocker lock(&m_mutex);

    const FileIdentity id = identityOf(filePath);
    if (id.canonicalPath.isEmpty()) {
        return;
    }

    ensureCacheDir();

    const QByteArray hex = cacheKey(id);
    const QString path   = cachePath(hex);

    // Atomic write via QSaveFile – the file only appears once commit() succeeds.
    QSaveFile saveFile(path);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning() << "PeaksCacheManager: cannot open for writing:" << path
                    << saveFile.errorString();
        return;
    }

    QDataStream out(&saveFile);
    out.setVersion(QDataStream::Qt_6_5);
    out.setByteOrder(QDataStream::LittleEndian);

    // ── header ──
    out << kMagic << kFormatVersion << kAnalyzerVersion;

    // ── identity (for double-check on read) ──
    out << id.canonicalPath << id.lastModifiedMs << id.fileSize;

    // ── peaks payload ──
    const auto count = static_cast<quint32>(peaks.size());
    out << count;
    for (const float v : peaks) {
        out << v;
    }

    if (out.status() != QDataStream::Ok) {
        saveFile.cancelWriting();
        qWarning() << "PeaksCacheManager: serialization error for" << path;
        return;
    }

    if (!saveFile.commit()) {
        qWarning() << "PeaksCacheManager: commit failed for" << path
                    << saveFile.errorString();
        return;
    }

    qDebug() << "PeaksCacheManager: stored" << count << "peaks for"
             << id.canonicalPath << "→" << path;

    evictIfNeeded();
}

// ── Maintenance ─────────────────────────────────────────────────────

void PeaksCacheManager::clear()
{
    QMutexLocker lock(&m_mutex);
    QDir dir(cacheDir());
    if (!dir.exists()) {
        return;
    }
    const QStringList entries = dir.entryList({QStringLiteral("*.peaks")}, QDir::Files);
    for (const QString &name : entries) {
        dir.remove(name);
    }
    qDebug() << "PeaksCacheManager: cache cleared (" << entries.size() << "files)";
}

void PeaksCacheManager::evictIfNeeded()
{
    // Caller must already hold m_mutex.

    const QDir dir(cacheDir());
    if (!dir.exists()) {
        return;
    }

    // Collect entries sorted by last-access time ascending (oldest first).
    struct Entry {
        QString path;
        qint64  size;
        qint64  lastAccess; // msecs since epoch
    };
    QVector<Entry> entries;
    qint64 totalSize = 0;

    QDirIterator it(dir.absolutePath(), {QStringLiteral("*.peaks")}, QDir::Files);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        const qint64 sz = fi.size();
        entries.append({fi.absoluteFilePath(), sz, fi.lastRead().toMSecsSinceEpoch()});
        totalSize += sz;
    }

    if (totalSize <= m_maxCacheBytes) {
        return;
    }

    // Sort oldest-accessed first.
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.lastAccess < b.lastAccess; });

    int removed = 0;
    for (const Entry &e : std::as_const(entries)) {
        if (totalSize <= m_maxCacheBytes) {
            break;
        }
        if (QFile::remove(e.path)) {
            totalSize -= e.size;
            ++removed;
        }
    }

    if (removed > 0) {
        qDebug() << "PeaksCacheManager: evicted" << removed
                 << "files, cache size now ~" << (totalSize / 1024) << "KiB";
    }
}
