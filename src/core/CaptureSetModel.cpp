#include "CaptureSetModel.h"

#include "Geometry.h"

#include <QUndoCommand>

namespace mp {

class StateCommand : public QUndoCommand
{
public:
    StateCommand(CaptureSetModel *model, const QString &text, CaptureSet before, CaptureSet after)
        : QUndoCommand(text), m_model(model), m_before(std::move(before)), m_after(std::move(after))
    {
    }
    void undo() override { m_model->setState(m_before); }
    void redo() override { m_model->setState(m_after); }

private:
    CaptureSetModel *m_model;
    CaptureSet m_before;
    CaptureSet m_after;
};

CaptureSetModel::CaptureSetModel(QObject *parent)
    : QObject(parent)
{
}

const Shot *CaptureSetModel::shot(const QUuid &shotId) const
{
    const int i = m_set.shotIndex(shotId);
    return i < 0 ? nullptr : &m_set.shots[i];
}

bool CaptureSetModel::apply(const QString &text, const Mutation &mutate)
{
    CaptureSet next = m_set;
    if (!mutate(next) || next == m_set)
        return false;
    // push() сразу вызывает redo(), то есть применяет «после».
    m_undo.push(new StateCommand(this, text, m_set, std::move(next)));
    return true;
}

void CaptureSetModel::setState(const CaptureSet &set)
{
    m_set = set;
    emit changed();
}

void CaptureSetModel::resetWithoutHistory(const CaptureSet &set)
{
    m_undo.clear();
    setState(set);
}

QRect CaptureSetModel::normalizedInside(const QRect &rect, const QRect &bounds)
{
    return rect.normalized().intersected(bounds);
}

bool CaptureSetModel::fitMark(Mark &mark, const QRect &bounds, qreal dpr)
{
    switch (mark.shape) {
    case MarkShape::Rect:
        mark.points.clear();
        mark.rect = normalizedInside(mark.rect, bounds);
        return !mark.rect.isEmpty();
    case MarkShape::Arrow: {
        if (mark.points.size() != 2)
            return false;
        mark.points = clipPolyline(mark.points, bounds);
        if (mark.points.size() != 2)
            return false;
        break;
    }
    case MarkShape::Freehand: {
        // Допуск упрощения — полпикселя экрана: глазу не видно, а точек в разы меньше
        const qreal eps = 0.5 * (dpr > 0 ? dpr : 1.0);
        mark.points = simplifyPolyline(clipPolyline(mark.points, bounds), eps);
        if (mark.points.size() < 2)
            return false;
        break;
    }
    }
    mark.rect = boundsOf(mark.points);
    return true;
}

QUuid CaptureSetModel::addShot(Shot shot)
{
    if (m_set.isFull() || shot.sourceSize.isEmpty())
        return {};
    if (shot.id.isNull())
        shot.id = QUuid::createUuid();
    const QUuid id = shot.id;
    const bool ok = apply(tr("New shot"), [&](CaptureSet &s) {
        s.shots.append(shot);
        return true;
    });
    return ok ? id : QUuid();
}

bool CaptureSetModel::removeShot(const QUuid &shotId)
{
    return apply(tr("Delete shot"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        s.shots.removeAt(i);
        return true;
    });
}

bool CaptureSetModel::moveShot(const QUuid &shotId, int newIndex)
{
    return apply(tr("Reorder shots"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0 || newIndex < 0 || newIndex >= s.shots.size())
            return false;
        s.shots.move(i, newIndex);
        return true;
    });
}

void CaptureSetModel::clear()
{
    apply(tr("Clear set"), [](CaptureSet &s) {
        s.shots.clear();
        return true;
    });
}

QUuid CaptureSetModel::addMark(const QUuid &shotId, const QRect &rect, const QString &comment)
{
    const QUuid markId = QUuid::createUuid();
    const bool ok = apply(tr("Add mark"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        const QRect r = normalizedInside(rect, shot.visibleRect());
        if (r.isEmpty())
            return false;
        shot.marks.append(Mark{markId, r, comment.left(kMaxCommentLength)});
        return true;
    });
    return ok ? markId : QUuid();
}

bool CaptureSetModel::setMarkRect(const QUuid &shotId, const QUuid &markId, const QRect &rect)
{
    return apply(tr("Move mark"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        const int m = shot.markIndex(markId);
        const QRect r = normalizedInside(rect, shot.visibleRect());
        if (m < 0 || r.isEmpty() || shot.marks[m].isLinear())
            return false;
        shot.marks[m].rect = r;
        return true;
    });
}

