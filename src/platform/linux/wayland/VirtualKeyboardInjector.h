#pragma once

#include "platform/Interfaces.h"

struct zwp_virtual_keyboard_manager_v1;
struct zwp_virtual_keyboard_v1;

namespace mp::lx {

// Эмуляция Ctrl+V через zwp_virtual_keyboard_v1 (ТЗ разд. 9): Sway, Hyprland и другие
// композиторы на wlroots. Там нет портала RemoteDesktop, зато протокол доступен любому
// клиенту без диалога. В KWin и Mutter протокола нет.
//
// Клавиатуре нужна своя раскладка (keymap). Берём US из libxkbcommon: клавиша V с кодом
// KEY_V даёт «v» независимо от раскладки пользователя (ТЗ разд. 10). Композитор на время
// наших нажатий отдаёт окну нашу раскладку; следующее нажатие настоящей клавиатуры
// возвращает её раскладку (так же работает wtype).
//
// Запросы протокола уходят из потока WaylandConnection (там живёт подключение).
class VirtualKeyboardInjector : public IInputInjector
{
    Q_OBJECT
public:
    // nullptr, если композитор не даёт zwp_virtual_keyboard_manager_v1.
    static VirtualKeyboardInjector *create(QObject *parent = nullptr);
    ~VirtualKeyboardInjector() override;

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override;
    void end() override {}
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("virtual-keyboard-v1"); }

private:
    VirtualKeyboardInjector(QObject *parent);
    Status setup();                  // в потоке Wayland
    void key(int evdevCode, bool pressed);
    void modifiers(bool ctrl);

    zwp_virtual_keyboard_manager_v1 *m_manager = nullptr;
    zwp_virtual_keyboard_v1 *m_keyboard = nullptr;
    uint32_t m_ctrlMask = 0;
};

} // namespace mp::lx
