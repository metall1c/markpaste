#include "X11InputInjector.h"

#include "X11Keys.h"

#include <QElapsedTimer>
#include <QTimer>

#include <xcb/xcb.h>
#include <xcb/xtest.h>

#include <functional>
#include <memory>

namespace mp::lx {

namespace {
constexpr uint32_t kXkControlL = 0xffe3;
constexpr uint32_t kXkV = 0x0076;
constexpr uint8_t kEvdevOffset = 8;   // X code = evdev code + 8
constexpr uint8_t kKeyLeftCtrl = 29 + kEvdevOffset;
constexpr uint8_t kKeyV = 47 + kEvdevOffset;
} // namespace

X11InputInjector *X11InputInjector::create(QObject *parent)
{
    xcb_connection_t *c = x11Connection();
    if (!c)
        return nullptr;
    const xcb_query_extension_reply_t *ext = xcb_get_extension_data(c, &xcb_test_id);
    if (!ext || !ext->present)
        return nullptr;
    auto *inj = new X11InputInjector(parent);
    const QList<uint8_t> ctrl = keycodesFor(kXkControlL);
    const QList<uint8_t> v = keycodesFor(kXkV);
    // A layout without Latin letters may have no "v": then use the V key code of a standard keyboard
    inj->m_ctrl = ctrl.isEmpty() ? kKeyLeftCtrl : ctrl.first();
    inj->m_v = v.isEmpty() ? kKeyV : v.first();
    return inj;
}

void X11InputInjector::key(uint8_t keycode, bool pressed)
{
    xcb_connection_t *c = x11Connection();
    if (!c)
        return;
    xcb_test_fake_input(c, pressed ? XCB_KEY_PRESS : XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_flush(c);
}

QFuture<Status> X11InputInjector::waitModifiersReleased(std::chrono::milliseconds timeout)
{
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, promise, timer, poll, timeout] {
        if (!anyModifierDown() || timer->elapsed() >= timeout.count()) {
            promise->addResult(Status::success());
            promise->finish();
            *poll = nullptr;   // break the reference cycle
            return;
        }
        QTimer::singleShot(20, this, [poll] {
            if (*poll)
                (*poll)();
        });
    };
    (*poll)();
    return promise->future();
}

QFuture<Status> X11InputInjector::sendModifierMask()
{
    key(m_ctrl, true);
    key(m_ctrl, false);
    return readyFuture(Status::success());
}

QFuture<Status> X11InputInjector::sendPaste()
{
    // Ctrl↓ V↓ V↑ Ctrl↑ with 20 ms pauses, as on Wayland
    const std::function<void()> steps[] = {
        [this] { key(m_ctrl, true); },
        [this] { key(m_v, true); },
        [this] { key(m_v, false); },
        [this] { key(m_ctrl, false); },
    };
    constexpr int kGapMs = 20;
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    for (int i = 0; i < int(std::size(steps)); ++i)
        QTimer::singleShot(i * kGapMs, this, steps[i]);
    QTimer::singleShot(int(std::size(steps)) * kGapMs, this, [promise] {
        promise->addResult(Status::success());
        promise->finish();
    });
    return promise->future();
}

} // namespace mp::lx
