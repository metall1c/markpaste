#include "TestUtil.h"
#include "core/CaptureSetModel.h"
#include "core/Geometry.h"

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

TEST(Labels, LettersAndNumbers)
{
    EXPECT_EQ(shotLetter(0), QChar(u'A'));
    EXPECT_EQ(shotLetter(25), QChar(u'Z'));
    EXPECT_EQ(markLabel(0, 0), QStringLiteral("A1"));
    EXPECT_EQ(markLabel(1, 11), QStringLiteral("B12"));
}

TEST(Labels, RectFromPoints)
{
    EXPECT_EQ(rectFromPoints(QPoint(10, 20), QPoint(0, 0)), QRect(0, 0, 11, 21));
    EXPECT_EQ(rectFromPoints(QPoint(0, 0), QPoint(10, 20)), QRect(0, 0, 11, 21));
}

TEST(CaptureSetModel, AddShotsUpTo26)
{
    CaptureSetModel m;
    for (int i = 0; i < kMaxShots; ++i)
        EXPECT_FALSE(m.addShot(makeShot(10, 10)).isNull());
    EXPECT_TRUE(m.set().isFull());
    EXPECT_TRUE(m.addShot(makeShot(10, 10)).isNull());   // CAP-8
    EXPECT_EQ(m.set().shots.size(), kMaxShots);
}

TEST(CaptureSetModel, RemoveMarkRenumbers)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot());
    const QUuid a1 = m.addMark(s, QRect(0, 0, 10, 10), QStringLiteral("one"));
    const QUuid a2 = m.addMark(s, QRect(20, 0, 10, 10), QStringLiteral("two"));
    const QUuid a3 = m.addMark(s, QRect(40, 0, 10, 10), QStringLiteral("three"));
    ASSERT_FALSE(a2.isNull());

    EXPECT_TRUE(m.removeMark(s, a2));
    const Shot *shot = m.shot(s);
    ASSERT_EQ(shot->marks.size(), 2);
    EXPECT_EQ(shot->markIndex(a1), 0);
    EXPECT_EQ(shot->markIndex(a3), 1);   // it was A3 and became A2 (EDT-2)
}

TEST(CaptureSetModel, MarkIsClampedToVisibleArea)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(100, 100));
    const QUuid id = m.addMark(s, rectFromPoints(QPoint(150, 150), QPoint(50, 50)));   // inverted, beyond the edge
    ASSERT_FALSE(id.isNull());
    EXPECT_EQ(m.shot(s)->marks[0].rect, QRect(50, 50, 50, 50));
    EXPECT_TRUE(m.addMark(s, QRect(200, 200, 10, 10)).isNull());             // entirely outside
}

TEST(CaptureSetModel, CommentIsLimited)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot());
    const QUuid id = m.addMark(s, QRect(0, 0, 5, 5));
    m.setMarkComment(s, id, QString(600, u'x'));
    EXPECT_EQ(m.shot(s)->marks[0].comment.size(), kMaxCommentLength);
}

TEST(CaptureSetModel, UndoRedoEverything)
{
    CaptureSetModel m;
    const QUuid a = m.addShot(makeShot());
    const QUuid b = m.addShot(makeShot());
    const QUuid mark = m.addMark(b, QRect(0, 0, 10, 10), QStringLiteral("x"));
    const CaptureSet full = m.set();

    m.removeShot(a);
    EXPECT_EQ(m.set().shots.size(), 1);
    EXPECT_EQ(m.set().shotIndex(b), 0);   // B became A (STR-4)

    m.undoStack()->undo();
    EXPECT_EQ(m.set(), full);
    m.undoStack()->undo();                 // the mark
    EXPECT_EQ(m.shot(b)->marks.size(), 0);
    m.undoStack()->redo();
    m.undoStack()->redo();
    EXPECT_EQ(m.set().shots.size(), 1);
    EXPECT_FALSE(mark.isNull());
}

TEST(CaptureSetModel, ClearIsUndoable)
{
    CaptureSetModel m;
    m.addShot(makeShot());
    m.addShot(makeShot());
    m.clear();
    EXPECT_TRUE(m.set().isEmpty());
    m.undoStack()->undo();
    EXPECT_EQ(m.set().shots.size(), 2);
}

TEST(CaptureSetModel, NoOpDoesNotPushHistory)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot());
    const QUuid id = m.addMark(s, QRect(0, 0, 10, 10), QStringLiteral("same"));
    const int count = m.undoStack()->count();
    EXPECT_FALSE(m.setMarkComment(s, id, QStringLiteral("same")));
    EXPECT_EQ(m.undoStack()->count(), count);
}

TEST(CaptureSetModel, MoveShotReletters)
{
    CaptureSetModel m;
    const QUuid a = m.addShot(makeShot());
    const QUuid b = m.addShot(makeShot());
    const QUuid c = m.addShot(makeShot());
    EXPECT_TRUE(m.moveShot(c, 0));
    EXPECT_EQ(m.set().shotIndex(c), 0);
    EXPECT_EQ(m.set().shotIndex(a), 1);
    EXPECT_EQ(m.set().shotIndex(b), 2);
}

