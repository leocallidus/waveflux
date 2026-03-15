#pragma once

#include <QDir>
#include <QFile>
#include <QString>

#include <memory>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/mpegfile.h>
#include <taglib/tiostream.h>

namespace WaveFlux::TagLibPath {

class NativePath
{
public:
    explicit NativePath(const QString &path)
        : m_path(QDir::toNativeSeparators(QDir::cleanPath(path)))
#ifdef _WIN32
        , m_storage(m_path.toStdWString())
#else
        , m_storage(QFile::encodeName(m_path))
#endif
    {
    }

    TagLib::FileName fileName() const
    {
#ifdef _WIN32
        return TagLib::FileName(m_storage.c_str());
#else
        return m_storage.constData();
#endif
    }

private:
    QString m_path;
#ifdef _WIN32
    std::wstring m_storage;
#else
    QByteArray m_storage;
#endif
};

class QtFileStream final : public TagLib::IOStream
{
public:
    explicit QtFileStream(const QString &path, bool openReadOnly = false)
        : m_path(QDir::cleanPath(path))
        , m_namePath(m_path)
    {
        open(openReadOnly);
    }

    TagLib::FileName name() const override
    {
        return m_namePath.fileName();
    }

    TagLib::ByteVector readBlock(size_t length) override
    {
        if (!m_file.isOpen() || length == 0) {
            return {};
        }

        const QByteArray bytes = m_file.read(static_cast<qint64>(length));
        if (bytes.isEmpty()) {
            return {};
        }
        return TagLib::ByteVector(bytes.constData(), static_cast<unsigned int>(bytes.size()));
    }

    void writeBlock(const TagLib::ByteVector &data) override
    {
        if (!ensureWritable() || data.size() == 0) {
            return;
        }
        (void)m_file.write(data.data(), data.size());
    }

    void insert(const TagLib::ByteVector &data,
                TagLib::offset_t start = 0,
                size_t replace = 0) override
    {
        if (!ensureWritable()) {
            return;
        }

        const TagLib::offset_t fileLength = length();
        const TagLib::offset_t boundedStart = qBound<TagLib::offset_t>(0, start, fileLength);
        const TagLib::offset_t maxReplace = qMax<TagLib::offset_t>(0, fileLength - boundedStart);
        const TagLib::offset_t boundedReplace = qMin<TagLib::offset_t>(
            static_cast<TagLib::offset_t>(replace),
            maxReplace);

        seek(boundedStart + boundedReplace, Beginning);
        const QByteArray tail = m_file.readAll();

        seek(boundedStart, Beginning);
        if (data.size() > 0) {
            (void)m_file.write(data.data(), data.size());
        }
        if (!tail.isEmpty()) {
            (void)m_file.write(tail);
        }

        const qint64 newLength = static_cast<qint64>(boundedStart) + data.size() + tail.size();
        (void)m_file.resize(newLength);
        (void)m_file.seek(static_cast<qint64>(boundedStart) + data.size());
    }

    void removeBlock(TagLib::offset_t start = 0, size_t lengthToRemove = 0) override
    {
        if (!ensureWritable()) {
            return;
        }

        const TagLib::offset_t fileLength = length();
        const TagLib::offset_t boundedStart = qBound<TagLib::offset_t>(0, start, fileLength);
        const TagLib::offset_t maxRemove = qMax<TagLib::offset_t>(0, fileLength - boundedStart);
        const TagLib::offset_t boundedRemove = qMin<TagLib::offset_t>(
            static_cast<TagLib::offset_t>(lengthToRemove),
            maxRemove);

        seek(boundedStart + boundedRemove, Beginning);
        const QByteArray tail = m_file.readAll();

        seek(boundedStart, Beginning);
        if (!tail.isEmpty()) {
            (void)m_file.write(tail);
        }

        const qint64 newLength = static_cast<qint64>(boundedStart) + tail.size();
        (void)m_file.resize(newLength);
        (void)m_file.seek(static_cast<qint64>(boundedStart));
    }

    bool readOnly() const override
    {
        return m_readOnly;
    }

    bool isOpen() const override
    {
        return m_file.isOpen();
    }

    void seek(TagLib::offset_t offset, Position p = Beginning) override
    {
        if (!m_file.isOpen()) {
            return;
        }

        qint64 base = 0;
        switch (p) {
        case Beginning:
            base = 0;
            break;
        case Current:
            base = m_file.pos();
            break;
        case End:
            base = m_file.size();
            break;
        }

        (void)m_file.seek(qMax<qint64>(0, base + static_cast<qint64>(offset)));
    }

    void clear() override
    {
    }

    TagLib::offset_t tell() const override
    {
        return m_file.isOpen() ? static_cast<TagLib::offset_t>(m_file.pos()) : 0;
    }

