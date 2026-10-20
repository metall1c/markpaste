#pragma once

#include <QString>

namespace mp::win {

// Сочетания клавиш для WinAPI. Общее для глобальных клавиш и эмуляции ввода.

struct WinTrigger {
    unsigned vk = 0;          // виртуальный код клавиши (VK_*)
    unsigned modifiers = 0;   // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN — как у RegisterHotKey
    bool valid() const { return vk != 0; }
};

// «Primary+Alt+V», «Print», «Ctrl+Shift+F12» → код клавиши и модификаторы. Primary = Ctrl (ТЗ разд. 9).
WinTrigger parseTrigger(const QString &portable);

// Для людей: «Ctrl+Alt+V», а не переносимое «Primary+Alt+V»
QString displayTrigger(const QString &portable);

// Состояние клавиатуры сейчас (GetAsyncKeyState): работает и когда окна программы не в фокусе
bool isKeyDown(unsigned vk);
bool anyModifierDown();                   // Shift, Ctrl, Alt, Win
bool modifiersDown(unsigned modifiers);      // хотя бы один из MOD_* зажат
bool modifiersDownAll(unsigned modifiers);   // зажаты все из MOD_*

} // namespace mp::win
