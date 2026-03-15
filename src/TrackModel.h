#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <QtGlobal>
#include <QUrl>
#include <QVector>

class LibraryRepository;
class SearchRepository;
class PlaybackController;

/**
 * @brief Track - Represents a single audio track in the playlist
 */
struct Track {
    QString filePath;
    QString title;
    QString artist;
    QString album;
    qint64 duration = 0; // in milliseconds
    qint64 addedAt = 0;  // unix ms timestamp
    QString format;
    int bitrate = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    int bpm = 0;
    QString albumArt;
    bool cueSegment = false;
    qint64 cueStartMs = 0;
    qint64 cueEndMs = -1;
    int cueTrackNumber = 0;
    QString cueSheetPath;
    
    QString displayName() const {
        if (!title.isEmpty()) {
            if (!artist.isEmpty()) {
                return artist + " - " + title;
            }
            return title;
        }
        // Extract filename from path
        int lastSlash = filePath.lastIndexOf('/');
        if (lastSlash >= 0) {
            return filePath.mid(lastSlash + 1);
        }
        return filePath;
    }
};

struct TrackPlaybackEvent {
    QString filePath;
    qint64 startedAtMs = 0;
    qint64 endedAtMs = 0;
    qint64 listenMs = 0;
    double completionRatio = 0.0;
    QString source;
    bool wasSkipped = false;
    bool wasCompleted = false;
    QString sessionId;
};

/**
 * @brief TrackModel - QML-compatible list model for the playlist
 * 
 * Provides the data model for displaying tracks in the playlist view.
 * Supports drag & drop, track reordering, and removal.
 */
class TrackModel : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentAlbum READ currentAlbum NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentFormat READ currentFormat NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentBitrate READ currentBitrate NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentSampleRate READ currentSampleRate NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentBitDepth READ currentBitDepth NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentBpm READ currentBpm NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentAlbumArt READ currentAlbumArt NOTIFY currentTrackChanged)
    Q_PROPERTY(bool currentIsLossless READ currentIsLossless NOTIFY currentTrackChanged)
    Q_PROPERTY(bool currentIsHiRes READ currentIsHiRes NOTIFY currentTrackChanged)
    Q_PROPERTY(int searchRevision READ searchRevision NOTIFY searchRevisionChanged)
    Q_PROPERTY(bool deterministicShuffleEnabled READ deterministicShuffleEnabled WRITE setDeterministicShuffleEnabled NOTIFY deterministicShuffleEnabledChanged)
    Q_PROPERTY(quint32 shuffleSeed READ shuffleSeed WRITE setShuffleSeed NOTIFY shuffleSeedChanged)
    Q_PROPERTY(bool repeatableShuffle READ repeatableShuffle WRITE setRepeatableShuffle NOTIFY repeatableShuffleChanged)
    
