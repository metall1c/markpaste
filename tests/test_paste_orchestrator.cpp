#include "FakePlatform.h"
#include "TestUtil.h"
#include "paste/PasteOrchestrator.h"
#include "paste/TempFiles.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QUrl>

#include <gtest/gtest.h>

#include <optional>

using namespace mp;
using namespace mp::test;

namespace {

PasteOptions fastOptions()
{
    PasteOptions o;
    o.messageTemplate = MessageTemplate::defaultFor(QLocale(QLocale::English));
    o.timing = PasteTiming{0, 0, 0, 0, 200, 0};
    return o;
}

CaptureSet threeShots()
{
    CaptureSet set;
    for (int i = 0; i < 3; ++i) {
        Shot s = makeShot(60, 40);
        s.id = QUuid::createUuid();
        s.marks.append(Mark{QUuid::createUuid(), QRect(5, 5, 20, 10), QStringLiteral("c%1").arg(i)});
        set.shots << s;
    }
    return set;
}

// Waits for the finished signal while spinning the event loop.
Status runToEnd(PasteOrchestrator &o, int timeoutMs = 5000)
{
    Status result = Status::failure(QStringLiteral("timeout"));
    bool done = false;
    QObject::connect(&o, &PasteOrchestrator::finished, [&](const Status &s, PasteMode) {
        result = s;
        done = true;
    });
    QElapsedTimer t;
    t.start();
    while (!done && t.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return result;
}

// For failures that come right from start(): subscribe before starting
Status startAndFinish(PasteOrchestrator &o, const CaptureSet &set, const PasteOptions &opt)
{
    std::optional<Status> result;
    const auto c = QObject::connect(&o, &PasteOrchestrator::finished, [&](const Status &s, PasteMode) { result = s; });
    o.start(set, opt);
    QObject::disconnect(c);
    return result.value_or(Status::success());   // did not finish right away: the test sees ok and fails
}

bool isImage(const ClipboardPayload &p) { return p.find(QStringLiteral("image/png")) && !p.find(QStringLiteral("text/plain")); }

} // namespace

TEST(PasteOrchestrator, SequentialOrderAndRestore)
{
    FakeClipboard cb;
    cb.current = ClipboardPayload::text(QStringLiteral("user's clipboard"));
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);

    o.start(threeShots(), fastOptions());
    const Status s = runToEnd(o);
    ASSERT_TRUE(s.ok) << s.error.toStdString();

    ASSERT_EQ(inj.pasted.size(), 4);
    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(isImage(inj.pasted[i])) << i;
    const ClipboardEntry *text = inj.pasted[3].find(QStringLiteral("text/plain"));
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(QString::fromUtf8(text->data), QStringLiteral("Shot A\nA1 c0\n\nShot B\nB1 c1\n\nShot C\nC1 c2"));

    // The user's clipboard came back (PST-8), the input session is closed
    EXPECT_EQ(QString::fromUtf8(cb.current.find(QStringLiteral("text/plain"))->data), QStringLiteral("user's clipboard"));
    EXPECT_EQ(inj.begins, 1);
    EXPECT_FALSE(inj.sessionOpen);
}

TEST(PasteOrchestrator, TextBeforeImages)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.messageTemplate.textBeforeImages = true;
    o.start(threeShots(), opt);
    ASSERT_TRUE(runToEnd(o).ok);
    ASSERT_EQ(inj.pasted.size(), 4);
    EXPECT_NE(inj.pasted[0].find(QStringLiteral("text/plain")), nullptr);
    EXPECT_TRUE(isImage(inj.pasted[3]));
}

TEST(PasteOrchestrator, CombinedIsOneImagePlusText)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.mode = PasteMode::Combined;
    o.start(threeShots(), opt);
    ASSERT_TRUE(runToEnd(o).ok);
    ASSERT_EQ(inj.pasted.size(), 2);
    EXPECT_TRUE(isImage(inj.pasted[0]));
    EXPECT_NE(inj.pasted[1].find(QStringLiteral("text/plain")), nullptr);
}

TEST(PasteOrchestrator, WaitsForHotkeyRelease)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.waitHotkeyRelease = true;
    opt.timing.releaseTimeoutMs = 10000;
    o.start(threeShots(), opt);

    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 200)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    EXPECT_EQ(inj.pasted.size(), 0);   // the key is still held
    o.hotkeyReleased();
    ASSERT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
}

// Alt+V: the system took the V, the chat window sees a lone Alt and opens its menu (Mattermost).
// While Alt is held, the session opens right away and the "mask" is sent; one session for the whole paste.
TEST(PasteOrchestrator, AltHotkeyIsMaskedWhileHeld)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.waitHotkeyRelease = true;
    opt.maskAltTap = true;
    opt.timing.releaseTimeoutMs = 10000;
    o.start(threeShots(), opt);
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 100)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    EXPECT_EQ(inj.masks, 1);
    EXPECT_EQ(inj.pasted.size(), 0);
    o.hotkeyReleased();
    ASSERT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
    EXPECT_EQ(inj.begins, 1);
    EXPECT_EQ(inj.masks, 1);
    EXPECT_LT(cb.log.indexOf(QStringLiteral("begin")), cb.log.indexOf(QStringLiteral("mask")));
}

