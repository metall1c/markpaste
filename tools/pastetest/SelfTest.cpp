#include "SelfTest.h"

#include "core/CaptureSet.h"
#include "platform/linux/portal/PortalRemoteDesktop.h"
#include "platform/linux/wayland/DataControlClipboard.h"
#include "platform/linux/wayland/VirtualKeyboardInjector.h"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QMimeData>
#include <QProcess>
#include <QSettings>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <functional>
#include <memory>

using namespace mp;
using namespace mp::lx;

namespace {

QTextStream sout(stdout);

// Каждая вставка — строка: "PASTE image 640x360 read=…" или "PASTE text <hex> read=…".
class Receiver : public QTextEdit
{
public:
    std::function<void(const QString &)> report;

protected:
    bool canInsertFromMimeData(const QMimeData *) const override { return true; }
    void insertFromMimeData(const QMimeData *md) override
    {
        QElapsedTimer t;
        t.start();
        if (md->hasImage()) {
            const QImage img = qvariant_cast<QImage>(md->imageData());
            report(QStringLiteral("PASTE image %1x%2 read=%3ms").arg(img.width()).arg(img.height()).arg(t.elapsed()));
            insertPlainText(QStringLiteral("[image %1x%2]\n").arg(img.width()).arg(img.height()));
        } else if (md->hasText()) {
            const QString text = md->text();
            report(QStringLiteral("PASTE text %1 read=%2ms")
                       .arg(QString::fromLatin1(text.toUtf8().toHex()))
                       .arg(t.elapsed()));
            insertPlainText(text + QStringLiteral("\n"));
        } else {
            report(QStringLiteral("PASTE other %1").arg(md->formats().join(u',')));
        }
    }
};

QWidget *makeReceiverWindow(Receiver **out)
{
    auto *window = new QWidget;
    window->setWindowTitle(QStringLiteral("Markpaste paste self-test"));
    auto *layout = new QVBoxLayout(window);
    layout->addWidget(new QLabel(QStringLiteral("Не трогайте клавиатуру: сюда сейчас будет вставлен набор.")));
    auto *receiver = new Receiver;
    layout->addWidget(receiver);
    window->resize(520, 360);
    *out = receiver;
    return window;
}

template <typename T>
T await(QFuture<T> f)
{
    QEventLoop loop;
    auto w = f.then(&loop, [&loop](T) { loop.quit(); });
    if (!f.isFinished())
        loop.exec();
    return f.result();
}

void sleepMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

} // namespace

int runReceiver()
{
    Receiver *receiver = nullptr;
    QWidget *window = makeReceiverWindow(&receiver);
    receiver->report = [](const QString &line) { sout << line << Qt::endl; };
    window->show();
    receiver->setFocus();
    QTimer::singleShot(300, [] { sout << "READY" << Qt::endl; });
    QTimer::singleShot(60000, qApp, &QCoreApplication::quit);   // страховка, если родитель упал
    return qApp->exec();
}

