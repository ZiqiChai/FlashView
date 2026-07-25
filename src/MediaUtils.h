#ifndef MEDIAUTILS_H
#define MEDIAUTILS_H

#include <QCollator>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>

#include <algorithm>

namespace MediaUtils {

inline const QStringList &imageExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
        QStringLiteral("tiff"), QStringLiteral("tif"), QStringLiteral("svg"),
        QStringLiteral("ico")
    };
    return extensions;
}

inline const QStringList &videoExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("flv"),
        QStringLiteral("webm"), QStringLiteral("m4v")
    };
    return extensions;
}

inline bool hasExtension(const QString &fileName, const QStringList &extensions)
{
    return extensions.contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive);
}

inline bool isImageFile(const QString &fileName)
{
    return hasExtension(fileName, imageExtensions());
}

inline bool isVideoFile(const QString &fileName)
{
    return hasExtension(fileName, videoExtensions());
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
