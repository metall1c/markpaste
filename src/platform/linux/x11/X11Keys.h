#pragma once

#include <QList>
#include <QString>

#include <cstdint>

struct xcb_connection_t;

namespace mp::lx {

// Общее для X11-реализаций: подключение Qt к X-серверу и разбор сочетаний.

xcb_connection_t *x11Connection();   // nullptr, если Qt работает не через xcb
uint32_t x11RootWindow();

struct X11Trigger {
    uint32_t keysym = 0;
    uint16_t modifiers = 0;   // маска X11: Shift, Control, Mod1 (Alt), Mod4 (Super)
    bool valid() const { return keysym != 0; }
};

// «Primary+Alt+V», «Print», «Ctrl+Shift+F12» → keysym и маска. Primary = Ctrl (ТЗ разд. 9).
X11Trigger parseTrigger(const QString &portable);

// Все коды клавиш, которые дают keysym в текущей раскладке (обычно один).
QList<uint8_t> keycodesFor(uint32_t keysym);

// Нажатые сейчас клавиши-модификаторы (Shift, Ctrl, Alt, Super…), по запросу QueryKeymap.
bool anyModifierDown();
bool isKeyDown(uint8_t keycode);

} // namespace mp::lx
