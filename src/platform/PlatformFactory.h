#pragma once

#include "Interfaces.h"

#include <QStringList>

#include <memory>

namespace mp {

// Набор реализаций для текущей системы. Любой указатель может быть пустым:
// ядро и UI обязаны работать с тем, что есть (например, без глобальных клавиш —
// через CLI и кнопки).
struct Platform {
    std::unique_ptr<ICapture> capture;
    std::unique_ptr<IHotkeys> hotkeys;
    std::unique_ptr<IClipboard> clipboard;
    std::unique_ptr<IInputInjector> injector;

    QString environment;        // «Wayland · KDE» — для мастера и логов
    bool nativeWayland = false;
    bool runningUnderXWayland = false;   // сессия Wayland, а Qt запущен через xcb (ТЗ разд. 14)
    bool supportsLayerShell = false;
    QStringList notes;          // что недоступно и почему — для мастера первого запуска
};

// Определяет окружение при старте (ТЗ разд. 9) и включает доступные механизмы.
// Вызывать после создания QApplication и до показа окон.
Platform createPlatform(const QString &appId);

} // namespace mp
