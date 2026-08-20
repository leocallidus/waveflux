#include "TagEditor.h"
#include "TagLibPath.h"
#include "playback/PlaybackBackendRouting.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpression>
#include <QUrl>

#include <taglib/taglib.h>
#include <taglib/tbytevector.h>
#include <taglib/tstring.h>
#include <taglib/tpropertymap.h>
#include <taglib/tfile.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/commentsframe.h>
#include <taglib/urllinkframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/chapterframe.h>
#include <taglib/tableofcontentsframe.h>
#include <taglib/id3v2tag.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/flacproperties.h>
#include <taglib/mpegfile.h>
#include <taglib/mpegheader.h>
#include <taglib/mpegproperties.h>
#include <taglib/wavfile.h>
#include <taglib/wavproperties.h>
#include <taglib/aifffile.h>
#include <taglib/aiffproperties.h>
#include <taglib/mp4file.h>
#include <taglib/mp4properties.h>
#if __has_include(<taglib/mp4chapter.h>)
#include <taglib/mp4chapter.h>
#endif
#include <taglib/xiphcomment.h>

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

int propertyToRoundedPositiveInt(const TagLib::PropertyMap &properties, const char *key)
{
    const auto values = properties[TagLib::String(key)];
    if (values.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const double value = QString::fromUtf8(values.front().toCString(true)).trimmed().toDouble(&ok);
    if (!ok || value <= 0.0) {
        return 0;
    }
    return qRound(value);
}

int bpmFromFile(TagLib::File *file)
{
    if (!file) {
        return 0;
    }

    const TagLib::PropertyMap properties = file->properties();
    int bpm = propertyToRoundedPositiveInt(properties, "BPM");
    if (bpm <= 0) {
        bpm = propertyToRoundedPositiveInt(properties, "TBPM");
    }
    if (bpm <= 0) {
        bpm = propertyToRoundedPositiveInt(properties, "TEMPO");
    }
    if (bpm <= 0) {
        bpm = propertyToRoundedPositiveInt(properties, "BEATS_PER_MINUTE");
    }
    return bpm;
}

void applyBpmProperty(TagLib::File *file, int bpm)
{
    if (!file) {
        return;
    }

    TagLib::PropertyMap properties = file->properties();
    properties.erase(TagLib::String("BPM"));
    properties.erase(TagLib::String("TBPM"));
    properties.erase(TagLib::String("TEMPO"));
    properties.erase(TagLib::String("BEATS_PER_MINUTE"));

    const int normalizedBpm = qMax(0, bpm);
    if (normalizedBpm > 0) {
        properties.insert(TagLib::String("BPM"),
                          TagLib::StringList(toTagLibString(QString::number(normalizedBpm))));
    }
    file->setProperties(properties);
}

QString normalizeLocalPath(const QString &pathOrUrl)
{
    QString trimmed = pathOrUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (trimmed.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        const QUrl asUrl(trimmed);
        if (asUrl.isValid() && asUrl.isLocalFile()) {
            return QDir::cleanPath(asUrl.toLocalFile());
        }
        trimmed = trimmed.mid(7);
    }

    if (trimmed.size() >= 3
        && trimmed[1] == QLatin1Char(':')
        && (trimmed[2] == QLatin1Char('/') || trimmed[2] == QLatin1Char('\\'))
        && trimmed[0].isLetter()) {
        if (trimmed.contains(QLatin1Char('%'))) {
            trimmed = QUrl::fromPercentEncoding(trimmed.toUtf8());
        }
        return QDir::cleanPath(trimmed);
    }

    if (trimmed.size() >= 4
        && trimmed[0] == QLatin1Char('/')
        && trimmed[2] == QLatin1Char(':')
        && (trimmed[3] == QLatin1Char('/') || trimmed[3] == QLatin1Char('\\'))
        && trimmed[1].isLetter()) {
        QString local = trimmed.mid(1);
        if (local.contains(QLatin1Char('%'))) {
            local = QUrl::fromPercentEncoding(local.toUtf8());
        }
        return QDir::cleanPath(local);
    }

    const QUrl asUrl(trimmed);
    if (asUrl.isValid() && !asUrl.scheme().isEmpty()) {
        if (!asUrl.isLocalFile()) {
            return {};
        }
        return QDir::cleanPath(asUrl.toLocalFile());
    }

    if (trimmed.contains(QLatin1Char('%'))) {
        trimmed = QUrl::fromPercentEncoding(trimmed.toUtf8());
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

QString dataUrlFromBytes(const TagLib::ByteVector &bytes, const QString &mimeType)
{
    if (bytes.isEmpty()) {
        return {};
    }

    const QByteArray raw(bytes.data(), static_cast<qsizetype>(bytes.size()));
    QString mime = mimeType.trimmed().toLower();
    if (mime == QStringLiteral("image/jpg")) {
        mime = QStringLiteral("image/jpeg");
    }
    if (!mime.startsWith(QStringLiteral("image/"))) {
        mime = QStringLiteral("image/jpeg");
    }

    return QStringLiteral("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(raw.toBase64()));
}

QString selectedCoverPreviewSource(const QString &imagePath)
{
    return imagePath.isEmpty() ? QString() : QUrl::fromLocalFile(imagePath).toString();
}

QString embeddedCoverPreviewSource(const QString &filePath)
{
    const QString extension = upperExtension(filePath);
    if (extension == QStringLiteral("MP3")) {
        const auto file = WaveFlux::TagLibPath::openMpegFile(filePath, false);
        if (!file) {
            return {};
        }

        TagLib::ID3v2::Tag *id3v2Tag = file->ID3v2Tag(false);
        if (!id3v2Tag) {
            return {};
        }

        const auto frames = id3v2Tag->frameListMap()["APIC"];
        if (frames.isEmpty()) {
            return {};
        }

        const TagLib::ID3v2::AttachedPictureFrame *fallbackFrame = nullptr;
        for (auto *rawFrame : frames) {
            auto *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(rawFrame);
            if (!frame) {
                continue;
            }
            if (!fallbackFrame) {
                fallbackFrame = frame;
            }
            if (frame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
                return dataUrlFromBytes(frame->picture(), toQString(frame->mimeType()));
            }
        }

        return fallbackFrame
            ? dataUrlFromBytes(fallbackFrame->picture(), toQString(fallbackFrame->mimeType()))
            : QString();
    }

    if (extension == QStringLiteral("FLAC")) {
        const auto file = WaveFlux::TagLibPath::openFlacFile(filePath, false);
        if (!file) {
            return {};
        }

        const auto pictures = file->pictureList();
        if (pictures.isEmpty()) {
            return {};
        }

        const TagLib::FLAC::Picture *fallbackPicture = nullptr;
        for (const auto *picture : pictures) {
            if (!picture) {
                continue;
            }
            if (!fallbackPicture) {
                fallbackPicture = picture;
            }
            if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
                return dataUrlFromBytes(picture->data(), toQString(picture->mimeType()));
            }
        }

        return fallbackPicture
            ? dataUrlFromBytes(fallbackPicture->data(), toQString(fallbackPicture->mimeType()))
            : QString();
    }

    return {};
}

QString unsupportedCoverEditingMessage(const QString &extension)
{
    const QString normalizedExtension = extension.trimmed().toUpper();
    if (normalizedExtension == QStringLiteral("WAV")) {
        return QStringLiteral("WAV cover art is not supported. Use MP3 or FLAC for embedded cover editing.");
    }

    if (!normalizedExtension.isEmpty()) {
        return QStringLiteral("Cover editing is not supported for %1 files. Use MP3 or FLAC for embedded cover editing.")
            .arg(normalizedExtension);
    }

    return QStringLiteral("Cover editing is currently supported for MP3 and FLAC files.");
}

void applyCommonTags(TagLib::Tag *tag,
                     const QString &title,
                     const QString &artist,
                     const QString &album,
                     const QString &genre,
                     int year,
                     int trackNumber,
                     const QString &comment)
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
    tag->setComment(toTagLibString(comment));
}

void applyExtendedId3v2Tags(TagLib::ID3v2::Tag *id3v2Tag,
                            const QString &composer,
                            const QString &originalArtist,
                            const QString &copyright,
                            const QString &url,
                            const QString &encoder,
                            int bpm)
{
    if (!id3v2Tag) {
        return;
    }

    // TCOM - Composer
    id3v2Tag->removeFrames("TCOM");
    if (!composer.trimmed().isEmpty()) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::UTF8);
        frame->setText(toTagLibString(composer));
        id3v2Tag->addFrame(frame);
    }

    // TOPE - Original Artist
    id3v2Tag->removeFrames("TOPE");
    if (!originalArtist.trimmed().isEmpty()) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TOPE", TagLib::String::UTF8);
        frame->setText(toTagLibString(originalArtist));
        id3v2Tag->addFrame(frame);
    }

    // TCOP - Copyright
    id3v2Tag->removeFrames("TCOP");
    if (!copyright.trimmed().isEmpty()) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TCOP", TagLib::String::UTF8);
        frame->setText(toTagLibString(copyright));
        id3v2Tag->addFrame(frame);
    }

    // WXXX / WOAR - URL
    id3v2Tag->removeFrames("WXXX");
    id3v2Tag->removeFrames("WOAR");
    if (!url.trimmed().isEmpty()) {
        auto *frame = new TagLib::ID3v2::UserUrlLinkFrame(TagLib::String::UTF8);
        frame->setUrl(toTagLibString(url));
        id3v2Tag->addFrame(frame);
    }

    // TSSE - Encoder
    id3v2Tag->removeFrames("TSSE");
    if (!encoder.trimmed().isEmpty()) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSSE", TagLib::String::UTF8);
        frame->setText(toTagLibString(encoder));
        id3v2Tag->addFrame(frame);
    }

    // TBPM - BPM
    id3v2Tag->removeFrames("TBPM");
    if (bpm > 0) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TBPM", TagLib::String::Latin1);
        frame->setText(TagLib::String(QString::number(bpm).toLatin1().constData()));
        id3v2Tag->addFrame(frame);
    }
}

