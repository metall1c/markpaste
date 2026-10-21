#include "ShotRenderer.h"

#include <QBuffer>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace mp {

// --- Contrast (WCAG 2.x) ---

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
    // Darken by 2 % towards black: the hue is kept, the contrast grows monotonically.
    for (int i = 0; i < 100 && contrastRatio(c, Qt::white) < 4.5; ++i)
        c = QColor::fromRgbF(c.redF() * 0.98f, c.greenF() * 0.98f, c.blueF() * 0.98f);
    return c;
}

// --- Pixelation ---

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

// --- Marks ---

static QRectF drawBadge(QPainter &p, const QString &label, QPointF anchor, const QRectF &bounds, const QColor &color,
                        const RenderStyle &style, qreal dpr)
{
    const qreal d = style.badgeDiameter * dpr;
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(std::max(1, int(std::round(d * 0.46))));
    const QFontMetricsF fm(font);
    const qreal w = std::max(d, fm.horizontalAdvance(label) + d * 0.5);

    // The badge center is at the mark's anchor point, but the whole badge stays inside the shot (EDT-3).
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

// Pen for a mark line. The dash pattern is set in absolute pixels from the colored line width w,
// not in fractions of the pen width: the white underlay pen is wider, and with dashes "in widths"
// its dashes would drift apart from the colored ones.
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
        // A dot is a near-zero-length dash with a round cap: a circle as wide as the pen
        pen.setCapStyle(Qt::RoundCap);
        pen.setDashPattern({0.001, 2 * w / penWidth});
    }
    return pen;
}

// Line halo (RenderStyle::halo) in physical pixels of the shot
struct Halo {
    QColor color;
    qreal width = 0;
    bool on() const { return width > 0; }
};

// Frame outline by its shape (rectangle, rounded, ellipse). The line is inside rect:
// a frame of any shape does not go beyond the bounding rectangle the editor sees.
QPainterPath frameOutline(const QRectF &rect, FrameShape frame, qreal lineWidth, qreal dpr)
{
    const QRectF r = rect.adjusted(lineWidth / 2, lineWidth / 2, -lineWidth / 2, -lineWidth / 2);
    QPainterPath path;
    switch (frame) {
    case FrameShape::Rect:
        path.addRect(r);
        break;
    case FrameShape::Rounded: {
        // The radius is 10 logical px; for a small frame, no more than a third of its side
        const qreal radius = std::min(10.0 * dpr, std::min(r.width(), r.height()) / 3);
        path.addRoundedRect(r, radius, radius);
        break;
    }
    case FrameShape::Ellipse:
        path.addEllipse(r);
        break;
    }
    return path;
}

static void drawRectMark(QPainter &p, const Mark &m, const QColor &color, qreal fw, const Halo &halo, qreal dpr)
{
    const QPainterPath path = frameOutline(QRectF(m.rect), m.pen.frame, fw, dpr);
    // A rectangle has sharp corners, rounded frames and ellipses have round joins
    const Qt::PenJoinStyle join = m.pen.frame == FrameShape::Rect ? Qt::MiterJoin : Qt::RoundJoin;
    p.setBrush(Qt::NoBrush);
    if (halo.on()) {
        p.setPen(strokePen(halo.color, fw + 2 * halo.width, fw, m.pen.line, Qt::SquareCap, join));
        p.drawPath(path);
    }
    p.setPen(strokePen(color, fw, fw, m.pen.line, Qt::SquareCap, join));
    p.drawPath(path);
}

// Points are pixel centers: a 1 px wide line through (x, y) paints exactly that pixel.
static QPointF pixelCenter(const QPoint &p)
{
    return QPointF(p) + QPointF(0.5, 0.5);
}

