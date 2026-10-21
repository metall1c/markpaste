#pragma once

#include "platform/Interfaces.h"

namespace mp::lx {

// Capture via the xdg-desktop-portal Screenshot (interactive=false). Works wherever there is a
// portal, but: the whole desktop comes as one file, the first time with a permission dialog,
// and it is slower than KWin ScreenShot2 (spec section 9, NFR-1).
//
// The frame is cut by monitors. The scale is the ratio of the image width to the desktop
// width in logical coordinates; with monitors of different scales the portal
// returns the frame at one scale, and some monitors are captured below their native resolution.
class PortalScreenshot : public ICapture
{
    Q_OBJECT
public:
    explicit PortalScreenshot(QObject *parent = nullptr);
    static bool isAvailable();

    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override { return CaptureCap::None; }
    QString name() const override { return QStringLiteral("xdg-desktop-portal Screenshot"); }
};

} // namespace mp::lx