int runSelfTest(const QList<QByteArray> &images, const QString &text, int settleMs, int delayMs, bool inProcess)
{
    QStringList lines;   // строки отчёта приёмника
    std::unique_ptr<QWidget> window;
    QProcess child;

    if (inProcess) {
        Receiver *receiver = nullptr;
        window.reset(makeReceiverWindow(&receiver));
        receiver->report = [&lines](const QString &l) { lines << l; };
        window->show();
        receiver->setFocus();
    } else {
        child.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QObject::connect(&child, &QProcess::readyReadStandardOutput, [&] {
            while (child.canReadLine())
                lines << QString::fromUtf8(child.readLine()).trimmed();
        });
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--receiver")});
        QElapsedTimer w;
        w.start();
        while (!lines.contains(QStringLiteral("READY")) && w.elapsed() < 10000)
            sleepMs(50);
        if (!lines.contains(QStringLiteral("READY"))) {
            sout << "FAIL: receiver process did not start" << Qt::endl;
            return 8;
        }
        lines.clear();
    }

    std::unique_ptr<DataControlClipboard> clipboard(DataControlClipboard::create());
    if (!clipboard) {
        sout << "FAIL: ext-data-control-v1 unavailable" << Qt::endl;
        return 3;
    }

    // Ctrl+V: virtual-keyboard-v1, если композитор его даёт (wlroots), иначе портал RemoteDesktop
    QSettings settings;
    std::unique_ptr<IInputInjector> injectorPtr;
    bool hadToken = false;
    if (qEnvironmentVariable("MARKPASTE_INPUT") != u"portal")
        injectorPtr.reset(VirtualKeyboardInjector::create());
    if (!injectorPtr) {
        auto rd = std::make_unique<PortalRemoteDesktop>();
        rd->setRestoreToken(settings.value(QStringLiteral("remoteDesktopRestoreToken")).toString());
        hadToken = !rd->restoreToken().isEmpty();
        QObject::connect(rd.get(), &PortalRemoteDesktop::restoreTokenChanged, [&settings](const QString &t) {
            settings.setValue(QStringLiteral("remoteDesktopRestoreToken"), t);
            settings.sync();
        });
        injectorPtr = std::move(rd);
    }
    IInputInjector &injector = *injectorPtr;

    sout << "Receiver: " << (inProcess ? "same process" : "separate process") << Qt::endl;
    sout << "Starting input session via " << injector.name();
    if (qobject_cast<PortalRemoteDesktop *>(&injector))
        sout << " (" << (hadToken ? "with restore token" : "no token, dialog expected") << ")";
    sout << "…" << Qt::endl;
    QElapsedTimer t;
    t.start();
    const Status begun = await(injector.begin());
    if (!begun.ok) {
        sout << "FAIL: input session: " << begun.error << Qt::endl;
        return 5;
    }
    sout << "Session ready in " << t.elapsed() << " ms" << Qt::endl;

    if (window) {
        window->raise();
        window->activateWindow();
        window->windowHandle()->requestActivate();
    }
    sleepMs(700);

    const ClipboardPayload saved = await(clipboard->snapshot());

    t.restart();
    for (int i = 0; i < images.size(); ++i) {
        await(clipboard->write(ClipboardPayload::image(images[i]), ClipboardFlag::ExcludeFromHistory));
        sleepMs(settleMs);
        const Status s = await(injector.sendPaste());
        if (!s.ok)
            sout << "Ctrl+V failed: " << s.error << Qt::endl;
        sleepMs(delayMs);
    }
    await(clipboard->write(ClipboardPayload::text(text), ClipboardFlag::ExcludeFromHistory));
    sleepMs(settleMs);
    await(injector.sendPaste());
    const qint64 elapsed = t.elapsed();
    sleepMs(delayMs);
    injector.end();
    sleepMs(1000);   // ждём, пока приёмник прочитает последнее

    int imagesGot = 0, textsGot = 0, other = 0;
    bool textOk = false;
    const QString expectedHex = QString::fromLatin1(text.toUtf8().toHex());
    for (const QString &l : std::as_const(lines)) {
        if (l.startsWith(QStringLiteral("PASTE image ")) && !l.startsWith(QStringLiteral("PASTE image 0x0")))
            ++imagesGot;
        else if (l.startsWith(QStringLiteral("PASTE text "))) {
            ++textsGot;
            textOk = l.section(u' ', 2, 2) == expectedHex;
        } else if (l.startsWith(QStringLiteral("PASTE")))
            ++other;
    }
    const bool ok = imagesGot == images.size() && textsGot == 1 && textOk && other == 0;

    sout << "Sequence took " << elapsed << " ms" << Qt::endl;
    sout << "Paste events: " << lines.size() << " (expected " << images.size() + 1 << ")" << Qt::endl;
    for (int i = 0; i < lines.size() && i < 8; ++i)
        sout << "  " << lines[i].left(110) << Qt::endl;
    sout << "Images " << imagesGot << "/" << images.size() << ", text " << (textOk ? "matches" : "MISMATCH")
         << " -> " << (ok ? "PASS" : "FAIL") << Qt::endl;

    if (child.state() != QProcess::NotRunning) {
        child.terminate();
        child.waitForFinished(2000);
    }

    if (!saved.isEmpty()) {
        await(clipboard->write(saved, ClipboardFlag::None));
        sout << "Clipboard restored: " << saved.mimeTypes().join(QStringLiteral(", ")) << Qt::endl;
        sleepMs(3000);   // даём Klipper забрать восстановленное содержимое
    }
    return ok ? 0 : 7;
}
