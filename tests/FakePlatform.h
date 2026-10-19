#pragma once

#include "platform/Interfaces.h"

#include <QImage>

namespace mp::test {

// Заглушки платформенных интерфейсов для тестов оркестратора вставки и мастера.

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
};

} // namespace mp::test
