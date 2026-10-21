#include "KeyboardHook.h"

namespace mp::win {

namespace {
// The hook is a global function without context: one instance per process
std::atomic<KeyboardHook *> g_instance{nullptr};
} // namespace

KeyboardHook::KeyboardHook(QObject *parent)
    : QObject(parent)
{
}

KeyboardHook::~KeyboardHook()
{
    stop();
}

void KeyboardHook::setTriggers(const QList<Trigger> &triggers)
{
    {
        std::lock_guard lock(m_mutex);
        m_states.clear();
        for (const Trigger &t : triggers)
            m_states << State{t, false};
    }
    if (triggers.isEmpty())
        stop();
    else if (!isRunning())
        start();
}

void KeyboardHook::start()
{
    KeyboardHook *expected = nullptr;
    if (!g_instance.compare_exchange_strong(expected, this))
        return;   // a second hook in the process is not needed
    std::promise<bool> started;
    std::future<bool> ready = started.get_future();
    m_thread = std::thread([this, &started] { run(&started); });
    if (!ready.get()) {
        m_thread.join();
        g_instance = nullptr;
    }
}

void KeyboardHook::stop()
{
    if (!m_thread.joinable())
        return;
    PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    m_thread.join();
    m_threadId = 0;
    g_instance = nullptr;
}

void KeyboardHook::run(std::promise<bool> *started)
{
    // The thread's message queue appears with the first call of a message function, before PostThreadMessage from stop()
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    m_threadId = GetCurrentThreadId();
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardHook::hookProc, GetModuleHandleW(nullptr), 0);
    started->set_value(hook != nullptr);
    if (!hook)
        return;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnhookWindowsHookEx(hook);
}

bool KeyboardHook::handle(unsigned vk, bool down)
{
    std::lock_guard lock(m_mutex);
    for (State &s : m_states) {
        if (s.trigger.keys.vk != vk)
            continue;
        if (down) {
            // Exactly the modifiers of the shortcut: Ctrl+Print must not fire on Print
            const unsigned all = MOD_SHIFT | MOD_CONTROL | MOD_ALT | MOD_WIN;
            const unsigned want = s.trigger.keys.modifiers;
            if (!s.down && (modifiersDown(all & ~want) || (want && !modifiersDownAll(want))))
                continue;
            if (!s.down) {
                s.down = true;
                emit pressed(s.trigger.id);
            }
            return true;   // autorepeat too
        }
        if (s.down) {
            s.down = false;
            emit released(s.trigger.id);
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK KeyboardHook::hookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION) {
        const auto *k = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        KeyboardHook *self = g_instance.load();
        if (self && !(k->flags & LLKHF_INJECTED)) {
            const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
            if (self->handle(k->vkCode, down))
                return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace mp::win