// The Ctrl mask turns a held Alt+V into Ctrl+Alt+V, and KDE reports "released" and right after it "pressed"
// (problems.md 2026-10-21): no Ctrl+V while the keys are held, only after the real release
TEST(PasteOrchestrator, ChordChangeIsNotARelease)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.waitHotkeyRelease = true;
    opt.maskAltTap = true;
    opt.timing.releaseTimeoutMs = 10000;
    opt.timing.releaseSettleMs = 100;
    o.start(threeShots(), opt);
    auto spin = [](int ms) {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    };
    spin(30);
    o.hotkeyReleased();   // Alt+V "released" because of the injected Ctrl…
    spin(20);
    o.hotkeyPressed();    // …and Ctrl+Alt+V "pressed"
    spin(300);
    EXPECT_EQ(inj.pasted.size(), 0);   // still held: nothing sent
    o.hotkeyReleased();   // the real release
    ASSERT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
}

// A press reported after the release had already settled, but before the first Ctrl+V: wait again
TEST(PasteOrchestrator, LatePressBeforeFirstKeysWaitsAgain)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.waitHotkeyRelease = true;
    opt.timing.releaseTimeoutMs = 10000;
    opt.timing.releaseSettleMs = 0;
    opt.timing.afterReleaseMs = 200;
    o.start(threeShots(), opt);
    o.hotkeyReleased();
    o.hotkeyPressed();   // within afterReleaseMs: the step must not send keys
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 400)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    EXPECT_EQ(inj.pasted.size(), 0);
    o.hotkeyReleased();
    ASSERT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
}

TEST(PasteOrchestrator, NoMaskWithoutAltOrAfterRelease)
{
    for (const bool mask : {false, true}) {
        FakeClipboard cb;
        FakeInjector inj(&cb);
        PasteOrchestrator o(&cb, &inj);
        PasteOptions opt = fastOptions();
        opt.maskAltTap = mask;
        opt.waitHotkeyRelease = !mask;   // with a mask, but the key is already released (strip button)
        opt.timing.releaseTimeoutMs = 50;
        o.start(threeShots(), opt);
        ASSERT_TRUE(runToEnd(o).ok);
        EXPECT_EQ(inj.masks, 0);
        EXPECT_EQ(inj.begins, 1);
    }
}

TEST(PasteOrchestrator, ReleaseTimeoutStillPastes)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.waitHotkeyRelease = true;
    opt.timing.releaseTimeoutMs = 50;
    o.start(threeShots(), opt);
    ASSERT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
}

TEST(PasteOrchestrator, SessionDeniedFailsAndRestores)
{
    FakeClipboard cb;
    cb.current = ClipboardPayload::text(QStringLiteral("keep me"));
    FakeInjector inj(&cb);
    inj.beginStatus = Status::failure(QStringLiteral("denied"));
    PasteOrchestrator o(&cb, &inj);
    o.start(threeShots(), fastOptions());
    const Status s = runToEnd(o);
    EXPECT_FALSE(s.ok);
    EXPECT_EQ(inj.pasted.size(), 0);
    EXPECT_EQ(QString::fromUtf8(cb.current.find(QStringLiteral("text/plain"))->data), QStringLiteral("keep me"));
}

TEST(PasteOrchestrator, SessionBoundClipboardIsUsedInsideSession)
{
    // Clipboard portal: the clipboard is saved after the session opens and restored before it closes
    FakeClipboard cb;
    FakeInjector inj(&cb);
    cb.sessionOpen = [&inj] { return inj.sessionOpen; };
    inj.sessionOpen = true;   // to put the original content
    cb.current = ClipboardPayload::text(QStringLiteral("keep me"));
    inj.sessionOpen = false;
    cb.log.clear();

    PasteOrchestrator o(&cb, &inj);
    o.start(threeShots(), fastOptions());
    EXPECT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
    ASSERT_GE(cb.log.size(), 4);
    EXPECT_EQ(cb.log.mid(0, 2), (QStringList{QStringLiteral("begin"), QStringLiteral("snapshot")}));
    EXPECT_EQ(cb.log.last(), QStringLiteral("end"));
    EXPECT_EQ(cb.log[cb.log.size() - 2], QStringLiteral("write"));   // the restore comes before end
    EXPECT_EQ(QString::fromUtf8(cb.current.find(QStringLiteral("text/plain"))->data), QStringLiteral("keep me"));
}

