#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QUuid>

namespace mp {

inline constexpr int kMaxShots = 26;            // A–Z
inline constexpr int kMaxCommentLength = 500;   // EDT-5

// All rectangles are in physical pixels of the source shot (NFR-7, CAP-6),
// not relative to the crop: the crop is kept separately and can be undone without a frame copy.

// Mark shape (EDT-9). All shapes are numbered together: A1 may be an arrow, A2 a frame.
// A point marker is the number badge right on the spot, without a frame or a line.
enum class MarkShape { Rect, Arrow, Freehand, Point };

enum class LineStyle { Solid, Dash, Dot };

// Frame shape (the "Mark" tool): rectangle, rounded rectangle, ellipse.
// The geometry is the same for all, the bounding rect; only what is drawn changes.
enum class FrameShape { Rect, Rounded, Ellipse };

// Mark color, width, line style and frame shape. Empty values come from the common style (RenderStyle):
// this way marks without a chosen color are recolored together with the accent in settings.
// The frame shape lives here too, not in a separate mark field: it is chosen for new marks and
// changed on the selected one just like the color (docs/decisions/0020-frame-shape.md). Not used
// by arrows, freehand lines and points.
struct MarkPen {
    QColor color;                  // invalid means the accent color from settings
    qreal width = 0;               // logical px; 0 means the shape's default width
    LineStyle line = LineStyle::Solid;
    FrameShape frame = FrameShape::Rect;

    bool operator==(const MarkPen &) const = default;
};

struct Mark {
    QUuid id;
    QRect rect;                    // frame; for arrows and freehand lines, the bounding rect of points
    QString comment;
    MarkShape shape = MarkShape::Rect;
    QList<QPoint> points;          // arrow: {tail, tip}; freehand: polyline after simplification; point: {center}
    MarkPen pen;

    // The geometry is given by points, not by a frame (arrow, freehand, point)
    bool isLinear() const { return shape != MarkShape::Rect; }
    // Where the number badge goes: for a frame, the top-left corner; for an ellipse, the point on it
    // at 45° top-left (the corner of the bounding rect would hang in empty space);
    // for an arrow, the tail; for freehand, the first point; for a point, the point itself
    QPoint badgeAnchor() const;

    bool operator==(const Mark &) const = default;
};

struct Redaction {
    QUuid id;
    QRect rect;

    bool operator==(const Redaction &) const = default;
};

struct Shot {
    QUuid id;
    QByteArray sourcePng;          // source without marks, compressed (NFR-2)
    QSize sourceSize;              // physical pixels
    qreal devicePixelRatio = 1.0;  // scale of the monitor the frame was taken from
    QRect crop;                    // empty means no crop
    QList<Mark> marks;             // mark number = position + 1
    QList<Redaction> redactions;

    QRect sourceRect() const { return QRect(QPoint(0, 0), sourceSize); }
    QRect visibleRect() const { return crop.isNull() ? sourceRect() : crop; }
    int commentCount() const;      // marks with a non-empty comment
    int markIndex(const QUuid &markId) const;
    int redactionIndex(const QUuid &redactionId) const;

    bool operator==(const Shot &) const = default;
};

struct CaptureSet {
    QList<Shot> shots;             // shot letter = position in the list

    int shotIndex(const QUuid &shotId) const;
    int totalMarks() const;
    int totalComments() const;
    bool isFull() const { return shots.size() >= kMaxShots; }
    bool isEmpty() const { return shots.isEmpty(); }

    bool operator==(const CaptureSet &) const = default;
};

// Shot letter by position: 0 → 'A'. Numbers are not stored but computed,
// so reordering, deleting and undo cannot leave numbers out of sync.
QChar shotLetter(int shotIndex);
QString markLabel(int shotIndex, int markIndex);   // (0, 0) → "A1"

// Rectangle from two corners, both included. QRect(p1, p2).normalized() does not fit:
// in Qt 6 an inverted rectangle is normalized with a one-pixel shift.
QRect rectFromPoints(const QPoint &a, const QPoint &b);

} // namespace mp
