#include "WinHotkeys.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWinKeys, "markpaste.win.hotkeys")

namespace mp::win {

namespace {

const wchar_t kWindowClass[] = L"MarkpasteHotkeys";

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY) {
        if (auto *self = reinterpret_cast<WinHotkeys *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)))
            self->onHotkey(int(wParam));
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int idFor(Action a)
{
    return 1 + int(a);   // RegisterHotKey ids are 0 to 0xBFFF, each action has its own
}

// RegisterHotKey would take Ctrl+V from every program, so it goes through the hook, conditionally
bool isPlainPaste(const WinTrigger &t)
{
    return t.vk == 'V' && t.modifiers == MOD_CONTROL;
}

} // namespace

WinHotkeys::WinHotkeys(QObject *parent)
    : IHotkeys(parent)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);   // repeated registration is not an error for us
    // A message-only window (HWND_MESSAGE): it is invisible, and Qt's loop dispatches WM_HOTKEY
    m_window = CreateWindowExW(0, kWindowClass, L"Markpaste hotkeys", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                               wc.hInstance, nullptr);
    if (m_window)
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    else
        qCWarning(lcWinKeys) << "CreateWindowEx failed:" << GetLastError();

    m_releasePoll.setInterval(20);
    connect(&m_releasePoll, &QTimer::timeout, this, &WinHotkeys::pollReleases);
    connect(&m_hook, &KeyboardHook::pressed, this, [this](int id) { activate(id, false); });
    connect(&m_hook, &KeyboardHook::released, this, [this](int id) {
        if (const auto a = actionFor(id))
            emit released(*a);
    });
}

WinHotkeys::~WinHotkeys()
{
    for (const Binding &b : std::as_const(m_bindings))
        ungrab(b);
    m_hook.setTriggers({});
    if (m_window)
        DestroyWindow(m_window);
}

std::optional<Action> WinHotkeys::actionFor(int id) const
{
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it)
        if (it->id == id)
            return it.key();
    return std::nullopt;
}

void WinHotkeys::ungrab(const Binding &b)
{
    if (!b.viaHook && m_window)
        UnregisterHotKey(m_window, b.id);
}

void WinHotkeys::updateHook()
{
    QList<KeyboardHook::Trigger> triggers;
    for (const Binding &b : std::as_const(m_bindings))
        if (b.viaHook)
            triggers << KeyboardHook::Trigger{b.id, b.trigger, isPlainPaste(b.trigger)};
    m_hook.setTriggers(triggers);
}

BindResult WinHotkeys::grab(Action action, const QString &preferred)
{
    if (m_bindings.contains(action))
        ungrab(m_bindings.take(action));
    if (preferred.trimmed().isEmpty())
        return {Status::success(), QString()};

    Binding b;
    b.trigger = parseTrigger(preferred);
    b.id = idFor(action);
    if (!b.trigger.valid())
        return {Status::failure(QStringLiteral("unsupported key: %1").arg(preferred)), QString()};

    b.viaHook = b.trigger.vk == VK_SNAPSHOT || isPlainPaste(b.trigger);
    if (!b.viaHook) {
        if (!m_window)
            return {Status::failure(QStringLiteral("no message window for hotkeys")), QString()};
        if (!RegisterHotKey(m_window, b.id, b.trigger.modifiers | MOD_NOREPEAT, b.trigger.vk)) {
            const DWORD err = GetLastError();
            if (err == ERROR_HOTKEY_ALREADY_REGISTERED)
                return {Status::failure(QStringLiteral("%1 is already used by another program").arg(displayTrigger(preferred))),
                        QString()};
            return {Status::failure(QStringLiteral("RegisterHotKey failed: error %1").arg(err)), QString()};
        }
    }
    m_bindings.insert(action, b);
    if (b.viaHook) {
        updateHook();
        if (!m_hook.isRunning()) {
            m_bindings.remove(action);
            return {Status::failure(QStringLiteral("keyboard hook is not available")), QString()};
        }
    }
    qCInfo(lcWinKeys) << "bound" << preferred << (b.viaHook ? "via WH_KEYBOARD_LL" : "via RegisterHotKey");
    return {Status::success(), displayTrigger(preferred)};
}

QFuture<BindResult> WinHotkeys::bind(Action action, const QString &preferred)
{
    const BindResult r = grab(action, preferred);
    updateHook();
    return readyFuture(r);
}

QFuture<QHash<Action, BindResult>> WinHotkeys::bindAll(const QHash<Action, QString> &preferred)
{
    QHash<Action, BindResult> result;
    for (auto it = preferred.cbegin(); it != preferred.cend(); ++it)
        result.insert(it.key(), grab(it.key(), it.value()));
    updateHook();
    return readyFuture(result);
}

void WinHotkeys::onHotkey(int id)
{
    activate(id, true);
}

void WinHotkeys::activate(int id, bool watchRelease)
{
    const auto action = actionFor(id);
    if (!action)
        return;
    if (watchRelease) {
        m_bindings[*action].waitingRelease = true;
        m_releasePoll.start();
    }
    emit activated(*action, ActivationContext{});
}

void WinHotkeys::pollReleases()
{
    bool waiting = false;
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        Binding &b = it.value();
        if (!b.waitingRelease)
            continue;
        if (isKeyDown(b.trigger.vk) || modifiersDown(b.trigger.modifiers)) {
            waiting = true;
            continue;
        }
        b.waitingRelease = false;
        emit released(it.key());
    }
    if (!waiting)
        m_releasePoll.stop();
}

} // namespace mp::win
