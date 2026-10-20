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

static QRectF drawBadge(QPainter &p, const QString &label, QPointF anchor, const QRectF &bounds, const QColor &color,
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
    p.setBrush(badgeFillColor(color));
    p.drawPath(path);

    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, label);
    return r;
}

qreal defaultMarkWidth(MarkShape shape, const RenderStyle &style)
{
    return shape == MarkShape::Arrow ? style.arrowWidth : style.frameWidth;
}

QColor markColor(const Mark &mark, const RenderStyle &style)
{
    return mark.pen.color.isValid() ? mark.pen.color : style.accent;
}

qreal markWidth(const Mark &mark, const RenderStyle &style)
{
    return mark.pen.width > 0 ? mark.pen.width : defaultMarkWidth(mark.shape, style);
}

// Перо линии метки. Штрих задаётся в абсолютных пикселях от толщины цветной линии w,
// а не в долях толщины пера: у белой подложки перо шире, и при штрихе «в толщинах»
// её штрихи разъехались бы с цветными.
static QPen strokePen(const QColor &color, qreal penWidth, qreal w, LineStyle line, Qt::PenCapStyle solidCap,
                      Qt::PenJoinStyle join)
{
    QPen pen(color, penWidth, Qt::SolidLine, solidCap, join);
    if (line == LineStyle::Solid)
        return pen;
    if (line == LineStyle::Dash) {
        pen.setCapStyle(Qt::FlatCap);
        pen.setDashPattern({3 * w / penWidth, 2 * w / penWidth});
    } else {
        // Точка — штрих почти нулевой длины с круглым концом: круг диаметром в толщину пера
        pen.setCapStyle(Qt::RoundCap);
        pen.setDashPattern({0.001, 2 * w / penWidth});
    }
    return pen;
}

static void drawRectMark(QPainter &p, const Mark &m, const QColor &color, qreal fw, qreal ow)
{
    const QRectF r = QRectF(m.rect).adjusted(fw / 2, fw / 2, -fw / 2, -fw / 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(Qt::white, fw + 2 * ow, fw, m.pen.line, Qt::SquareCap, Qt::MiterJoin));
    p.drawRect(r);
    p.setPen(strokePen(color, fw, fw, m.pen.line, Qt::SquareCap, Qt::MiterJoin));
    p.drawRect(r);
}

// Точки — центры пикселей: линия толщиной в 1 px через (x, y) закрашивает ровно этот пиксель.
static QPointF pixelCenter(const QPoint &p)
{
    return QPointF(p) + QPointF(0.5, 0.5);
}

