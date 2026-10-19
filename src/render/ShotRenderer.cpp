#include "ShotRenderer.h"

#include <QBuffer>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace mp {

// --- Контраст (WCAG 2.x) ---

static double channelToLinear(double c)
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

static double relativeLuminance(const QColor &c)
{
    return 0.2126 * channelToLinear(c.redF()) + 0.7152 * channelToLinear(c.greenF())
         + 0.0722 * channelToLinear(c.blueF());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    double la = relativeLuminance(a);
    double lb = relativeLuminance(b);
    if (la < lb)
        std::swap(la, lb);
    return (la + 0.05) / (lb + 0.05);
}

QColor badgeFillColor(const QColor &accent)
{
    QColor c = accent.toRgb();
    // Затемняем по 2 % к чёрному: оттенок сохраняется, контраст растёт монотонно.
    for (int i = 0; i < 100 && contrastRatio(c, Qt::white) < 4.5; ++i)
        c = QColor::fromRgbF(c.redF() * 0.98f, c.greenF() * 0.98f, c.blueF() * 0.98f);
    return c;
}

// --- Пикселизация ---

void pixelate(QImage &image, const QRect &rect, int block)
{
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_RGB32
        && image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32);

    const QRect area = rect.normalized().intersected(image.rect());
    if (area.isEmpty() || block < 1)
        return;

    for (int by = area.top(); by <= area.bottom(); by += block) {
        const int bh = std::min(block, area.bottom() - by + 1);
        for (int bx = area.left(); bx <= area.right(); bx += block) {
            const int bw = std::min(block, area.right() - bx + 1);
            quint64 a = 0, r = 0, g = 0, b = 0;
            for (int y = by; y < by + bh; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                for (int x = bx; x < bx + bw; ++x) {
                    a += qAlpha(line[x]);
                    r += qRed(line[x]);
                    g += qGreen(line[x]);
                    b += qBlue(line[x]);
                }
            }
            const quint64 n = quint64(bw) * quint64(bh);
            const QRgb avg = qRgba(int(r / n), int(g / n), int(b / n), int(a / n));
            for (int y = by; y < by + bh; ++y) {
                QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
                std::fill(line + bx, line + bx + bw, avg);
            }
        }
    }
}

// --- Метки ---

static void drawBadge(QPainter &p, const QString &label, QPointF anchor, const QRectF &bounds,
                      const RenderStyle &style, qreal dpr)
{
    const qreal d = style.badgeDiameter * dpr;
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(std::max(1, int(std::round(d * 0.46))));
    const QFontMetricsF fm(font);
    const qreal w = std::max(d, fm.horizontalAdvance(label) + d * 0.5);

    // Центр значка — в точке привязки метки, но значок целиком внутри снимка (EDT-3).
    QRectF r(anchor.x() - d / 2, anchor.y() - d / 2, w, d);
    if (r.right() > bounds.right())
        r.moveRight(bounds.right());
    if (r.bottom() > bounds.bottom())
        r.moveBottom(bounds.bottom());
    if (r.left() < bounds.left())
        r.moveLeft(bounds.left());
    if (r.top() < bounds.top())
        r.moveTop(bounds.top());

    QPainterPath path;
    path.addRoundedRect(r, d / 2, d / 2);
    p.setPen(QPen(Qt::white, style.outlineWidth * dpr));
    p.setBrush(badgeFillColor(style.accent));
    p.drawPath(path);

    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, label);
}

