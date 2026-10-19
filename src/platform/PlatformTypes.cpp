#include "PlatformTypes.h"

namespace mp {

QStringList ClipboardPayload::mimeTypes() const
{
    QStringList list;
    for (const ClipboardEntry &e : entries)
        list << e.mimeType;
    return list;
}

const ClipboardEntry *ClipboardPayload::find(const QString &mimeType) const
{
    for (const ClipboardEntry &e : entries)
        if (e.mimeType == mimeType)
            return &e;
    return nullptr;
}

void ClipboardPayload::addText(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    for (const char *mime : {"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT"})
        entries.append({QString::fromLatin1(mime), utf8});
}

void ClipboardPayload::addImage(const QByteArray &encoded, const QByteArray &mimeType)
{
    entries.append({QString::fromLatin1(mimeType), encoded});
}

ClipboardPayload ClipboardPayload::text(const QString &text)
{
    ClipboardPayload p;
    p.addText(text);
    return p;
}

ClipboardPayload ClipboardPayload::image(const QByteArray &encoded, const QByteArray &mimeType)
{
    ClipboardPayload p;
    p.addImage(encoded, mimeType);
    return p;
}

} // namespace mp
