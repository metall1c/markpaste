#pragma once

#include "platform/Interfaces.h"

namespace mp {

// Capture via QScreen::grabWindow(0). Works on X11 and Windows (through GDI, no HDR),
// does not work on Wayland. A temporary implementation until there are WinCapture and X11Capture
// with MIT-SHM (spec section 9). Main thread only.
class QtScreenCapture : public ICapture
{
    Q_OBJECT
public:
    using ICapture::ICapture;
    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override { return CaptureCap::Silent; }
    QString name() const override { return QStringLiteral("QScreen::grabWindow"); }
};

} // namespace mp
