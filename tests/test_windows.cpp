// Windows-реализации платформы на настоящем платформенном модуле Qt (windows, не offscreen):
// буфер обмена OLE, хук клавиатуры, захват GDI. Запускаются под Wine на Linux (docs/build.md)
// и на Windows. Чего Wine не умеет (DXGI Desktop Duplication, UIPI), здесь не проверяется.

#include "platform/windows/DxgiCapture.h"
#include "platform/windows/KeyboardHook.h"
#include "platform/windows/WinClipboard.h"
#include "platform/windows/WinHotkeys.h"
#include "platform/windows/WinInputInjector.h"
#include "platform/windows/WinKeys.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <gtest/gtest.h>

#include <future>
#include <thread>

#include <windows.h>

using namespace mp;
using namespace mp::win;

TEST(WinKeys, ParsesPortableTriggers)
{
    WinTrigger t = parseTrigger(QStringLiteral("Primary+Alt+V"));
    EXPECT_EQ(t.vk, unsigned('V'));
    EXPECT_EQ(t.modifiers, unsigned(MOD_CONTROL | MOD_ALT));

    t = parseTrigger(QStringLiteral("Print"));
    EXPECT_EQ(t.vk, unsigned(VK_SNAPSHOT));
    EXPECT_EQ(t.modifiers, 0u);

    t = parseTrigger(QStringLiteral("Ctrl+Shift+F12"));
    EXPECT_EQ(t.vk, unsigned(VK_F12));
    EXPECT_EQ(t.modifiers, unsigned(MOD_CONTROL | MOD_SHIFT));

    t = parseTrigger(QStringLiteral("Super+1"));
    EXPECT_EQ(t.vk, unsigned('1'));
    EXPECT_EQ(t.modifiers, unsigned(MOD_WIN));

    EXPECT_FALSE(parseTrigger(QString()).valid());
    EXPECT_EQ(displayTrigger(QStringLiteral("Primary+Alt+V")), QStringLiteral("Ctrl+Alt+V"));
}

static bool hasRegisteredFormat(const wchar_t *name)
{
    return IsClipboardFormatAvailable(RegisterClipboardFormatW(name));
}

// Буфер после каждой смены читают и другие программы: VBoxTray в виртуалке, rdpclip, журнал Win+V.
// Пока они читают, буфер занят, а данные им отдаёт наш поток (отложенная отрисовка OLE) — поэтому
// ждём с обработкой событий, а не блокирующим result()
template <class T>
static T await(const QFuture<T> &f)
{
    if (!QTest::qWaitFor([&] { return f.isFinished(); }, 15000)) {
        ADD_FAILURE() << "future did not finish";
        return T{};
    }
    return f.result();
}

static bool openClipboard()
{
    return QTest::qWaitFor([] { return OpenClipboard(nullptr) != FALSE; }, 5000);
}

TEST(WinClipboard, WritesImageFilesTextAndHistoryExclusion)
{
    WinClipboard cb;
    EXPECT_TRUE(cb.canWriteInBackground());

    QImage img(40, 30, QImage::Format_RGB32);
    img.fill(Qt::red);
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");

    ClipboardPayload p = ClipboardPayload::image(png);
    p.entries << ClipboardEntry{QStringLiteral("text/uri-list"), QByteArrayLiteral("file:///C:/Temp/A.png\r\n")};
    p.addText(QStringLiteral("A1 кнопка"));
    ASSERT_TRUE(await(cb.write(p, ClipboardFlag::ExcludeFromHistory)).ok);
    QCoreApplication::processEvents();

    // Форматы — так, как их видят другие программы Windows
    ASSERT_TRUE(openClipboard());
    EXPECT_TRUE(IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5));
    EXPECT_TRUE(hasRegisteredFormat(L"PNG"));
    EXPECT_TRUE(IsClipboardFormatAvailable(CF_HDROP));
    EXPECT_TRUE(IsClipboardFormatAvailable(CF_UNICODETEXT));
    EXPECT_TRUE(hasRegisteredFormat(L"ExcludeClipboardContentFromMonitorProcessing"));
    EXPECT_TRUE(hasRegisteredFormat(L"CanIncludeInClipboardHistory"));
    EXPECT_TRUE(hasRegisteredFormat(L"CanUploadToCloudClipboard"));
    if (HANDLE h = GetClipboardData(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"))) {
        const auto *v = static_cast<const DWORD *>(GlobalLock(h));
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(*v, 0u);   // 0 — не в историю Win+V
        GlobalUnlock(h);
    } else {
        ADD_FAILURE() << "no CanIncludeInClipboardHistory data";
    }
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        EXPECT_EQ(QString::fromWCharArray(static_cast<const wchar_t *>(GlobalLock(h))), QStringLiteral("A1 кнопка"));
        GlobalUnlock(h);
    }
    CloseClipboard();

    // Восстановление буфера после вставки — без пометок: это снова буфер пользователя
    ASSERT_TRUE(await(cb.write(ClipboardPayload::text(QStringLiteral("old")), ClipboardFlag::None)).ok);
    QCoreApplication::processEvents();
    ASSERT_TRUE(openClipboard());
    EXPECT_FALSE(hasRegisteredFormat(L"CanIncludeInClipboardHistory"));
    CloseClipboard();
}

