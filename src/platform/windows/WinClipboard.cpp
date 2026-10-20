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
    // Буфер не открыт другой программой прямо сейчас. Открываем от своего окна: в Wine повторное
    // открытие с тем же окном (в том числе NULL) из другого потока проходит
    if (!OpenClipboard(static_cast<HWND>(m_probeWindow)))
        return false;
    CloseClipboard();
    return true;
}

QString WinClipboard::windowsFormat(const QString &name)
{
    // Так Qt называет произвольный формат буфера Windows (QWindowsMimeRegistry)
    return QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(name);
}

bool WinClipboard::keepEntry(const QString &mimeType) const
{
    // Синонимы текста и файлов для X11 и GNOME (UTF8_STRING, x-special/gnome-copied-files…) в Windows
    // стали бы лишними зарегистрированными форматами: текст и файлы Qt и так кладёт в CF_UNICODETEXT
    // и CF_HDROP из text/plain и text/uri-list
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

    // Не записалось — буфер держит другая программа. QMimeData Qt уже удалил: собираем заново
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    auto retry = std::make_shared<std::function<void(int)>>();
    *retry = [this, payload, flags, promise, retry, generation](int left) {
        if (generation != m_writeGeneration) {
            // Пока ждали, в буфер уже записали другое (следующая вставка): старое не возвращаем поверх
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
            *retry = nullptr;   // разрываем цикл ссылок
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
    ClipboardPayload p = QtClipboard::snapshot().result();   // у QtClipboard результат готов сразу
    bool hasImage = false;
    for (const ClipboardEntry &e : std::as_const(p.entries))
        hasImage = hasImage || e.mimeType.startsWith(QStringLiteral("image/"));
    if (!hasImage)
        p.entries << readNativeImage().entries;
    return p;
}

QFuture<ClipboardPayload> WinClipboard::snapshot()
{
    // Буфер открыт другой программой — прочиталось бы пусто, и после вставки буфер пользователя
    // не вернулся бы. Ждём, пока его отпустят
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
