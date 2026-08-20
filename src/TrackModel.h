#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QQueue>
#include <QTimer>
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

struct TrackChapter {
    QString title;
    qint64 startTimeMs = 0;
    qint64 endTimeMs = 0;

    bool operator==(const TrackChapter &other) const {
        return title == other.title
            && startTimeMs == other.startTimeMs
            && endTimeMs == other.endTimeMs;
    }
    bool operator!=(const TrackChapter &other) const {
        return !(*this == other);
    }
};

/**
 * @brief Track - Represents a single audio track in the playlist
 */
struct Track {
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString comment;
    QString genre;
    QString year;
    QString trackNumber;
    QString description;
    QString composer;
    QString originalArtist;
    QString copyright;
    QString url;
    QString encoder;
    quint32 metadataCompletenessMask = 0;
    qint64 duration = 0; // in milliseconds
    qint64 addedAt = 0;  // unix ms timestamp
    QString format;
    int bitrate = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    int bpm = 0;
    int channelCount = 0;
    QString albumArt;
    bool cueSegment = false;
    qint64 cueStartMs = 0;
    qint64 cueEndMs = -1;
    int cueTrackNumber = 0;
    QString cueSheetPath;
    QString searchBlob;
    QVector<TrackChapter> chapters;

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
    Q_PROPERTY(QString currentComment READ currentComment NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentGenre READ currentGenre NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentYear READ currentYear NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentTrackNumber READ currentTrackNumber NOTIFY currentTrackChanged)
    Q_PROPERTY(qint64 currentDuration READ currentDuration NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentFormat READ currentFormat NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentBitrate READ currentBitrate NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentSampleRate READ currentSampleRate NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentBitDepth READ currentBitDepth NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentBpm READ currentBpm NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentChannelCount READ currentChannelCount NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentAlbumArt READ currentAlbumArt NOTIFY currentTrackChanged)
    Q_PROPERTY(bool currentIsLossless READ currentIsLossless NOTIFY currentTrackChanged)
    Q_PROPERTY(bool currentIsHiRes READ currentIsHiRes NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentDescription READ currentDescription NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentComposer READ currentComposer NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentOriginalArtist READ currentOriginalArtist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentCopyright READ currentCopyright NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentEncoder READ currentEncoder NOTIFY currentTrackChanged)
    Q_PROPERTY(qint64 currentDateAdded READ currentDateAdded NOTIFY currentTrackChanged)
    Q_PROPERTY(qint64 playlistDuration READ playlistDuration NOTIFY playlistDurationChanged)
    Q_PROPERTY(int searchRevision READ searchRevision NOTIFY searchRevisionChanged)
    Q_PROPERTY(bool deterministicShuffleEnabled READ deterministicShuffleEnabled WRITE setDeterministicShuffleEnabled NOTIFY deterministicShuffleEnabledChanged)
    Q_PROPERTY(quint32 shuffleSeed READ shuffleSeed WRITE setShuffleSeed NOTIFY shuffleSeedChanged)
    Q_PROPERTY(bool canResetPlaylist READ canResetPlaylist NOTIFY canResetPlaylistChanged)
    Q_PROPERTY(bool hasChapters READ currentHasChapters NOTIFY currentChaptersChanged)
    Q_PROPERTY(QVariantList currentChapters READ currentChapters NOTIFY currentChaptersChanged)
    Q_PROPERTY(int currentChapterCount READ currentChapterCount NOTIFY currentChaptersChanged)

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
        CommentRole,
        GenreRole,
        YearRole,
        TrackNumberRole,
        DurationRole,
        DisplayNameRole,
        FormatRole,
        BitrateRole,
        SampleRateRole,
        BitDepthRole,
        BpmRole,
        ChannelCountRole,
        AlbumArtRole,
        HasChaptersRole,
        DescriptionRole,
        ComposerRole,
        OriginalArtistRole,
        CopyrightRole,
        UrlRole,
        EncoderRole,
        FileNameRole,
        DateAddedRole,
        TrackSummaryRole,
        PlaylistPositionRole
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
    QString currentComment() const;
    QString currentGenre() const;
    QString currentYear() const;
    QString currentTrackNumber() const;
    qint64 currentDuration() const;
    QString currentFilePath() const;
    QString currentFormat() const;
    int currentBitrate() const;
    int currentSampleRate() const;
    int currentBitDepth() const;
    int currentBpm() const;
    int currentChannelCount() const;
    QString currentAlbumArt() const;
    bool currentIsLossless() const;
    bool currentIsHiRes() const;
    QString currentDescription() const;
    QString currentComposer() const;
    QString currentOriginalArtist() const;
    QString currentCopyright() const;
    QString currentUrl() const;
    QString currentEncoder() const;
    qint64 currentDateAdded() const;
    qint64 playlistDuration() const;
    int searchRevision() const { return m_searchUiRevision; }
    bool deterministicShuffleEnabled() const { return m_deterministicShuffleEnabled; }
    quint32 shuffleSeed() const { return m_shuffleSeed; }
    bool repeatableShuffle() const { return m_repeatableShuffle; }
    void setDeterministicShuffleEnabled(bool enabled);
    void setShuffleSeed(quint32 seed);
    void setRepeatableShuffle(bool enabled);
    void setAutoAddTracksFromPlaylistFolderEnabled(bool enabled);
    void configureLibraryStorage(bool enabled, const QString &databasePath);
    void recordPlaybackEvents(const QVector<TrackPlaybackEvent> &events, bool blocking = false);

