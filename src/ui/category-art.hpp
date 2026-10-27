#pragma once

#include <QBuffer>
#include <QImageReader>

namespace tca {
inline QImage decodeCategoryArt(const QByteArray &data)
{
    if (data.size() > 1024 * 1024) return {};
    QByteArray format;
    if (data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) format = "png";
    else if (data.startsWith(QByteArray::fromHex("ffd8ff"))) format = "jpeg";
    else return {};
    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly)) return {};
    QImageReader reader(&buffer, format);
    reader.setAutoDetectImageFormat(false);
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 2048 || size.height() > 2048) return {};
    reader.setScaledSize(QSize(90, 120));
    return reader.read();
}
}
