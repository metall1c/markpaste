#include "TestUtil.h"
#include "paste/PasteOrchestrator.h"
#include "paste/TempFiles.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QUrl>

#include <gtest/gtest.h>

using namespace ps;
using namespace ps::test;

namespace {

class FakeClipboard : public IClipboard
{
public:
    bool background = true;
    ClipboardPayload current;
    QList<ClipboardPayload> writes;
    Status nextWriteStatus;

    bool canWriteInBackground() const override { return background; }
    QFuture<Status> write(const ClipboardPayload &p, ClipboardFlags) override
    {
        writes << p;
        if (nextWriteStatus.ok)
            current = p;
        return readyFuture(nextWriteStatus);
    }
    QFuture<ClipboardPayload> snapshot() override { return readyFuture(current); }
    QString name() const override { return QStringLiteral("fake"); }
};

class FakeInjector : public IInputInjector
{
public:
    explicit FakeInjector(FakeClipboard *cb) : clipboard(cb) {}
    FakeClipboard *clipboard;
    QList<ClipboardPayload> pasted;   // что лежало в буфере в момент каждого Ctrl+V
    int begins = 0, ends = 0;
    bool sessionOpen = false;
    Status beginStatus;
    int failOnPaste = -1;

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override
    {
        ++begins;
        sessionOpen = beginStatus.ok;
        return readyFuture(beginStatus);
    }
    void end() override
    {
        ++ends;
        sessionOpen = false;
    }
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds) override { return readyFuture(Status::success()); }
    QFuture<Status> sendPaste() override
    {
        if (!sessionOpen)
            return readyFuture(Status::failure(QStringLiteral("no session")));
        if (pasted.size() == failOnPaste)
            return readyFuture(Status::failure(QStringLiteral("boom")));
        pasted << clipboard->current;
        return readyFuture(Status::success());
    }
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("fake"); }
};

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

// Ждёт сигнал finished, крутя цикл событий.
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

    // Буфер пользователя вернулся (PST-8), сессия ввода закрыта
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
    EXPECT_EQ(inj.pasted.size(), 0);   // клавиша ещё зажата
    o.hotkeyReleased();
    ASSERT_TRUE(runToEnd(o).ok);
    EXPECT_EQ(inj.pasted.size(), 4);
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
    o.start(threeShots(), fastOptions());
    EXPECT_FALSE(runToEnd(o).ok);
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
    o.start(CaptureSet{}, fastOptions());
    EXPECT_FALSE(runToEnd(o).ok);
}