// «Другая программа», которая держит буфер открытым (так делают rdpclip и менеджеры буфера):
// поток, открывший буфер, — OleSetClipboard из главного потока в это время не проходит
class ClipboardHolder
{
public:
    ClipboardHolder()
    {
        std::promise<bool> opened;
        std::future<bool> isOpen = opened.get_future();
        std::future<void> released = m_release.get_future();
        m_thread = std::thread([&opened, released = std::move(released)]() mutable {
            bool ok = false;
            for (int i = 0; i < 500 && !ok; ++i) {   // буфер мог на миг занять кто-то ещё
                ok = OpenClipboard(nullptr);
                if (!ok)
                    Sleep(10);
            }
            opened.set_value(ok);
            released.wait();
            if (ok)
                CloseClipboard();
        });
        (void)QTest::qWaitFor([&] { return isOpen.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }, 10000);
        m_open = isOpen.get();
    }
    ~ClipboardHolder() { release(); }
    bool isOpen() const { return m_open; }
    void release()
    {
        if (!m_thread.joinable())
            return;
        m_release.set_value();
        m_thread.join();
    }

private:
    std::promise<void> m_release;
    std::thread m_thread;
    bool m_open = false;
};

static QString clipboardText()
{
    QString text;
    if (!openClipboard())
        return QStringLiteral("<busy>");
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        text = QString::fromWCharArray(static_cast<const wchar_t *>(GlobalLock(h)));
        GlobalUnlock(h);
    }
    CloseClipboard();
    return text;
}

TEST(WinClipboard, BusyClipboardIsReportedNotIgnored)
{
    // Раньше запись «удавалась» всегда: Qt только писал предупреждение, «Набор скопирован» было
    // неправдой, а Ctrl+V вставлял прежнее содержимое буфера (docs/problems.md, 2026-10-20)
    WinClipboard cb;
    ASSERT_TRUE(await(cb.write(ClipboardPayload::text(QStringLiteral("before")), ClipboardFlag::None)).ok);
    QCoreApplication::processEvents();
    cb.setBusyWait(3, 50);

    ClipboardHolder holder;
    ASSERT_TRUE(holder.isOpen());
    QFuture<Status> f = cb.write(ClipboardPayload::text(QStringLiteral("set")), ClipboardFlag::None);
    EXPECT_FALSE(await(f).ok);
    holder.release();
    EXPECT_NE(clipboardText(), QStringLiteral("set"));
}

TEST(WinClipboard, WriteWaitsUntilClipboardIsFree)
{
    WinClipboard cb;
    cb.setBusyWait(40, 50);
    ClipboardHolder holder;
    ASSERT_TRUE(holder.isOpen());
    QFuture<Status> f = cb.write(ClipboardPayload::text(QStringLiteral("set")), ClipboardFlag::ExcludeFromHistory);
    QTimer::singleShot(300, [&holder] { holder.release(); });
    const Status written = await(f);
    EXPECT_TRUE(written.ok) << written.error.toStdString();
    EXPECT_EQ(clipboardText(), QStringLiteral("set"));
}

TEST(WinClipboard, SnapshotWaitsForBusyClipboard)
{
    // Прочитать занятый буфер — получить пусто, и после вставки буфер пользователя не вернулся бы
    WinClipboard cb;
    cb.setBusyWait(40, 50);
    ASSERT_TRUE(await(cb.write(ClipboardPayload::text(QStringLiteral("mine")), ClipboardFlag::None)).ok);
    QCoreApplication::processEvents();

    ClipboardHolder holder;
    ASSERT_TRUE(holder.isOpen());
    QFuture<ClipboardPayload> f = cb.snapshot();
    EXPECT_FALSE(f.isFinished());
    QTimer::singleShot(300, [&holder] { holder.release(); });
    const ClipboardPayload snap = await(f);
    const ClipboardEntry *e = snap.find(QStringLiteral("text/plain"));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(QString::fromUtf8(e->data), QStringLiteral("mine"));
}