static void drawArrowMark(QPainter &p, const Mark &m, const QColor &accent, qreal w, qreal ow, qreal dpr)
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

    // Наконечник в 4 толщины древка, но не короче 12 логических px;
    // на короткой стрелке — не длиннее 2/3 её длины
    const qreal headLength = std::min(std::max(4.0 * w, 12.0 * dpr), length * 2.0 / 3.0);
    const qreal headHalf = headLength * 0.55;
    const QPointF base = tip - dir * headLength;
    const QPolygonF head{tip, base + normal * headHalf, base - normal * headHalf};
    // Древко заходит в наконечник, чтобы между ними не было щели, но не торчит из острия
    const QPointF shaftEnd = base + dir * (headLength * 0.3);

    // Наконечник всегда сплошной: пунктирный треугольник не читается как остриё
    p.setPen(strokePen(Qt::white, w + 2 * ow, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(tail, shaftEnd);
    p.setPen(QPen(Qt::white, 2 * ow, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::white);
    p.drawPolygon(head);

    p.setPen(strokePen(accent, w, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
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
    p.setPen(strokePen(Qt::white, fw + 2 * ow, fw, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.setPen(strokePen(accent, fw, fw, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

// Кольцо точки-маркера вокруг её значка: значок и есть метка, кольцо отличает её от значков
// других форм и несёт цвет, толщину и тип линии пера. Центр — центр значка: у края снимка
// значок сдвигается внутрь (EDT-3), кольцо — вместе с ним.
static void drawPointRing(QPainter &p, const Mark &m, const QRectF &badge, const QColor &color, qreal w, qreal ow)
{
    // Зазор в толщину линии между значком и кольцом; у значка «A10» кольцо — вытянутое, как он сам
    const qreal grow = ow + w / 2 + w;
    const QRectF ring = badge.adjusted(-grow, -grow, grow, grow);
    QPainterPath path;
    path.addRoundedRect(ring, ring.height() / 2, ring.height() / 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(Qt::white, w + 2 * ow, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.setPen(strokePen(color, w, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

// Подпись комментария на картинке (EDT-10): белая плашка с рамкой цвета метки справа от
// значка, если не помещается — слева, а если и там нет места — под значком. Плашка целиком
// внутри снимка; длинный текст переносится по словам.
static void drawCaption(QPainter &p, const QString &text, const QRectF &badge, const QRectF &bounds,
                        const QColor &color, const RenderStyle &style, qreal dpr)
{
    QFont font = p.font();
    font.setBold(false);
    font.setPixelSize(std::max(1, int(std::round(style.badgeDiameter * 0.5 * dpr))));
    const QFontMetricsF fm(font);
    const qreal padX = 6 * dpr, padY = 3 * dpr, gap = 4 * dpr;
    const qreal maxText = std::max(fm.averageCharWidth() * 8,
                                   std::min(style.captionMaxWidth * dpr, bounds.width() - 2 * padX - 2));
    QRectF textRect = fm.boundingRect(QRectF(0, 0, maxText, 1e6), Qt::TextWordWrap, text);
    textRect.setHeight(std::min(textRect.height(), bounds.height() - 2 * padY - 2));
    QRectF box(0, 0, textRect.width() + 2 * padX, textRect.height() + 2 * padY);

    box.moveTopLeft(QPointF(badge.right() + gap, badge.center().y() - std::min(box.height(), badge.height()) / 2));
    if (box.right() > bounds.right()) {
        box.moveRight(badge.left() - gap);
        if (box.left() < bounds.left())
            box.moveTopLeft(QPointF(badge.left(), badge.bottom() + gap));
    }
    if (box.right() > bounds.right())
        box.moveRight(bounds.right());
    if (box.left() < bounds.left())
        box.moveLeft(bounds.left());
    if (box.bottom() > bounds.bottom())
        box.moveBottom(bounds.bottom());
    if (box.top() < bounds.top())
        box.moveTop(bounds.top());

    p.setPen(QPen(color, std::max(1.0, 1.5 * dpr)));
    p.setBrush(Qt::white);
    p.drawRoundedRect(box, 4 * dpr, 4 * dpr);
    p.setFont(font);
    p.setPen(QColor(0x20, 0x20, 0x20));
    p.save();
    p.setClipRect(box);
    p.drawText(box.adjusted(padX, padY, -padX, -padY), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
    p.restore();
}

void paintMarks(QPainter &p, const Shot &shot, int shotIndex, const RenderStyle &style, const QRectF &bounds)
{
    const qreal dpr = shot.devicePixelRatio > 0 ? shot.devicePixelRatio : 1.0;
    const qreal ow = style.outlineWidth * dpr;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    // Сначала все формы, потом все значки: значок не перекрывается чужой рамкой.
    for (const Mark &m : shot.marks) {
        const QColor color = markColor(m, style);
        const qreal w = markWidth(m, style) * dpr;
        switch (m.shape) {
        case MarkShape::Rect:
            drawRectMark(p, m, color, w, ow);
            break;
        case MarkShape::Arrow:
            drawArrowMark(p, m, color, w, ow, dpr);
            break;
        case MarkShape::Freehand:
            drawFreehandMark(p, m, color, w, ow);
            break;
        case MarkShape::Point:
            break;   // точка — это её значок, рисуется ниже
        }
    }
    // Значок рамки — в её левом верхнем углу, стрелки — у хвоста, карандаша — у первой точки,
    // точки — по центру на ней. Цвет значка — цвет метки, затемнённый до контраста с белой цифрой.
    QList<QRectF> badges;
    for (int i = 0; i < shot.marks.size(); ++i) {
        const Mark &m = shot.marks[i];
        const QPointF anchor = m.shape == MarkShape::Point && !m.points.isEmpty() ? pixelCenter(m.points.first())
                                                                                  : QPointF(m.badgeAnchor());
        const QRectF badge = drawBadge(p, markLabel(shotIndex, i), anchor, bounds, markColor(m, style), style, dpr);
        if (m.shape == MarkShape::Point)
            drawPointRing(p, m, badge, markColor(m, style), markWidth(m, style) * dpr, ow);
        badges << badge;
    }
    // Подписи — поверх всего: их задача — читаться
    if (style.captions)
        for (int i = 0; i < shot.marks.size(); ++i)
            if (const QString text = shot.marks[i].comment.trimmed(); !text.isEmpty())
                drawCaption(p, text, badges[i], bounds, markColor(shot.marks[i], style), style, dpr);
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
