#pragma once

#include "platform/Interfaces.h"

#include <windows.h>

namespace mp::win {

// Ctrl+V emulation via SendInput (spec sections 9, 10). No dialog and no session.
//
// V is pressed by the virtual code VK_V (spec section 10): Windows programs match shortcuts
// by VK, so Ctrl+V pastes with a Russian layout too.
//
// The window active at the start of the paste is remembered: if the user switched to another one,
// the paste stops instead of throwing images anywhere (spec section 10, step 6).
//
// UIPI: SendInput silently does not reach windows started as administrator when the program
// itself runs without elevation, and returns no error. So the window's integrity level
// is checked in advance, and this is reported right away.
class WinInputInjector : public IInputInjector
{
    Q_OBJECT
public:
    explicit WinInputInjector(QObject *parent = nullptr);

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override;
    void end() override;
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    QFuture<Status> sendModifierMask() override;
    bool canDetectForegroundChange() const override { return true; }
    QString name() const override { return QStringLiteral("SendInput"); }

    // The window runs with a higher integrity level than the program (UIPI)
    static bool isElevatedAbove(HWND window);
    static QString processName(HWND window);   // for the log and delay profiles

private:
    HWND m_target = nullptr;
};

} // namespace mp::win
