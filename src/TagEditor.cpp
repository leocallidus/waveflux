#include "TagEditor.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>

#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tag.h>

namespace {
TagLib::String toTagLibString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return TagLib::String(utf8.constData(), TagLib::String::UTF8);
}

QString toQString(const TagLib::String &value)
{
    return QString::fromUtf8(value.toCString(true));
}

QString normalizeLocalPath(const QString &pathOrUrl)
{
    const QString trimmed = pathOrUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl asUrl(trimmed);
    if (asUrl.isValid() && !asUrl.scheme().isEmpty()) {
        if (!asUrl.isLocalFile()) {
            return {};
        }
        return QDir::cleanPath(asUrl.toLocalFile());
    }

    return QDir::cleanPath(trimmed);
}

QString upperExtension(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().trimmed();
    return suffix.isEmpty() ? QString() : suffix.toUpper();
}

QString resolveImageMimeType(const QString &imagePath)
{
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(imagePath, QMimeDatabase::MatchContent);
    if (!mimeType.isValid() || mimeType.name().isEmpty()) {
        mimeType = mimeDb.mimeTypeForFile(imagePath, QMimeDatabase::MatchExtension);
    }

    QString mime = mimeType.name().trimmed().toLower();
    if (mime == QStringLiteral("image/jpg")) {
        mime = QStringLiteral("image/jpeg");
    }

    return mime.startsWith(QStringLiteral("image/")) ? mime : QString();
}

void applyCommonTags(TagLib::Tag *tag,
                     const QString &title,
                     const QString &artist,
                     const QString &album,
                     const QString &genre,
                     int year,
                     int trackNumber)
{
    if (!tag) {
        return;
    }

    tag->setTitle(toTagLibString(title));
    tag->setArtist(toTagLibString(artist));
    tag->setAlbum(toTagLibString(album));
    tag->setGenre(toTagLibString(genre));
    tag->setYear(static_cast<unsigned int>(qMax(0, year)));
    tag->setTrack(static_cast<unsigned int>(qMax(0, trackNumber)));
}

bool readCoverImage(const QString &imagePath,
                    TagLib::ByteVector *data,
                    QString *mimeType,
                    QString *error)
{
    if (!data || !mimeType || !error) {
        return false;
    }

    if (imagePath.trimmed().isEmpty()) {
        *error = QStringLiteral("No cover image selected.");
        return false;
    }

    QFile imageFile(imagePath);
    if (!imageFile.exists()) {
        *error = QStringLiteral("Cover image file does not exist.");
        return false;
    }

    if (!imageFile.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Failed to read cover image file.");
        return false;
    }

    const QByteArray raw = imageFile.readAll();
    if (raw.isEmpty()) {
        *error = QStringLiteral("Cover image file is empty.");
        return false;
    }

    const QString resolvedMimeType = resolveImageMimeType(imagePath);
    if (resolvedMimeType.isEmpty()) {
        *error = QStringLiteral("Unsupported cover image format. Use PNG or JPEG.");
        return false;
    }

    *data = TagLib::ByteVector(raw.constData(), static_cast<unsigned int>(raw.size()));
    *mimeType = resolvedMimeType;
    return true;
}

bool applyMp3Cover(TagLib::MPEG::File *file,
                   const TagLib::ByteVector &imageData,
                   const QString &mimeType,
                   bool removeCover,
                   QString *error)
{
    if (!file) {
        if (error) {
            *error = QStringLiteral("Internal error while writing MP3 cover.");
        }
        return false;
    }

    TagLib::ID3v2::Tag *id3v2Tag = file->ID3v2Tag(true);
    if (!id3v2Tag) {
        if (error) {
            *error = QStringLiteral("Failed to create ID3v2 tag for cover image.");
        }
        return false;
    }

    id3v2Tag->removeFrames("APIC");
    if (removeCover) {
        return true;
    }

    auto *coverFrame = new TagLib::ID3v2::AttachedPictureFrame;
    coverFrame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    coverFrame->setMimeType(toTagLibString(mimeType));
    coverFrame->setDescription(TagLib::String("Cover", TagLib::String::UTF8));
    coverFrame->setPicture(imageData);
    id3v2Tag->addFrame(coverFrame);
    return true;
}

bool applyFlacCover(TagLib::FLAC::File *file,
                    const TagLib::ByteVector &imageData,
                    const QString &mimeType,
                    bool removeCover,
                    QString *error)
{
    if (!file) {
        if (error) {
            *error = QStringLiteral("Internal error while writing FLAC cover.");
        }
        return false;
    }

    file->removePictures();
    if (removeCover) {
        return true;
    }

    auto *picture = new TagLib::FLAC::Picture;
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(toTagLibString(mimeType));
    picture->setDescription(TagLib::String("Cover", TagLib::String::UTF8));
    picture->setData(imageData);
    file->addPicture(picture);
    return true;
}
} // namespace