TEST(CaptureSetModel, CropRemovesOutsideAndClipsPartial)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    m.addMark(s, QRect(0, 0, 20, 20));      // outside
    m.addMark(s, QRect(90, 40, 40, 20));    // partly inside
    m.addMark(s, QRect(110, 50, 10, 10));   // inside
    m.addRedaction(s, QRect(0, 80, 20, 20)); // outside

    const QRect crop(100, 30, 100, 70);
    EXPECT_EQ(m.marksOutsideCrop(s, crop), 1);
    EXPECT_TRUE(m.setCrop(s, crop));

    const Shot *shot = m.shot(s);
    EXPECT_EQ(shot->crop, crop);
    ASSERT_EQ(shot->marks.size(), 2);
    EXPECT_EQ(shot->marks[0].rect, QRect(100, 40, 30, 20));
    EXPECT_EQ(shot->marks[1].rect, QRect(110, 50, 10, 10));
    EXPECT_TRUE(shot->redactions.isEmpty());

    m.undoStack()->undo();
    EXPECT_EQ(m.shot(s)->marks.size(), 3);
    EXPECT_TRUE(m.shot(s)->crop.isNull());
}

TEST(CaptureSetModel, CommentCounters)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot());
    m.addMark(s, QRect(0, 0, 5, 5), QStringLiteral("a"));
    m.addMark(s, QRect(0, 0, 5, 5), QStringLiteral("   "));
    m.addMark(s, QRect(0, 0, 5, 5));
    EXPECT_EQ(m.set().totalMarks(), 3);
    EXPECT_EQ(m.set().totalComments(), 1);
}

// --- Arrow and freehand (EDT-9) ---

TEST(Geometry, ClipSegment)
{
    QPointF a(-10, 5), b(30, 5);
    ASSERT_TRUE(clipSegment(a, b, QRectF(0, 0, 20, 10)));
    EXPECT_EQ(a, QPointF(0, 5));
    EXPECT_EQ(b, QPointF(20, 5));
    QPointF c(-10, -5), d(30, -5);
    EXPECT_FALSE(clipSegment(c, d, QRectF(0, 0, 20, 10)));
}

TEST(Geometry, ClipPolylineKeepsLongestRun)
{
    // A short piece, a trip outside, a long piece
    const QList<QPoint> pts{{10, 10}, {20, 10}, {20, -10}, {30, -10}, {30, 50}, {35, 50}};
    const QList<QPoint> clipped = clipPolyline(pts, QRect(0, 0, 100, 100));
    ASSERT_EQ(clipped.size(), 3);
    EXPECT_EQ(clipped.first(), QPoint(30, 0));
    EXPECT_EQ(clipped.last(), QPoint(35, 50));
    EXPECT_TRUE(clipPolyline({{-5, -5}, {-1, -1}}, QRect(0, 0, 10, 10)).isEmpty());
}

TEST(Geometry, SimplifyDropsCollinearPoints)
{
    QList<QPoint> pts;
    for (int x = 0; x <= 100; ++x)
        pts.append(QPoint(x, 0));
    pts.append(QPoint(100, 50));
    EXPECT_EQ(simplifyPolyline(pts, 0.5), (QList<QPoint>{{0, 0}, {100, 0}, {100, 50}}));
}

TEST(Geometry, DistanceToPolyline)
{
    const QList<QPointF> pts{{0, 0}, {10, 0}, {10, 10}};
    EXPECT_DOUBLE_EQ(distanceToPolyline(QPointF(5, 3), pts), 3.0);
    EXPECT_DOUBLE_EQ(distanceToPolyline(QPointF(13, 5), pts), 3.0);
    EXPECT_DOUBLE_EQ(distanceToPolyline(QPointF(-4, 3), pts), 5.0);
}

TEST(CaptureSetModel, MixedShapesShareNumbering)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    const QUuid arrow = m.addLinearMark(s, MarkShape::Arrow, {{10, 10}, {60, 40}}, QStringLiteral("сюда"));
    const QUuid rect = m.addMark(s, QRect(100, 10, 30, 30));
    const QUuid pen = m.addLinearMark(s, MarkShape::Freehand, {{150, 50}, {160, 60}, {170, 50}});
    const Shot *shot = m.shot(s);
    ASSERT_EQ(shot->marks.size(), 3);
    EXPECT_EQ(shot->markIndex(arrow), 0);   // A1 is the arrow
    EXPECT_EQ(shot->markIndex(rect), 1);    // A2 is the frame
    EXPECT_EQ(shot->markIndex(pen), 2);     // A3 is the freehand line
    EXPECT_EQ(shot->marks[0].rect, QRect(10, 10, 51, 31));
    EXPECT_EQ(shot->marks[0].badgeAnchor(), QPoint(10, 10));
    EXPECT_EQ(shot->marks[2].badgeAnchor(), QPoint(150, 50));
    EXPECT_EQ(m.set().totalComments(), 1);
}

