#ifndef MEDIAUTILS_H
#define MEDIAUTILS_H

#include <QCollator>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QMovie>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

namespace MediaUtils {

inline QStringList imageExtensions()
{
    static const QStringList result = [] {
        QSet<QString> extensions;
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        for (const QByteArray &format : formats)
            extensions.insert(QString::fromLatin1(format).toLower());
        for (const QByteArray &format : QMovie::supportedFormats())
            extensions.insert(QString::fromLatin1(format).toLower());

        // Common aliases are not always all advertised by an image plugin.
        if (extensions.contains(QStringLiteral("jpg"))
            || extensions.contains(QStringLiteral("jpeg"))) {
            extensions.insert(QStringLiteral("jpg"));
            extensions.insert(QStringLiteral("jpeg"));
        }
        if (extensions.contains(QStringLiteral("tif"))
            || extensions.contains(QStringLiteral("tiff"))) {
            extensions.insert(QStringLiteral("tif"));
            extensions.insert(QStringLiteral("tiff"));
        }
        if (extensions.contains(QStringLiteral("heif"))
            || extensions.contains(QStringLiteral("heic"))) {
            extensions.insert(QStringLiteral("heif"));
            extensions.insert(QStringLiteral("heic"));
        }

        QStringList sorted(extensions.begin(), extensions.end());
        sorted.sort(Qt::CaseInsensitive);
        return sorted;
    }();
    return result;
}

inline QStringList videoExtensions()
{
    static const QStringList extensions = [] {
        QSet<QString> result;
        const QList<QMimeType> mimeTypes = QMimeDatabase().allMimeTypes();
        for (const QMimeType &mime : mimeTypes) {
            if (!mime.name().startsWith(QStringLiteral("video/")))
                continue;
            for (const QString &suffix : mime.suffixes())
                result.insert(suffix.toLower());
        }
        QStringList sorted(result.begin(), result.end());
        sorted.sort(Qt::CaseInsensitive);
        return sorted;
    }();
    return extensions;
}

inline bool hasExtension(const QString &fileName, const QStringList &extensions)
{
    return extensions.contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive);
}

inline bool isImageFile(const QString &fileName)
{
    if (hasExtension(fileName, imageExtensions()))
        return true;
    const QFileInfo info(fileName);
    return info.isFile() && info.isReadable() && QImageReader(fileName).canRead();
}

inline bool isVideoFile(const QString &fileName)
{
    if (hasExtension(fileName, videoExtensions()))
        return true;
    const QFileInfo info(fileName);
    if (!info.isFile() || !info.isReadable())
        return false;
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(
        fileName, QMimeDatabase::MatchContent);
    return mime.isValid() && mime.name().startsWith(QStringLiteral("video/"));
}

inline bool isSupportedFile(const QString &fileName)
{
    return isImageFile(fileName) || isVideoFile(fileName);
}

inline QStringList supportedExtensions()
{
    QStringList extensions = imageExtensions();
    extensions.append(videoExtensions());
    return extensions;
}

inline QStringList missingModernImageFormats()
{
    const QStringList available = imageExtensions();
    const QList<QPair<QString, QStringList>> desired = {
        {QStringLiteral("APNG"), {QStringLiteral("apng")}},
        {QStringLiteral("AVIF"), {QStringLiteral("avif")}},
        {QStringLiteral("HEIF/HEIC"), {QStringLiteral("heif"), QStringLiteral("heic")}},
        {QStringLiteral("JPEG XL"), {QStringLiteral("jxl")}},
        {QStringLiteral("RAW"), {QStringLiteral("raw"), QStringLiteral("dng"),
                                  QStringLiteral("cr2"), QStringLiteral("nef"),
                                  QStringLiteral("arw")}}
    };
    QStringList missing;
    for (const auto &entry : desired) {
        bool found = false;
        for (const QString &extension : entry.second)
            found = found || available.contains(extension, Qt::CaseInsensitive);
        if (!found)
            missing.append(entry.first);
    }
    return missing;
}