TagEditor::TagEditor(QObject *parent)
    : QObject(parent)
{
}

void TagEditor::setFilePath(const QString &path)
{
    if (m_filePath != path) {
        m_filePath = path;
        emit filePathChanged();
        loadTags();
    }
}

void TagEditor::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
        markChanged();
    }
}

void TagEditor::setArtist(const QString &artist)
{
    if (m_artist != artist) {
        m_artist = artist;
        emit artistChanged();
        markChanged();
    }
}

void TagEditor::setAlbum(const QString &album)
{
    if (m_album != album) {
        m_album = album;
        emit albumChanged();
        markChanged();
    }
}

void TagEditor::setGenre(const QString &genre)
{
    if (m_genre != genre) {
        m_genre = genre;
        emit genreChanged();
        markChanged();
    }
}

void TagEditor::setYear(int year)
{
    if (m_year != year) {
        m_year = year;
        emit yearChanged();
        markChanged();
    }
}

void TagEditor::setTrackNumber(int track)
{
    if (m_trackNumber != track) {
        m_trackNumber = track;
        emit trackNumberChanged();
        markChanged();
    }
}

void TagEditor::setCoverImagePath(const QString &coverImagePath)
{
    const QString normalizedPath = normalizeLocalPath(coverImagePath);
    if (m_coverImagePath != normalizedPath) {
        m_coverImagePath = normalizedPath;
        emit coverImagePathChanged();

        if (!m_coverImagePath.isEmpty() && m_removeCover) {
            m_removeCover = false;
            emit removeCoverChanged();
        }

        markChanged();
    }
}

void TagEditor::setRemoveCover(bool removeCover)
{
    if (m_removeCover == removeCover) {
        return;
    }

    m_removeCover = removeCover;
    emit removeCoverChanged();

    if (m_removeCover && !m_coverImagePath.isEmpty()) {
        m_coverImagePath.clear();
        emit coverImagePathChanged();
    }

    markChanged();
}

void TagEditor::loadTags()
{
    if (m_filePath.isEmpty()) return;
    
    TagLib::FileRef file(m_filePath.toUtf8().constData());
    
    if (file.isNull() || !file.tag()) {
        qWarning() << "Failed to read tags from:" << m_filePath;
        return;
    }
    
    TagLib::Tag *tag = file.tag();
    
    m_title = toQString(tag->title());
    m_artist = toQString(tag->artist());
    m_album = toQString(tag->album());
    m_genre = toQString(tag->genre());
    m_year = tag->year();
    m_trackNumber = tag->track();
    m_coverImagePath.clear();
    m_removeCover = false;
    
    // Store originals for revert
    m_originalTitle = m_title;
    m_originalArtist = m_artist;
    m_originalAlbum = m_album;
    m_originalGenre = m_genre;
    m_originalYear = m_year;
    m_originalTrackNumber = m_trackNumber;
    m_originalCoverImagePath = m_coverImagePath;
    m_originalRemoveCover = m_removeCover;
    
    m_hasChanges = false;
    
    emit titleChanged();
    emit artistChanged();
    emit albumChanged();
    emit genreChanged();
    emit yearChanged();
    emit trackNumberChanged();
    emit coverImagePathChanged();
    emit removeCoverChanged();
    emit hasChangesChanged();
}

