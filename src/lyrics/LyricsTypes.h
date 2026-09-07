#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QObject>
#include <QtGlobal>
#include <vector>

namespace WaveFlux::Lyrics {

enum class LyricsKind {
    None = 0,
    Synced,
    Plain,
    Instrumental
};

enum class LyricsState {
    NoTrack = 0,
    Loading,
    OnlineDisabled,
    NeedsMetadata,
    NeedsSelection,
    ReadySynced,
    ReadyPlain,
    Instrumental,
    NotFound,
    Error
};

enum class LyricsProvenance {
    None = 0,
    Sidecar,
    Imported,
    CacheLrclib,
    CacheLyricsOvh,
    OnlineLrclib,
    OnlineLyricsOvh
};

constexpr qint64 kMaxLrcFileSize = 256 * 1024;      // 256 KiB
constexpr int kMaxLrcCues = 5000;
constexpr qint64 kSyncedDurationMaxDiffMs = 2000;   // 2 seconds
constexpr qint64 kPlainDurationMaxDiffMs = 5000;    // 5 seconds
constexpr int kMaxCachedDocuments = 1000;
constexpr qint64 kMaxCacheBytes = 50 * 1024 * 1024; // 50 MiB
constexpr qint64 kPositiveTtlSecs = 30 * 86400;     // 30 days
constexpr qint64 kNegativeTtlSecs = 6 * 3600;       // 6 hours
constexpr int kProviderCooldownMs = 500;
constexpr qint64 kMinUserDelayMs = -30000;          // -30 seconds
constexpr qint64 kMaxUserDelayMs = 30000;           // +30 seconds
constexpr qint64 kUserDelayStepMs = 100;            // 100 ms

struct CueGroup {
    qint64 effectiveCueMs = 0;
    qint64 rawCueMs = 0;
    QString text;
    bool isBlank = false;
};

struct TrackSnapshot {
    int trackIndex = -1;
    QString filePath;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    qint64 cueStartMs = 0;
    qint64 cueEndMs = 0;
    bool isCueTrack = false;
    bool isStream = false;
    bool isTracker = false;

    QString cacheKey() const;
    QString normalizedLookupKey() const;
    bool hasMinimalMetadata() const;
};

struct QuerySnapshot {
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
};

struct RequestToken {
    quint64 id = 0;
    int trackIndex = -1;
    QString cacheKey;
};

struct LyricsCandidate {
    QString id;
    QString providerId; // "lrclib", "lyricsovh"
    LyricsKind kind = LyricsKind::None;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    bool hasSynced = false;
    bool isInstrumental = false;
    QString lrcText;
    QString plainText;
    qint64 durationDiffMs = 0;
    int score = 0;
};

struct LyricsDocument {
    QString id;
    LyricsKind kind = LyricsKind::None;
    LyricsProvenance provenance = LyricsProvenance::None;
    QString text;
    QString plainText;
    std::vector<CueGroup> cues;
    qint64 lrcOffsetMs = 0;
    QString contentHash;
};

} // namespace WaveFlux::Lyrics