void applyExtendedPropertyMap(TagLib::File *file,
                              const QString &composer,
                              const QString &originalArtist,
                              const QString &copyright,
                              const QString &url,
                              const QString &encoder,
                              int bpm)
{
    if (!file) {
        return;
    }

    TagLib::PropertyMap properties = file->properties();

    if (!composer.trimmed().isEmpty()) {
        properties.insert(TagLib::String("COMPOSER"), TagLib::StringList(toTagLibString(composer)));
    } else {
        properties.erase(TagLib::String("COMPOSER"));
    }

    if (!originalArtist.trimmed().isEmpty()) {
        properties.insert(TagLib::String("ORIGINALARTIST"), TagLib::StringList(toTagLibString(originalArtist)));
    } else {
        properties.erase(TagLib::String("ORIGINALARTIST"));
        properties.erase(TagLib::String("ORIGINAL ARTIST"));
    }

    if (!copyright.trimmed().isEmpty()) {
        properties.insert(TagLib::String("COPYRIGHT"), TagLib::StringList(toTagLibString(copyright)));
    } else {
        properties.erase(TagLib::String("COPYRIGHT"));
    }

    if (!url.trimmed().isEmpty()) {
        properties.insert(TagLib::String("URL"), TagLib::StringList(toTagLibString(url)));
    } else {
        properties.erase(TagLib::String("URL"));
        properties.erase(TagLib::String("WEBSITE"));
        properties.erase(TagLib::String("CONTACT"));
    }

    if (!encoder.trimmed().isEmpty()) {
        properties.insert(TagLib::String("ENCODER"), TagLib::StringList(toTagLibString(encoder)));
    } else {
        properties.erase(TagLib::String("ENCODER"));
        properties.erase(TagLib::String("ENCODEDBY"));
        properties.erase(TagLib::String("ENCODED_BY"));
        properties.erase(TagLib::String("TOOL"));
    }

    if (bpm > 0) {
        properties.insert(TagLib::String("BPM"), TagLib::StringList(toTagLibString(QString::number(bpm))));
    } else {
        properties.erase(TagLib::String("BPM"));
        properties.erase(TagLib::String("TBPM"));
        properties.erase(TagLib::String("TEMPO"));
        properties.erase(TagLib::String("BEATS_PER_MINUTE"));
    }

    file->setProperties(properties);
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

QString formatChapterTime(qint64 ms)
{
    if (ms < 0) {
        ms = 0;
    }
    const qint64 totalSec = ms / 1000;
    const qint64 h = totalSec / 3600;
    const qint64 m = (totalSec % 3600) / 60;
    const qint64 s = totalSec % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

qint64 parseVorbisChapterTimestamp(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    if (!trimmed.contains(QLatin1Char(':'))) {
        return -1;
    }
    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.size() < 2 || parts.size() > 3) {
        return -1;
    }
    bool ok1 = false, ok2 = false, ok3 = false;
    if (parts.size() == 3) {
        const double h = parts[0].toDouble(&ok1);
        const double m = parts[1].toDouble(&ok2);
        const double s = parts[2].toDouble(&ok3);
        if (ok1 && ok2 && ok3 && h >= 0 && m >= 0 && m < 60 && s >= 0 && s < 60) {
            return static_cast<qint64>((h * 3600.0 + m * 60.0 + s) * 1000.0);
        }
    } else if (parts.size() == 2) {
        const double m = parts[0].toDouble(&ok1);
        const double s = parts[1].toDouble(&ok2);
        if (ok1 && ok2 && m >= 0 && s >= 0 && s < 60) {
            return static_cast<qint64>((m * 60.0 + s) * 1000.0);
        }
    }
    return -1;
}

QVector<TagChapterItem> extractId3v2ChaptersFromTag(TagLib::ID3v2::Tag *id3v2)
{
    QVector<TagChapterItem> chapters;
    if (!id3v2) {
        return chapters;
    }
    const auto chapFrames = id3v2->frameListMap()["CHAP"];
    if (chapFrames.isEmpty()) {
        return chapters;
    }

    QHash<QByteArray, TagChapterItem> chaptersById;
    QList<QByteArray> orderedIds;

    const auto ctocFrames = id3v2->frameListMap()["CTOC"];
    for (const auto *rawCtoc : ctocFrames) {
        const auto *ctoc = dynamic_cast<const TagLib::ID3v2::TableOfContentsFrame *>(rawCtoc);
        if (ctoc) {
            const auto childList = ctoc->childElements();
            for (const auto &childId : childList) {
                orderedIds.append(QByteArray(childId.data(), childId.size()));
            }
        }
    }

    for (const auto *rawFrame : chapFrames) {
        const auto *chap = dynamic_cast<const TagLib::ID3v2::ChapterFrame *>(rawFrame);
        if (!chap) {
            continue;
        }
        TagChapterItem ch;
        ch.startTimeMs = chap->startTime();
        ch.endTimeMs = chap->endTime();

        const auto tit2List = chap->embeddedFrameList("TIT2");
        if (!tit2List.isEmpty() && tit2List.front()) {
            ch.title = toQString(tit2List.front()->toString()).trimmed();
        }
        if (ch.title.isEmpty()) {
            const auto tit1List = chap->embeddedFrameList("TIT1");
            if (!tit1List.isEmpty() && tit1List.front()) {
                ch.title = toQString(tit1List.front()->toString()).trimmed();
            }
        }
        if (ch.title.isEmpty()) {
            ch.title = toQString(chap->toString()).trimmed();
        }

        const QByteArray elementId(chap->elementID().data(), chap->elementID().size());
        chaptersById.insert(elementId, ch);
        if (!orderedIds.contains(elementId)) {
            orderedIds.append(elementId);
        }
    }

    for (const auto &id : orderedIds) {
        if (chaptersById.contains(id)) {
            chapters.append(chaptersById.value(id));
        }
    }

    std::sort(chapters.begin(), chapters.end(), [](const TagChapterItem &a, const TagChapterItem &b) {
        return a.startTimeMs < b.startTimeMs;
    });

    for (int i = 0; i < chapters.size(); ++i) {
        if (chapters[i].endTimeMs <= chapters[i].startTimeMs && i + 1 < chapters.size()) {
            chapters[i].endTimeMs = chapters[i + 1].startTimeMs;
        }
    }

    return chapters;
}

QVector<TagChapterItem> extractVorbisChaptersFromProps(const TagLib::PropertyMap &properties)
{
    QVector<TagChapterItem> chapters;
    QMap<QString, qint64> timeMap;
    QMap<QString, QString> nameMap;

    for (auto it = properties.begin(); it != properties.end(); ++it) {
        const QString key = toQString(it->first).toUpper();
        if (key.startsWith(QStringLiteral("CHAPTER")) && !it->second.isEmpty()) {
            const QString val = toQString(it->second.front()).trimmed();
            if (key.endsWith(QStringLiteral("NAME"))) {
                const QString numStr = key.mid(7, key.length() - 11);
                if (!numStr.isEmpty() && (numStr.at(0).isDigit() || numStr.startsWith(QLatin1Char('_')))) {
                    nameMap.insert(numStr, val);
                }
            } else if (!key.endsWith(QStringLiteral("URL")) && !key.endsWith(QStringLiteral("HIDDEN")) && !key.endsWith(QStringLiteral("ENABLED"))) {
                const QString numStr = key.mid(7);
                if (!numStr.isEmpty() && (numStr.at(0).isDigit() || numStr.startsWith(QLatin1Char('_')))) {
                    const qint64 ts = parseVorbisChapterTimestamp(val);
                    if (ts >= 0) {
                        timeMap.insert(numStr, ts);
                    }
                }
            }
        }
    }

    if (timeMap.isEmpty()) {
        return {};
    }

    for (auto it = timeMap.begin(); it != timeMap.end(); ++it) {
        const QString num = it.key();
        TagChapterItem ch;
        ch.startTimeMs = it.value();
        ch.title = nameMap.value(num, QStringLiteral("Chapter %1").arg(num));
        chapters.append(ch);
    }

    std::sort(chapters.begin(), chapters.end(), [](const TagChapterItem &a, const TagChapterItem &b) {
        return a.startTimeMs < b.startTimeMs;
    });

    for (int i = 0; i < chapters.size(); ++i) {
        if (chapters[i].endTimeMs <= 0 && i + 1 < chapters.size()) {
            chapters[i].endTimeMs = chapters[i + 1].startTimeMs;
        }
    }
    return chapters;
}

void writeId3v2Chapters(TagLib::ID3v2::Tag *id3v2Tag, const QVector<TagChapterItem> &chapters)
{
    if (!id3v2Tag) {
        return;
    }

    id3v2Tag->removeFrames("CHAP");
    id3v2Tag->removeFrames("CTOC");

    if (chapters.isEmpty()) {
        return;
    }

    auto *ctoc = new TagLib::ID3v2::TableOfContentsFrame(TagLib::ByteVector("toc"));
    ctoc->setIsTopLevel(true);
    ctoc->setIsOrdered(true);
    for (int i = 0; i < chapters.size(); ++i) {
        const auto &ch = chapters.at(i);
        const QByteArray elId = QStringLiteral("chp%1").arg(i).toLatin1();
        const TagLib::ByteVector elementId(elId.constData(), elId.size());
        const unsigned int start = static_cast<unsigned int>(qMax<qint64>(0, ch.startTimeMs));
        const unsigned int end = static_cast<unsigned int>(ch.endTimeMs > ch.startTimeMs ? ch.endTimeMs : 0);

        auto *chap = new TagLib::ID3v2::ChapterFrame(elementId, start, end, 0xFFFFFFFF, 0xFFFFFFFF);
        if (!ch.title.trimmed().isEmpty()) {
            auto *tit2 = new TagLib::ID3v2::TextIdentificationFrame("TIT2", TagLib::String::UTF8);
            tit2->setText(toTagLibString(ch.title));
            chap->addEmbeddedFrame(tit2);
        }
        id3v2Tag->addFrame(chap);
        ctoc->addChildElement(elementId);
    }
    id3v2Tag->addFrame(ctoc);
}

void writeVorbisChapters(TagLib::File *file, const QVector<TagChapterItem> &chapters)
{
    if (!file) {
        return;
    }

    TagLib::PropertyMap properties = file->properties();
    TagLib::StringList keysToRemove;
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        const QString key = toQString(it->first).toUpper();
        if (key.startsWith(QStringLiteral("CHAPTER"))) {
            keysToRemove.append(it->first);
        }
    }
    for (const auto &k : keysToRemove) {
        properties.erase(k);
    }

    for (int i = 0; i < chapters.size(); ++i) {
        const auto &ch = chapters.at(i);
        const QString idxStr = QStringLiteral("%1").arg(i + 1, 3, 10, QLatin1Char('0'));
        const QString timeKey = QStringLiteral("CHAPTER%1").arg(idxStr);
        const QString nameKey = QStringLiteral("CHAPTER%1NAME").arg(idxStr);

        const qint64 ms = qMax<qint64>(0, ch.startTimeMs);
        const qint64 totalSec = ms / 1000;
        const qint64 h = totalSec / 3600;
        const qint64 m = (totalSec % 3600) / 60;
        const qint64 s = totalSec % 60;
        const qint64 msRem = ms % 1000;
        const QString timeVal = QStringLiteral("%1:%2:%3.%4")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'))
            .arg(msRem, 3, 10, QLatin1Char('0'));

        properties.insert(toTagLibString(timeKey), TagLib::StringList(toTagLibString(timeVal)));
        properties.insert(toTagLibString(nameKey), TagLib::StringList(toTagLibString(ch.title)));
    }

    file->setProperties(properties);
}

} // namespace