static void drawArrowMark(QPainter &p, const Mark &m, const QColor &accent, qreal w, const Halo &halo, qreal dpr)
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

    // The arrowhead is 4 shaft widths long, but no shorter than 12 logical px;
    // on a short arrow, no longer than 2/3 of its length
    const qreal headLength = std::min(std::max(4.0 * w, 12.0 * dpr), length * 2.0 / 3.0);
    const qreal headHalf = headLength * 0.55;
    const QPointF base = tip - dir * headLength;
    const QPolygonF head{tip, base + normal * headHalf, base - normal * headHalf};
    // The shaft goes into the head so there is no gap between them, but does not stick out of the tip
    const QPointF shaftEnd = base + dir * (headLength * 0.3);

    // The head is always solid: a dashed triangle does not read as a tip
    if (halo.on()) {
        // The head's halo is an outline only: a semi-transparent fill under it would give
        // double darkening along the edge
        p.setPen(strokePen(halo.color, w + 2 * halo.width, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(tail, shaftEnd);
        p.setPen(QPen(halo.color, 2 * halo.width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(head);
    }

    p.setPen(strokePen(accent, w, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(tail, shaftEnd);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawPolygon(head);
}

// Smoothing: the curve passes through the midpoints of the segments, the polyline vertices are control points.
// After simplification (simplifyPolyline) there are few points, and without smoothing the line looks angular.
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

static void drawFreehandMark(QPainter &p, const Mark &m, const QColor &accent, qreal fw, const Halo &halo)
{
    const QPainterPath path = smoothPath(m.points);
    p.setBrush(Qt::NoBrush);
    if (halo.on()) {
        p.setPen(strokePen(halo.color, fw + 2 * halo.width, fw, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
    p.setPen(strokePen(accent, fw, fw, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

// The ring of a point marker around its badge: the badge is the mark itself, the ring tells it apart from the badges
// of other shapes and carries the pen's color, width and line style. The center is the badge center: at the shot edge
// the badge moves inwards (EDT-3), and the ring moves with it.
static void drawPointRing(QPainter &p, const Mark &m, const QRectF &badge, const QColor &color, qreal w, qreal ow,
                          const Halo &halo)
{
    // A gap of one line width between the badge and the ring; for an "A10" badge the ring is elongated, like the badge
    const qreal grow = ow + w / 2 + w;
    const QRectF ring = badge.adjusted(-grow, -grow, grow, grow);
    QPainterPath path;
    path.addRoundedRect(ring, ring.height() / 2, ring.height() / 2);
    p.setBrush(Qt::NoBrush);
    if (halo.on()) {
        p.setPen(strokePen(halo.color, w + 2 * halo.width, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
    p.setPen(strokePen(color, w, w, m.pen.line, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

// Comment caption on the image (EDT-10): a white plate with a border of the mark color to the right of
// the badge, if it does not fit, to the left, and if there is no room there either, below the badge. The plate is entirely
// inside the shot; long text wraps by words.
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
    const Halo halo{style.haloColor, style.halo ? style.haloWidth * dpr : 0.0};
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    // All shapes first, then all badges: a badge is not covered by another mark's frame.
    for (const Mark &m : shot.marks) {
        const QColor color = markColor(m, style);
        const qreal w = markWidth(m, style) * dpr;
        switch (m.shape) {
        case MarkShape::Rect:
            drawRectMark(p, m, color, w, halo, dpr);
            break;
        case MarkShape::Arrow:
            drawArrowMark(p, m, color, w, halo, dpr);
            break;
        case MarkShape::Freehand:
            drawFreehandMark(p, m, color, w, halo);
            break;
        case MarkShape::Point:
            break;   // a point is its badge, drawn below
        }
    }
    // A frame's badge is at its top-left corner, an arrow's at the tail, freehand's at the first point,
    // a point's centered on it. The badge color is the mark color darkened for contrast with the white digit.
    QList<QRectF> badges;
    for (int i = 0; i < shot.marks.size(); ++i) {
        const Mark &m = shot.marks[i];
        const QPointF anchor = m.shape == MarkShape::Point && !m.points.isEmpty() ? pixelCenter(m.points.first())
                                                                                  : QPointF(m.badgeAnchor());
        const QRectF badge = drawBadge(p, markLabel(shotIndex, i), anchor, bounds, markColor(m, style), style, dpr);
        if (m.shape == MarkShape::Point)
            drawPointRing(p, m, badge, markColor(m, style), markWidth(m, style) * dpr, ow, halo);
        badges << badge;
    }
    // Captions go on top of everything: their job is to be read
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
    // Blur goes before the crop and the marks, in source coordinates.
    const QImage source = sourceWithRedactions(shot, style);
    if (source.isNull())
        return {};

    const QRect visible = shot.visibleRect().intersected(source.rect());
    QImage out = source.copy(visible);

    QPainter p(&out);
    p.translate(-visible.topLeft());
    paintMarks(p, shot, shotIndex, style, QRectF(visible));
    p.end();
    out.setDevicePixelRatio(1.0);  // physical pixels go out
    return out;
}

// --- Stitching ---

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

// --- Encoding ---

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
