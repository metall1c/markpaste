#include "VirtualKeyboardInjector.h"

#include "WaylandConnection.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QTimer>

#include <xkbcommon/xkbcommon.h>

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <memory>

Q_DECLARE_LOGGING_CATEGORY(lcWayland)

namespace mp::lx {

namespace {
constexpr int kKeyLeftCtrl = 29;   // evdev, linux/input-event-codes.h
constexpr int kKeyV = 47;
constexpr uint32_t kKeymapFormatXkbV1 = 1;   // wl_keyboard.keymap_format.xkb_v1

uint32_t nowMs()
{
    static QElapsedTimer clock = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return uint32_t(clock.elapsed());
}

// Выполнить в потоке подключения и дождаться (запросы — быстрые, без roundtrip)
template <typename Fn>
void inWaylandThread(Fn fn)
{
    WaylandConnection *conn = WaylandConnection::instance();
    QMetaObject::invokeMethod(conn, [conn, fn] {
        fn();
        conn->flush();
    }, Qt::BlockingQueuedConnection);
}
} // namespace

VirtualKeyboardInjector *VirtualKeyboardInjector::create(QObject *parent)
{
    WaylandConnection *conn = WaylandConnection::instance();
    if (!conn || !conn->seat() || !conn->hasGlobal(zwp_virtual_keyboard_manager_v1_interface.name))
        return nullptr;
    return new VirtualKeyboardInjector(parent);
}

VirtualKeyboardInjector::VirtualKeyboardInjector(QObject *parent)
    : IInputInjector(parent)
{
}

VirtualKeyboardInjector::~VirtualKeyboardInjector()
{
    if (!m_keyboard && !m_manager)
        return;
    inWaylandThread([this] {
        if (m_keyboard)
            zwp_virtual_keyboard_v1_destroy(m_keyboard);
        if (m_manager)
            zwp_virtual_keyboard_manager_v1_destroy(m_manager);
    });
}

Status VirtualKeyboardInjector::setup()
{
    WaylandConnection *conn = WaylandConnection::instance();
    m_manager = static_cast<zwp_virtual_keyboard_manager_v1 *>(
        conn->bind(&zwp_virtual_keyboard_manager_v1_interface, 1));
    if (!m_manager)
        return Status::failure(QStringLiteral("cannot bind zwp_virtual_keyboard_manager_v1"));

    // Раскладка US: KEY_V всегда «v», и Ctrl+V распознаётся при любой раскладке пользователя
    std::unique_ptr<xkb_context, decltype(&xkb_context_unref)> ctx(xkb_context_new(XKB_CONTEXT_NO_FLAGS),
                                                                  xkb_context_unref);
    const xkb_rule_names names{"evdev", "pc105", "us", "", ""};
    std::unique_ptr<xkb_keymap, decltype(&xkb_keymap_unref)> keymap(
        ctx ? xkb_keymap_new_from_names(ctx.get(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : nullptr, xkb_keymap_unref);
    if (!keymap)
        return Status::failure(QStringLiteral("cannot compile the XKB keymap"));
    const xkb_mod_index_t ctrl = xkb_keymap_mod_get_index(keymap.get(), XKB_MOD_NAME_CTRL);
    m_ctrlMask = ctrl == XKB_MOD_INVALID ? 0 : (1u << ctrl);

    std::unique_ptr<char, decltype(&free)> text(xkb_keymap_get_as_string(keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1), free);
    const size_t size = std::strlen(text.get()) + 1;   // с завершающим нулём: так ждёт композитор
    const int fd = memfd_create("markpaste-keymap", MFD_CLOEXEC);
    if (fd < 0 || ::write(fd, text.get(), size) != ssize_t(size)) {
        if (fd >= 0)
            ::close(fd);
        return Status::failure(QStringLiteral("cannot pass the keymap to the compositor"));
    }

    m_keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(m_manager, conn->seat());
    zwp_virtual_keyboard_v1_keymap(m_keyboard, kKeymapFormatXkbV1, fd, uint32_t(size));
    ::close(fd);   // libwayland передал копию
    // Композитор, не доверяющий клиенту, отвечает ошибкой протокола unauthorized и рвёт
    // подключение; roundtrip показывает это сразу, а не на первой вставке.
    if (!conn->roundtrip())
        return Status::failure(QStringLiteral("the compositor refused the virtual keyboard"));
    return Status::success();
}

QFuture<Status> VirtualKeyboardInjector::begin()
{
    if (m_keyboard)
        return readyFuture(Status::success());
    Status result;
    inWaylandThread([this, &result] { result = setup(); });
    if (!result.ok)
        qCWarning(lcWayland) << "virtual keyboard:" << result.error;
    return readyFuture(result);
}

QFuture<Status> VirtualKeyboardInjector::waitModifiersReleased(std::chrono::milliseconds timeout)
{
    // Состояние чужой клавиатуры без фокуса не узнать: просто ждём, как и с порталом
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    QTimer::singleShot(int(timeout.count()), this, [promise] {
        promise->addResult(Status::success());
        promise->finish();
    });
    return promise->future();
}

void VirtualKeyboardInjector::key(int evdevCode, bool pressed)
{
    inWaylandThread([this, evdevCode, pressed] {
        zwp_virtual_keyboard_v1_key(m_keyboard, nowMs(), uint32_t(evdevCode),
                                    pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
    });
}

void VirtualKeyboardInjector::modifiers(bool ctrl)
{
    inWaylandThread([this, ctrl] {
        zwp_virtual_keyboard_v1_modifiers(m_keyboard, ctrl ? m_ctrlMask : 0, 0, 0, 0);
    });
}

QFuture<Status> VirtualKeyboardInjector::sendModifierMask()
{
    if (!m_keyboard)
        return readyFuture(Status::failure(QStringLiteral("virtual keyboard is not set up")));
    key(kKeyLeftCtrl, true);
    modifiers(true);
    key(kKeyLeftCtrl, false);
    modifiers(false);
    return readyFuture(Status::success());
}

QFuture<Status> VirtualKeyboardInjector::sendPaste()
{
    if (!m_keyboard)
        return readyFuture(Status::failure(QStringLiteral("virtual keyboard is not set up")));

    // Ctrl↓ (+модификаторы) V↓ V↑ Ctrl↑ (−модификаторы) с паузами, как и через портал:
    // часть клиентов не успевает обработать модификатор в том же кадре.
    const std::function<void()> steps[] = {
        [this] { key(kKeyLeftCtrl, true); modifiers(true); },
        [this] { key(kKeyV, true); },
        [this] { key(kKeyV, false); },
        [this] { key(kKeyLeftCtrl, false); modifiers(false); },
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
