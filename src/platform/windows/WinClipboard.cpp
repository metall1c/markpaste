#include "WinClipboard.h"

#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QMimeData>
#include <QTimer>
#include <QtEndian>

#include <functional>
#include <memory>

#include <windows.h>

Q_LOGGING_CATEGORY(lcWinClipboard, "markpaste.win.clipboard")

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

// CF_DIB is a BMP file without the 14-byte file header: add it and read as BMP
QImage imageFromDib(const QByteArray &dib)
{
    if (dib.size() < qsizetype(sizeof(BITMAPINFOHEADER)))
        return {};
    BITMAPINFOHEADER bih;
    memcpy(&bih, dib.constData(), sizeof(bih));
    quint32 palette = bih.biClrUsed ? bih.biClrUsed : (bih.biBitCount <= 8 ? (1u << bih.biBitCount) : 0u);
    quint32 offset = 14 + bih.biSize + palette * 4;
    if (bih.biCompression == BI_BITFIELDS && bih.biSize == sizeof(BITMAPINFOHEADER))
        offset += 12;   // color masks follow right after the header
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

WinClipboard::WinClipboard(QObject *parent)
    : QtClipboard(true, parent)
{
    m_probeWindow = CreateWindowExW(0, L"STATIC", L"Markpaste clipboard probe", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    GetModuleHandleW(nullptr), nullptr);
}

WinClipboard::~WinClipboard()
{
    if (m_probeWindow)
        DestroyWindow(static_cast<HWND>(m_probeWindow));
}

bool WinClipboard::clipboardFree() const
{
    // The clipboard is not opened by another program right now. Open it from our own window: in Wine re-opening
    // with the same window (NULL included) from another thread succeeds
    if (!OpenClipboard(static_cast<HWND>(m_probeWindow)))
        return false;
    CloseClipboard();
    return true;
}

QString WinClipboard::windowsFormat(const QString &name)
{
    // This is how Qt names an arbitrary Windows clipboard format (QWindowsMimeRegistry)
    return QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(name);
}

bool WinClipboard::keepEntry(const QString &mimeType) const
{
    // Text and file synonyms for X11 and GNOME (UTF8_STRING, x-special/gnome-copied-files…) would become
    // extra registered formats on Windows: Qt already puts text and files into CF_UNICODETEXT
    // and CF_HDROP from text/plain and text/uri-list
    return mimeType == u"text/plain" || mimeType == u"text/uri-list" || mimeType.startsWith(u"image/");
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

QFuture<Status> WinClipboard::write(const ClipboardPayload &payload, ClipboardFlags flags)
{
    const quint64 generation = ++m_writeGeneration;
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setMimeData(makeMimeData(payload, flags));
    if (clipboard->ownsClipboard())
        return readyFuture(Status::success());

    // Not written: another program holds the clipboard. Qt has already deleted the QMimeData, so build it again
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    auto retry = std::make_shared<std::function<void(int)>>();
    *retry = [this, payload, flags, promise, retry, generation](int left) {
        if (generation != m_writeGeneration) {
            // While we waited, something else was written to the clipboard (the next paste): do not put the old one back over it
            promise->addResult(Status::failure(QStringLiteral("Clipboard write superseded")));
            promise->finish();
            *retry = nullptr;
            return;
        }
        QClipboard *cb = QGuiApplication::clipboard();
        cb->setMimeData(makeMimeData(payload, flags));
        const bool ok = cb->ownsClipboard();
        if (ok || left <= 1) {
            qCInfo(lcWinClipboard) << "clipboard was busy;" << (ok ? "written on retry" : "giving up");
            promise->addResult(ok ? Status::success()
                                  : Status::failure(QStringLiteral("The clipboard is busy: another program keeps it open")));
            promise->finish();
            *retry = nullptr;   // break the reference cycle
            return;
        }
        QTimer::singleShot(m_busyIntervalMs, this, [retry, left] {
            if (*retry)
                (*retry)(left - 1);
        });
    };
    QTimer::singleShot(m_busyIntervalMs, this, [retry, n = m_busyAttempts] {
        if (*retry)
            (*retry)(n);
    });
    return promise->future();
}

ClipboardPayload WinClipboard::readNow()
{
    ClipboardPayload p = QtClipboard::snapshot().result();   // QtClipboard's result is ready immediately
    bool hasImage = false;
    for (const ClipboardEntry &e : std::as_const(p.entries))
        hasImage = hasImage || e.mimeType.startsWith(QStringLiteral("image/"));
    if (!hasImage)
        p.entries << readNativeImage().entries;
    return p;
}

QFuture<ClipboardPayload> WinClipboard::snapshot()
{
    // The clipboard is opened by another program: the read would come out empty, and after the paste the user's
    // clipboard would not come back. Wait until it is released
    if (clipboardFree())
        return readyFuture(readNow());
    auto promise = std::make_shared<QPromise<ClipboardPayload>>();
    promise->start();
    auto retry = std::make_shared<std::function<void(int)>>();
    *retry = [this, promise, retry](int left) {
        if (!clipboardFree() && left > 1) {
            QTimer::singleShot(m_busyIntervalMs, this, [retry, left] {
                if (*retry)
                    (*retry)(left - 1);
            });
            return;
        }
        promise->addResult(readNow());
        promise->finish();
        *retry = nullptr;
    };
    QTimer::singleShot(m_busyIntervalMs, this, [retry, n = m_busyAttempts] {
        if (*retry)
            (*retry)(n);
    });
    return promise->future();
}

} // namespace mp::win
