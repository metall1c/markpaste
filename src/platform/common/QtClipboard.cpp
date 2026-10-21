#include "QtClipboard.h"

#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>

namespace mp {

QtClipboard::QtClipboard(bool backgroundWrites, QObject *parent)
    : IClipboard(parent), m_background(backgroundWrites)
{
}

QFuture<Status> QtClipboard::write(const ClipboardPayload &payload, ClipboardFlags flags)
{
    QGuiApplication::clipboard()->setMimeData(makeMimeData(payload, flags));
    return readyFuture(Status::success());
}

bool QtClipboard::keepEntry(const QString &) const
{
    return true;
}

QMimeData *QtClipboard::makeMimeData(const ClipboardPayload &payload, ClipboardFlags flags) const
{
    auto *md = new QMimeData;
    for (const ClipboardEntry &e : payload.entries)
        if (keepEntry(e.mimeType))
            md->setData(e.mimeType, e.data);
    // The image also as a QImage: this way Qt offers it in other formats too (image/bmp…), and
    // a receiver in our own process sees hasImage(). Otherwise only raw PNG bytes.
    for (const ClipboardEntry &e : payload.entries)
        if (e.mimeType.startsWith(QStringLiteral("image/"))) {
            const QImage image = QImage::fromData(e.data);
            if (!image.isNull())
                md->setImageData(image);
            break;
        }
    addPlatformFormats(md, payload, flags);
    return md;
}

void QtClipboard::addPlatformFormats(QMimeData *md, const ClipboardPayload &, ClipboardFlags flags) const
{
    if (flags & ClipboardFlag::ExcludeFromHistory)
        md->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
}

QFuture<ClipboardPayload> QtClipboard::snapshot()
{
    ClipboardPayload p;
    const QMimeData *md = QGuiApplication::clipboard()->mimeData();
    if (!md)
        return readyFuture(p);
    if (md->hasText())
        p.addText(md->text());
    for (const char *mime : {"image/png", "image/jpeg"}) {
        if (md->hasFormat(QString::fromLatin1(mime))) {
            p.addImage(md->data(QString::fromLatin1(mime)), mime);
            return readyFuture(p);
        }
    }
    // An image without PNG, e.g. only CF_DIB on Windows: Qt provides it as a QImage
    if (md->hasImage()) {
        const QImage image = qvariant_cast<QImage>(md->imageData());
        QByteArray png;
        QBuffer buf(&png);
        if (!image.isNull() && buf.open(QIODevice::WriteOnly) && image.save(&buf, "PNG"))
            p.addImage(png, "image/png");
    }
    return readyFuture(p);
}

} // namespace mp
