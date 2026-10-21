#pragma once

#include "platform/Interfaces.h"

#include <QRect>

namespace mp::win {

// Capturing all monitors on Windows (spec section 9): DXGI Desktop Duplication, and for a monitor
// it did not provide, GDI BitBlt. No flash and no frame; the cursor does not get into the shot.
//
// Why DXGI and not Windows.Graphics.Capture: on Windows 10 WGC draws a yellow border around
// the capture, and it can be removed only on Windows 11; WinRT needs C++/WinRT, which MinGW
// does not have. DXGI works on both systems (docs/decisions/0016-*.md).
//
// HDR: we ask DuplicateOutput1 for 8-bit BGRA only, and the system itself converts the HDR
// desktop to SDR. GDI on an HDR monitor gives a washed-out image, so it is only a fallback.
//
// The capture runs in the thread pool: creating the D3D11 device and copying 4K frames do not block
// the UI. Monitor data (QScreen) is taken beforehand in the main thread.
class DxgiCapture : public ICapture
{
    Q_OBJECT
public:
    // useDxgi = false means GDI only (MARKPASTE_CAPTURE=gdi: the driver or Wine cannot cope with DXGI)
    explicit DxgiCapture(bool useDxgi = true, QObject *parent = nullptr)
        : ICapture(parent), m_useDxgi(useDxgi)
    {
    }

    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override { return CaptureCap::Silent; }
    QString name() const override;

    struct ScreenInfo {
        QString name;    // QScreen::name()
        QRect logical;   // QScreen::geometry()
        qreal scale = 1.0;
    };
    static CaptureResult grab(const QList<ScreenInfo> &screens, bool useDxgi = true);
    // GDI BitBlt of a desktop rectangle in physical pixels
    static QImage gdiGrab(const QRect &physical);

private:
    bool m_useDxgi;
};

} // namespace mp::win