static void drawRectMark(QPainter &p, const Mark &m, const QColor &accent, qreal fw, qreal ow)
{
    const QRectF r = QRectF(m.rect).adjusted(fw / 2, fw / 2, -fw / 2, -fw / 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, fw + 2 * ow, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.drawRect(r);
    p.setPen(QPen(accent, fw, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.drawRect(r);
}

// Точки — центры пикселей: линия толщиной в 1 px через (x, y) закрашивает ровно этот пиксель.
static QPointF pixelCenter(const QPoint &p)
{
    return QPointF(p) + QPointF(0.5, 0.5);
}

static void drawArrowMark(QPainter &p, const Mark &m, const QColor &accent, qreal w, qreal ow)
{
    if (m.points.size() != 2)
        return;
    const QPointF tail = pixelCenter(m.points[0]);
    const QPointF tip = pixelCenter(m.points[1]);
    const QLineF line(tail, tip);
    const qreal length = line.length();
    if (length <= 0)
        return;
    const QPointF dir = (tip - tail) / length;
    const QPointF normal(-dir.y(), dir.x());

    // Наконечник в 4 толщины древка; на короткой стрелке — не длиннее 2/3 её длины
    const qreal headLength = std::min(4.0 * w, length * 2.0 / 3.0);
    const qreal headHalf = headLength * 0.55;
    const QPointF base = tip - dir * headLength;
    const QPolygonF head{tip, base + normal * headHalf, base - normal * headHalf};
    // Древко заходит в наконечник, чтобы между ними не было щели, но не торчит из острия
    const QPointF shaftEnd = base + dir * (headLength * 0.3);

    p.setPen(QPen(Qt::white, w + 2 * ow, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(tail, shaftEnd);
    p.setPen(QPen(Qt::white, 2 * ow, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::white);
    p.drawPolygon(head);

    p.setPen(QPen(accent, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(tail, shaftEnd);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawPolygon(head);
}

// Сглаживание: кривая проходит через середины звеньев, вершины ломаной — контрольные точки.
// После упрощения (simplifyPolyline) точек мало, и без сглаживания линия выглядит угловатой.
static QPainterPath smoothPath(const QList<QPoint> &points)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;
    path.moveTo(pixelCenter(points[0]));
    if (points.size() == 2) {
        path.lineTo(pixelCenter(points[1]));
        return path;
    }
    for (int i = 1; i + 1 < points.size(); ++i) {
        const QPointF c = pixelCenter(points[i]);
        const QPointF mid = (c + pixelCenter(points[i + 1])) / 2;
        path.quadTo(c, mid);
    }
    path.lineTo(pixelCenter(points.last()));
    return path;
}

static void drawFreehandMark(QPainter &p, const Mark &m, const QColor &accent, qreal fw, qreal ow)
{
    const QPainterPath path = smoothPath(m.points);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, fw + 2 * ow, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.setPen(QPen(accent, fw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

void paintMarks(QPainter &p, const Shot &shot, int shotIndex, const RenderStyle &style, const QRectF &bounds)
{
    const qreal dpr = shot.devicePixelRatio > 0 ? shot.devicePixelRatio : 1.0;
    const qreal fw = style.frameWidth * dpr;
    const qreal aw = style.arrowWidth * dpr;
    const qreal ow = style.outlineWidth * dpr;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    // Сначала все формы, потом все значки: значок не перекрывается чужой рамкой.
    for (const Mark &m : shot.marks) {
        switch (m.shape) {
        case MarkShape::Rect:
            drawRectMark(p, m, style.accent, fw, ow);
            break;
        case MarkShape::Arrow:
            drawArrowMark(p, m, style.accent, aw, ow);
            break;
        case MarkShape::Freehand:
            drawFreehandMark(p, m, style.accent, fw, ow);
            break;
        }
    }
    // Значок рамки — в её левом верхнем углу, стрелки — у хвоста, карандаша — у первой точки
    for (int i = 0; i < shot.marks.size(); ++i)
        drawBadge(p, markLabel(shotIndex, i), shot.marks[i].badgeAnchor(), bounds, style, dpr);
    p.restore();
}

QImage sourceWithRedactions(const Shot &shot, const RenderStyle &style)
{
    QImage source;
    if (!source.loadFromData(shot.sourcePng, "PNG"))
        return {};
    source = source.convertToFormat(QImage::Format_ARGB32);
    const qreal dpr = shot.devicePixelRatio > 0 ? shot.devicePixelRatio : 1.0;
    const int block = std::max(1, int(std::round(style.pixelBlock * dpr)));
    for (const Redaction &red : shot.redactions)
        pixelate(source, red.rect, block);
    return source;
}

QImage renderShot(const Shot &shot, int shotIndex, const RenderStyle &style)
{
    // Размытие — до обрезки и меток, в координатах исходника.
    const QImage source = sourceWithRedactions(shot, style);
    if (source.isNull())
        return {};

    const QRect visible = shot.visibleRect().intersected(source.rect());
    QImage out = source.copy(visible);

    QPainter p(&out);
    p.translate(-visible.topLeft());
    paintMarks(p, shot, shotIndex, style, QRectF(visible));
    p.end();
    out.setDevicePixelRatio(1.0);  // наружу отдаём физические пиксели
    return out;
}

// --- Склейка ---

QImage composeVertical(const QList<QImage> &images, const QList<QString> &labels, qreal dpr)
{
    if (images.isEmpty())
        return {};
    const int gap = int(std::round(16 * dpr));
    const int labelH = int(std::round(28 * dpr));

    int width = 0;
    int height = gap;
    for (const QImage &img : images) {
        width = std::max(width, img.width());
        height += labelH + img.height() + gap;
    }
    width += 2 * gap;

    QImage out(width, height, QImage::Format_ARGB32);
    out.fill(Qt::white);
    QPainter p(&out);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(std::max(1, int(std::round(18 * dpr))));
    p.setFont(font);
    p.setPen(QColor(0x20, 0x20, 0x20));

    int y = gap;
    for (int i = 0; i < images.size(); ++i) {
        const QString label = i < labels.size() ? labels[i] : QString();
        p.drawText(QRect(gap, y, width - 2 * gap, labelH), Qt::AlignLeft | Qt::AlignVCenter, label);
        y += labelH;
        p.drawImage(QPoint(gap, y), images[i]);
        y += images[i].height() + gap;
    }
    return out;
}

// --- Кодирование ---

EncodedImage encodeForPaste(QImage image, const EncodeOptions &options)
{
    EncodedImage result;
    if (image.isNull())
        return result;

    const int longSide = std::max(image.width(), image.height());
    if (options.maxLongSide > 0 && longSide > options.maxLongSide)
        image = image.scaled(QSize(options.maxLongSide, options.maxLongSide), Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);

    QBuffer buf(&result.data);
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, "PNG");
    buf.close();
    result.mimeType = "image/png";
    result.extension = QStringLiteral("png");

    if (options.jpegThresholdBytes > 0 && result.data.size() > options.jpegThresholdBytes) {
        result.data.clear();
        QBuffer jbuf(&result.data);
        jbuf.open(QIODevice::WriteOnly);
        image.convertToFormat(QImage::Format_RGB888).save(&jbuf, "JPEG", options.jpegQuality);
        result.mimeType = "image/jpeg";
        result.extension = QStringLiteral("jpg");
    }
    result.size = image.size();
    return result;
}

} // namespace mp