TEST(PasteOrchestrator, SessionBoundClipboardRestoresBeforeEndOnFailure)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    cb.sessionOpen = [&inj] { return inj.sessionOpen; };
    inj.sessionOpen = true;
    cb.current = ClipboardPayload::text(QStringLiteral("keep me"));
    inj.sessionOpen = false;
    inj.failOnPaste = 1;
    PasteOrchestrator o(&cb, &inj);
    o.start(threeShots(), fastOptions());
    EXPECT_FALSE(runToEnd(o).ok);
    EXPECT_EQ(QString::fromUtf8(cb.current.find(QStringLiteral("text/plain"))->data), QStringLiteral("keep me"));
}

TEST(PasteOrchestrator, FailureMidwayClosesSessionAndRestores)
{
    FakeClipboard cb;
    cb.current = ClipboardPayload::text(QStringLiteral("keep me"));
    FakeInjector inj(&cb);
    inj.failOnPaste = 1;
    PasteOrchestrator o(&cb, &inj);
    o.start(threeShots(), fastOptions());
    EXPECT_FALSE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 1);
    EXPECT_FALSE(inj.sessionOpen);
    EXPECT_EQ(QString::fromUtf8(cb.current.find(QStringLiteral("text/plain"))->data), QStringLiteral("keep me"));
}

TEST(PasteOrchestrator, CancelStops)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    PasteOptions opt = fastOptions();
    opt.timing.afterImageMs = 100;
    QObject::connect(&o, &PasteOrchestrator::progress, [&o](int done, int) {
        if (done == 1)
            o.cancel();
    });
    o.start(threeShots(), opt);
    EXPECT_FALSE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 1);
    EXPECT_FALSE(o.isRunning());
}

TEST(PasteOrchestrator, NoBackgroundClipboardRefusesSequential)
{
    FakeClipboard cb;
    cb.background = false;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    EXPECT_FALSE(o.canSequential());
    EXPECT_FALSE(startAndFinish(o, threeShots(), fastOptions()).ok);
    EXPECT_FALSE(o.isRunning());
    EXPECT_EQ(inj.begins, 0);
}

TEST(PasteOrchestrator, FilesAndTextPayload)
{
    QStringList files;
    const ClipboardPayload p = PasteOrchestrator::filesAndTextPayload(threeShots(), fastOptions(), &files);
    ASSERT_EQ(files.size(), 3);
    for (const QString &f : files)
        EXPECT_TRUE(QFile::exists(f)) << f.toStdString();
    EXPECT_TRUE(files[0].endsWith(QStringLiteral("shot-A.png")));

    const ClipboardEntry *uris = p.find(QStringLiteral("text/uri-list"));
    ASSERT_NE(uris, nullptr);
    EXPECT_EQ(uris->data.count("\r\n"), 3);
    EXPECT_EQ(QUrl(QString::fromUtf8(uris->data.split('\r').first())).toLocalFile(), files[0]);
    EXPECT_NE(p.find(QStringLiteral("x-special/gnome-copied-files")), nullptr);
    EXPECT_NE(p.find(QStringLiteral("text/plain")), nullptr);
    EXPECT_NE(p.find(QStringLiteral("image/png")), nullptr);

    TempFiles::clear();
    EXPECT_FALSE(QFile::exists(files[0]));
}

TEST(PasteOrchestrator, EmptySetFails)
{
    FakeClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    EXPECT_FALSE(startAndFinish(o, CaptureSet{}, fastOptions()).ok);
    EXPECT_FALSE(o.isRunning());
}

namespace {
// A clipboard whose write never finishes (the connection to the compositor broke)
class HangingClipboard : public FakeClipboard
{
public:
    QPromise<Status> never;
    QFuture<Status> write(const ClipboardPayload &, ClipboardFlags) override
    {
        log << QStringLiteral("write");
        never.start();
        return never.future();
    }
};
} // namespace

TEST(PasteOrchestrator, CancelWorksWhileClipboardHangs)
{
    HangingClipboard cb;
    FakeInjector inj(&cb);
    PasteOrchestrator o(&cb, &inj);
    o.start(threeShots(), fastOptions());
    QElapsedTimer t;
    t.start();
    while (!cb.log.contains(QStringLiteral("write")) && t.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    ASSERT_TRUE(o.isRunning());
    bool finished = false;
    QObject::connect(&o, &PasteOrchestrator::finished, [&](const Status &s, PasteMode) {
        finished = true;
        EXPECT_FALSE(s.ok);
    });
    o.cancel();   // "Stop" right away, without waiting for the clipboard
    EXPECT_TRUE(finished);
    EXPECT_FALSE(o.isRunning());
    EXPECT_FALSE(inj.sessionOpen);
    EXPECT_TRUE(inj.pasted.isEmpty());

    // The next paste is not blocked by the hung one
    FakeClipboard cb2;
    FakeInjector inj2(&cb2);
    PasteOrchestrator o2(&cb2, &inj2);
    o2.start(threeShots(), fastOptions());
    EXPECT_TRUE(runToEnd(o2).ok);
}
