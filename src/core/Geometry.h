#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRect>

namespace mp {

// Геометрия линейных меток (стрелка, карандаш). Координаты — физические пиксели исходника.

// Охватывающий прямоугольник точек, включая крайние (как rectFromPoints).
QRect boundsOf(const QList<QPoint> &points);

// Отрезок, обрезанный по прямоугольнику (Лянг — Барски). false — отрезок целиком снаружи.
bool clipSegment(QPointF &a, QPointF &b, const QRectF &rect);

// Ломаная, обрезанная по прямоугольнику. Если она выходит наружу и возвращается,
// получается несколько кусков; метка — одна линия со значком у начала,
// поэтому оставляем самый длинный кусок. Пусто — ломаная целиком снаружи.
QList<QPoint> clipPolyline(const QList<QPoint> &points, const QRect &rect);

// Упрощение Рамера — Дугласа — Пекера: убирает точки, отклоняющиеся от хорды меньше epsilon.
// Первая и последняя точки сохраняются.
QList<QPoint> simplifyPolyline(const QList<QPoint> &points, qreal epsilon);

qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b);
qreal distanceToPolyline(const QPointF &p, const QList<QPointF> &points);

} // namespace mp
