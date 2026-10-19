#include "Geometry.h"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mp {

QRect boundsOf(const QList<QPoint> &points)
{
    if (points.isEmpty())
        return {};
    int x0 = points[0].x(), x1 = x0, y0 = points[0].y(), y1 = y0;
    for (const QPoint &p : points) {
        x0 = std::min(x0, p.x());
        x1 = std::max(x1, p.x());
        y0 = std::min(y0, p.y());
        y1 = std::max(y1, p.y());
    }
    return QRect(QPoint(x0, y0), QPoint(x1, y1));
}

bool clipSegment(QPointF &a, QPointF &b, const QRectF &rect)
{
    const qreal dx = b.x() - a.x();
    const qreal dy = b.y() - a.y();
    qreal t0 = 0, t1 = 1;
    const qreal p[4] = {-dx, dx, -dy, dy};
    const qreal q[4] = {a.x() - rect.left(), rect.right() - a.x(), a.y() - rect.top(), rect.bottom() - a.y()};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;   // параллелен стороне и снаружи
            continue;
        }
        const qreal t = q[i] / p[i];
        if (p[i] < 0)
            t0 = std::max(t0, t);
        else
            t1 = std::min(t1, t);
        if (t0 > t1)
            return false;
    }
    const QPointF start = a;
    a = start + QPointF(dx, dy) * t0;
    b = start + QPointF(dx, dy) * t1;
    return true;
}

QList<QPoint> clipPolyline(const QList<QPoint> &points, const QRect &rect)
{
    if (points.isEmpty() || rect.isEmpty())
        return {};
    // Целочисленный прямоугольник включает правый и нижний пиксель: right() = left + width - 1
    const QRectF r(QPointF(rect.left(), rect.top()), QPointF(rect.right(), rect.bottom()));
    if (points.size() == 1)
        return r.contains(points[0]) ? points : QList<QPoint>{};

    QList<QPoint> best, run;
    qreal bestLength = -1, runLength = 0;
    auto finishRun = [&] {
        if (run.size() >= 2 && runLength > bestLength) {
            best = run;
            bestLength = runLength;
        }
        run.clear();
        runLength = 0;
    };
    for (int i = 0; i + 1 < points.size(); ++i) {
        QPointF a = points[i], b = points[i + 1];
        if (!clipSegment(a, b, r)) {
            finishRun();
            continue;
        }
        const QPoint pa = a.toPoint(), pb = b.toPoint();
        if (run.isEmpty() || run.last() != pa) {
            finishRun();   // начало нового куска: предыдущий кончился на краю
            run.append(pa);
        }
        if (pb != run.last())
            run.append(pb);
        runLength += QLineF(a, b).length();
        if (b != QPointF(points[i + 1]))
            finishRun();   // вышли за край
    }
    finishRun();
    return best;
}

static void rdp(const QList<QPoint> &pts, int first, int last, qreal eps, QList<bool> &keep)
{
    if (last <= first + 1)
        return;
    qreal maxDist = -1;
    int index = first;
    for (int i = first + 1; i < last; ++i) {
        const qreal d = distanceToSegment(pts[i], pts[first], pts[last]);
        if (d > maxDist) {
            maxDist = d;
            index = i;
        }
    }
    if (maxDist > eps) {
        keep[index] = true;
        rdp(pts, first, index, eps, keep);
        rdp(pts, index, last, eps, keep);
    }
}

QList<QPoint> simplifyPolyline(const QList<QPoint> &points, qreal epsilon)
{
    // Подряд одинаковые точки не несут формы
    QList<QPoint> pts;
    for (const QPoint &p : points)
        if (pts.isEmpty() || pts.last() != p)
            pts.append(p);
    if (pts.size() <= 2)
        return pts;
    QList<bool> keep(pts.size(), false);
    keep.first() = keep.last() = true;
    rdp(pts, 0, int(pts.size()) - 1, epsilon, keep);
    QList<QPoint> out;
    for (int i = 0; i < pts.size(); ++i)
        if (keep[i])
            out.append(pts[i]);
    return out;
}

qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    if (len2 == 0)
        return QLineF(p, a).length();
    const qreal t = std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0);
    return QLineF(p, a + ab * t).length();
}

qreal distanceToPolyline(const QPointF &p, const QList<QPointF> &points)
{
    if (points.isEmpty())
        return std::numeric_limits<qreal>::infinity();
    if (points.size() == 1)
        return QLineF(p, points[0]).length();
    qreal d = std::numeric_limits<qreal>::infinity();
    for (int i = 0; i + 1 < points.size(); ++i)
        d = std::min(d, distanceToSegment(p, points[i], points[i + 1]));
    return d;
}

} // namespace mp
