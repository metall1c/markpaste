#include "TestUtil.h"
#include "core/CaptureSetModel.h"

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
    EXPECT_EQ(shot->markIndex(a3), 1);   // была A3, стала A2 (EDT-2)
}

TEST(CaptureSetModel, MarkIsClampedToVisibleArea)
{
    CaptureSetModel m;
    const QUuid s = m.addShot(makeShot(100, 100));
    const QUuid id = m.addMark(s, rectFromPoints(QPoint(150, 150), QPoint(50, 50)));   // перевёрнутый, за краем
    ASSERT_FALSE(id.isNull());
    EXPECT_EQ(m.shot(s)->marks[0].rect, QRect(50, 50, 50, 50));
    EXPECT_TRUE(m.addMark(s, QRect(200, 200, 10, 10)).isNull());             // целиком снаружи
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
    EXPECT_EQ(m.set().shotIndex(b), 0);   // B стал A (STR-4)

    m.undoStack()->undo();
    EXPECT_EQ(m.set(), full);
    m.undoStack()->undo();                 // метка
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
    m.addMark(s, QRect(0, 0, 20, 20));      // снаружи
    m.addMark(s, QRect(90, 40, 40, 20));    // частично
    m.addMark(s, QRect(110, 50, 10, 10));   // внутри
    m.addRedaction(s, QRect(0, 80, 20, 20)); // снаружи

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