TEST(CaptureSetModel, LinearMarksRejectDegenerate)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(100, 100));
    EXPECT_TRUE(m.addLinearMark(s, MarkShape::Arrow, {{10, 10}, {10, 10}}).isNull());
    EXPECT_TRUE(m.addLinearMark(s, MarkShape::Arrow, {{10, 10}}).isNull());
    EXPECT_TRUE(m.addLinearMark(s, MarkShape::Freehand, {{200, 200}, {300, 300}}).isNull());   // outside
}

TEST(CaptureSetModel, FreehandIsSimplified)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    QList<QPoint> pts;
    for (int x = 10; x <= 150; ++x)
        pts.append(QPoint(x, 50));
    const QUuid id = m.addLinearMark(s, MarkShape::Freehand, pts);
    ASSERT_FALSE(id.isNull());
    EXPECT_EQ(m.shot(s)->marks[0].points, (QList<QPoint>{{10, 50}, {150, 50}}));
}

TEST(CaptureSetModel, SetMarkPointsMovesArrowAndUndoes)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    const QUuid id = m.addLinearMark(s, MarkShape::Arrow, {{10, 10}, {60, 40}});
    EXPECT_TRUE(m.setMarkPoints(s, id, {{10, 10}, {150, 90}}));
    EXPECT_EQ(m.shot(s)->marks[0].rect, QRect(10, 10, 141, 81));
    EXPECT_FALSE(m.setMarkRect(s, id, QRect(0, 0, 5, 5)));   // an arrow has no frame
    m.undoStack()->undo();
    EXPECT_EQ(m.shot(s)->marks[0].points, (QList<QPoint>{{10, 10}, {60, 40}}));
}

TEST(CaptureSetModel, CropClipsLinesNotTheirBounds)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    // A diagonal: the bounding rectangle touches the crop, but the line itself does not
    m.addLinearMark(s, MarkShape::Arrow, {{0, 0}, {60, 60}});
    // Crosses the crop edge: the tail moves to the edge
    m.addLinearMark(s, MarkShape::Arrow, {{50, 80}, {150, 80}});
    const QRect crop(100, 0, 100, 40);
    const QRect crop2(70, 50, 130, 50);
    EXPECT_EQ(m.marksOutsideCrop(s, crop2), 1);
    EXPECT_EQ(m.marksOutsideCrop(s, crop), 2);
    EXPECT_TRUE(m.setCrop(s, crop2));
    const Shot *shot = m.shot(s);
    ASSERT_EQ(shot->marks.size(), 1);
    EXPECT_EQ(shot->marks[0].points, (QList<QPoint>{{70, 80}, {150, 80}}));
    EXPECT_EQ(shot->marks[0].shape, MarkShape::Arrow);
}

TEST(CaptureSetModel, MarkPenIsStoredAndUndoable)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    const MarkPen blue{QColor(Qt::blue), 6, LineStyle::Dash};
    const QUuid id = m.addMark(s, QRect(10, 10, 50, 40), {}, blue);
    EXPECT_EQ(m.shot(s)->marks[0].pen, blue);
    EXPECT_TRUE(m.setMarkPen(s, id, MarkPen{}));
    EXPECT_EQ(m.shot(s)->marks[0].pen, MarkPen{});
    EXPECT_FALSE(m.setMarkPen(s, id, MarkPen{}));   // no change, no history entry
    m.undoStack()->undo();
    EXPECT_EQ(m.shot(s)->marks[0].pen, blue);
    // Clipping on crop does not lose the pen
    m.setCrop(s, QRect(20, 0, 180, 100));
    EXPECT_EQ(m.shot(s)->marks[0].pen, blue);
}

TEST(CaptureSetModel, PointMarkSharesNumberingAndLeavesWithCrop)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(200, 100));
    m.addMark(s, QRect(10, 10, 20, 20));
    const QUuid p = m.addLinearMark(s, MarkShape::Point, {{150, 50}}, QStringLiteral("здесь"));
    ASSERT_FALSE(p.isNull());
    const Shot *shot = m.shot(s);
    ASSERT_EQ(shot->marks.size(), 2);
    EXPECT_EQ(shot->markIndex(p), 1);   // A2
    EXPECT_EQ(shot->marks[1].points, QList<QPoint>{QPoint(150, 50)});
    // A point outside the shot is not placed, two points are not a point
    EXPECT_TRUE(m.addLinearMark(s, MarkShape::Point, {{250, 50}}).isNull());
    EXPECT_TRUE(m.addLinearMark(s, MarkShape::Point, {{1, 1}, {2, 2}}).isNull());
    // Dragging and cropping
    EXPECT_TRUE(m.setMarkPoints(s, p, {{120, 40}}));
    EXPECT_EQ(m.marksOutsideCrop(s, QRect(0, 0, 100, 100)), 1);
    EXPECT_TRUE(m.setCrop(s, QRect(0, 0, 100, 100)));
    EXPECT_EQ(m.shot(s)->marks.size(), 1);
}
