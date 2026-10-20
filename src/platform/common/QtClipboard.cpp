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
    auto *md = new QMimeData;
    for (const ClipboardEntry &e : payload.entries)
        md->setData(e.mimeType, e.data);
    // Картинка ещё и как QImage: так Qt отдаёт её и в других форматах (image/bmp…), а
    // получатель в нашем же процессе видит hasImage(). Иначе — только сырые байты PNG.
    for (const ClipboardEntry &e : payload.entries)
        if (e.mimeType.startsWith(QStringLiteral("image/"))) {
            const QImage image = QImage::fromData(e.data);
            if (!image.isNull())
                md->setImageData(image);
            break;
        }
    addPlatformFormats(md, payload, flags);
    QGuiApplication::clipboard()->setMimeData(md);
    return readyFuture(Status::success());
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
    // Картинка без PNG — например, только CF_DIB на Windows: Qt отдаёт её как QImage
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