QUuid CaptureSetModel::addLinearMark(const QUuid &shotId, MarkShape shape, const QList<QPoint> &points,
                                     const QString &comment)
{
    if (shape == MarkShape::Rect)
        return addMark(shotId, boundsOf(points), comment);
    const QUuid markId = QUuid::createUuid();
    const bool ok = apply(shape == MarkShape::Arrow ? tr("Add arrow") : tr("Add drawing"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        Mark m{markId, {}, comment.left(kMaxCommentLength), shape, points};
        if (!fitMark(m, shot.visibleRect(), shot.devicePixelRatio))
            return false;
        shot.marks.append(m);
        return true;
    });
    return ok ? markId : QUuid();
}

bool CaptureSetModel::setMarkPoints(const QUuid &shotId, const QUuid &markId, const QList<QPoint> &points)
{
    return apply(tr("Move mark"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        const int m = shot.markIndex(markId);
        if (m < 0 || !shot.marks[m].isLinear())
            return false;
        Mark mark = shot.marks[m];
        mark.points = points;
        if (!fitMark(mark, shot.visibleRect(), shot.devicePixelRatio))
            return false;
        shot.marks[m] = mark;
        return true;
    });
}

bool CaptureSetModel::setMarkComment(const QUuid &shotId, const QUuid &markId, const QString &comment)
{
    return apply(tr("Edit comment"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        const int m = s.shots[i].markIndex(markId);
        if (m < 0)
            return false;
        s.shots[i].marks[m].comment = comment.left(kMaxCommentLength);
        return true;
    });
}

bool CaptureSetModel::removeMark(const QUuid &shotId, const QUuid &markId)
{
    return apply(tr("Delete mark"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        const int m = s.shots[i].markIndex(markId);
        if (m < 0)
            return false;
        s.shots[i].marks.removeAt(m);   // номера остальных пересчитаются сами (EDT-2)
        return true;
    });
}

QUuid CaptureSetModel::addRedaction(const QUuid &shotId, const QRect &rect)
{
    const QUuid redactionId = QUuid::createUuid();
    const bool ok = apply(tr("Blur area"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        const QRect r = normalizedInside(rect, shot.visibleRect());
        if (r.isEmpty())
            return false;
        shot.redactions.append(Redaction{redactionId, r});
        return true;
    });
    return ok ? redactionId : QUuid();
}

bool CaptureSetModel::setRedactionRect(const QUuid &shotId, const QUuid &redactionId, const QRect &rect)
{
    return apply(tr("Move blur"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        const int r = shot.redactionIndex(redactionId);
        const QRect clipped = normalizedInside(rect, shot.visibleRect());
        if (r < 0 || clipped.isEmpty())
            return false;
        shot.redactions[r].rect = clipped;
        return true;
    });
}

bool CaptureSetModel::removeRedaction(const QUuid &shotId, const QUuid &redactionId)
{
    return apply(tr("Remove blur"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        const int r = s.shots[i].redactionIndex(redactionId);
        if (r < 0)
            return false;
        s.shots[i].redactions.removeAt(r);
        return true;
    });
}

int CaptureSetModel::marksOutsideCrop(const QUuid &shotId, const QRect &crop) const
{
    const Shot *s = shot(shotId);
    if (!s)
        return 0;
    const QRect c = normalizedInside(crop, s->sourceRect());
    int n = 0;
    for (Mark m : s->marks)
        if (!fitMark(m, c, s->devicePixelRatio))
            ++n;
    return n;
}

bool CaptureSetModel::setCrop(const QUuid &shotId, const QRect &crop)
{
    return apply(tr("Crop"), [&](CaptureSet &s) {
        const int i = s.shotIndex(shotId);
        if (i < 0)
            return false;
        Shot &shot = s.shots[i];
        const QRect c = normalizedInside(crop, shot.sourceRect());
        if (c.isEmpty())
            return false;
        shot.crop = (c == shot.sourceRect()) ? QRect() : c;

        QList<Mark> marks;
        for (Mark m : shot.marks)
            if (fitMark(m, c, shot.devicePixelRatio))
                marks.append(m);
        shot.marks = marks;

        QList<Redaction> redactions;
        for (Redaction r : shot.redactions) {
            r.rect = r.rect.intersected(c);
            if (!r.rect.isEmpty())
                redactions.append(r);
        }
        shot.redactions = redactions;
        return true;
    });
}

} // namespace mp
