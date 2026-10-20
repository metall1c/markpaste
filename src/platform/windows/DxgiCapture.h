#pragma once

#include "platform/Interfaces.h"

#include <QRect>

namespace mp::win {

// Снимок всех мониторов на Windows (ТЗ разд. 9): DXGI Desktop Duplication, а для монитора,
// который она не отдала, — GDI BitBlt. Без вспышки и без рамки, курсор в снимок не попадает.
//
// Почему DXGI, а не Windows.Graphics.Capture: WGC на Windows 10 рисует жёлтую рамку вокруг
// захвата, а убрать её можно только в Windows 11; для WinRT нужен C++/WinRT, которого нет в
// MinGW. DXGI работает на обеих системах (docs/decisions/0016-*.md).
//
// HDR: просим у DuplicateOutput1 только 8-битный BGRA — тогда система сама переводит HDR-
// рабочий стол в SDR. GDI на HDR-мониторе даёт выцветшую картинку, поэтому он — только запасной.
//
// Снимок идёт в пуле потоков: создание устройства D3D11 и копирование 4K-кадров не держат
// интерфейс. Данные о мониторах (QScreen) берутся заранее в главном потоке.
class DxgiCapture : public ICapture
{
    Q_OBJECT
public:
    // useDxgi = false — только GDI (MARKPASTE_CAPTURE=gdi: драйвер или Wine с DXGI не справляются)
    explicit DxgiCapture(bool useDxgi = true, QObject *parent = nullptr)
        : ICapture(parent), m_useDxgi(useDxgi)
    {
    }

    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override { return CaptureCap::Silent; }
    QString name() const override
    {
        return m_useDxgi ? QStringLiteral("DXGI Desktop Duplication") : QStringLiteral("GDI BitBlt");
    }

    struct ScreenInfo {
        QString name;    // QScreen::name()
        QRect logical;   // QScreen::geometry()
        qreal scale = 1.0;
    };
    static CaptureResult grab(const QList<ScreenInfo> &screens, bool useDxgi = true);
    // GDI BitBlt прямоугольника рабочего стола в физических пикселях
    static QImage gdiGrab(const QRect &physical);

private:
    bool m_useDxgi;
};

} // namespace mp::win
