// Прототип вставки (ТЗ разд. 13, первый этап): вставляет картинки и текст в активное окно
// режимом 1 (последовательная вставка) или кладёт их в буфер режимом 2 (файлы + текст).
// Нужен, чтобы заполнить матрицу совместимости до того, как строить основное приложение.

#include "SelfTest.h"
#include "core/CaptureSet.h"
#include "platform/linux/kde/KWinScreenshot.h"
#include "platform/linux/portal/PortalBus.h"
#include "platform/linux/portal/PortalScreenshot.h"
#include "platform/linux/portal/PortalRemoteDesktop.h"
#include "platform/linux/wayland/DataControlClipboard.h"
#include "platform/linux/wayland/WaylandConnection.h"

#include <QBuffer>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include <csignal>
#include <memory>

using namespace ps;
using namespace ps::lx;

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
    const Status reg = bus.registerApp(QStringLiteral(POINTSHOT_APP_ID));
    say(QStringLiteral("\nhost Registry.Register(%1): %2")
            .arg(QStringLiteral(POINTSHOT_APP_ID), reg.ok ? QStringLiteral("ok") : reg.error));

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

} // namespace

int main(int argc, char **argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("pointshot"));
    QCoreApplication::setApplicationName(QStringLiteral("pastetest"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Pointshot paste prototype"));
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
    cli.addOptions({captureOpt, outOpt, inProcessOpt, receiverOpt, selfTestOpt, checkOpt, modeOpt, delayOpt, settleOpt, countdownOpt, textOpt, noRestoreOpt, forgetOpt, onlyTextOpt});
    cli.process(app);

    if (cli.isSet(checkOpt))
        return runCheck();
    if (cli.isSet(captureOpt)) {
        PortalBus::instance().registerApp(QStringLiteral(POINTSHOT_APP_ID));
        return runCapture(cli.value(captureOpt), cli.value(outOpt));
    }
    if (cli.isSet(receiverOpt))
        return runReceiver();

    // --- Входные данные ---
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
            lines << QStringLiteral("Снимок %1\n%2 Тестовый комментарий").arg(shotLetter(i)).arg(markLabel(i, 0));
        text = lines.join(QStringLiteral("\n\n"));
    }

    PortalBus &bus = PortalBus::instance();
    if (cli.isSet(selfTestOpt)) {
        const Status r = bus.registerApp(QStringLiteral(POINTSHOT_APP_ID));
        if (!r.ok)
            say(QStringLiteral("warning: portal app registration failed: %1").arg(r.error));
        return runSelfTest(images, text, cli.value(settleOpt).toInt(), cli.value(delayOpt).toInt(),
                           cli.isSet(inProcessOpt));
    }
    const Status reg = bus.registerApp(QStringLiteral(POINTSHOT_APP_ID));
    if (!reg.ok)
        say(QStringLiteral("warning: portal app registration failed: %1").arg(reg.error));

    DataControlClipboard *clipboard = DataControlClipboard::create(&app);
    if (!clipboard) {
        say(QStringLiteral("ext-data-control-v1 is not available: background clipboard is impossible here"));
        return 3;
    }

    const int mode = cli.value(modeOpt).toInt();

    // --- Режим 2: одна запись с файлами, текстом и первой картинкой ---
    if (mode == 2) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/pointshot");
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

    // --- Режим 1: последовательная вставка ---
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
