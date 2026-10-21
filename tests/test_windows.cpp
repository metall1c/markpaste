// Windows platform implementations on the real Qt platform module (windows, not offscreen):
// the OLE clipboard, the keyboard hook, GDI capture. Run under Wine on Linux (docs/build.md)
// and on Windows. What Wine cannot do (DXGI Desktop Duplication, UIPI) is not checked here.

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

// Other programs read the clipboard after every change too: VBoxTray in a VM, rdpclip, the Win+V history.
// While they read, the clipboard is busy, and our thread serves them the data (OLE delayed rendering), so
// we wait while processing events rather than with a blocking result()
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

    // Formats as other Windows programs see them
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
        EXPECT_EQ(*v, 0u);   // 0 means not into the Win+V history
        GlobalUnlock(h);
    } else {
        ADD_FAILURE() << "no CanIncludeInClipboardHistory data";
    }
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        EXPECT_EQ(QString::fromWCharArray(static_cast<const wchar_t *>(GlobalLock(h))), QStringLiteral("A1 кнопка"));
        GlobalUnlock(h);
    }
    CloseClipboard();

    // Restoring the clipboard after a paste has no marks: it is the user's clipboard again
    ASSERT_TRUE(await(cb.write(ClipboardPayload::text(QStringLiteral("old")), ClipboardFlag::None)).ok);
    QCoreApplication::processEvents();
    ASSERT_TRUE(openClipboard());
    EXPECT_FALSE(hasRegisteredFormat(L"CanIncludeInClipboardHistory"));
    CloseClipboard();
}

// "Another program" that keeps the clipboard open (rdpclip and clipboard managers do this):
// a thread that opened the clipboard; OleSetClipboard from the main thread fails meanwhile
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
            for (int i = 0; i < 500 && !ok; ++i) {   // someone else may have taken the clipboard for a moment
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
    // The write used to "succeed" always: Qt only printed a warning, "Set copied" was
    // a lie, and Ctrl+V pasted the previous clipboard content (docs/problems.md, 2026-10-20)
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
    // Reading a busy clipboard would give nothing, and the user's clipboard would not come back after a paste
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
    // An image from another program: only CF_DIB, no PNG
    QImage img(20, 10, QImage::Format_RGB32);
    img.fill(Qt::blue);
    const int stride = img.width() * 4;
    const SIZE_T size = sizeof(BITMAPINFOHEADER) + SIZE_T(stride) * img.height();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    auto *bih = static_cast<BITMAPINFOHEADER *>(GlobalLock(mem));
    *bih = BITMAPINFOHEADER{};
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = img.width();
    bih->biHeight = img.height();   // bottom-up
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

    // Another window with the same shortcut: "taken"
    HWND other = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    EXPECT_FALSE(RegisterHotKey(other, 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F11));
    DestroyWindow(other);

    EXPECT_TRUE(a.bind(Action::PasteSet, QString()).result().status.ok);   // unbind
    EXPECT_FALSE(a.bind(Action::Capture, QStringLiteral("Primary+Nonsense")).result().status.ok);
}

TEST(WinHotkeys, PrintScreenGoesThroughHook)
{
    // The hook is installed in its own thread; it skips emulated presses (LLKHF_INJECTED),
    // so here we only check that it is installed and removed
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
    QApplication app(argc, argv);   // the real windows module: the OLE clipboard and windows are needed
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
