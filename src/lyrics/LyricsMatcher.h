#pragma once

#include "LyricsTypes.h"
#include <QString>
#include <vector>

namespace WaveFlux::Lyrics {

class LyricsMatcher {
public:
    static QString normalize(const QString &input);
    static QString stripEditions(const QString &input);
    static double stringSimilarity(const QString &s1, const QString &s2);
    static int levenshteinDistance(const QString &s1, const QString &s2);
    static bool isEligible(const LyricsCandidate &candidate, const TrackSnapshot &track);
    static int calculateScore(const LyricsCandidate &candidate, const TrackSnapshot &track);
    static std::vector<LyricsCandidate> rankCandidates(std::vector<LyricsCandidate> candidates, const TrackSnapshot &track);
};

} // namespace WaveFlux::Lyrics