TagEditor::TagEditor(QObject *parent)
    : QObject(parent)
{
}

bool TagEditor::supportsCoverEditing() const
{
    const QString extension = upperExtension(m_filePath);
    return extension == QStringLiteral("MP3") || extension == QStringLiteral("FLAC");
}

QString TagEditor::coverEditingUnsupportedMessage() const
{
    return unsupportedCoverEditingMessage(upperExtension(m_filePath));
}

bool TagEditor::hasCoverImage() const
{
    return (!m_removeCover && !m_coverPreviewSource.isEmpty()) || !m_coverImagePath.isEmpty();
}

QString TagEditor::suggestedCoverFileName() const
{
    QString name;
    if (!m_artist.trimmed().isEmpty() && !m_album.trimmed().isEmpty()) {
        name = QStringLiteral("%1 - %2 - Cover.jpg").arg(m_artist.trimmed(), m_album.trimmed());
    } else if (!m_title.trimmed().isEmpty()) {
        name = QStringLiteral("%1 - Cover.jpg").arg(m_title.trimmed());
    } else if (!m_filePath.isEmpty()) {
        name = QStringLiteral("%1_cover.jpg").arg(QFileInfo(m_filePath).completeBaseName());
    } else {
        name = QStringLiteral("cover.jpg");
    }

    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    return name;
}