    Q_INVOKABLE void addFile(const QString &filePath);
    Q_INVOKABLE void addFiles(const QStringList &filePaths);
    Q_INVOKABLE QVariantMap addFilesWithReport(const QStringList &filePaths);
    Q_INVOKABLE void addFolder(const QUrl &folderUrl);
    Q_INVOKABLE void addUrl(const QUrl &url);
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void insertUrlsAt(int index, const QList<QUrl> &urls);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void move(int from, int to);

    Q_INVOKABLE QString getFilePath(int index) const;
    Q_INVOKABLE QVariantMap trackInfoAt(int index) const;
    Q_INVOKABLE QVariantMap currentTrackInfo() const;
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
    Q_INVOKABLE bool sortByColumn(const QString &columnId, Qt::SortOrder order);
    Q_INVOKABLE void restoreBaselineOrder();
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
    Q_INVOKABLE bool canResetPlaylist() const;
    Q_INVOKABLE bool resetPlaylist();
    Q_INVOKABLE void refreshPlaylist();
    Q_INVOKABLE void captureBaselineSnapshot();
    Q_INVOKABLE QVariantList exportBaselineSnapshot() const;
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
    Q_INVOKABLE bool hasChapters(int index) const;
    Q_INVOKABLE QVariantList chaptersForIndex(int index) const;
    Q_INVOKABLE QVariantList currentChapters() const;
    Q_INVOKABLE bool currentHasChapters() const;
    Q_INVOKABLE int currentChapterCount() const;
    Q_INVOKABLE int chapterIndexAtPosition(int trackIndex, qint64 positionMs) const;
    Q_INVOKABLE int currentChapterIndexAtPosition(qint64 positionMs) const;
    Q_INVOKABLE QVariantMap chapterAt(int trackIndex, int chapterIndex) const;
    Q_INVOKABLE QString chapterTitleAtPosition(int trackIndex, qint64 positionMs) const;
    Q_INVOKABLE QString currentChapterTitleAtPosition(qint64 positionMs) const;

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
    void playlistDurationChanged();
    void canResetPlaylistChanged();
    void currentChaptersChanged();

private:
    friend class PlaybackController;