bool TagEditor::saveTags()
{
    if (m_filePath.isEmpty()) {
        emit saveFailed("No file loaded");
        return false;
    }
    
    const bool coverChangeRequested = m_removeCover || !m_coverImagePath.isEmpty();
    TagLib::ByteVector imageData;
    QString imageMimeType;
    QString coverError;
    if (coverChangeRequested && !m_removeCover) {
        if (!readCoverImage(m_coverImagePath, &imageData, &imageMimeType, &coverError)) {
            emit saveFailed(coverError);
            return false;
        }
    }

    const QByteArray filePathUtf8 = m_filePath.toUtf8();
    const QString extension = upperExtension(m_filePath);

    if (coverChangeRequested && extension == QStringLiteral("MP3")) {
        TagLib::MPEG::File file(filePathUtf8.constData());
        if (!file.isValid()) {
            emit saveFailed("Failed to open MP3 file for writing");
            return false;
        }

        (void)file.ID3v2Tag(true);
        TagLib::Tag *tag = file.tag();
        if (!tag) {
            emit saveFailed("Failed to access MP3 tag");
            return false;
        }

        applyCommonTags(tag, m_title, m_artist, m_album, m_genre, m_year, m_trackNumber);
        if (!applyMp3Cover(&file, imageData, imageMimeType, m_removeCover, &coverError)) {
            emit saveFailed(coverError.isEmpty() ? QStringLiteral("Failed to update MP3 cover") : coverError);
            return false;
        }

        if (!file.save()) {
            emit saveFailed("Failed to save MP3 tags");
            return false;
        }
    } else if (coverChangeRequested && extension == QStringLiteral("FLAC")) {
        TagLib::FLAC::File file(filePathUtf8.constData());
        if (!file.isValid()) {
            emit saveFailed("Failed to open FLAC file for writing");
            return false;
        }

        if (!file.tag()) {
            (void)file.xiphComment(true);
        }

        TagLib::Tag *tag = file.tag();
        if (!tag) {
            emit saveFailed("Failed to access FLAC tag");
            return false;
        }

        applyCommonTags(tag, m_title, m_artist, m_album, m_genre, m_year, m_trackNumber);
        if (!applyFlacCover(&file, imageData, imageMimeType, m_removeCover, &coverError)) {
            emit saveFailed(coverError.isEmpty() ? QStringLiteral("Failed to update FLAC cover") : coverError);
            return false;
        }

        if (!file.save()) {
            emit saveFailed("Failed to save FLAC tags");
            return false;
        }
    } else if (coverChangeRequested) {
        emit saveFailed("Cover editing is currently supported for MP3 and FLAC files.");
        return false;
    } else {
        TagLib::FileRef file(filePathUtf8.constData());
        if (file.isNull() || !file.tag()) {
            emit saveFailed("Failed to open file for writing");
            return false;
        }

        applyCommonTags(file.tag(), m_title, m_artist, m_album, m_genre, m_year, m_trackNumber);
        if (!file.save()) {
            emit saveFailed("Failed to save tags");
            return false;
        }
    }
    
    // Update originals
    m_originalTitle = m_title;
    m_originalArtist = m_artist;
    m_originalAlbum = m_album;
    m_originalGenre = m_genre;
    m_originalYear = m_year;
    m_originalTrackNumber = m_trackNumber;
    m_originalCoverImagePath = m_coverImagePath;
    m_originalRemoveCover = m_removeCover;
    
    m_hasChanges = false;
    emit hasChangesChanged();
    emit saveSucceeded();
    
    return true;
}

bool TagEditor::saveTagsForFiles(const QStringList &filePaths,
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
                                 int trackNumber)
{
    if (filePaths.isEmpty()) {
        emit saveFailed("No files selected");
        return false;
    }

    if (!applyTitle && !applyArtist && !applyAlbum && !applyGenre && !applyYear && !applyTrackNumber) {
        emit saveFailed("No tag fields selected");
        return false;
    }

    int updatedCount = 0;
    int failedCount = 0;
    QString firstError;

    for (const QString &path : filePaths) {
        if (path.isEmpty()) {
            continue;
        }

        TagLib::FileRef file(path.toUtf8().constData());
        if (file.isNull() || !file.tag()) {
            ++failedCount;
            if (firstError.isEmpty()) {
                firstError = QStringLiteral("Failed to open %1").arg(path);
            }
            continue;
        }

        TagLib::Tag *tag = file.tag();
        if (applyTitle) {
            tag->setTitle(toTagLibString(title));
        }
        if (applyArtist) {
            tag->setArtist(toTagLibString(artist));
        }
        if (applyAlbum) {
            tag->setAlbum(toTagLibString(album));
        }
        if (applyGenre) {
            tag->setGenre(toTagLibString(genre));
        }
        if (applyYear) {
            tag->setYear(static_cast<unsigned int>(qMax(0, year)));
        }
        if (applyTrackNumber) {
            tag->setTrack(static_cast<unsigned int>(qMax(0, trackNumber)));
        }

        if (!file.save()) {
            ++failedCount;
            if (firstError.isEmpty()) {
                firstError = QStringLiteral("Failed to save %1").arg(path);
            }
            continue;
        }

        ++updatedCount;
    }

    if (failedCount > 0) {
        QString message = QStringLiteral("Updated %1 file(s), failed %2.").arg(updatedCount).arg(failedCount);
        if (!firstError.isEmpty()) {
            message += QStringLiteral(" ") + firstError;
        }
        emit saveFailed(message);
        return false;
    }

    if (updatedCount <= 0) {
        emit saveFailed("No files were updated");
        return false;
    }

    emit saveSucceeded();
    return true;
}

void TagEditor::revertChanges()
{
    m_title = m_originalTitle;
    m_artist = m_originalArtist;
    m_album = m_originalAlbum;
    m_genre = m_originalGenre;
    m_year = m_originalYear;
    m_trackNumber = m_originalTrackNumber;
    m_coverImagePath = m_originalCoverImagePath;
    m_removeCover = m_originalRemoveCover;
    
    m_hasChanges = false;
    
    emit titleChanged();
    emit artistChanged();
    emit albumChanged();
    emit genreChanged();
    emit yearChanged();
    emit trackNumberChanged();
    emit coverImagePathChanged();
    emit removeCoverChanged();
    emit hasChangesChanged();
}

void TagEditor::clearCover()
{
    setRemoveCover(true);
}

void TagEditor::markChanged()
{
    if (!m_hasChanges) {
        m_hasChanges = true;
        emit hasChangesChanged();
    }
}