    TagLib::offset_t length() override
    {
        return m_file.isOpen() ? static_cast<TagLib::offset_t>(m_file.size()) : 0;
    }

    void truncate(TagLib::offset_t newLength) override
    {
        if (!ensureWritable()) {
            return;
        }

        const qint64 boundedLength = qMax<qint64>(0, static_cast<qint64>(newLength));
        (void)m_file.resize(boundedLength);
        if (m_file.pos() > boundedLength) {
            (void)m_file.seek(boundedLength);
        }
    }

private:
    bool open(bool openReadOnly)
    {
        m_file.setFileName(m_path);

        if (!openReadOnly && m_file.open(QIODevice::ReadWrite)) {
            m_readOnly = false;
            return true;
        }

        if (m_file.open(QIODevice::ReadOnly)) {
            m_readOnly = true;
            return true;
        }

        m_readOnly = true;
        return false;
    }

    bool ensureWritable()
    {
        if (!m_file.isOpen()) {
            return false;
        }
        if (!m_readOnly) {
            return true;
        }

        const qint64 currentPosition = m_file.pos();
        m_file.close();
        if (m_file.open(QIODevice::ReadWrite)) {
            m_readOnly = false;
            (void)m_file.seek(currentPosition);
            return true;
        }

        (void)m_file.open(QIODevice::ReadOnly);
        m_readOnly = true;
        (void)m_file.seek(currentPosition);
        return false;
    }

    QString m_path;
    QFile m_file;
    NativePath m_namePath;
    bool m_readOnly = true;
};

class FileRefHandle
{
public:
    explicit FileRefHandle(const QString &path,
                           bool readAudioProperties = true,
                           TagLib::AudioProperties::ReadStyle readStyle = TagLib::AudioProperties::Average)
        : m_stream(std::make_unique<QtFileStream>(path, false))
        , m_fileRef((m_stream && m_stream->isOpen())
                        ? TagLib::FileRef(m_stream.get(), readAudioProperties, readStyle)
                        : TagLib::FileRef())
    {
    }

    const TagLib::FileRef &ref() const { return m_fileRef; }
    TagLib::FileRef &ref() { return m_fileRef; }
    bool isNull() const { return m_fileRef.isNull(); }
    TagLib::Tag *tag() const { return m_fileRef.tag(); }
    TagLib::AudioProperties *audioProperties() const { return m_fileRef.audioProperties(); }
    TagLib::File *file() const { return m_fileRef.file(); }
    bool save() { return m_fileRef.save(); }

private:
    std::unique_ptr<QtFileStream> m_stream;
    TagLib::FileRef m_fileRef;
};

class MpegFileHandle
{
public:
    explicit MpegFileHandle(const QString &path, bool readProperties = true)
        : m_stream(std::make_unique<QtFileStream>(path, false))
    {
        if (m_stream && m_stream->isOpen()) {
            m_file = std::make_unique<TagLib::MPEG::File>(m_stream.get(), readProperties);
        }
    }

    TagLib::MPEG::File *get() const { return m_file.get(); }
    TagLib::MPEG::File &operator*() const { return *m_file; }
    TagLib::MPEG::File *operator->() const { return m_file.get(); }
    explicit operator bool() const { return m_file && m_file->isValid(); }

private:
    std::unique_ptr<QtFileStream> m_stream;
    std::unique_ptr<TagLib::MPEG::File> m_file;
};

class FlacFileHandle
{
public:
    explicit FlacFileHandle(const QString &path, bool readProperties = true)
        : m_stream(std::make_unique<QtFileStream>(path, false))
    {
        if (m_stream && m_stream->isOpen()) {
            m_file = std::make_unique<TagLib::FLAC::File>(m_stream.get(), readProperties);
        }
    }

    TagLib::FLAC::File *get() const { return m_file.get(); }
    TagLib::FLAC::File &operator*() const { return *m_file; }
    TagLib::FLAC::File *operator->() const { return m_file.get(); }
    explicit operator bool() const { return m_file && m_file->isValid(); }

private:
    std::unique_ptr<QtFileStream> m_stream;
    std::unique_ptr<TagLib::FLAC::File> m_file;
};

inline FileRefHandle makeFileRef(
    const QString &path,
    bool readAudioProperties = true,
    TagLib::AudioProperties::ReadStyle readStyle = TagLib::AudioProperties::Average)
{
    return FileRefHandle(path, readAudioProperties, readStyle);
}

inline MpegFileHandle openMpegFile(const QString &path, bool readProperties = true)
{
    return MpegFileHandle(path, readProperties);
}

inline FlacFileHandle openFlacFile(const QString &path, bool readProperties = true)
{
    return FlacFileHandle(path, readProperties);
}

} // namespace WaveFlux::TagLibPath
