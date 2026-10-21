#pragma once

#include "CaptureSet.h"

#include <QObject>
#include <QUndoStack>

#include <functional>

namespace mp {

// Owner of the set and the only place where it changes.
//
// Undo is built on state snapshots: every command keeps the CaptureSet "before" and "after".
// QByteArray and QList are implicitly shared, so a snapshot costs a copy of the lists, not of the frames.
// This way undo works for any operation, including deleting and reordering shots (EDT-8),
// and no inverse operation has to be written for each edit.
class CaptureSetModel : public QObject
{
    Q_OBJECT
public:
    explicit CaptureSetModel(QObject *parent = nullptr);

    const CaptureSet &set() const { return m_set; }
    const Shot *shot(const QUuid &shotId) const;
    QUndoStack *undoStack() { return &m_undo; }

    // Shots. addShot returns a null QUuid if the set is full (CAP-8).
    QUuid addShot(Shot shot);
    bool removeShot(const QUuid &shotId);
    bool moveShot(const QUuid &shotId, int newIndex);
    void clear();                                   // clearing the set can be undone too

    // Marks
    QUuid addMark(const QUuid &shotId, const QRect &rect, const QString &comment = {}, const MarkPen &pen = {});
    bool setMarkRect(const QUuid &shotId, const QUuid &markId, const QRect &rect);
    // Arrow, freehand and point (EDT-9). Points are clipped to the visible area, the freehand line
    // is simplified; the mark's rect becomes the bounding rectangle.
    QUuid addLinearMark(const QUuid &shotId, MarkShape shape, const QList<QPoint> &points,
                        const QString &comment = {}, const MarkPen &pen = {});
    bool setMarkPoints(const QUuid &shotId, const QUuid &markId, const QList<QPoint> &points);
    bool setMarkComment(const QUuid &shotId, const QUuid &markId, const QString &comment);
    bool setMarkPen(const QUuid &shotId, const QUuid &markId, const MarkPen &pen);
    bool removeMark(const QUuid &shotId, const QUuid &markId);

    // Blur
    QUuid addRedaction(const QUuid &shotId, const QRect &rect);
    bool setRedactionRect(const QUuid &shotId, const QUuid &redactionId, const QRect &rect);
    bool removeRedaction(const QUuid &shotId, const QUuid &redactionId);

    // Crop (EDT-7): marks and blurs outside the new area are removed, the ones partly
    // inside are clipped to it (arrows and freehand along the line, not the frame). marksOutsideCrop tells in advance how many go away.
    bool setCrop(const QUuid &shotId, const QRect &crop);
    int marksOutsideCrop(const QUuid &shotId, const QRect &crop) const;

    // No history: only for loading a saved session.
    void resetWithoutHistory(const CaptureSet &set);

signals:
    void changed();

private:
    friend class StateCommand;
    using Mutation = std::function<bool(CaptureSet &)>;
    bool apply(const QString &text, const Mutation &mutate);
    void setState(const CaptureSet &set);

    static QRect normalizedInside(const QRect &rect, const QRect &bounds);
    // Fits a mark of any shape into bounds. false: nothing is left of the mark.
    static bool fitMark(Mark &mark, const QRect &bounds, qreal dpr);

    CaptureSet m_set;
    QUndoStack m_undo;
};

} // namespace mp