public:
    enum SearchFieldFlag {
        SearchFieldNone = 0,
        SearchFieldTitle = 1 << 0,
        SearchFieldArtist = 1 << 1,
        SearchFieldAlbum = 1 << 2,
        SearchFieldPath = 1 << 3,
        SearchFieldAll = SearchFieldTitle | SearchFieldArtist | SearchFieldAlbum | SearchFieldPath
    };
    Q_ENUM(SearchFieldFlag)

    enum SearchQuickFilterFlag {
        SearchQuickFilterNone = 0,
        SearchQuickFilterLossless = 1 << 0,
        SearchQuickFilterHiRes = 1 << 1
    };
    Q_ENUM(SearchQuickFilterFlag)

    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        DurationRole,
        DisplayNameRole,
        FormatRole,
        BitrateRole,
        SampleRateRole,
        BitDepthRole,
        BpmRole,
        AlbumArtRole
    };
    Q_ENUM(Roles)
    
    explicit TrackModel(QObject *parent = nullptr);
    ~TrackModel() override;
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    
    QString currentTitle() const;
    QString currentArtist() const;
    QString currentAlbum() const;
    QString currentFormat() const;
    int currentBitrate() const;
    int currentSampleRate() const;
    int currentBitDepth() const;
    int currentBpm() const;
    QString currentAlbumArt() const;
    bool currentIsLossless() const;
    bool currentIsHiRes() const;
    int searchRevision() const { return m_searchUiRevision; }
    bool deterministicShuffleEnabled() const { return m_deterministicShuffleEnabled; }
    quint32 shuffleSeed() const { return m_shuffleSeed; }
    bool repeatableShuffle() const { return m_repeatableShuffle; }
    void setDeterministicShuffleEnabled(bool enabled);
    void setShuffleSeed(quint32 seed);
    void setRepeatableShuffle(bool enabled);
    void configureLibraryStorage(bool enabled, const QString &databasePath);
    void recordPlaybackEvents(const QVector<TrackPlaybackEvent> &events, bool blocking = false);
    
    Q_INVOKABLE void addFile(const QString &filePath);
    Q_INVOKABLE void addFiles(const QStringList &filePaths);
    Q_INVOKABLE void addFolder(const QUrl &folderUrl);
    Q_INVOKABLE void addUrl(const QUrl &url);
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void move(int from, int to);
    
    Q_INVOKABLE QString getFilePath(int index) const;
    Q_INVOKABLE qint64 cueStartMs(int index) const;
    Q_INVOKABLE qint64 cueEndMs(int index) const;
    Q_INVOKABLE bool isCueTrack(int index) const;
    Q_INVOKABLE int cueTrackNumber(int index) const;
    Q_INVOKABLE QString getNextFilePath() const;
    Q_INVOKABLE QString getPreviousFilePath() const;
    Q_INVOKABLE int countMatching(const QString &query) const;
    Q_INVOKABLE int countMatchingNormalized(const QString &normalizedQuery) const;
    Q_INVOKABLE int countMatchingAdvancedNormalized(const QString &normalizedQuery,
                                                    int fieldMask,
                                                    int quickFilterMask) const;
    Q_INVOKABLE int countMatchingAdvancedNormalizedBefore(int index,
                                                          const QString &normalizedQuery,
                                                          int fieldMask,
                                                          int quickFilterMask) const;
    Q_INVOKABLE void sortByNameAsc();
    Q_INVOKABLE void sortByNameDesc();
    Q_INVOKABLE void sortByDateAsc();
    Q_INVOKABLE void sortByDateDesc();
    Q_INVOKABLE void sortByIndexAsc();
    Q_INVOKABLE void sortByIndexDesc();
    Q_INVOKABLE void sortByDurationAsc();
    Q_INVOKABLE void sortByDurationDesc();
    Q_INVOKABLE void sortByBitrateAsc();
    Q_INVOKABLE void sortByBitrateDesc();
    Q_INVOKABLE void sortByArtistAsc();
    Q_INVOKABLE void sortByArtistDesc();
    Q_INVOKABLE void sortByAlbumAsc();
    Q_INVOKABLE void sortByAlbumDesc();
    Q_INVOKABLE void restoreOrder(const QVariantList &filePaths);
    Q_INVOKABLE void shuffleOrder();
    Q_INVOKABLE QVariantList exportTracksSnapshot() const;
    Q_INVOKABLE void importTracksSnapshot(const QVariantList &snapshot, int requestedCurrentIndex = -1);
    Q_INVOKABLE void applySmartCollectionRows(const QVariantList &rows);
    
    Q_INVOKABLE void playNext();
    Q_INVOKABLE void playPrevious();
    Q_INVOKABLE void applyTagOverridesForFiles(const QStringList &filePaths,
                                               bool applyTitle,
                                               const QString &title,
                                               bool applyArtist,
                                               const QString &artist,
                                               bool applyAlbum,
                                               const QString &album);
    Q_INVOKABLE bool matchesSearchQuery(int index, const QString &query) const;
    Q_INVOKABLE bool matchesSearchQueryNormalized(int index, const QString &normalizedQuery) const;
    Q_INVOKABLE bool matchesSearchAdvancedNormalized(int index,
                                                     const QString &normalizedQuery,
                                                     int fieldMask,
                                                     int quickFilterMask) const;
    Q_INVOKABLE void refreshMetadataForFile(const QString &filePath, bool includeAlbumArt = true);
    Q_INVOKABLE QVariantList cueSegmentsForFile(const QString &filePath,
                                                qint64 fallbackDurationMs = -1) const;

    const QVector<Track> &tracks() const { return m_tracks; }
    void setTracks(QVector<Track> tracks);
    
signals:
    void countChanged();
    void currentIndexChanged(int index);
    void currentTrackChanged();
    void searchRevisionChanged();
    void xspfImportSummaryReady(const QString &sourcePath,
                                int addedCount,
                                int skippedCount,
                                const QString &errorMessage);
    void trackSelected(const QString &filePath);
    void deterministicShuffleEnabledChanged();
    void shuffleSeedChanged();
    void repeatableShuffleChanged();
    
