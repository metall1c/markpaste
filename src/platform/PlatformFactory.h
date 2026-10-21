#pragma once

#include "Interfaces.h"

#include <QStringList>

#include <memory>

namespace mp {

// The set of implementations for the current system. Any pointer may be null:
// the core and the UI must work with what there is (for example, without global hotkeys,
// through the CLI and buttons).
struct Platform {
    std::unique_ptr<ICapture> capture;
    std::unique_ptr<IHotkeys> hotkeys;
    std::unique_ptr<IClipboard> clipboard;
    std::unique_ptr<IInputInjector> injector;

    QString environment;        // "Wayland · KDE", for the wizard and logs
    bool nativeWayland = false;
    bool runningUnderXWayland = false;   // a Wayland session, but Qt runs through xcb (spec section 14)
    bool supportsLayerShell = false;
    QStringList notes;          // what is unavailable and why, for the first-start wizard
};

// Detects the environment at startup (spec section 9) and enables the available mechanisms.
// Call after creating QApplication and before showing windows.
Platform createPlatform(const QString &appId);

} // namespace mp
