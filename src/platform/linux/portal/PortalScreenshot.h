#pragma once

#include "platform/Interfaces.h"

namespace mp::lx {

// Захват через xdg-desktop-portal Screenshot (interactive=false). Работает везде, где есть
// портал, но: снимок всего рабочего стола одним файлом, первый раз — диалог разрешения,
// и он медленнее KWin ScreenShot2 (ТЗ разд. 9, NFR-1).
//
// Кадр режется по мониторам. Масштаб берётся как отношение ширины картинки к ширине
// рабочего стола в логических координатах; при мониторах с разным масштабом портал
// отдаёт кадр в одном масштабе, и часть мониторов будет снята не в родном разрешении.
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