private:
    friend class PlaybackController;

    struct ParsedMetadata {
        QString filePath;
        QString title;
        QString artist;
        QString album;
        qint64 duration = 0;
        QString format;
        int bitrate = 0;
        int sampleRate = 0;
        int bitDepth = 0;
        int bpm = 0;
        QString albumArt;
        bool albumArtChecked = false;
    };

    static ParsedMetadata readMetadataForFile(const QString &filePath, bool includeAlbumArt);
    struct AsyncSearchTrackSnapshot {
        QString filePath;
        QString title;
        QString artist;
        QString album;
        QString format;
        QString searchTextLower;
        int sampleRate = 0;
        int bitDepth = 0;
    };
    struct AsyncSearchRequest {
        int token = 0;
        int modelRevision = 0;
        QString normalizedQuery;
        int fieldMask = SearchFieldAll;
        int quickFilterMask = SearchQuickFilterNone;
        bool sqliteEnabled = false;
        QString sqliteDatabasePath;
        QVector<AsyncSearchTrackSnapshot> tracks;
    };
    struct AsyncSearchResult {
        int token = 0;
        int modelRevision = 0;
        QString normalizedQuery;
        int fieldMask = SearchFieldAll;
        int quickFilterMask = SearchQuickFilterNone;
        QVector<quint8> matches;
        QVector<int> prefixMatches;
        int matchCount = 0;
        bool success = false;
    };
    static AsyncSearchResult computeAsyncSearch(AsyncSearchRequest request);
    void scheduleAsyncSearch(const QString &normalizedQuery,
                             int fieldMask,
                             int quickFilterMask) const;
    void launchAsyncSearch(const QString &normalizedQuery,
                           int fieldMask,
                           int quickFilterMask) const;
    void onAsyncSearchFinished();
    void notifySearchResultsUpdated();
    void applyParsedMetadata(const ParsedMetadata &metadata);
    void scheduleMetadataRead(const QString &filePath, bool includeAlbumArt);
    void pumpMetadataReadQueue();
    static QString buildSearchTextLower(const Track &track);
    void internTrackStrings(Track &track);
    QString internString(const QString &value);
    void clearSearchTextCache();
    const QString &searchTextLowerAt(int index) const;
    void invalidateSearchCache();
    void resetTransientSearchState();
    void resetTransientMetadataState();
    bool shouldMaterializeAsyncSearchText(int fieldMask, int quickFilterMask) const;
    void ensureSearchCache(const QString &normalizedQuery,
                           int fieldMask,
                           int quickFilterMask) const;
    const Track *currentTrackPtr() const;
    static bool hasSupportedAudioExtension(const QString &filePath);
    static bool isLosslessFormat(const QString &format);
    int findIndexByPath(const QString &filePath) const;
    quint32 nextShuffleSeed() const;
    void setCurrentIndexSilently(int index);
    void applyCurrentIndex(int index, bool emitTrackSelectedSignal);
    void appendAcceptedTracks(QVector<Track> acceptedTracks,
                              const QVector<int> &ingestTrackOffsets,
                              const QVector<int> &metadataTrackOffsets);
    void loadMetadata(int index, bool includeAlbumArt = false, bool forceReload = false);
    void trimAlbumArtToCurrentTrack(bool emitDataChangedForRows = false);
    void syncCurrentAlbumArtCache();
    void updateProfilerPlaylistCount();
    
    QVector<Track> m_tracks;
    int m_currentIndex = -1;
    int m_searchRevision = 0;
    mutable int m_cachedSearchRevision = -1;
    mutable QString m_cachedSearchQuery;
    mutable int m_cachedSearchFieldMask = SearchFieldAll;
    mutable int m_cachedSearchQuickFilterMask = SearchQuickFilterNone;
    mutable QVector<quint8> m_cachedSearchMatches;
    mutable QVector<int> m_cachedSearchPrefixMatches;
    mutable int m_cachedSearchMatchCount = 0;
    mutable QFutureWatcher<AsyncSearchResult> m_searchFutureWatcher;
    mutable int m_nextSearchToken = 1;
    mutable int m_inFlightSearchToken = 0;
    mutable int m_inFlightModelRevision = -1;
    mutable QString m_inFlightSearchQuery;
    mutable int m_inFlightSearchFieldMask = SearchFieldAll;
    mutable int m_inFlightSearchQuickFilterMask = SearchQuickFilterNone;
    mutable bool m_hasPendingSearchRequest = false;
    mutable QString m_pendingSearchQuery;
    mutable int m_pendingSearchFieldMask = SearchFieldAll;
    mutable int m_pendingSearchQuickFilterMask = SearchQuickFilterNone;
    int m_searchUiRevision = 0;
    bool m_deterministicShuffleEnabled = false;
    quint32 m_shuffleSeed = 0xA5C3D791u;
    bool m_repeatableShuffle = true;
    mutable quint64 m_shuffleGeneration = 0;
    std::unique_ptr<LibraryRepository> m_libraryRepository;
    std::unique_ptr<SearchRepository> m_searchRepository;
    bool m_collectionViewActive = false;
    QSet<QString> m_stringPool;
    QThreadPool m_metadataThreadPool;
    QHash<QString, bool> m_pendingMetadataReads;
    QHash<QString, bool> m_inFlightMetadataReads;
    quint64 m_metadataReadGeneration = 0;
    QString m_currentAlbumArt;
    mutable QVector<QString> m_searchTextLowerCache;
    mutable QVector<quint8> m_searchTextLowerReady;

    static constexpr int kExpandedAsyncSearchTextTrackBudget = 1500;
};

#endif // TRACKMODEL_H
