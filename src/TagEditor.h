#ifndef TAGEDITOR_H
#define TAGEDITOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

/**
 * @brief TagChapterItem - Chapter data representation for TagEditor
 */
struct TagChapterItem {
    QString title;
    qint64 startTimeMs = 0;
    qint64 endTimeMs = 0;

    bool operator==(const TagChapterItem &other) const {
        return title == other.title
            && startTimeMs == other.startTimeMs
            && endTimeMs == other.endTimeMs;
    }
    bool operator!=(const TagChapterItem &other) const {
        return !(*this == other);
    }
};

/**
 * @brief TagEditor - Interface for reading/writing audio metadata using TagLib
 * 
 * Provides methods for editing standard audio tags (title, artist, album, genre, year, track, bpm,
 * comment, composer, original artist, copyright, url, encoder), managing cover artwork with export support,
 * chapter inspection and modification, and audio technical specifications readout.
 */
class TagEditor : public QObject
{
    Q_OBJECT
    
    // Core & Metadata properties
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString artist READ artist WRITE setArtist NOTIFY artistChanged)
    Q_PROPERTY(QString album READ album WRITE setAlbum NOTIFY albumChanged)
    Q_PROPERTY(QString genre READ genre WRITE setGenre NOTIFY genreChanged)
    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(int trackNumber READ trackNumber WRITE setTrackNumber NOTIFY trackNumberChanged)
    Q_PROPERTY(int bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
    Q_PROPERTY(QString composer READ composer WRITE setComposer NOTIFY composerChanged)
    Q_PROPERTY(QString originalArtist READ originalArtist WRITE setOriginalArtist NOTIFY originalArtistChanged)
    Q_PROPERTY(QString copyright READ copyright WRITE setCopyright NOTIFY copyrightChanged)
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QString encoder READ encoder WRITE setEncoder NOTIFY encoderChanged)
    
    // Cover properties
    Q_PROPERTY(QString coverImagePath READ coverImagePath WRITE setCoverImagePath NOTIFY coverImagePathChanged)
    Q_PROPERTY(QString coverPreviewSource READ coverPreviewSource NOTIFY coverPreviewSourceChanged)
    Q_PROPERTY(bool removeCover READ removeCover WRITE setRemoveCover NOTIFY removeCoverChanged)
    Q_PROPERTY(bool hasCoverImage READ hasCoverImage NOTIFY coverPreviewSourceChanged)

    // Technical Info properties
    Q_PROPERTY(QString fileFormat READ fileFormat NOTIFY techInfoChanged)
    Q_PROPERTY(int bitrate READ bitrate NOTIFY techInfoChanged)
    Q_PROPERTY(QString bitrateFormatted READ bitrateFormatted NOTIFY techInfoChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY techInfoChanged)
    Q_PROPERTY(QString sampleRateFormatted READ sampleRateFormatted NOTIFY techInfoChanged)
    Q_PROPERTY(int channels READ channels NOTIFY techInfoChanged)
    Q_PROPERTY(QString channelMode READ channelMode NOTIFY techInfoChanged)
    Q_PROPERTY(qint64 fileSizeBytes READ fileSizeBytes NOTIFY techInfoChanged)
    Q_PROPERTY(QString fileSizeFormatted READ fileSizeFormatted NOTIFY techInfoChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY techInfoChanged)
    Q_PROPERTY(QString durationFormatted READ durationFormatted NOTIFY techInfoChanged)
    Q_PROPERTY(bool isTrackerModule READ isTrackerModule NOTIFY filePathChanged)
    Q_PROPERTY(QString trackerWarningMessage READ trackerWarningMessage CONSTANT)

    // Chapters properties
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int chapterCount READ chapterCount NOTIFY chaptersChanged)
    Q_PROPERTY(bool hasChapters READ hasChapters NOTIFY chaptersChanged)

    // State properties
    Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY hasChangesChanged)

