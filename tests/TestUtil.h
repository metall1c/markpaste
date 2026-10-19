#pragma once

#include "core/CaptureSet.h"

#include <QBuffer>
#include <QImage>
#include <QRandomGenerator>

namespace mp::test {

// Шумная картинка: у соседних пикселей почти всегда разные цвета,
// поэтому по ней видно, остались ли в размытой области исходные пиксели.
inline QImage noiseImage(int w, int h, quint32 seed = 42)
{
    QImage img(w, h, QImage::Format_ARGB32);
    QRandomGenerator rng(seed);
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x)
            line[x] = qRgb(rng.bounded(256), rng.bounded(256), rng.bounded(256));
    }
    return img;
}

inline QByteArray toPng(const QImage &img)
{
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return data;
}

inline Shot makeShot(int w = 200, int h = 100, qreal dpr = 1.0)
{
    Shot s;
    const QImage img = noiseImage(w, h);
    s.sourcePng = toPng(img);
    s.sourceSize = img.size();
    s.devicePixelRatio = dpr;
    return s;
}

} // namespace mp::test