inline QStringList missingCommonVideoCodecs()
{
    static const QStringList missing = [] {
        const QString inspector = QStandardPaths::findExecutable(
            QStringLiteral("gst-inspect-1.0"));
        if (inspector.isEmpty())
            return QStringList{QStringLiteral("GStreamer")};

        const QList<QPair<QString, QStringList>> codecs = {
            {QStringLiteral("H.264"), {QStringLiteral("avdec_h264"),
                                        QStringLiteral("openh264dec")}},
            {QStringLiteral("H.265/HEVC"), {QStringLiteral("avdec_h265")}},
            {QStringLiteral("VP9"), {QStringLiteral("vp9dec"),
                                      QStringLiteral("avdec_vp9")}},
            {QStringLiteral("AV1"), {QStringLiteral("av1dec"),
                                      QStringLiteral("dav1ddec"),
                                      QStringLiteral("avdec_av1")}}
        };
        QStringList result;
        for (const auto &codec : codecs) {
            bool available = false;
            for (const QString &element : codec.second) {
                QProcess process;
                process.start(inspector, {element});
                if (process.waitForFinished(1200)
                    && process.exitStatus() == QProcess::NormalExit
                    && process.exitCode() == 0) {
                    available = true;
                    break;
                }
                if (process.state() != QProcess::NotRunning) {
                    process.kill();
                    process.waitForFinished(200);
                }
            }
            if (!available)
                result.append(codec.first);
        }
        return result;
    }();
    return missing;
}

inline void naturalSort(QStringList &fileNames)
{
    QCollator collator{QLocale()};
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(false);
    std::stable_sort(fileNames.begin(), fileNames.end(),
                     [&collator](const QString &left, const QString &right) {
        qsizetype leftPos = 0;
        qsizetype rightPos = 0;
        while (leftPos < left.size() && rightPos < right.size()) {
            if (left.at(leftPos).isDigit() && right.at(rightPos).isDigit()) {
                const qsizetype leftRunStart = leftPos;
                const qsizetype rightRunStart = rightPos;
                while (leftPos < left.size() && left.at(leftPos).isDigit())
                    ++leftPos;
                while (rightPos < right.size() && right.at(rightPos).isDigit())
                    ++rightPos;

                qsizetype leftSignificant = leftRunStart;
                qsizetype rightSignificant = rightRunStart;
                while (leftSignificant + 1 < leftPos
                       && left.at(leftSignificant) == QLatin1Char('0'))
                    ++leftSignificant;
                while (rightSignificant + 1 < rightPos
                       && right.at(rightSignificant) == QLatin1Char('0'))
                    ++rightSignificant;

                const qsizetype leftDigits = leftPos - leftSignificant;
                const qsizetype rightDigits = rightPos - rightSignificant;
                if (leftDigits != rightDigits)
                    return leftDigits < rightDigits;

                for (qsizetype i = 0; i < leftDigits; ++i) {
                    const QChar leftDigit = left.at(leftSignificant + i);
                    const QChar rightDigit = right.at(rightSignificant + i);
                    if (leftDigit != rightDigit)
                        return leftDigit < rightDigit;
                }

                // Equal numeric values: prefer the shorter spelling ("2"
                // before "02") to keep the result deterministic.
                const qsizetype leftRunLength = leftPos - leftRunStart;
                const qsizetype rightRunLength = rightPos - rightRunStart;
                if (leftRunLength != rightRunLength)
                    return leftRunLength < rightRunLength;
                continue;
            }

            const int comparison = collator.compare(
                left.mid(leftPos, 1), right.mid(rightPos, 1));
            if (comparison != 0)
                return comparison < 0;
            ++leftPos;
            ++rightPos;
        }
        return leftPos == left.size() && rightPos < right.size();
    });
}

// Include file metadata in the key so an image replaced in-place is decoded
// again instead of showing a stale pixmap from the global cache.
inline QString imageCacheKey(const QString &filePath)
{
    const QFileInfo info(filePath);
    return QStringLiteral("flashview:%1:%2:%3")
        .arg(info.absoluteFilePath())
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch());
}

} // namespace MediaUtils

#endif // MEDIAUTILS_H
