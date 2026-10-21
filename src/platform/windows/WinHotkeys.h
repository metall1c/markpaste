#pragma once

#include "KeyboardHook.h"
#include "WinKeys.h"
#include "platform/Interfaces.h"

#include <QTimer>

#include <optional>

#include <windows.h>

namespace mp::win {

// Global hotkeys on Windows (spec section 9).
//
// Ordinary shortcuts use RegisterHotKey on a hidden message window: the system itself handles
// modifiers, sends no autorepeat (MOD_NOREPEAT) and reports if a shortcut is taken.
// Print Screen goes through the WH_KEYBOARD_LL hook (KeyboardHook): on Windows 11 the key is taken by
// the Snipping Tool, and RegisterHotKey does not receive it.
//
// RegisterHotKey does not report the release. The paste waits for it (release Alt before
// pressing Ctrl+V), so after the hotkey fires we poll the keyboard until the key and the
// shortcut's modifiers are released.
class WinHotkeys : public IHotkeys
{
    Q_OBJECT
public:
    explicit WinHotkeys(QObject *parent = nullptr);
    ~WinHotkeys() override;

    QFuture<BindResult> bind(Action action, const QString &preferred) override;
    QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) override;
    QString name() const override { return QStringLiteral("RegisterHotKey"); }

    // For the message window
    void onHotkey(int id);

private:
    struct Binding {
        WinTrigger trigger;
        int id = 0;
        bool viaHook = false;
        bool waitingRelease = false;
    };
    BindResult grab(Action action, const QString &preferred);
    void ungrab(const Binding &b);
    void updateHook();
    void pollReleases();
    void activate(int id, bool watchRelease);
    std::optional<Action> actionFor(int id) const;

    HWND m_window = nullptr;
    QHash<Action, Binding> m_bindings;
    KeyboardHook m_hook;
    QTimer m_releasePoll;
};

} // namespace mp::win
