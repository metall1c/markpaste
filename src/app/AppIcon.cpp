#include "AppIcon.h"

#include "render/ShotRenderer.h"

#include <QPainter>
#include <QPainterPath>

namespace mp {

static QPixmap drawIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = size;
    const QColor accent(0xFF, 0x5A, 0x1F);

    // Рамка выделения
    QPen frame(accent, s * 0.09, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    p.setPen(frame);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(s * 0.18, s * 0.22, s * 0.72, s * 0.62), s * 0.06, s * 0.06);

    // Значок метки в углу
    const QRectF badge(s * 0.04, s * 0.06, s * 0.42, s * 0.42);
    p.setPen(QPen(Qt::white, s * 0.035));
    p.setBrush(badgeFillColor(accent));
    p.drawEllipse(badge);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(std::max(1, int(s * 0.22)));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(badge, Qt::AlignCenter, QStringLiteral("A1"));
    return pm;
}

QIcon appIcon()
{
    static const QIcon icon = [] {
        QIcon i;
        for (int sz : {16, 22, 24, 32, 48, 64, 128, 256})
            i.addPixmap(drawIcon(sz));
        return i;
    }();
    return icon;
}

} // namespace mp