bool TagEditor::exportCoverImage(const QString &targetPath)
{
    if (m_removeCover) {
        emit coverExportFailed(QStringLiteral("No cover image to export (cover marked for removal)."));
        return false;
    }

    const QString cleanTarget = normalizeLocalPath(targetPath);
    if (cleanTarget.isEmpty()) {
        emit coverExportFailed(QStringLiteral("Invalid target destination path."));
        return false;
    }

    // 1. If newly selected image path exists, copy from it
    if (!m_coverImagePath.isEmpty() && QFile::exists(m_coverImagePath)) {
        if (QFile::exists(cleanTarget)) {
            QFile::remove(cleanTarget);
        }
        if (QFile::copy(m_coverImagePath, cleanTarget)) {
            emit coverExportSucceeded(cleanTarget);
            return true;
        }
    }

    // 2. Extract embedded cover bytes
    TagLib::ByteVector imageData;
    const QString extension = upperExtension(m_filePath);

    if (extension == QStringLiteral("MP3")) {
        const auto file = WaveFlux::TagLibPath::openMpegFile(m_filePath, false);
        if (file && file->ID3v2Tag(false)) {
            const auto frames = file->ID3v2Tag(false)->frameListMap()["APIC"];
            for (auto *rawFrame : frames) {
                if (auto *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(rawFrame)) {
                    imageData = frame->picture();
                    if (frame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
                        break;
                    }
                }
            }
        }
    } else if (extension == QStringLiteral("FLAC")) {
        const auto file = WaveFlux::TagLibPath::openFlacFile(m_filePath, false);
        if (file) {
            const auto pictures = file->pictureList();
            for (const auto *picture : pictures) {
                if (picture) {
                    imageData = picture->data();
                    if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
                        break;
                    }
                }
            }
        }
    }

    if (imageData.isEmpty()) {
        emit coverExportFailed(QStringLiteral("No embedded cover artwork found to export."));
        return false;
    }

    QFile outFile(cleanTarget);
    if (outFile.exists()) {
        outFile.remove();
    }
    if (!outFile.open(QIODevice::WriteOnly)) {
        emit coverExportFailed(QStringLiteral("Failed to open output file for writing: %1").arg(outFile.errorString()));
        return false;
    }

    const qint64 written = outFile.write(imageData.data(), static_cast<qint64>(imageData.size()));
    outFile.close();

    if (written != static_cast<qint64>(imageData.size())) {
        emit coverExportFailed(QStringLiteral("Failed to write complete cover image data."));
        return false;
    }

    emit coverExportSucceeded(cleanTarget);
    return true;
}

bool TagEditor::isFileTrackerModule(const QString &path) const
{
    return WaveFlux::isTrackerModuleSource(path);
}

bool TagEditor::isTrackerModule() const
{
    return WaveFlux::isTrackerModuleSource(m_filePath);
}

