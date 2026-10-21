#pragma once

#include <QString>

namespace mp::win {

// Key shortcuts for WinAPI. Shared by global hotkeys and input emulation.

struct WinTrigger {
    unsigned vk = 0;          // virtual key code (VK_*)
    unsigned modifiers = 0;   // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN, as for RegisterHotKey
    bool valid() const { return vk != 0; }
};

// "Primary+Alt+V", "Print", "Ctrl+Shift+F12" → key code and modifiers. Primary = Ctrl (spec section 9).
WinTrigger parseTrigger(const QString &portable);

// For people: "Ctrl+Alt+V", not the portable "Primary+Alt+V"
QString displayTrigger(const QString &portable);

// The keyboard state right now (GetAsyncKeyState): works when the program's windows are not focused too
bool isKeyDown(unsigned vk);
bool anyModifierDown();                   // Shift, Ctrl, Alt, Win
bool modifiersDown(unsigned modifiers);      // at least one of the MOD_* is held
bool modifiersDownAll(unsigned modifiers);   // all of the MOD_* are held

} // namespace mp::win