TEST(WinClipboard, NoX11FormatNamesOnWindows)
{
    WinClipboard cb;
    ASSERT_TRUE(await(cb.write(ClipboardPayload::text(QStringLiteral("x")), ClipboardFlag::None)).ok);
    QCoreApplication::processEvents();
    ASSERT_TRUE(openClipboard());
    EXPECT_TRUE(IsClipboardFormatAvailable(CF_UNICODETEXT));
    EXPECT_FALSE(hasRegisteredFormat(L"UTF8_STRING"));
    EXPECT_FALSE(hasRegisteredFormat(L"text/plain;charset=utf-8"));
    CloseClipboard();
}

TEST(WinClipboard, SnapshotTurnsDibIntoPng)
{
    // Картинка от другой программы — только CF_DIB, без PNG
    QImage img(20, 10, QImage::Format_RGB32);
    img.fill(Qt::blue);
    const int stride = img.width() * 4;
    const SIZE_T size = sizeof(BITMAPINFOHEADER) + SIZE_T(stride) * img.height();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    auto *bih = static_cast<BITMAPINFOHEADER *>(GlobalLock(mem));
    *bih = BITMAPINFOHEADER{};
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = img.width();
    bih->biHeight = img.height();   // снизу вверх
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = DWORD(stride * img.height());
    auto *bits = reinterpret_cast<uchar *>(bih + 1);
    for (int y = 0; y < img.height(); ++y)
        memcpy(bits + y * stride, img.constScanLine(img.height() - 1 - y), size_t(stride));
    GlobalUnlock(mem);
    ASSERT_TRUE(openClipboard());
    EmptyClipboard();
    SetClipboardData(CF_DIB, mem);
    CloseClipboard();
    QCoreApplication::processEvents();

    WinClipboard cb;
    const ClipboardPayload snap = await(cb.snapshot());
    const ClipboardEntry *e = snap.find(QStringLiteral("image/png"));
    ASSERT_NE(e, nullptr);
    const QImage back = QImage::fromData(e->data, "PNG");
    EXPECT_EQ(back.size(), img.size());
    EXPECT_EQ(QColor(back.pixel(5, 5)), QColor(Qt::blue));
}

TEST(WinCapture, GdiGrabsPrimaryScreen)
{
    QScreen *s = QGuiApplication::primaryScreen();
    ASSERT_NE(s, nullptr);
    const CaptureResult r = DxgiCapture::grab({{s->name(), s->geometry(), s->devicePixelRatio()}});
    ASSERT_TRUE(r.status.ok) << r.status.error.toStdString();
    ASSERT_EQ(r.frames.size(), 1);
    EXPECT_EQ(r.frames[0].screenName, s->name());
    EXPECT_EQ(r.frames[0].image.size(), (QSizeF(s->geometry().size()) * s->devicePixelRatio()).toSize());
    EXPECT_EQ(qAlpha(r.frames[0].image.pixel(0, 0)), 255);
}

TEST(WinHotkeys, RegistersAndReportsConflicts)
{
    WinHotkeys a;
    const BindResult r = a.bind(Action::PasteSet, QStringLiteral("Primary+Alt+Shift+F11")).result();
    ASSERT_TRUE(r.status.ok) << r.status.error.toStdString();
    EXPECT_EQ(r.trigger, QStringLiteral("Ctrl+Alt+Shift+F11"));

    // Чужое окно с тем же сочетанием — «занято»
    HWND other = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    EXPECT_FALSE(RegisterHotKey(other, 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F11));
    DestroyWindow(other);

    EXPECT_TRUE(a.bind(Action::PasteSet, QString()).result().status.ok);   // снять
    EXPECT_FALSE(a.bind(Action::Capture, QStringLiteral("Primary+Nonsense")).result().status.ok);
}

TEST(WinHotkeys, PrintScreenGoesThroughHook)
{
    // Хук ставится в своём потоке; эмулированные нажатия (LLKHF_INJECTED) он пропускает,
    // поэтому здесь проверяем только, что он встаёт и снимается
    KeyboardHook hook;
    hook.setTriggers({{1, parseTrigger(QStringLiteral("Print"))}});
    EXPECT_TRUE(hook.isRunning());
    hook.setTriggers({});
    EXPECT_FALSE(hook.isRunning());

    WinHotkeys keys;
    const BindResult r = keys.bind(Action::Capture, QStringLiteral("Print")).result();
    EXPECT_TRUE(r.status.ok) << r.status.error.toStdString();
}

TEST(WinInput, OwnWindowIsNotElevatedAndPasteChecksForeground)
{
    QWidget w;
    w.show();
    QApplication::processEvents();
    EXPECT_FALSE(WinInputInjector::isElevatedAbove(reinterpret_cast<HWND>(w.winId())));
    EXPECT_TRUE(WinInputInjector::processName(reinterpret_cast<HWND>(w.winId())).endsWith(QStringLiteral(".exe")));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);   // настоящий модуль windows: нужен OLE-буфер и окна
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
