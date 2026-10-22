#pragma once

#include "platform/Interfaces.h"

#include <QImage>
#include <QStringList>

#include <functional>

namespace mp::test {

// Platform interface stubs for the paste orchestrator and wizard tests.

class FakeClipboard : public IClipboard
{
public:
    bool background = true;
    // Like the Clipboard portal: reading and writing only while an input session is open
    std::function<bool()> sessionOpen;
    QStringList log;   // "snapshot", "write": the order of operations
    ClipboardPayload current;
    QList<ClipboardPayload> writes;
    Status nextWriteStatus;

    bool canWriteInBackground() const override { return background; }
    bool needsInputSession() const override { return bool(sessionOpen); }
    QFuture<Status> write(const ClipboardPayload &p, ClipboardFlags) override
    {
        log << QStringLiteral("write");
        if (sessionOpen && !sessionOpen())
            return readyFuture(Status::failure(QStringLiteral("no session")));
        writes << p;
        if (nextWriteStatus.ok)
            current = p;
        return readyFuture(nextWriteStatus);
    }
    QFuture<ClipboardPayload> snapshot() override
    {
        log << QStringLiteral("snapshot");
        if (sessionOpen && !sessionOpen())
            return readyFuture(ClipboardPayload());
        return readyFuture(current);
    }
    QString name() const override { return QStringLiteral("fake"); }
};

class FakeInjector : public IInputInjector
{
public:
    explicit FakeInjector(FakeClipboard *cb) : clipboard(cb) {}
    FakeClipboard *clipboard;
    QList<ClipboardPayload> pasted;   // what was in the clipboard at each Ctrl+V
    int begins = 0, ends = 0;
    bool sessionOpen = false;
    Status beginStatus;
    int failOnPaste = -1;

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override
    {
        clipboard->log << QStringLiteral("begin");
        ++begins;
        sessionOpen = beginStatus.ok;
        return readyFuture(beginStatus);
    }
    void end() override
    {
        clipboard->log << QStringLiteral("end");
        ++ends;
        sessionOpen = false;
    }
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds) override { return readyFuture(Status::success()); }
    int masks = 0;
    QFuture<Status> sendModifierMask() override
    {
        clipboard->log << QStringLiteral("mask");
        ++masks;
        return readyFuture(sessionOpen ? Status::success() : Status::failure(QStringLiteral("no session")));
    }
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
    bool ctrlMask = false;   // the mask is a Ctrl tap, as on Linux
    bool maskAddsCtrl() const override { return ctrlMask; }
};

class FakeCapture : public ICapture
{
public:
    CaptureResult result;
    int calls = 0;

    QFuture<CaptureResult> captureAllScreens() override
    {
        ++calls;
        return readyFuture(result);
    }
    CaptureCaps capabilities() const override { return CaptureCap::Silent; }
    QString name() const override { return QStringLiteral("fake capture"); }
};

class FakeHotkeys : public IHotkeys
{
public:
    QFuture<BindResult> bind(Action, const QString &preferred) override { return readyFuture(BindResult{{}, preferred}); }
    QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) override
    {
        QHash<Action, BindResult> r;
        for (auto it = preferred.cbegin(); it != preferred.cend(); ++it)
            r.insert(it.key(), BindResult{{}, it.value()});
        return readyFuture(r);
    }
    QString name() const override { return QStringLiteral("fake hotkeys"); }
    bool interceptPasteKey = false;   // Ctrl+V can be a conditional shortcut, as on Windows
    bool armed = false;
    bool canInterceptPasteKey() const override { return interceptPasteKey; }
    void setPasteKeyArmed(bool a) override { armed = a; }

    // The desktop takes these keys first: key → who
    QHash<QString, QString> desktopHolders;
    QStringList released;
    QString desktopKeyHolder(const QString &trigger) const override { return desktopHolders.value(trigger); }
    Status releaseDesktopKey(const QString &trigger) override
    {
        released << trigger;
        desktopHolders.remove(trigger);
        return Status::success();
    }
};

} // namespace mp::test