public:
    explicit TagEditor(QObject *parent = nullptr);

    // Getters & Setters
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);

    QString title() const { return m_title; }
    void setTitle(const QString &title);

    QString artist() const { return m_artist; }
    void setArtist(const QString &artist);

    QString album() const { return m_album; }
    void setAlbum(const QString &album);

    QString genre() const { return m_genre; }
    void setGenre(const QString &genre);

    int year() const { return m_year; }
    void setYear(int year);

    int trackNumber() const { return m_trackNumber; }
    void setTrackNumber(int track);

    int bpm() const { return m_bpm; }
    void setBpm(int bpm);

    QString comment() const { return m_comment; }
    void setComment(const QString &comment);

    QString composer() const { return m_composer; }
    void setComposer(const QString &composer);

    QString originalArtist() const { return m_originalArtist; }
    void setOriginalArtist(const QString &originalArtist);

    QString copyright() const { return m_copyright; }
    void setCopyright(const QString &copyright);

    QString url() const { return m_url; }
    void setUrl(const QString &url);

    QString encoder() const { return m_encoder; }
    void setEncoder(const QString &encoder);

    QString coverImagePath() const { return m_coverImagePath; }
    void setCoverImagePath(const QString &coverImagePath);
    QString coverPreviewSource() const { return m_coverPreviewSource; }

    bool removeCover() const { return m_removeCover; }
    void setRemoveCover(bool removeCover);
    bool hasCoverImage() const;

    // Technical Info Getters
    QString fileFormat() const { return m_fileFormat; }
    int bitrate() const { return m_bitrate; }
    QString bitrateFormatted() const { return m_bitrateFormatted; }
    int sampleRate() const { return m_sampleRate; }
    QString sampleRateFormatted() const { return m_sampleRateFormatted; }
    int channels() const { return m_channels; }
    QString channelMode() const { return m_channelMode; }
    qint64 fileSizeBytes() const { return m_fileSizeBytes; }
    QString fileSizeFormatted() const { return m_fileSizeFormatted; }
    qint64 durationMs() const { return m_durationMs; }
    QString durationFormatted() const { return m_durationFormatted; }
    bool isTrackerModule() const;
    QString trackerWarningMessage() const;

    // Chapters
    QVariantList chapters() const;
    int chapterCount() const { return m_chapters.size(); }
    bool hasChapters() const { return !m_chapters.isEmpty(); }

    Q_INVOKABLE void addChapter(const QString &title, qint64 startTimeMs, qint64 endTimeMs = 0);
    Q_INVOKABLE void updateChapter(int index, const QString &title, qint64 startTimeMs, qint64 endTimeMs = 0);
    Q_INVOKABLE void addChapterSeconds(const QString &title, int startTimeSec, int endTimeSec = 0);
    Q_INVOKABLE void updateChapterSeconds(int index, const QString &title, int startTimeSec, int endTimeSec = 0);
    Q_INVOKABLE void removeChapter(int index);
    Q_INVOKABLE void clearChapters();
    Q_INVOKABLE void setChapters(const QVariantList &chaptersList);

    bool hasChanges() const { return m_hasChanges; }
    Q_INVOKABLE bool supportsCoverEditing() const;
    Q_INVOKABLE QString coverEditingUnsupportedMessage() const;
    Q_INVOKABLE QString suggestedCoverFileName() const;
    Q_INVOKABLE bool exportCoverImage(const QString &targetPath);
    Q_INVOKABLE bool isFileTrackerModule(const QString &path) const;

    Q_INVOKABLE void loadTags();
    Q_INVOKABLE bool saveTags();
    Q_INVOKABLE bool saveTagsForFiles(const QStringList &filePaths,
                                      bool applyTitle, const QString &title,
                                      bool applyArtist, const QString &artist,
                                      bool applyAlbum, const QString &album,
                                      bool applyGenre, const QString &genre,
                                      bool applyYear, int year,
                                      bool applyTrackNumber, int trackNumber,
                                      bool applyBpm = false, int bpm = 0,
                                      bool applyComment = false, const QString &comment = QString(),
                                      bool applyComposer = false, const QString &composer = QString(),
                                      bool applyOriginalArtist = false, const QString &originalArtist = QString(),
                                      bool applyCopyright = false, const QString &copyright = QString(),
                                      bool applyUrl = false, const QString &url = QString(),
                                      bool applyEncoder = false, const QString &encoder = QString());
    Q_INVOKABLE void revertChanges();
    Q_INVOKABLE void clearCover();

signals:
    void filePathChanged();
    void titleChanged();
    void artistChanged();
    void albumChanged();
    void genreChanged();
    void yearChanged();
    void trackNumberChanged();
    void bpmChanged();
    void commentChanged();
    void composerChanged();
    void originalArtistChanged();
    void copyrightChanged();
    void urlChanged();
    void encoderChanged();
    void coverImagePathChanged();
    void coverPreviewSourceChanged();
    void removeCoverChanged();
    void techInfoChanged();
    void chaptersChanged();
    void hasChangesChanged();
    void saveSucceeded();
    void saveFailed(const QString &error);
    void coverExportSucceeded(const QString &savedPath);
    void coverExportFailed(const QString &error);

private:
    void markChanged();
    void extractTechnicalInfo();
    void resetTechnicalInfo();

    QString m_filePath;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_genre;
    int m_year = 0;
    int m_trackNumber = 0;
    int m_bpm = 0;
    QString m_comment;
    QString m_composer;
    QString m_originalArtist;
    QString m_copyright;
    QString m_url;
    QString m_encoder;

    QString m_coverImagePath;
    QString m_coverPreviewSource;
    bool m_removeCover = false;

    // Technical specifications
    QString m_fileFormat;
    int m_bitrate = 0;
    QString m_bitrateFormatted;
    int m_sampleRate = 0;
    QString m_sampleRateFormatted;
    int m_channels = 0;
    QString m_channelMode;
    qint64 m_fileSizeBytes = 0;
    QString m_fileSizeFormatted;
    qint64 m_durationMs = 0;
    QString m_durationFormatted;

    // Chapters
    QVector<TagChapterItem> m_chapters;
    bool m_chaptersModified = false;

    bool m_hasChanges = false;

    // Original values for revert
    QString m_originalTitle;
    QString m_originalArtistTrack;
    QString m_originalAlbum;
    QString m_originalGenre;
    int m_originalYear = 0;
    int m_originalTrackNumber = 0;
    int m_originalBpm = 0;
    QString m_originalComment;
    QString m_originalComposer;
    QString m_originalOriginalArtist;
    QString m_originalCopyright;
    QString m_originalUrl;
    QString m_originalEncoder;
    QString m_originalCoverImagePath;
    QString m_originalCoverPreviewSource;
    bool m_originalRemoveCover = false;
    QVector<TagChapterItem> m_originalChapters;
};

#endif // TAGEDITOR_H
