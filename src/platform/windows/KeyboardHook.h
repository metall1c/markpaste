#pragma once

#include "WinKeys.h"

#include <QList>
#include <QObject>

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

#include <windows.h>

namespace mp::win {

// Low-level keyboard hook WH_KEYBOARD_LL (spec section 9) for keys that
// RegisterHotKey does not receive: on Windows 11 Print Screen is taken by the Snipping Tool.
//
// The hook lives in its own thread with its own message loop. Windows waits for the hook's answer no longer
// than LowLevelHooksTimeout (about 1 s), and after several delays silently removes it. In the main
// thread the hook would wait for image baking and painting, hence a separate thread that
// does nothing but compare key codes.
//
// Presses emulated by the program itself (LLKHF_INJECTED) are skipped: otherwise our own
// Ctrl+V would get caught. A matching shortcut is swallowed, autorepeat included.
// The hook writes nothing to the log.
class KeyboardHook : public QObject
{
    Q_OBJECT
public:
    struct Trigger {
        int id = 0;
        WinTrigger keys;
    };

    explicit KeyboardHook(QObject *parent = nullptr);
    ~KeyboardHook() override;

    // An empty list removes the hook and stops the thread
    void setTriggers(const QList<Trigger> &triggers);
    bool isRunning() const { return m_thread.joinable(); }

signals:
    // From the hook thread; connections to main-thread objects go through the queue
    void pressed(int id);
    void released(int id);

private:
    struct State {
        Trigger trigger;
        bool down = false;
    };
    void start();
    void stop();
    void run(std::promise<bool> *started);
    bool handle(unsigned vk, bool down);   // true means swallow
    static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam);

    std::thread m_thread;
    std::atomic<DWORD> m_threadId{0};
    std::mutex m_mutex;
    QList<State> m_states;
};

} // namespace mp::win
