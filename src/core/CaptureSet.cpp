#include "CaptureSet.h"

#include <algorithm>
#include <cmath>

namespace mp {

QPoint Mark::badgeAnchor() const
{
    if (isLinear())
        return points.isEmpty() ? rect.topLeft() : points.first();
    if (pen.frame == FrameShape::Ellipse) {
        const QPointF c = QRectF(rect).center();
        const qreal k = std::sqrt(0.5);
        return QPointF(c.x() - rect.width() / 2.0 * k, c.y() - rect.height() / 2.0 * k).toPoint();
    }
    return rect.topLeft();
}

int Shot::commentCount() const
{
    int n = 0;
    for (const Mark &m : marks)
        if (!m.comment.trimmed().isEmpty())
            ++n;
    return n;
}

int Shot::markIndex(const QUuid &markId) const
{
    for (int i = 0; i < marks.size(); ++i)
        if (marks[i].id == markId)
            return i;
    return -1;
}

int Shot::redactionIndex(const QUuid &redactionId) const
{
    for (int i = 0; i < redactions.size(); ++i)
        if (redactions[i].id == redactionId)
            return i;
    return -1;
}

int CaptureSet::shotIndex(const QUuid &shotId) const
{
    for (int i = 0; i < shots.size(); ++i)
        if (shots[i].id == shotId)
            return i;
    return -1;
}

int CaptureSet::totalMarks() const
{
    int n = 0;
    for (const Shot &s : shots)
        n += s.marks.size();
    return n;
}

int CaptureSet::totalComments() const
{
    int n = 0;
    for (const Shot &s : shots)
        n += s.commentCount();
    return n;
}

QChar shotLetter(int shotIndex)
{
    Q_ASSERT(shotIndex >= 0 && shotIndex < kMaxShots);
    return QChar(u'A' + shotIndex);
}

QString markLabel(int shotIndex, int markIndex)
{
    return QString(shotLetter(shotIndex)) + QString::number(markIndex + 1);
}

QRect rectFromPoints(const QPoint &a, const QPoint &b)
{
    return QRect(QPoint(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                 QPoint(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

} // namespace mp
