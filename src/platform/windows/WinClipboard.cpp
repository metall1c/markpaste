#include "WinClipboard.h"

#include <QBuffer>
#include <QImage>
#include <QMimeData>
#include <QtEndian>

#include <windows.h>

namespace mp::win {

namespace {

QByteArray globalBytes(HANDLE h)
{
    const SIZE_T size = h ? GlobalSize(h) : 0;
    const void *p = size ? GlobalLock(h) : nullptr;
    if (!p)
        return {};
    QByteArray data(static_cast<const char *>(p), qsizetype(size));
    GlobalUnlock(h);
    return data;
}

// CF_DIB — это BMP-файл без 14-байтного заголовка файла: дописываем его и читаем как BMP
QImage imageFromDib(const QByteArray &dib)
{
    if (dib.size() < qsizetype(sizeof(BITMAPINFOHEADER)))
        return {};
    BITMAPINFOHEADER bih;
    memcpy(&bih, dib.constData(), sizeof(bih));
    quint32 palette = bih.biClrUsed ? bih.biClrUsed : (bih.biBitCount <= 8 ? (1u << bih.biBitCount) : 0u);
    quint32 offset = 14 + bih.biSize + palette * 4;
    if (bih.biCompression == BI_BITFIELDS && bih.biSize == sizeof(BITMAPINFOHEADER))
        offset += 12;   // маски цветов идут сразу за заголовком
    QByteArray bmp(14, '\0');
    bmp[0] = 'B';
    bmp[1] = 'M';
    qToLittleEndian<quint32>(quint32(14 + dib.size()), bmp.data() + 2);
    qToLittleEndian<quint32>(offset, bmp.data() + 10);
    bmp += dib;
    return QImage::fromData(bmp, "BMP");
}

ClipboardPayload readNativeImage()
{
    ClipboardPayload p;
    if (!OpenClipboard(nullptr))
        return p;
    const UINT pngFormat = RegisterClipboardFormatW(L"PNG");
    if (const QByteArray png = globalBytes(GetClipboardData(pngFormat)); !png.isEmpty()) {
        p.addImage(png, "image/png");
    } else if (const QImage img = imageFromDib(globalBytes(GetClipboardData(CF_DIB))); !img.isNull()) {
        QByteArray encoded;
        QBuffer buf(&encoded);
        if (buf.open(QIODevice::WriteOnly) && img.save(&buf, "PNG"))
            p.addImage(encoded, "image/png");
    }
    CloseClipboard();
    return p;
}

} // namespace

QString WinClipboard::windowsFormat(const QString &name)
{
    // Так Qt называет произвольный формат буфера Windows (QWindowsMimeRegistry)
    return QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(name);
}

void WinClipboard::addPlatformFormats(QMimeData *md, const ClipboardPayload &payload, ClipboardFlags flags) const
{
    if (const ClipboardEntry *png = payload.find(QStringLiteral("image/png")))
        md->setData(windowsFormat(QStringLiteral("PNG")), png->data);
    if (flags & ClipboardFlag::ExcludeFromHistory) {
        const QByteArray zero(4, '\0');   // DWORD 0
        md->setData(windowsFormat(QStringLiteral("ExcludeClipboardContentFromMonitorProcessing")), zero);
        md->setData(windowsFormat(QStringLiteral("CanIncludeInClipboardHistory")), zero);
        md->setData(windowsFormat(QStringLiteral("CanUploadToCloudClipboard")), zero);
    }
}

QFuture<ClipboardPayload> WinClipboard::snapshot()
{
    ClipboardPayload p = QtClipboard::snapshot().result();   // у QtClipboard результат готов сразу
    bool hasImage = false;
    for (const ClipboardEntry &e : std::as_const(p.entries))
        hasImage = hasImage || e.mimeType.startsWith(QStringLiteral("image/"));
    if (!hasImage)
        p.entries << readNativeImage().entries;
    return readyFuture(p);
}

} // namespace mp::win