    struct ParsedMetadata {
        QString filePath;
        QString title;
        QString artist;
        QString album;
        QString comment;
        QString genre;
        QString year;
        QString trackNumber;
        QString description;
        QString composer;
        QString originalArtist;
        QString copyright;
        QString url;
        QString encoder;
        qint64 duration = 0;
        QString format;
        int bitrate = 0;
        int sampleRate = 0;
        int bitDepth = 0;
        int bpm = 0;
        int channelCount = 0;
        QString albumArt;
        bool albumArtChecked = false;
        QVector<TrackChapter> chapters;
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
        int searchRevision = 0;
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
        int searchRevision = 0;
        QString normalizedQuery;
        int fieldMask = SearchFieldAll;
        int quickFilterMask = SearchQuickFilterNone;
        QVector<quint8> matches;
        QVector<int> prefixMatches;
        int matchCount = 0;
        bool success = false;
    };
    static AsyncSearchResult computeAsyncSearch(AsyncSearchRequest request);
    struct AppendReport {
        int firstInsertedIndex = -1;
        int lastInsertedIndex = -1;
        int insertedCount = 0;
        QStringList insertedFilePaths;
    };
    void scheduleAsyncSearch(const QString &normalizedQuery,
                             int fieldMask,
                             int quickFilterMask) const;
    void launchAsyncSearch(const QString &normalizedQuery,
                           int fieldMask,
                           int quickFilterMask) const;
    void onAsyncSearchFinished();
    void notifySearchResultsUpdated();
    void applyParsedMetadata(const ParsedMetadata &metadata);
    void applyParsedMetadataBatch(const QVector<ParsedMetadata> &batch);
    void scheduleMetadataRead(const QString &filePath, bool includeAlbumArt);
    void enqueueMetadataRead(const QString &filePath, bool includeAlbumArt, bool highPriority = false);
    void pumpMetadataReadQueue();
    static QString buildSearchTextLower(const Track &track);
    static void updateTrackSearchBlob(Track &track);
    void rebuildFilePathIndexCache();
    void internTrackStrings(Track &track);
    QString internString(const QString &value);
    void invalidateSearchCache(bool rebuildPathIndex = true);
    void resetTransientSearchState();
    void resetTransientMetadataState();
    void ensureSearchCache(const QString &normalizedQuery,
                           int fieldMask,
                           int quickFilterMask) const;
    const Track *currentTrackPtr() const;
    static bool hasSupportedAudioExtension(const QString &filePath);
    static bool isLosslessFormat(const QString &format);
    static bool isWatchedPlaylistCandidateFile(const QString &filePath);
    static QString normalizedLocalTrackPath(const QString &filePath);
    static QString dominantPlaylistFolder(const QVector<Track> &tracks, bool collectionViewActive);
    static QStringList watchedPlaylistFolderEntries(const QString &folderPath);
    int findIndexByPath(const QString &filePath) const;
    quint32 nextShuffleSeed() const;
    void setCurrentIndexSilently(int index);
    void applyCurrentIndex(int index, bool emitTrackSelectedSignal);
    AppendReport appendAcceptedTracks(QVector<Track> acceptedTracks,
                                      const QVector<int> &ingestTrackOffsets,
                                      const QVector<int> &metadataTrackOffsets);
    AppendReport insertAcceptedTracks(int index,
                                      QVector<Track> acceptedTracks,
                                      const QVector<int> &ingestTrackOffsets,
                                      const QVector<int> &metadataTrackOffsets);
    void updatePlaylistFolderWatch();
    void rescanWatchedPlaylistFolder();
    void loadMetadata(int index, bool includeAlbumArt = false, bool forceReload = false);
    void trimAlbumArtToCurrentTrack(bool emitDataChangedForRows = false);
    void syncCurrentAlbumArtCache();
    void updateProfilerPlaylistCount();

    QVector<Track> m_tracks;
    int m_currentIndex = -1;
    int m_searchRevision = 0;
    int m_structureRevision = 0;
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
    bool m_autoAddTracksFromPlaylistFolderEnabled = true;
    mutable quint64 m_shuffleGeneration = 0;
    std::unique_ptr<LibraryRepository> m_libraryRepository;
    std::unique_ptr<SearchRepository> m_searchRepository;
    bool m_collectionViewActive = false;
    QSet<QString> m_stringPool;
    QThreadPool m_metadataThreadPool;
    QHash<QString, bool> m_pendingMetadataReads;
    QQueue<QString> m_pendingMetadataReadOrder;
    QHash<QString, bool> m_inFlightMetadataReads;
    int m_inFlightMetadataBatches = 0;
    int m_metadataFastStartBatchesRemaining = 4;
    QHash<QString, QVector<int>> m_filePathToIndices;
    quint64 m_metadataReadGeneration = 0;
    QString m_currentAlbumArt;
    QFileSystemWatcher m_playlistFolderWatcher;
    QTimer m_playlistFolderRescanTimer;
    QString m_watchedPlaylistFolder;
    QStringList m_sourceFolders;
    QSet<QString> m_knownWatchedFolderEntries;
    QVector<Track> m_baselineTracks;
};

#endif // TRACKMODEL_H
