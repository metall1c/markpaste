#pragma once

#include <QList>
#include <QString>

#include <cstdint>

struct xcb_connection_t;

namespace mp::lx {

// Shared by the X11 implementations: Qt's connection to the X server and shortcut parsing.

xcb_connection_t *x11Connection();   // nullptr if Qt does not run through xcb
uint32_t x11RootWindow();

struct X11Trigger {
    uint32_t keysym = 0;
    uint16_t modifiers = 0;   // X11 mask: Shift, Control, Mod1 (Alt), Mod4 (Super)
    bool valid() const { return keysym != 0; }
};

// "Primary+Alt+V", "Print", "Ctrl+Shift+F12" → keysym and mask. Primary = Ctrl (spec section 9).
X11Trigger parseTrigger(const QString &portable);

// All key codes that produce the keysym in the current layout (usually one).
QList<uint8_t> keycodesFor(uint32_t keysym);

// Modifier keys pressed right now (Shift, Ctrl, Alt, Super…), via QueryKeymap.
bool anyModifierDown();
bool isKeyDown(uint8_t keycode);

} // namespace mp::lx
