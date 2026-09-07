#include "LyricsMatcher.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace WaveFlux::Lyrics {

QString LyricsMatcher::stripEditions(const QString &input)
{
    static const QRegularExpression editionRegex(
        QStringLiteral(R"(\s*[\(\[](?:remaster(?:ed)?|bonus(?:\s+track)?|feat(?:\.|\s+)|ft(?:\.|\s+)|deluxe|live|mono|stereo|anniversary|edit|version|radio\s+edit|original\s+mix)[^\)\]]*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression suffixRegex(
        QStringLiteral(R"(\s+[-–—]\s+(?:(?:\d{4}\s+)?remaster(?:ed)?|radio\s+edit|deluxe|anniversary)\b.*$)"),
        QRegularExpression::CaseInsensitiveOption);
    return QString(input).remove(editionRegex).remove(suffixRegex).trimmed();
}

QString LyricsMatcher::normalize(const QString &input)
{
    QString s = input.normalized(QString::NormalizationForm_KD);
    // Strip diacritical marks
    static const QRegularExpression diacritics(QStringLiteral(R"(\p{M})"));
    s.remove(diacritics);

    // Replace punctuation with spaces
    static const QRegularExpression punct(QStringLiteral(R"([^\p{L}\p{N}\s])"));
    s.replace(punct, QStringLiteral(" "));

    // Collapse whitespace
    static const QRegularExpression ws(QStringLiteral(R"(\s+)"));
    s.replace(ws, QStringLiteral(" "));

    return s.trimmed().toLower();
}

int LyricsMatcher::levenshteinDistance(const QString &s1, const QString &s2)
{
    const int len1 = s1.length();
    const int len2 = s2.length();
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;

    std::vector<int> col(len2 + 1);
    for (int j = 0; j <= len2; ++j) {
        col[j] = j;
    }

    for (int i = 0; i < len1; ++i) {
        std::vector<int> nextCol(len2 + 1);
        nextCol[0] = i + 1;
        for (int j = 0; j < len2; ++j) {
            const int cost = (s1[i] == s2[j]) ? 0 : 1;
            nextCol[j + 1] = std::min({
                nextCol[j] + 1,       // insertion
                col[j + 1] + 1,       // deletion
                col[j] + cost         // substitution
            });
        }
        col = std::move(nextCol);
    }
    return col[len2];
}

double LyricsMatcher::stringSimilarity(const QString &s1, const QString &s2)
{
    if (s1 == s2) return 1.0;
    const int maxLen = std::max(s1.length(), s2.length());
    if (maxLen == 0) return 1.0;
    const int dist = levenshteinDistance(s1, s2);
    return 1.0 - static_cast<double>(dist) / maxLen;
}

bool LyricsMatcher::isEligible(const LyricsCandidate &candidate, const TrackSnapshot &track)
{
    if (track.durationMs > 0 && candidate.durationMs > 0) {
        const qint64 diff = std::abs(candidate.durationMs - track.durationMs);
        if (candidate.hasSynced) {
            if (diff > kSyncedDurationMaxDiffMs) {
                return false;
            }
        } else {
            if (diff > kPlainDurationMaxDiffMs) {
                return false;
            }
        }
    }

    const QString normTrackTitle = normalize(stripEditions(track.title));
    const QString normCandTitle = normalize(stripEditions(candidate.title));
    if (!normTrackTitle.isEmpty() && !normCandTitle.isEmpty()) {
        const double titleSim = stringSimilarity(normTrackTitle, normCandTitle);
        if (titleSim < 0.5 && !normCandTitle.contains(normTrackTitle) && !normTrackTitle.contains(normCandTitle)) {
            return false;
        }
    }

    const QString normTrackArtist = normalize(track.artist);
    const QString normCandArtist = normalize(candidate.artist);
    if (!normTrackArtist.isEmpty() && !normCandArtist.isEmpty()) {
        const double artistSim = stringSimilarity(normTrackArtist, normCandArtist);
        if (artistSim < 0.4 && !normCandArtist.contains(normTrackArtist) && !normTrackArtist.contains(normCandArtist)) {
            return false;
        }
    }

    return true;
}

int LyricsMatcher::calculateScore(const LyricsCandidate &candidate, const TrackSnapshot &track)
{
    int score = 0;
    if (candidate.hasSynced) {
        score += 1000;
    }

    const QString normTrackTitle = normalize(stripEditions(track.title));
    const QString normCandTitle = normalize(stripEditions(candidate.title));
    if (!normTrackTitle.isEmpty() && normTrackTitle == normCandTitle) {
        score += 500;
    } else {
        score += static_cast<int>(stringSimilarity(normTrackTitle, normCandTitle) * 400);
    }

    const QString normTrackArtist = normalize(track.artist);
    const QString normCandArtist = normalize(candidate.artist);
    if (!normTrackArtist.isEmpty() && normTrackArtist == normCandArtist) {
        score += 300;
    } else {
        score += static_cast<int>(stringSimilarity(normTrackArtist, normCandArtist) * 200);
    }

    const QString normTrackAlbum = normalize(stripEditions(track.album));
    const QString normCandAlbum = normalize(stripEditions(candidate.album));
    if (!normTrackAlbum.isEmpty() && !normCandAlbum.isEmpty()) {
        if (normTrackAlbum == normCandAlbum) {
            score += 100;
        } else {
            score += static_cast<int>(stringSimilarity(normTrackAlbum, normCandAlbum) * 80);
        }
    }

    if (track.durationMs > 0 && candidate.durationMs > 0) {
        const qint64 diff = std::abs(candidate.durationMs - track.durationMs);
        const int durBonus = std::max<int>(0, 200 - static_cast<int>(diff / 10));
        score += durBonus;
    }

    if (candidate.providerId == QLatin1String("lrclib")) {
        score += 50;
    }

    return score;
}

std::vector<LyricsCandidate> LyricsMatcher::rankCandidates(std::vector<LyricsCandidate> candidates, const TrackSnapshot &track)
{
    std::vector<LyricsCandidate> eligible;
    eligible.reserve(candidates.size());

    for (auto &cand : candidates) {
        if (isEligible(cand, track)) {
            cand.durationDiffMs = (track.durationMs > 0 && cand.durationMs > 0)
                ? std::abs(cand.durationMs - track.durationMs)
                : 0;
            cand.score = calculateScore(cand, track);
            eligible.push_back(std::move(cand));
        }
    }

    std::sort(eligible.begin(), eligible.end(), [](const LyricsCandidate &a, const LyricsCandidate &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.hasSynced != b.hasSynced) {
            return a.hasSynced;
        }
        return a.durationDiffMs < b.durationDiffMs;
    });

    return eligible;
}

} // namespace WaveFlux::Lyrics