QString TagEditor::trackerWarningMessage() const
{
    return QStringLiteral("Warning: Tracker modules (MOD, XM, S3M, IT, etc.) use internal song data structures. Standard tag editing is not supported and changes may not persist.");
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

void TagEditor::setBpm(int bpm)
{
    const int normalized = qBound(0, bpm, 999);
    if (m_bpm != normalized) {
        m_bpm = normalized;
        emit bpmChanged();
        markChanged();
    }
}

void TagEditor::setComment(const QString &comment)
{
    if (m_comment != comment) {
        m_comment = comment;
        emit commentChanged();
        markChanged();
    }
}

void TagEditor::setComposer(const QString &composer)
{
    if (m_composer != composer) {
        m_composer = composer;
        emit composerChanged();
        markChanged();
    }
}

void TagEditor::setOriginalArtist(const QString &originalArtist)
{
    if (m_originalArtist != originalArtist) {
        m_originalArtist = originalArtist;
        emit originalArtistChanged();
        markChanged();
    }
}

void TagEditor::setCopyright(const QString &copyright)
{
    if (m_copyright != copyright) {
        m_copyright = copyright;
        emit copyrightChanged();
        markChanged();
    }
}

void TagEditor::setUrl(const QString &url)
{
    if (m_url != url) {
        m_url = url;
        emit urlChanged();
        markChanged();
    }
}

void TagEditor::setEncoder(const QString &encoder)
{
    if (m_encoder != encoder) {
        m_encoder = encoder;
        emit encoderChanged();
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

        const QString previewSource = selectedCoverPreviewSource(m_coverImagePath);
        if (m_coverPreviewSource != previewSource) {
            m_coverPreviewSource = previewSource;
            emit coverPreviewSourceChanged();
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

    const QString previewSource = m_removeCover
        ? QString()
        : selectedCoverPreviewSource(m_coverImagePath).isEmpty()
            ? m_originalCoverPreviewSource
            : selectedCoverPreviewSource(m_coverImagePath);
    if (m_coverPreviewSource != previewSource) {
        m_coverPreviewSource = previewSource;
        emit coverPreviewSourceChanged();
    }

    markChanged();
}

QVariantList TagEditor::chapters() const
{
    QVariantList list;
    list.reserve(m_chapters.size());
    for (int i = 0; i < m_chapters.size(); ++i) {
        const auto &ch = m_chapters.at(i);
        QVariantMap map;
        map.insert(QStringLiteral("index"), i);
        map.insert(QStringLiteral("title"), ch.title);
        map.insert(QStringLiteral("startTimeMs"), ch.startTimeMs);
        map.insert(QStringLiteral("endTimeMs"), ch.endTimeMs);
        map.insert(QStringLiteral("startTimeSec"), static_cast<int>(ch.startTimeMs / 1000));
        map.insert(QStringLiteral("endTimeSec"), static_cast<int>(ch.endTimeMs / 1000));
        map.insert(QStringLiteral("startTimeFormatted"), formatChapterTime(ch.startTimeMs));
        const qint64 effectiveEnd = (ch.endTimeMs > ch.startTimeMs) ? ch.endTimeMs : m_durationMs;
        map.insert(QStringLiteral("endTimeFormatted"), formatChapterTime(effectiveEnd));
        const qint64 dur = (effectiveEnd > ch.startTimeMs) ? (effectiveEnd - ch.startTimeMs) : 0;
        map.insert(QStringLiteral("durationMs"), dur);
        map.insert(QStringLiteral("durationSec"), static_cast<int>(dur / 1000));
        map.insert(QStringLiteral("durationFormatted"), formatChapterTime(dur));
        list.append(map);
    }
    return list;
}

void TagEditor::addChapter(const QString &title, qint64 startTimeMs, qint64 endTimeMs)
{
    TagChapterItem ch;
    ch.title = title.trimmed().isEmpty() ? QStringLiteral("Chapter %1").arg(m_chapters.size() + 1) : title.trimmed();
    ch.startTimeMs = qMax<qint64>(0, startTimeMs);
    ch.endTimeMs = (endTimeMs > ch.startTimeMs) ? endTimeMs : 0;

    m_chapters.append(ch);
    m_chaptersModified = true;
    emit chaptersChanged();
    markChanged();
}

void TagEditor::updateChapter(int index, const QString &title, qint64 startTimeMs, qint64 endTimeMs)
{
    if (index < 0 || index >= m_chapters.size()) {
        return;
    }

    m_chapters[index].title = title.trimmed();
    m_chapters[index].startTimeMs = qMax<qint64>(0, startTimeMs);
    m_chapters[index].endTimeMs = (endTimeMs > m_chapters[index].startTimeMs) ? endTimeMs : 0;

    m_chaptersModified = true;
    emit chaptersChanged();
    markChanged();
}

void TagEditor::addChapterSeconds(const QString &title, int startTimeSec, int endTimeSec)
{
    addChapter(title, static_cast<qint64>(qMax(0, startTimeSec)) * 1000, static_cast<qint64>(qMax(0, endTimeSec)) * 1000);
}

void TagEditor::updateChapterSeconds(int index, const QString &title, int startTimeSec, int endTimeSec)
{
    updateChapter(index, title, static_cast<qint64>(qMax(0, startTimeSec)) * 1000, static_cast<qint64>(qMax(0, endTimeSec)) * 1000);
}

void TagEditor::removeChapter(int index)
{
    if (index < 0 || index >= m_chapters.size()) {
        return;
    }

    m_chapters.removeAt(index);
    m_chaptersModified = true;
    emit chaptersChanged();
    markChanged();
}

void TagEditor::clearChapters()
{
    if (m_chapters.isEmpty()) {
        return;
    }

    m_chapters.clear();
    m_chaptersModified = true;
    emit chaptersChanged();
    markChanged();
}

void TagEditor::setChapters(const QVariantList &chapters)
{
    m_chapters.clear();
    for (const auto &var : chapters) {
        const auto map = var.toMap();
        TagChapterItem ch;
        ch.title = map.value(QStringLiteral("title")).toString().trimmed();
        ch.startTimeMs = qMax<qint64>(0, map.value(QStringLiteral("startTimeMs")).toLongLong());
        ch.endTimeMs = map.value(QStringLiteral("endTimeMs")).toLongLong();
        if (ch.endTimeMs <= ch.startTimeMs) {
            ch.endTimeMs = 0;
        }
        m_chapters.append(ch);
    }

    m_chaptersModified = true;
    emit chaptersChanged();
    markChanged();
}

void TagEditor::resetTechnicalInfo()
{
    m_fileFormat.clear();
    m_bitrate = 0;
    m_bitrateFormatted = QStringLiteral("—");
    m_sampleRate = 0;
    m_sampleRateFormatted = QStringLiteral("—");
    m_channels = 0;
    m_channelMode = QStringLiteral("—");
    m_fileSizeBytes = 0;
    m_fileSizeFormatted = QStringLiteral("—");
    m_durationMs = 0;
    m_durationFormatted = QStringLiteral("00:00");
}

void TagEditor::extractTechnicalInfo()
{
    resetTechnicalInfo();
    if (m_filePath.isEmpty()) {
        emit techInfoChanged();
        return;
    }

    QFileInfo fi(m_filePath);
    if (!fi.exists()) {
        emit techInfoChanged();
        return;
    }

    m_fileSizeBytes = fi.size();
    if (m_fileSizeBytes < 1024) {
        m_fileSizeFormatted = QStringLiteral("%1 B").arg(m_fileSizeBytes);
    } else if (m_fileSizeBytes < 1024 * 1024) {
        m_fileSizeFormatted = QStringLiteral("%1 KB (%2 bytes)")
            .arg(m_fileSizeBytes / 1024.0, 0, 'f', 1)
            .arg(QLocale().toString(m_fileSizeBytes));
    } else {
        m_fileSizeFormatted = QStringLiteral("%1 MB (%2 bytes)")
            .arg(m_fileSizeBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(QLocale().toString(m_fileSizeBytes));
    }

    if (WaveFlux::isTrackerModuleSource(m_filePath)) {
        m_fileFormat = QStringLiteral("Tracker Module (%1)").arg(upperExtension(m_filePath));
        m_channelMode = QStringLiteral("Stereo (Tracker Multi-channel)");
        emit techInfoChanged();
        return;
    }

    const QString extension = upperExtension(m_filePath);

    if (extension == QStringLiteral("MP3")) {
        const auto mpegFile = WaveFlux::TagLibPath::openMpegFile(m_filePath, true);
        if (mpegFile && mpegFile->audioProperties()) {
            const auto *props = mpegFile->audioProperties();
            QString version;
            switch (props->version()) {
            case TagLib::MPEG::Header::Version1: version = QStringLiteral("MPEG 1"); break;
            case TagLib::MPEG::Header::Version2: version = QStringLiteral("MPEG 2"); break;
            case TagLib::MPEG::Header::Version2_5: version = QStringLiteral("MPEG 2.5"); break;
            default: version = QStringLiteral("MPEG"); break;
            }

            QString layer;
            switch (props->layer()) {
            case 1: layer = QStringLiteral("Layer I"); break;
            case 2: layer = QStringLiteral("Layer II"); break;
            case 3: layer = QStringLiteral("Layer III"); break;
            default: layer = QStringLiteral("Audio"); break;
            }
            m_fileFormat = QStringLiteral("%1, %2").arg(version, layer);

            m_bitrate = props->bitrate();
            m_bitrateFormatted = (m_bitrate > 0) ? QStringLiteral("%1 kbps").arg(m_bitrate) : QStringLiteral("VBR");
            m_sampleRate = props->sampleRate();
            m_sampleRateFormatted = (m_sampleRate > 0)
                ? QStringLiteral("%1 Hz (%2 kHz)").arg(m_sampleRate).arg(m_sampleRate / 1000.0, 0, 'f', 1)
                : QStringLiteral("—");
            m_channels = props->channels();
            if (m_channels == 1) {
                m_channelMode = QStringLiteral("Mono (1 channel)");
            } else if (m_channels == 2) {
                if (props->channelMode() == TagLib::MPEG::Header::JointStereo) {
                    m_channelMode = QStringLiteral("Joint Stereo (2 channels)");
                } else {
                    m_channelMode = QStringLiteral("Stereo (2 channels)");
                }
            } else {
                m_channelMode = QStringLiteral("%1 Channels").arg(m_channels);
            }

            m_durationMs = props->lengthInMilliseconds();
            if (m_durationMs <= 0 && props->lengthInSeconds() > 0) {
                m_durationMs = static_cast<qint64>(props->lengthInSeconds()) * 1000;
            }
            if (m_durationMs <= 0 && props->length() > 0) {
                m_durationMs = static_cast<qint64>(props->length()) * 1000;
            }
            m_durationFormatted = formatChapterTime(m_durationMs);
            emit techInfoChanged();
            return;
        }
    }

    if (extension == QStringLiteral("FLAC")) {
        const auto flacFile = WaveFlux::TagLibPath::openFlacFile(m_filePath, true);
        if (flacFile && flacFile->audioProperties()) {
            const auto *props = flacFile->audioProperties();
            const int bits = props->bitsPerSample();
            m_fileFormat = (bits > 0)
                ? QStringLiteral("FLAC (%1-bit Lossless)").arg(bits)
                : QStringLiteral("FLAC (Lossless)");

            m_bitrate = props->bitrate();
            m_bitrateFormatted = (m_bitrate > 0) ? QStringLiteral("%1 kbps").arg(m_bitrate) : QStringLiteral("Lossless");
            m_sampleRate = props->sampleRate();
            m_sampleRateFormatted = (m_sampleRate > 0)
                ? QStringLiteral("%1 Hz (%2 kHz)").arg(m_sampleRate).arg(m_sampleRate / 1000.0, 0, 'f', 1)
                : QStringLiteral("—");
            m_channels = props->channels();
            m_channelMode = (m_channels == 1) ? QStringLiteral("Mono (1 channel)") : QStringLiteral("Stereo (%1 channels)").arg(m_channels);
            m_durationMs = props->lengthInMilliseconds();
            if (m_durationMs <= 0 && props->lengthInSeconds() > 0) {
                m_durationMs = static_cast<qint64>(props->lengthInSeconds()) * 1000;
            }
            if (m_durationMs <= 0 && props->length() > 0) {
                m_durationMs = static_cast<qint64>(props->length()) * 1000;
            }
            m_durationFormatted = formatChapterTime(m_durationMs);
            emit techInfoChanged();
            return;
        }
    }

    const auto fileRef = WaveFlux::TagLibPath::makeFileRef(m_filePath, true, TagLib::AudioProperties::Average);
    if (!fileRef.isNull() && fileRef.audioProperties()) {
        const auto *props = fileRef.audioProperties();
        if (extension == QStringLiteral("WAV")) {
            m_fileFormat = QStringLiteral("WAV (RIFF PCM)");
        } else if (extension == QStringLiteral("OGG")) {
            m_fileFormat = QStringLiteral("Ogg Vorbis");
        } else if (extension == QStringLiteral("OPUS")) {
            m_fileFormat = QStringLiteral("Opus Audio");
        } else if (extension == QStringLiteral("M4A") || extension == QStringLiteral("AAC")) {
            m_fileFormat = QStringLiteral("MPEG-4 AAC");
        } else if (extension == QStringLiteral("APE")) {
            m_fileFormat = QStringLiteral("Monkey's Audio (APE)");
        } else if (extension == QStringLiteral("WV")) {
            m_fileFormat = QStringLiteral("WavPack");
        } else {
            m_fileFormat = QStringLiteral("%1 Audio").arg(extension);
        }

        m_bitrate = props->bitrate();
        m_bitrateFormatted = (m_bitrate > 0) ? QStringLiteral("%1 kbps").arg(m_bitrate) : QStringLiteral("—");
        m_sampleRate = props->sampleRate();
        m_sampleRateFormatted = (m_sampleRate > 0)
            ? QStringLiteral("%1 Hz (%2 kHz)").arg(m_sampleRate).arg(m_sampleRate / 1000.0, 0, 'f', 1)
            : QStringLiteral("—");
        m_channels = props->channels();
        m_channelMode = (m_channels == 1) ? QStringLiteral("Mono (1 channel)") : QStringLiteral("Stereo (%1 channels)").arg(m_channels);
        m_durationMs = props->lengthInMilliseconds();
        if (m_durationMs <= 0 && props->lengthInSeconds() > 0) {
            m_durationMs = static_cast<qint64>(props->lengthInSeconds()) * 1000;
        }
        if (m_durationMs <= 0 && props->length() > 0) {
            m_durationMs = static_cast<qint64>(props->length()) * 1000;
        }
        m_durationFormatted = formatChapterTime(m_durationMs);
    } else {
        m_fileFormat = extension.isEmpty() ? QStringLiteral("Audio") : extension;
    }

    emit techInfoChanged();
}

void TagEditor::loadTags()
{
    if (m_filePath.isEmpty()) {
        return;
    }

    extractTechnicalInfo();

    const auto file = WaveFlux::TagLibPath::makeFileRef(m_filePath, false);

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
    m_comment = toQString(tag->comment());
    m_bpm = bpmFromFile(file.file());

    m_composer.clear();
    m_originalArtist.clear();
    m_copyright.clear();
    m_url.clear();
    m_encoder.clear();

    if (file.file()) {
        const TagLib::PropertyMap properties = file.file()->properties();

        if (m_comment.isEmpty()) {
            const auto commentVals = properties[TagLib::String("COMMENT")];
            if (!commentVals.isEmpty()) {
                m_comment = toQString(commentVals.front());
            }
        }

        const auto composerVals = properties[TagLib::String("COMPOSER")];
        if (!composerVals.isEmpty()) {
            m_composer = toQString(composerVals.front());
        }

        auto origArtistVals = properties[TagLib::String("ORIGINALARTIST")];
        if (origArtistVals.isEmpty()) {
            origArtistVals = properties[TagLib::String("ORIGINAL ARTIST")];
        }
        if (!origArtistVals.isEmpty()) {
            m_originalArtist = toQString(origArtistVals.front());
        }

        const auto copyrightVals = properties[TagLib::String("COPYRIGHT")];
        if (!copyrightVals.isEmpty()) {
            m_copyright = toQString(copyrightVals.front());
        }

        auto urlVals = properties[TagLib::String("URL")];
        if (urlVals.isEmpty()) urlVals = properties[TagLib::String("WEBSITE")];
        if (urlVals.isEmpty()) urlVals = properties[TagLib::String("CONTACT")];
        if (!urlVals.isEmpty()) {
            m_url = toQString(urlVals.front());
        }

        auto encoderVals = properties[TagLib::String("ENCODER")];
        if (encoderVals.isEmpty()) encoderVals = properties[TagLib::String("ENCODEDBY")];
        if (encoderVals.isEmpty()) encoderVals = properties[TagLib::String("ENCODED_BY")];
        if (encoderVals.isEmpty()) encoderVals = properties[TagLib::String("TOOL")];
        if (!encoderVals.isEmpty()) {
            m_encoder = toQString(encoderVals.front());
        }

        m_chapters = extractVorbisChaptersFromProps(properties);
    }

    const QString extension = upperExtension(m_filePath);
    if (extension == QStringLiteral("MP3")) {
        const auto mpegFile = WaveFlux::TagLibPath::openMpegFile(m_filePath, false);
        if (mpegFile && mpegFile->ID3v2Tag(false)) {
            auto *id3v2 = mpegFile->ID3v2Tag(false);
            const auto &map = id3v2->frameListMap();
            if (map.contains("TCOM") && !map["TCOM"].isEmpty()) {
                m_composer = toQString(map["TCOM"].front()->toString());
            }
            if (map.contains("TOPE") && !map["TOPE"].isEmpty()) {
                m_originalArtist = toQString(map["TOPE"].front()->toString());
            }
            if (map.contains("TCOP") && !map["TCOP"].isEmpty()) {
                m_copyright = toQString(map["TCOP"].front()->toString());
            }
            if (map.contains("WXXX") && !map["WXXX"].isEmpty()) {
                m_url = toQString(map["WXXX"].front()->toString());
            } else if (map.contains("WOAR") && !map["WOAR"].isEmpty()) {
                m_url = toQString(map["WOAR"].front()->toString());
            }
            if (map.contains("TSSE") && !map["TSSE"].isEmpty()) {
                m_encoder = toQString(map["TSSE"].front()->toString());
            } else if (map.contains("TENC") && !map["TENC"].isEmpty()) {
                m_encoder = toQString(map["TENC"].front()->toString());
            }

            const auto id3Chapters = extractId3v2ChaptersFromTag(id3v2);
            if (!id3Chapters.isEmpty()) {
                m_chapters = id3Chapters;
            }
        }
    }

    m_coverImagePath.clear();
    m_coverPreviewSource = embeddedCoverPreviewSource(m_filePath);
    m_removeCover = false;

    // Store originals for revert
    m_originalTitle = m_title;
    m_originalArtistTrack = m_artist;
    m_originalAlbum = m_album;
    m_originalGenre = m_genre;
    m_originalYear = m_year;
    m_originalTrackNumber = m_trackNumber;
    m_originalBpm = m_bpm;
    m_originalComment = m_comment;
    m_originalComposer = m_composer;
    m_originalOriginalArtist = m_originalArtist;
    m_originalCopyright = m_copyright;
    m_originalUrl = m_url;
    m_originalEncoder = m_encoder;
    m_originalCoverImagePath = m_coverImagePath;
    m_originalCoverPreviewSource = m_coverPreviewSource;
    m_originalRemoveCover = m_removeCover;
    m_originalChapters = m_chapters;
    m_chaptersModified = false;

    m_hasChanges = false;

    emit titleChanged();
    emit artistChanged();
    emit albumChanged();
    emit genreChanged();
    emit yearChanged();
    emit trackNumberChanged();
    emit bpmChanged();
    emit commentChanged();
    emit composerChanged();
    emit originalArtistChanged();
    emit copyrightChanged();
    emit urlChanged();
    emit encoderChanged();
    emit coverImagePathChanged();
    emit coverPreviewSourceChanged();
    emit removeCoverChanged();
    emit chaptersChanged();
    emit hasChangesChanged();
}

bool TagEditor::saveTags()
{
    if (m_filePath.isEmpty()) {
        emit saveFailed("No file loaded");
        return false;
    }

    std::sort(m_chapters.begin(), m_chapters.end(), [](const TagChapterItem &a, const TagChapterItem &b) {
        return a.startTimeMs < b.startTimeMs;
    });

    for (int i = 0; i < m_chapters.size(); ++i) {
        if (m_chapters[i].endTimeMs <= m_chapters[i].startTimeMs && i + 1 < m_chapters.size()) {
            m_chapters[i].endTimeMs = m_chapters[i + 1].startTimeMs;
        }
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

    const QString extension = upperExtension(m_filePath);

    if (extension == QStringLiteral("MP3")) {
        const auto file = WaveFlux::TagLibPath::openMpegFile(m_filePath, false);
        if (!file) {
            emit saveFailed("Failed to open MP3 file for writing");
            return false;
        }

        TagLib::ID3v2::Tag *id3v2Tag = file->ID3v2Tag(true);
        TagLib::Tag *tag = file->tag();
        if (!tag) {
            emit saveFailed("Failed to access MP3 tag");
            return false;
        }

        applyBpmProperty(file.get(), m_bpm);
        applyCommonTags(tag, m_title, m_artist, m_album, m_genre, m_year, m_trackNumber, m_comment);
        applyExtendedId3v2Tags(id3v2Tag, m_composer, m_originalArtist, m_copyright, m_url, m_encoder, m_bpm);
        writeId3v2Chapters(id3v2Tag, m_chapters);

        if (coverChangeRequested) {
            if (!applyMp3Cover(file.get(), imageData, imageMimeType, m_removeCover, &coverError)) {
                emit saveFailed(coverError.isEmpty() ? QStringLiteral("Failed to update MP3 cover") : coverError);
                return false;
            }
        }

        if (!file->save()) {
            emit saveFailed("Failed to save MP3 tags");
            return false;
        }
    } else if (extension == QStringLiteral("FLAC")) {
        const auto file = WaveFlux::TagLibPath::openFlacFile(m_filePath, false);
        if (!file) {
            emit saveFailed("Failed to open FLAC file for writing");
            return false;
        }

        if (!file->tag()) {
            (void)file->xiphComment(true);
        }

        TagLib::Tag *tag = file->tag();
        if (!tag) {
            emit saveFailed("Failed to access FLAC tag");
            return false;
        }

        applyCommonTags(tag, m_title, m_artist, m_album, m_genre, m_year, m_trackNumber, m_comment);
        applyExtendedPropertyMap(file.get(), m_composer, m_originalArtist, m_copyright, m_url, m_encoder, m_bpm);
        writeVorbisChapters(file.get(), m_chapters);

        if (coverChangeRequested) {
            if (!applyFlacCover(file.get(), imageData, imageMimeType, m_removeCover, &coverError)) {
                emit saveFailed(coverError.isEmpty() ? QStringLiteral("Failed to update FLAC cover") : coverError);
                return false;
            }
        }

        if (!file->save()) {
            emit saveFailed("Failed to save FLAC tags");
            return false;
        }
    } else if (coverChangeRequested) {
        emit saveFailed(unsupportedCoverEditingMessage(extension));
        return false;
    } else {
        auto file = WaveFlux::TagLibPath::makeFileRef(m_filePath, false);
        if (file.isNull() || !file.tag()) {
            emit saveFailed("Failed to open file for writing");
            return false;
        }

        applyCommonTags(file.tag(), m_title, m_artist, m_album, m_genre, m_year, m_trackNumber, m_comment);
        if (file.file()) {
            applyExtendedPropertyMap(file.file(), m_composer, m_originalArtist, m_copyright, m_url, m_encoder, m_bpm);
            writeVorbisChapters(file.file(), m_chapters);
        }

        if (!file.save()) {
            emit saveFailed("Failed to save tags");
            return false;
        }
    }

    // Update originals
    m_originalTitle = m_title;
    m_originalArtistTrack = m_artist;
    m_originalAlbum = m_album;
    m_originalGenre = m_genre;
    m_originalYear = m_year;
    m_originalTrackNumber = m_trackNumber;
    m_originalBpm = m_bpm;
    m_originalComment = m_comment;
    m_originalComposer = m_composer;
    m_originalOriginalArtist = m_originalArtist;
    m_originalCopyright = m_copyright;
    m_originalUrl = m_url;
    m_originalEncoder = m_encoder;
    m_originalCoverImagePath = m_coverImagePath;
    m_originalCoverPreviewSource = m_removeCover ? QString() : m_coverPreviewSource;
    m_originalRemoveCover = m_removeCover;
    m_originalChapters = m_chapters;
    m_chaptersModified = false;

    m_hasChanges = false;
    emit hasChangesChanged();
    emit saveSucceeded();

    return true;
}

bool TagEditor::saveTagsForFiles(const QStringList &filePaths,
                                 bool applyTitle, const QString &title,
                                 bool applyArtist, const QString &artist,
                                 bool applyAlbum, const QString &album,
                                 bool applyGenre, const QString &genre,
                                 bool applyYear, int year,
                                 bool applyTrackNumber, int trackNumber,
                                 bool applyBpm, int bpm,
                                 bool applyComment, const QString &comment,
                                 bool applyComposer, const QString &composer,
                                 bool applyOriginalArtist, const QString &originalArtist,
                                 bool applyCopyright, const QString &copyright,
                                 bool applyUrl, const QString &url,
                                 bool applyEncoder, const QString &encoder)
{
    if (filePaths.isEmpty()) {
        emit saveFailed("No files selected");
        return false;
    }

    if (!applyTitle && !applyArtist && !applyAlbum && !applyGenre && !applyYear && !applyTrackNumber && !applyBpm
        && !applyComment && !applyComposer && !applyOriginalArtist && !applyCopyright && !applyUrl && !applyEncoder) {
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

        const QString ext = upperExtension(path);
        if (ext == QStringLiteral("MP3")) {
            const auto file = WaveFlux::TagLibPath::openMpegFile(path, false);
            if (!file || !file->tag()) {
                ++failedCount;
                if (firstError.isEmpty()) {
                    firstError = QStringLiteral("Failed to open %1").arg(path);
                }
                continue;
            }

            TagLib::Tag *tag = file->tag();
            TagLib::ID3v2::Tag *id3v2Tag = file->ID3v2Tag(true);

            if (applyTitle) tag->setTitle(toTagLibString(title));
            if (applyArtist) tag->setArtist(toTagLibString(artist));
            if (applyAlbum) tag->setAlbum(toTagLibString(album));
            if (applyGenre) tag->setGenre(toTagLibString(genre));
            if (applyYear) tag->setYear(static_cast<unsigned int>(qMax(0, year)));
            if (applyTrackNumber) tag->setTrack(static_cast<unsigned int>(qMax(0, trackNumber)));
            if (applyComment) tag->setComment(toTagLibString(comment));

            if (applyBpm) applyBpmProperty(file.get(), qBound(0, bpm, 999));
            if (id3v2Tag) {
                if (applyComposer) {
                    id3v2Tag->removeFrames("TCOM");
                    if (!composer.trimmed().isEmpty()) {
                        auto *f = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::UTF8);
                        f->setText(toTagLibString(composer));
                        id3v2Tag->addFrame(f);
                    }
                }
                if (applyOriginalArtist) {
                    id3v2Tag->removeFrames("TOPE");
                    if (!originalArtist.trimmed().isEmpty()) {
                        auto *f = new TagLib::ID3v2::TextIdentificationFrame("TOPE", TagLib::String::UTF8);
                        f->setText(toTagLibString(originalArtist));
                        id3v2Tag->addFrame(f);
                    }
                }
                if (applyCopyright) {
                    id3v2Tag->removeFrames("TCOP");
                    if (!copyright.trimmed().isEmpty()) {
                        auto *f = new TagLib::ID3v2::TextIdentificationFrame("TCOP", TagLib::String::UTF8);
                        f->setText(toTagLibString(copyright));
                        id3v2Tag->addFrame(f);
                    }
                }
                if (applyUrl) {
                    id3v2Tag->removeFrames("WXXX");
                    id3v2Tag->removeFrames("WOAR");
                    if (!url.trimmed().isEmpty()) {
                        auto *f = new TagLib::ID3v2::UserUrlLinkFrame(TagLib::String::UTF8);
                        f->setUrl(toTagLibString(url));
                        id3v2Tag->addFrame(f);
                    }
                }
                if (applyEncoder) {
                    id3v2Tag->removeFrames("TSSE");
                    if (!encoder.trimmed().isEmpty()) {
                        auto *f = new TagLib::ID3v2::TextIdentificationFrame("TSSE", TagLib::String::UTF8);
                        f->setText(toTagLibString(encoder));
                        id3v2Tag->addFrame(f);
                    }
                }
            }

            if (!file->save()) {
                ++failedCount;
                if (firstError.isEmpty()) {
                    firstError = QStringLiteral("Failed to save %1").arg(path);
                }
                continue;
            }

            ++updatedCount;
        } else {
            auto file = WaveFlux::TagLibPath::makeFileRef(path, false);
            if (file.isNull() || !file.tag()) {
                ++failedCount;
                if (firstError.isEmpty()) {
                    firstError = QStringLiteral("Failed to open %1").arg(path);
                }
                continue;
            }

            TagLib::Tag *tag = file.tag();
            if (applyTitle) tag->setTitle(toTagLibString(title));
            if (applyArtist) tag->setArtist(toTagLibString(artist));
            if (applyAlbum) tag->setAlbum(toTagLibString(album));
            if (applyGenre) tag->setGenre(toTagLibString(genre));
            if (applyYear) tag->setYear(static_cast<unsigned int>(qMax(0, year)));
            if (applyTrackNumber) tag->setTrack(static_cast<unsigned int>(qMax(0, trackNumber)));
            if (applyComment) tag->setComment(toTagLibString(comment));

            if (file.file()) {
                TagLib::PropertyMap properties = file.file()->properties();
                if (applyBpm) {
                    if (bpm > 0) properties["BPM"] = TagLib::StringList(toTagLibString(QString::number(bpm)));
                    else { properties.erase("BPM"); properties.erase("TBPM"); }
                }
                if (applyComposer) {
                    if (!composer.trimmed().isEmpty()) properties["COMPOSER"] = TagLib::StringList(toTagLibString(composer));
                    else properties.erase("COMPOSER");
                }
                if (applyOriginalArtist) {
                    if (!originalArtist.trimmed().isEmpty()) properties["ORIGINALARTIST"] = TagLib::StringList(toTagLibString(originalArtist));
                    else { properties.erase("ORIGINALARTIST"); properties.erase("ORIGINAL ARTIST"); }
                }
                if (applyCopyright) {
                    if (!copyright.trimmed().isEmpty()) properties["COPYRIGHT"] = TagLib::StringList(toTagLibString(copyright));
                    else properties.erase("COPYRIGHT");
                }
                if (applyUrl) {
                    if (!url.trimmed().isEmpty()) properties["URL"] = TagLib::StringList(toTagLibString(url));
                    else { properties.erase("URL"); properties.erase("WEBSITE"); }
                }
                if (applyEncoder) {
                    if (!encoder.trimmed().isEmpty()) properties["ENCODER"] = TagLib::StringList(toTagLibString(encoder));
                    else { properties.erase("ENCODER"); properties.erase("ENCODEDBY"); }
                }
                file.file()->setProperties(properties);
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
    m_artist = m_originalArtistTrack;
    m_album = m_originalAlbum;
    m_genre = m_originalGenre;
    m_year = m_originalYear;
    m_trackNumber = m_originalTrackNumber;
    m_bpm = m_originalBpm;
    m_comment = m_originalComment;
    m_composer = m_originalComposer;
    m_originalArtist = m_originalOriginalArtist;
    m_copyright = m_originalCopyright;
    m_url = m_originalUrl;
    m_encoder = m_originalEncoder;
    m_coverImagePath = m_originalCoverImagePath;
    m_coverPreviewSource = m_originalCoverPreviewSource;
    m_removeCover = m_originalRemoveCover;
    m_chapters = m_originalChapters;
    m_chaptersModified = false;

    m_hasChanges = false;

    emit titleChanged();
    emit artistChanged();
    emit albumChanged();
    emit genreChanged();
    emit yearChanged();
    emit trackNumberChanged();
    emit bpmChanged();
    emit commentChanged();
    emit composerChanged();
    emit originalArtistChanged();
    emit copyrightChanged();
    emit urlChanged();
    emit encoderChanged();
    emit coverImagePathChanged();
    emit coverPreviewSourceChanged();
    emit removeCoverChanged();
    emit chaptersChanged();
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
