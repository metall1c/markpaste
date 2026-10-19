#include "X11Hotkeys.h"

#include <QCoreApplication>
#include <QLoggingCategory>

#include <xcb/xcb.h>

#include <cstdlib>
#include <memory>

Q_LOGGING_CATEGORY(lcX11, "markpaste.x11")

namespace mp::lx {

namespace {
// Фиксируемые модификаторы, с которыми тоже нужно захватить: CapsLock (Lock) и NumLock (обычно Mod2)
constexpr uint16_t kIgnored[] = {0, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_2, XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2};
constexpr uint16_t kRelevant = XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1 | XCB_MOD_MASK_4;
} // namespace

X11Hotkeys *X11Hotkeys::create(QObject *parent)
{
    return x11Connection() ? new X11Hotkeys(parent) : nullptr;
}

X11Hotkeys::X11Hotkeys(QObject *parent)
    : IHotkeys(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

X11Hotkeys::~X11Hotkeys()
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
    for (const Binding &b : std::as_const(m_bindings))
        ungrab(b);
}

void X11Hotkeys::ungrab(const Binding &b)
{
    xcb_connection_t *c = x11Connection();
    if (!c)
        return;
    for (uint8_t kc : b.keycodes)
        for (uint16_t extra : kIgnored)
            xcb_ungrab_key(c, kc, x11RootWindow(), b.trigger.modifiers | extra);
    xcb_flush(c);
}

BindResult X11Hotkeys::grab(Action action, const QString &preferred)
{
    if (m_bindings.contains(action))
        ungrab(m_bindings.take(action));
    if (preferred.trimmed().isEmpty())
        return {Status::success(), QString()};

    Binding b;
    b.trigger = parseTrigger(preferred);
    if (!b.trigger.valid())
        return {Status::failure(QStringLiteral("unsupported key: %1").arg(preferred)), QString()};
    b.keycodes = keycodesFor(b.trigger.keysym);
    if (b.keycodes.isEmpty())
        return {Status::failure(QStringLiteral("no such key on this keyboard: %1").arg(preferred)), QString()};

    xcb_connection_t *c = x11Connection();
    QList<xcb_void_cookie_t> cookies;
    for (uint8_t kc : b.keycodes)
        for (uint16_t extra : kIgnored)
            cookies << xcb_grab_key_checked(c, 1, x11RootWindow(), b.trigger.modifiers | extra, kc,
                                            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    bool taken = false;
    for (const xcb_void_cookie_t &cookie : cookies)
        if (std::unique_ptr<xcb_generic_error_t, decltype(&free)> err(xcb_request_check(c, cookie), free); err)
            taken = true;   // BadAccess: сочетание захватила другая программа
    if (taken) {
        ungrab(b);
        return {Status::failure(QStringLiteral("%1 is already used by another program").arg(preferred)), QString()};
    }
    m_bindings.insert(action, b);
    qCInfo(lcX11) << "grabbed" << preferred << "keycodes" << b.keycodes;
    // Для людей: «Ctrl+Alt+V», а не переносимое «Primary+Alt+V»
    QString shown = preferred;
    shown.replace(QStringLiteral("Primary"), QStringLiteral("Ctrl"), Qt::CaseInsensitive);
    return {Status::success(), shown};
}

QFuture<BindResult> X11Hotkeys::bind(Action action, const QString &preferred)
{
    return readyFuture(grab(action, preferred));
}

QFuture<QHash<Action, BindResult>> X11Hotkeys::bindAll(const QHash<Action, QString> &preferred)
{
    QHash<Action, BindResult> result;
    for (auto it = preferred.cbegin(); it != preferred.cend(); ++it)
        result.insert(it.key(), grab(it.key(), it.value()));
    return readyFuture(result);
}

bool X11Hotkeys::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (eventType != "xcb_generic_event_t")
        return false;
    auto *ev = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = ev->response_type & ~0x80;
    if (type != XCB_KEY_PRESS && type != XCB_KEY_RELEASE)
        return false;
    auto *key = reinterpret_cast<xcb_key_press_event_t *>(ev);
    if (key->event != x11RootWindow())
        return false;   // клавиши наших окон — не наше дело
    const uint16_t mods = key->state & kRelevant;
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        Binding &b = it.value();
        if (!b.keycodes.contains(key->detail))
            continue;
        if (type == XCB_KEY_PRESS) {
            if (mods != b.trigger.modifiers || b.down)
                return mods == b.trigger.modifiers;   // автоповтор — съедаем молча
            b.down = true;
            emit activated(it.key(), ActivationContext{});
        } else {
            if (!b.down)
                return false;
            if (isKeyDown(key->detail))
                return true;   // автоповтор: клавиша всё ещё нажата
            b.down = false;
            emit released(it.key());
        }
        return true;
    }
    return false;
}

} // namespace mp::lx
