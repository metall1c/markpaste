#pragma once

#include "platform/Interfaces.h"

struct zwp_virtual_keyboard_manager_v1;
struct zwp_virtual_keyboard_v1;

namespace mp::lx {

// Ctrl+V emulation via zwp_virtual_keyboard_v1 (spec section 9): Sway, Hyprland and other
// wlroots compositors. They have no RemoteDesktop portal, but the protocol is available to any
// client without a dialog. KWin and Mutter do not have the protocol.
//
// The keyboard needs its own keymap. We take US from libxkbcommon: the V key with code
// KEY_V gives "v" regardless of the user's layout (spec section 10). For the duration of
// our presses the compositor gives the window our keymap; the next press on the real keyboard
// brings its keymap back (wtype works the same way).
//
// Protocol requests go out from the WaylandConnection thread (the connection lives there).
class VirtualKeyboardInjector : public IInputInjector
{
    Q_OBJECT
public:
    // nullptr if the compositor does not provide zwp_virtual_keyboard_manager_v1.
    static VirtualKeyboardInjector *create(QObject *parent = nullptr);
    ~VirtualKeyboardInjector() override;

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override;
    void end() override {}
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    QFuture<Status> sendModifierMask() override;
    bool maskAddsCtrl() const override { return true; }
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("virtual-keyboard-v1"); }

private:
    VirtualKeyboardInjector(QObject *parent);
    Status setup();                  // in the Wayland thread
    void key(int evdevCode, bool pressed);
    void modifiers(bool ctrl);

    zwp_virtual_keyboard_manager_v1 *m_manager = nullptr;
    zwp_virtual_keyboard_v1 *m_keyboard = nullptr;
    uint32_t m_ctrlMask = 0;
};

} // namespace mp::lx
