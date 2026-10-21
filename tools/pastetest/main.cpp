// Paste prototype (spec section 13, first stage): pastes images and text into the active window
// in mode 1 (sequential paste) or puts them into the clipboard in mode 2 (files + text).
// Needed to fill in the compatibility matrix before building the main application.

#include "SelfTest.h"
#include "core/CaptureSet.h"
#include "platform/linux/kde/KWinScreenshot.h"
#include "platform/linux/portal/PortalBus.h"
#include "platform/linux/portal/PortalClipboard.h"
#include "platform/linux/portal/PortalScreenshot.h"
#include "platform/linux/portal/PortalRemoteDesktop.h"
#include "platform/linux/wayland/DataControlClipboard.h"
#include "platform/linux/wayland/WaylandConnection.h"
#ifdef MARKPASTE_HAVE_X11
#include "platform/linux/x11/X11Hotkeys.h"
#endif

#include <QBuffer>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include <csignal>
#include <memory>

using namespace mp;
using namespace mp::lx;

namespace {

QTextStream out(stdout);

void say(const QString &s)
{
    out << s << Qt::endl;
}

template <typename T>
T await(QFuture<T> future)
{
    QEventLoop loop;
    auto watcher = future.then(&loop, [&loop](T) { loop.quit(); });
    if (!future.isFinished())
        loop.exec();
    return future.result();
}

void sleepMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

QByteArray samplePng(int index)
{
    QImage img(640, 360, QImage::Format_ARGB32);
    const QColor colors[] = {QColor(0x3B, 0x82, 0xF6), QColor(0x10, 0xB9, 0x81), QColor(0xF5, 0x9E, 0x0B)};
    img.fill(colors[index % 3]);
    QPainter p(&img);
    QFont f = p.font();
    f.setPixelSize(200);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(img.rect(), Qt::AlignCenter, QString(shotLetter(index)));
    p.end();
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return data;
}

int runCheck()
{
    say(QStringLiteral("XDG_SESSION_TYPE=%1  XDG_CURRENT_DESKTOP=%2  QPA=%3")
            .arg(qEnvironmentVariable("XDG_SESSION_TYPE"), qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                 QGuiApplication::platformName()));

    WaylandConnection *wl = WaylandConnection::instance();
    say(QStringLiteral("\nWayland (own connection): %1").arg(wl ? QStringLiteral("ok") : QStringLiteral("unavailable")));
    if (wl) {
        for (const char *g : {"ext_data_control_manager_v1", "zwlr_data_control_manager_v1",
                              "zwp_virtual_keyboard_manager_v1", "zwlr_layer_shell_v1", "xdg_activation_v1",
                              "ext_image_copy_capture_manager_v1", "wp_fractional_scale_manager_v1"}) {
            const WaylandGlobal info = wl->global(g);
            say(QStringLiteral("  %1 %2").arg(QString::fromLatin1(g), -40)
                    .arg(info.name ? QStringLiteral("v%1").arg(info.version) : QStringLiteral("—")));
        }
    }

    PortalBus &bus = PortalBus::instance();
    say(QStringLiteral("\nPortals:"));
    for (const char *i : {"Screenshot", "ScreenCast", "GlobalShortcuts", "RemoteDesktop", "Clipboard", "FileTransfer",
                          "InputCapture"}) {
        const uint v = bus.interfaceVersion(QStringLiteral("org.freedesktop.portal.") + QString::fromLatin1(i));
        say(QStringLiteral("  %1 %2").arg(QString::fromLatin1(i), -40)
                .arg(v ? QStringLiteral("v%1").arg(v) : QStringLiteral("—")));
    }
    const Status reg = bus.registerApp(QStringLiteral(MARKPASTE_APP_ID));
    say(QStringLiteral("\nhost Registry.Register(%1): %2")
            .arg(QStringLiteral(MARKPASTE_APP_ID), reg.ok ? QStringLiteral("ok") : reg.error));

    if (auto *cb = DataControlClipboard::create()) {
        say(QStringLiteral("\nCurrent clipboard MIME types: %1").arg(cb->currentMimeTypes().join(QStringLiteral(", "))));
        delete cb;
    }
    return 0;
}

int runCapture(const QString &backend, const QString &outDir)
{
    std::unique_ptr<ICapture> cap;
    if (backend == u"portal")
        cap = std::make_unique<PortalScreenshot>();
    else
        cap = std::make_unique<KWinScreenshot>();
    QElapsedTimer t;
    t.start();
    const CaptureResult r = await(cap->captureAllScreens());
    say(QStringLiteral("%1: %2 in %3 ms").arg(cap->name(), r.status.ok ? QStringLiteral("ok") : r.status.error).arg(t.elapsed()));
    QDir().mkpath(outDir);
    for (const ScreenFrame &f : r.frames) {
        say(QStringLiteral("  %1 logical %2x%3+%4+%5 scale %6 -> image %7x%8 format %9")
                .arg(f.screenName).arg(f.logicalGeometry.width()).arg(f.logicalGeometry.height())
                .arg(f.logicalGeometry.x()).arg(f.logicalGeometry.y()).arg(f.scale)
                .arg(f.image.width()).arg(f.image.height()).arg(int(f.image.format())));
        if (!f.image.isNull() && !outDir.isEmpty())
            f.image.save(QStringLiteral("%1/%2.png").arg(outDir, f.screenName));
    }
    return r.status.ok ? 0 : 9;
}

QString textOf(const ClipboardPayload &p)
{
    const ClipboardEntry *e = p.find(QStringLiteral("text/plain;charset=utf-8"));
    if (!e)
        e = p.find(QStringLiteral("text/plain"));
    return e ? QString::fromUtf8(e->data) : QStringLiteral("<no text: %1>").arg(p.mimeTypes().join(u','));
}

// Checking the Clipboard portal (the fallback clipboard for GNOME) in an environment that also has data-control:
// write through the portal, read back through data-control, and the other way round.
int runPortalClipboard(bool forgetToken)
{
    PortalBus &bus = PortalBus::instance();
    const Status reg = bus.registerApp(QStringLiteral(MARKPASTE_APP_ID));
    if (!reg.ok)
        say(QStringLiteral("warning: portal app registration failed: %1").arg(reg.error));
    if (!PortalClipboard::isAvailable()) {
        say(QStringLiteral("No Clipboard portal"));
        return 3;
    }
    std::unique_ptr<DataControlClipboard> dc(DataControlClipboard::create());
    if (!dc)
        say(QStringLiteral("note: no data-control, cross-checks are skipped"));

    QSettings settings;
    PortalRemoteDesktop rd;
    if (forgetToken)
        settings.remove(QStringLiteral("portalClipboardRestoreToken"));
    rd.setRestoreToken(settings.value(QStringLiteral("portalClipboardRestoreToken")).toString());
    QObject::connect(&rd, &PortalRemoteDesktop::restoreTokenChanged, [&settings](const QString &t) {
        settings.setValue(QStringLiteral("portalClipboardRestoreToken"), t);
        settings.sync();
    });
    PortalClipboard portal(&rd);

    if (dc) {
        await(dc->write(ClipboardPayload::text(QStringLiteral("baseline from data-control")), ClipboardFlag::None));
        say(QStringLiteral("1. data-control wrote the baseline"));
    }
    say(QStringLiteral("2. Starting RemoteDesktop session with clipboard (a dialog may appear)…"));
    QElapsedTimer t;
    t.start();
    const Status begun = await(rd.begin());
    if (!begun.ok) {
        say(QStringLiteral("   failed: %1").arg(begun.error));
        return 5;
    }
    say(QStringLiteral("   started in %1 ms, clipboard_enabled = %2").arg(t.elapsed()).arg(rd.clipboardEnabled()));
    if (!rd.clipboardEnabled())
        return 6;
    sleepMs(300);   // SelectionOwnerChanged arrives after Start

    t.restart();
    const ClipboardPayload snap = await(portal.snapshot());
    say(QStringLiteral("3. portal snapshot (%1 ms): \"%2\"").arg(t.elapsed()).arg(textOf(snap)));

    t.restart();
    const Status w = await(portal.write(ClipboardPayload::text(QStringLiteral("written by portal ✓")),
                                        ClipboardFlag::ExcludeFromHistory));
    say(QStringLiteral("4. portal write: %1 (%2 ms)").arg(w.ok ? QStringLiteral("ok") : w.error).arg(t.elapsed()));
    sleepMs(200);
    if (dc) {
        t.restart();
        const ClipboardPayload back = await(dc->snapshot());
        say(QStringLiteral("5. data-control reads (%1 ms): \"%2\"").arg(t.elapsed()).arg(textOf(back)));
    }
    {
        // Pasting into our own window (the setup wizard test): Qt reads the offer synchronously in the main
        // thread, so SelectionTransfer must be served in another thread (problems.md, 2026-10-21)
        QLineEdit field;
        field.setWindowTitle(QStringLiteral("pastetest: own field"));
        field.show();
        field.activateWindow();
        field.setFocus();
        sleepMs(700);   // keyboard focus, otherwise Qt has no selection offer
        t.restart();
        const QString own = QApplication::clipboard()->text();
        say(QStringLiteral("5b. own QClipboard reads (%1 ms, focus %2): \"%3\"")
                .arg(t.elapsed())
                .arg(field.hasFocus() ? QStringLiteral("yes") : QStringLiteral("no"))
                .arg(own));
    }

    rd.end();
    say(QStringLiteral("6. session closed"));
    sleepMs(1000);
    if (dc) {
        const ClipboardPayload after = await(dc->snapshot());
        say(QStringLiteral("7. after close, data-control reads: \"%1\"").arg(textOf(after)));
    }
    return 0;
}

// X11 global hotkeys: grab Print and Ctrl+Alt+V, press them via xdotool (XTEST),
// check activated/released. holdMs > 0 keeps the grab (to check "taken").
int runX11Hotkeys(int holdMs)
{
#ifdef MARKPASTE_HAVE_X11
    std::unique_ptr<X11Hotkeys> hk(X11Hotkeys::create());
    if (!hk) {
        say(QStringLiteral("FAIL: not running on X11 (QT_QPA_PLATFORM=xcb)"));
        return 3;
    }
    QStringList events;
    QObject::connect(hk.get(), &IHotkeys::activated, [&events](Action a, const ActivationContext &) {
        events << QStringLiteral("activated %1").arg(int(a));
    });
    QObject::connect(hk.get(), &IHotkeys::released, [&events](Action a) { events << QStringLiteral("released %1").arg(int(a)); });
    // Print and Ctrl+Print are on the same key: only the shortcut with the same modifiers must fire
    const auto bound = await(hk->bindAll({{Action::Capture, QStringLiteral("Print")},
                                          {Action::PasteSet, QStringLiteral("Primary+Alt+V")},
                                          {Action::CaptureToStrip, QStringLiteral("Primary+Print")}}));
    bool allBound = true;
    for (auto it = bound.cbegin(); it != bound.cend(); ++it) {
        say(QStringLiteral("bind %1: %2").arg(int(it.key())).arg(it->status.ok ? it->trigger : QStringLiteral("FAILED: ") + it->status.error));
        allBound = allBound && it->status.ok;
    }
    if (holdMs > 0) {
        sleepMs(holdMs);
        return allBound ? 0 : 4;
    }
    if (!allBound)
        return 4;
    for (const QString &k : {QStringLiteral("Print"), QStringLiteral("ctrl+alt+v"), QStringLiteral("v"),
                             QStringLiteral("ctrl+v"), QStringLiteral("ctrl+Print"), QStringLiteral("shift+Print")}) {
        QProcess::execute(QStringLiteral("xdotool"), {QStringLiteral("key"), k});
        sleepMs(150);
    }
    // Holding with autorepeat: one activation, one release
    QProcess::execute(QStringLiteral("xdotool"), {QStringLiteral("keydown"), QStringLiteral("Print")});
    sleepMs(900);
    QProcess::execute(QStringLiteral("xdotool"), {QStringLiteral("keyup"), QStringLiteral("Print")});
    sleepMs(150);
    say(QStringLiteral("events: %1").arg(events.join(QStringLiteral(", "))));
    const QStringList expected{QStringLiteral("activated 0"), QStringLiteral("released 0"), QStringLiteral("activated 1"),
                               QStringLiteral("released 1"), QStringLiteral("activated 4"), QStringLiteral("released 4"),
                               QStringLiteral("activated 0"), QStringLiteral("released 0")};
    const bool ok = events == expected;
    say(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL, expected: %1").arg(expected.join(QStringLiteral(", "))));
    return ok ? 0 : 7;
#else
    Q_UNUSED(holdMs);
    say(QStringLiteral("built without X11 support"));
    return 3;
#endif
}

} // namespace

int main(int argc, char **argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("markpaste"));
    QCoreApplication::setApplicationName(QStringLiteral("pastetest"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Markpaste paste prototype"));
    cli.addHelpOption();
    cli.addPositionalArgument(QStringLiteral("images"), QStringLiteral("PNG files (default: 3 generated samples)"));
    QCommandLineOption checkOpt(QStringLiteral("check"), QStringLiteral("Print platform capabilities and exit"));
    QCommandLineOption modeOpt(QStringLiteral("mode"), QStringLiteral("1 = sequential paste, 2 = files + text"),
                               QStringLiteral("n"), QStringLiteral("1"));
    QCommandLineOption delayOpt(QStringLiteral("delay"), QStringLiteral("Delay after each image paste, ms"),
                                QStringLiteral("ms"), QStringLiteral("400"));
    QCommandLineOption settleOpt(QStringLiteral("settle"), QStringLiteral("Delay between clipboard write and Ctrl+V, ms"),
                                 QStringLiteral("ms"), QStringLiteral("80"));
    QCommandLineOption countdownOpt(QStringLiteral("countdown"),
                                    QStringLiteral("Seconds to switch to the chat window"), QStringLiteral("s"),
                                    QStringLiteral("5"));
    QCommandLineOption textOpt(QStringLiteral("text"), QStringLiteral("Message text"), QStringLiteral("text"));
    QCommandLineOption noRestoreOpt(QStringLiteral("no-restore"), QStringLiteral("Do not restore clipboard"));
    QCommandLineOption forgetOpt(QStringLiteral("forget-token"), QStringLiteral("Drop saved RemoteDesktop restore token"));
    QCommandLineOption onlyTextOpt(QStringLiteral("keys-only"),
                                   QStringLiteral("Mode 1 without clipboard writes: just send Ctrl+V N times"));
    QCommandLineOption selfTestOpt(QStringLiteral("self-test"),
                                   QStringLiteral("Mode 1 into an own receiver window, report what arrived"));
    QCommandLineOption inProcessOpt(QStringLiteral("in-process"), QStringLiteral("Self-test receiver in the same process"));
    QCommandLineOption receiverOpt(QStringLiteral("receiver"), QStringLiteral("Internal: self-test receiver window"));
    QCommandLineOption captureOpt(QStringLiteral("capture"), QStringLiteral("Capture all screens: kwin or portal"),
                                  QStringLiteral("backend"));
    QCommandLineOption outOpt(QStringLiteral("out"), QStringLiteral("Directory for --capture PNGs"), QStringLiteral("dir"));
    QCommandLineOption portalClipboardOpt(QStringLiteral("portal-clipboard"),
                                          QStringLiteral("Check the Clipboard portal against data-control"));
    cli.addOption(portalClipboardOpt);
    QCommandLineOption x11HotkeysOpt(QStringLiteral("x11-hotkeys"),
                                     QStringLiteral("Check X11 global hotkeys (run under X11 with xdotool)"));
    QCommandLineOption holdOpt(QStringLiteral("hold"), QStringLiteral("With --x11-hotkeys: only grab and hold, ms"),
                               QStringLiteral("ms"), QStringLiteral("0"));
    cli.addOptions({x11HotkeysOpt, holdOpt});
    cli.addOptions({captureOpt, outOpt, inProcessOpt, receiverOpt, selfTestOpt, checkOpt, modeOpt, delayOpt, settleOpt, countdownOpt, textOpt, noRestoreOpt, forgetOpt, onlyTextOpt});
    cli.process(app);

    if (cli.isSet(checkOpt))
        return runCheck();
    if (cli.isSet(x11HotkeysOpt))
        return runX11Hotkeys(cli.value(holdOpt).toInt());
    if (cli.isSet(portalClipboardOpt))
        return runPortalClipboard(cli.isSet(forgetOpt));
    if (cli.isSet(captureOpt)) {
        PortalBus::instance().registerApp(QStringLiteral(MARKPASTE_APP_ID));
        return runCapture(cli.value(captureOpt), cli.value(outOpt));
    }
    if (cli.isSet(receiverOpt))
        return runReceiver();

    // --- Input data ---
    QList<QByteArray> images;
    const QStringList files = cli.positionalArguments();
    if (files.isEmpty()) {
        for (int i = 0; i < 3; ++i)
            images << samplePng(i);
    } else {
        for (const QString &f : files) {
            QFile file(f);
            if (!file.open(QIODevice::ReadOnly)) {
                say(QStringLiteral("cannot read %1").arg(f));
                return 2;
            }
            images << file.readAll();
        }
    }
    QString text = cli.value(textOpt);
    if (text.isEmpty()) {
        QStringList lines;
        for (int i = 0; i < images.size(); ++i)
            lines << QStringLiteral("Shot %1\n%2 Test comment").arg(shotLetter(i)).arg(markLabel(i, 0));
        text = lines.join(QStringLiteral("\n\n"));
    }

    PortalBus &bus = PortalBus::instance();
    if (cli.isSet(selfTestOpt)) {
        const Status r = bus.registerApp(QStringLiteral(MARKPASTE_APP_ID));
        if (!r.ok)
            say(QStringLiteral("warning: portal app registration failed: %1").arg(r.error));
        return runSelfTest(images, text, cli.value(settleOpt).toInt(), cli.value(delayOpt).toInt(),
                           cli.isSet(inProcessOpt));
    }
    const Status reg = bus.registerApp(QStringLiteral(MARKPASTE_APP_ID));
    if (!reg.ok)
        say(QStringLiteral("warning: portal app registration failed: %1").arg(reg.error));

    DataControlClipboard *clipboard = DataControlClipboard::create(&app);
    if (!clipboard) {
        say(QStringLiteral("ext-data-control-v1 is not available: background clipboard is impossible here"));
        return 3;
    }

    const int mode = cli.value(modeOpt).toInt();

    // --- Mode 2: one entry with files, text and the first image ---
    if (mode == 2) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/markpaste");
        QDir().mkpath(dir);
        QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QStringList uris;
        for (int i = 0; i < images.size(); ++i) {
            const QString path = QStringLiteral("%1/shot-%2.png").arg(dir).arg(shotLetter(i));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly)) {
                say(QStringLiteral("cannot write %1").arg(path));
                return 2;
            }
            f.write(images[i]);
            uris << QUrl::fromLocalFile(path).toString();
        }
        ClipboardPayload p;
        p.entries.append({QStringLiteral("text/uri-list"), uris.join(QStringLiteral("\r\n")).toUtf8() + "\r\n"});
        p.entries.append({QStringLiteral("x-special/gnome-copied-files"),
                          (QStringLiteral("copy\n") + uris.join(u'\n')).toUtf8()});
        p.addText(text);
        p.addImage(images.first());
        const Status s = await(clipboard->write(p, ClipboardFlag::ExcludeFromHistory));
        say(s.ok ? QStringLiteral("Mode 2: clipboard set (%1 files + text + image/png). Press Ctrl+V in the chat. "
                                  "Ctrl+C here to quit (the clipboard dies with the process).")
                       .arg(images.size())
                 : QStringLiteral("clipboard write failed: %1").arg(s.error));
        return s.ok ? app.exec() : 4;
    }

    // --- Mode 1: sequential paste ---
    QSettings settings;
    PortalRemoteDesktop injector;
    if (cli.isSet(forgetOpt))
        settings.remove(QStringLiteral("remoteDesktopRestoreToken"));
    injector.setRestoreToken(settings.value(QStringLiteral("remoteDesktopRestoreToken")).toString());
    QObject::connect(&injector, &PortalRemoteDesktop::restoreTokenChanged, [&settings](const QString &t) {
        settings.setValue(QStringLiteral("remoteDesktopRestoreToken"), t);
        settings.sync();
    });

    say(QStringLiteral("Starting RemoteDesktop session (a permission dialog may appear)…"));
    QElapsedTimer t;
    t.start();
    const Status begun = await(injector.begin());
    if (!begun.ok) {
        say(QStringLiteral("RemoteDesktop failed: %1").arg(begun.error));
        return 5;
    }
    say(QStringLiteral("Session ready in %1 ms").arg(t.elapsed()));

    const int countdown = cli.value(countdownOpt).toInt();
    for (int i = countdown; i > 0; --i) {
        say(QStringLiteral("Switch to the chat input… %1").arg(i));
        sleepMs(1000);
    }

    const ClipboardPayload saved = cli.isSet(noRestoreOpt) ? ClipboardPayload() : await(clipboard->snapshot());
    const int settle = cli.value(settleOpt).toInt();
    const int delay = cli.value(delayOpt).toInt();
    t.restart();

    auto pasteOne = [&](const ClipboardPayload &p, const QString &what) -> bool {
        if (!cli.isSet(onlyTextOpt)) {
            const Status w = await(clipboard->write(p, ClipboardFlag::ExcludeFromHistory));
            if (!w.ok) {
                say(QStringLiteral("clipboard write failed (%1): %2").arg(what, w.error));
                return false;
            }
            sleepMs(settle);
        }
        const Status k = await(injector.sendPaste());
        if (!k.ok) {
            say(QStringLiteral("Ctrl+V failed (%1): %2").arg(what, k.error));
            return false;
        }
        say(QStringLiteral("  pasted %1 at %2 ms").arg(what).arg(t.elapsed()));
        return true;
    };

    bool ok = true;
    for (int i = 0; ok && i < images.size(); ++i) {
        ok = pasteOne(ClipboardPayload::image(images[i]), QStringLiteral("image %1").arg(shotLetter(i)));
        sleepMs(delay);
    }
    if (ok)
        ok = pasteOne(ClipboardPayload::text(text), QStringLiteral("text"));

    injector.end();
    say(QStringLiteral("Done in %1 ms, %2").arg(t.elapsed()).arg(ok ? QStringLiteral("ok") : QStringLiteral("FAILED")));

    if (!saved.isEmpty()) {
        sleepMs(1000);
        await(clipboard->write(saved, ClipboardFlag::None));
        say(QStringLiteral("Clipboard restored (%1). Keeping it alive for 10 s…").arg(saved.mimeTypes().join(QStringLiteral(", "))));
        sleepMs(10000);
    }
    return ok ? 0 : 6;
}
