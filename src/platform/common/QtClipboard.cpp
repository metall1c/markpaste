#include "QtClipboard.h"

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
    if (flags & ClipboardFlag::ExcludeFromHistory)
        md->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
    QGuiApplication::clipboard()->setMimeData(md);
    return readyFuture(Status::success());
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
            break;
        }
    }
    return readyFuture(p);
}

} // namespace mp
