#pragma once

#include "platform/Interfaces.h"

namespace mp {

// Захват через QScreen::grabWindow(0). Работает на X11 и Windows (через GDI, без HDR),
// на Wayland не работает. Временная реализация, пока нет WinCapture и X11Capture
// с MIT-SHM (ТЗ разд. 9). Только главный поток.
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
