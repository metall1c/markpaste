#pragma once

#include "platform/Interfaces.h"

namespace mp::lx {

// Fast capture on KDE via org.kde.KWin.ScreenShot2 (spec section 9).
//
// No dialogs and no flash, each monitor separately and in physical pixels
// (native-resolution), so mixed monitor scales do not spoil the image (CAP-6).
// KWin lets in only "trusted" programs: the .desktop file must contain
// X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2, and the Exec path must match
// the process executable. Otherwise the call returns a NoAuthorized error.
class KWinScreenshot : public ICapture
{
    Q_OBJECT
public:
    explicit KWinScreenshot(QObject *parent = nullptr);

    // The interface is on the bus. Permissions are checked only at call time.
    static bool isPresent();

    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override { return CaptureCap::Silent; }
    QString name() const override { return QStringLiteral("KWin ScreenShot2"); }
};

} // namespace mp::lx
