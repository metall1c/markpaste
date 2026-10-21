#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRect>

namespace mp {

// Geometry of line marks (arrow, freehand). Coordinates are physical pixels of the source.

// Bounding rectangle of the points, extreme ones included (like rectFromPoints).
QRect boundsOf(const QList<QPoint> &points);

// Segment clipped to a rectangle (Liang–Barsky). false: the segment is entirely outside.
bool clipSegment(QPointF &a, QPointF &b, const QRectF &rect);

// Polyline clipped to a rectangle. If it goes out and comes back, there are
// several pieces; a mark is one line with a badge at its start,
// so the longest piece is kept. Empty: the polyline is entirely outside.
QList<QPoint> clipPolyline(const QList<QPoint> &points, const QRect &rect);

// Ramer–Douglas–Peucker simplification: drops points that deviate from the chord by less than epsilon.
// The first and last points are kept.
QList<QPoint> simplifyPolyline(const QList<QPoint> &points, qreal epsilon);

qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b);
qreal distanceToPolyline(const QPointF &p, const QList<QPointF> &points);

} // namespace mp
