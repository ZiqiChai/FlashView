#ifndef MEDIAUTILS_H
#define MEDIAUTILS_H

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
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

// Browsing order of a folder. A file manager's own order is not exposed to
// other applications, so the user pins the one they want instead.
enum class SortKey {
    Name = 0,
    ModifiedTime = 1,
    CreationTime = 2,
    Size = 3,
    Type = 4
};

inline SortKey sortKeyFromInt(int value)
{
    return value >= 0 && value <= static_cast<int>(SortKey::Type)
        ? static_cast<SortKey>(value) : SortKey::Name;
}

// Creation time is not recorded by every filesystem; fall back to the closest
// available timestamp so the order stays meaningful instead of arbitrary.
inline QDateTime creationTime(const QFileInfo &info)
{
    const QDateTime born = info.birthTime();
    if (born.isValid())
        return born;
    const QDateTime changed = info.metadataChangeTime();
    return changed.isValid() ? changed : info.lastModified();
}

inline void sortFiles(const QDir &directory, QStringList &fileNames,
                      SortKey key, bool descending)
{
    // Natural name order is the default and also the tie-breaker for the other
    // keys, so files sharing a timestamp or size keep a predictable order.
    naturalSort(fileNames);

    if (key == SortKey::Name) {
        if (descending)
            std::reverse(fileNames.begin(), fileNames.end());
        return;
    }

    // Stat every file once: querying inside the comparator would hit the
    // filesystem O(n log n) times on large folders.
    QHash<QString, qint64> numbers;
    QHash<QString, QString> suffixes;
    numbers.reserve(fileNames.size());
    suffixes.reserve(fileNames.size());
    for (const QString &name : std::as_const(fileNames)) {
        const QFileInfo info(directory.absoluteFilePath(name));
        switch (key) {
        case SortKey::ModifiedTime:
            numbers.insert(name, info.lastModified().toMSecsSinceEpoch());
            break;
        case SortKey::CreationTime:
            numbers.insert(name, creationTime(info).toMSecsSinceEpoch());
            break;
        case SortKey::Size:
            numbers.insert(name, info.size());
            break;
        case SortKey::Type:
            suffixes.insert(name, info.suffix().toLower());
            break;
        case SortKey::Name:
            break;
        }
    }

    std::stable_sort(fileNames.begin(), fileNames.end(),
                     [&](const QString &left, const QString &right) {
        if (key == SortKey::Type) {
            const int comparison =
                QString::compare(suffixes.value(left), suffixes.value(right));
            if (comparison == 0)
                return false;
            return descending ? comparison > 0 : comparison < 0;
        }
        const qint64 leftValue = numbers.value(left);
        const qint64 rightValue = numbers.value(right);
        if (leftValue == rightValue)
            return false;
        return descending ? leftValue > rightValue : leftValue < rightValue;
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
