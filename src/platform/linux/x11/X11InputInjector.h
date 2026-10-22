#pragma once

#include "platform/Interfaces.h"

namespace mp::lx {

// Ctrl+V emulation on X11 via the XTEST extension (spec section 9). No dialog and no session.
// V is pressed by the code of the key that has "v" in the layout (spec section 10): with a Russian
// layout Qt and GTK on X11 map Ctrl+М to Ctrl+V by the key's Latin variant themselves.
class X11InputInjector : public IInputInjector
{
    Q_OBJECT
public:
    // nullptr if Qt does not run through xcb or the server has no XTEST
    static X11InputInjector *create(QObject *parent = nullptr);

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override { return readyFuture(Status::success()); }
    void end() override {}
    // On X11 the keyboard state is visible (QueryKeymap): wait until the modifiers are released
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    QFuture<Status> sendModifierMask() override;
    bool maskAddsCtrl() const override { return true; }
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("X11 XTEST"); }

private:
    using IInputInjector::IInputInjector;
    void key(uint8_t keycode, bool pressed);

    uint8_t m_ctrl = 0;
    uint8_t m_v = 0;
};

} // namespace mp::lx
