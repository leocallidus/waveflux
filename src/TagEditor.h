#ifndef TAGEDITOR_H
#define TAGEDITOR_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief TagEditor - Interface for reading/writing audio metadata using TagLib
 * 
 * Provides methods for editing common audio tags (title, artist, album)
 * and is exposed to QML for the tag editing dialog.
 */
class TagEditor : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString artist READ artist WRITE setArtist NOTIFY artistChanged)
    Q_PROPERTY(QString album READ album WRITE setAlbum NOTIFY albumChanged)
    Q_PROPERTY(QString genre READ genre WRITE setGenre NOTIFY genreChanged)
    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(int trackNumber READ trackNumber WRITE setTrackNumber NOTIFY trackNumberChanged)
    Q_PROPERTY(QString coverImagePath READ coverImagePath WRITE setCoverImagePath NOTIFY coverImagePathChanged)
    Q_PROPERTY(bool removeCover READ removeCover WRITE setRemoveCover NOTIFY removeCoverChanged)
    Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY hasChangesChanged)
    
public:
    explicit TagEditor(QObject *parent = nullptr);
    
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

    QString coverImagePath() const { return m_coverImagePath; }
    void setCoverImagePath(const QString &coverImagePath);

    bool removeCover() const { return m_removeCover; }
    void setRemoveCover(bool removeCover);
    
    bool hasChanges() const { return m_hasChanges; }
    
    Q_INVOKABLE void loadTags();
    Q_INVOKABLE bool saveTags();
    Q_INVOKABLE bool saveTagsForFiles(const QStringList &filePaths,
                                      bool applyTitle,
                                      const QString &title,
                                      bool applyArtist,
                                      const QString &artist,
                                      bool applyAlbum,
                                      const QString &album,
                                      bool applyGenre,
                                      const QString &genre,
                                      bool applyYear,
                                      int year,
                                      bool applyTrackNumber,
                                      int trackNumber);
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
    void coverImagePathChanged();
    void removeCoverChanged();
    void hasChangesChanged();
    void saveSucceeded();
    void saveFailed(const QString &error);
    
private:
    void markChanged();
    
    QString m_filePath;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_genre;
    int m_year = 0;
    int m_trackNumber = 0;
    QString m_coverImagePath;
    bool m_removeCover = false;
    bool m_hasChanges = false;
    
    // Original values for revert
    QString m_originalTitle;
    QString m_originalArtist;
    QString m_originalAlbum;
    QString m_originalGenre;
    int m_originalYear = 0;
    int m_originalTrackNumber = 0;
    QString m_originalCoverImagePath;
    bool m_originalRemoveCover = false;
};

#endif // TAGEDITOR_H
