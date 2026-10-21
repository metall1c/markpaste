#include "TestUtil.h"
#include "ui/CaptureOverlay.h"
#include "ui/PenPicker.h"
#include "ui/ShotEditor.h"

#include <QApplication>
#include <QMenu>
#include <QScreen>
#include <QPlainTextEdit>
#include <QTest>
#include <QToolButton>
#include <QTranslator>

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

namespace {

struct EditorFixture {
    CaptureSetModel model;
    ShotEditor editor{&model};
    QUuid shot;
    int done = 0;

    EditorFixture()
    {
        shot = model.addShot(makeShot(400, 300));
        QObject::connect(&editor, &ShotEditor::doneRequested, [this] { ++done; });
        editor.showShot(shot);
        editor.activateWindow();
        QTest::qWait(50);
    }
    ShotCanvas *canvas() { return editor.findChild<ShotCanvas *>(); }
};

} // namespace

TEST(ShotEditorUi, CtrlEnterOnCanvasFinishes)
{
    EditorFixture f;
    QTest::keyClick(f.canvas(), Qt::Key_Return, Qt::ControlModifier);
    EXPECT_EQ(f.done, 1);
}

TEST(ShotEditorUi, CtrlEnterInCommentSavesAndFinishes)
{
    EditorFixture f;
    const QUuid mark = f.model.addMark(f.shot, QRect(10, 10, 50, 40));
    f.canvas()->editMark(mark);
    QTest::qWait(20);
    auto *edit = f.canvas()->findChild<QPlainTextEdit *>();
    ASSERT_NE(edit, nullptr);
    ASSERT_TRUE(edit->isVisible());
    edit->setFocus();
    edit->insertPlainText(QStringLiteral("кнопку ярче"));   // keyClicks cannot type Cyrillic
    QTest::keyClick(edit, Qt::Key_Return, Qt::ControlModifier);

    EXPECT_EQ(f.done, 1);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].comment, QStringLiteral("кнопку ярче"));
}

TEST(ShotEditorUi, PlainEnterInCommentOnlySaves)
{
    EditorFixture f;
    const QUuid mark = f.model.addMark(f.shot, QRect(10, 10, 50, 40));
    f.canvas()->editMark(mark);
    QTest::qWait(20);
    auto *edit = f.canvas()->findChild<QPlainTextEdit *>();
    edit->setFocus();
    QTest::keyClicks(edit, QStringLiteral("x"));
    QTest::keyClick(edit, Qt::Key_Return);
    EXPECT_EQ(f.done, 0);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].comment, QStringLiteral("x"));
    EXPECT_TRUE(f.canvas()->selectedMark().isNull());   // Enter drops the selection
}

namespace {
struct FreshFixture : EditorFixture {
    FreshFixture() { editor.showShot(shot, true); QTest::qWait(20); }
};
} // namespace

TEST(ShotEditorUi, EscDiscardsFreshShot)
{
    FreshFixture f;
    QTest::keyClick(f.canvas(), Qt::Key_Escape);
    EXPECT_EQ(f.model.shot(f.shot), nullptr);
    EXPECT_FALSE(f.editor.isVisible());
}

TEST(ShotEditorUi, EscOnReopenedShotOnlyCloses)
{
    EditorFixture f;   // opened as from the strip (not fresh)
    QTest::keyClick(f.canvas(), Qt::Key_Escape);
    EXPECT_NE(f.model.shot(f.shot), nullptr);
    EXPECT_EQ(f.done, 1);
}

TEST(ShotEditorUi, EscInCommentClosesOnlyTheComment)
{
    FreshFixture f;
    const QUuid mark = f.model.addMark(f.shot, QRect(10, 10, 50, 40));
    f.canvas()->editMark(mark);
    QTest::qWait(20);
    auto *edit = f.canvas()->findChild<QPlainTextEdit *>();
    edit->setFocus();
    QTest::keyClick(edit, Qt::Key_Escape);
    EXPECT_FALSE(edit->isVisible());
    EXPECT_NE(f.model.shot(f.shot), nullptr);   // the shot is alive
    EXPECT_EQ(f.done, 0);
}

TEST(ShotEditorUi, DoneMakesShotNotFresh)
{
    FreshFixture f;
    QTest::keyClick(f.canvas(), Qt::Key_Return, Qt::ControlModifier);
    EXPECT_FALSE(f.editor.isFreshShot());
}

// The shot was deleted with "Delete shot", by undo (Ctrl+Z) or from the strip: the editor closes,
// as with Esc, and does not open another strip shot by itself (docs/problems.md, 2026-10-20).
TEST(ShotEditorUi, RemovedShotClosesEditor)
{
    EditorFixture f;                                    // A is accepted
    const QUuid b = f.model.addShot(makeShot(400, 300));
    f.editor.showShot(b, true);                         // B was just taken
    QTest::qWait(20);
    f.model.removeShot(b);                              // Ctrl+Z or deletion from the strip
    EXPECT_FALSE(f.editor.isVisible());
    EXPECT_FALSE(f.editor.isFreshShot());
    EXPECT_NE(f.model.shot(f.shot), nullptr);           // A is in place
}

TEST(ShotEditorUi, DeleteShotButtonRemovesAndCloses)
{
    EditorFixture f;
    const QUuid b = f.model.addShot(makeShot(400, 300));
    f.editor.showShot(b, true);
    QTest::qWait(20);
    QToolButton *del = nullptr;
    for (QToolButton *t : f.editor.findChildren<QToolButton *>())
        if (t->text() == QStringLiteral("Delete shot"))
            del = t;
    ASSERT_NE(del, nullptr);
    del->click();
    EXPECT_EQ(f.model.shot(b), nullptr);
    EXPECT_NE(f.model.shot(f.shot), nullptr);
    EXPECT_FALSE(f.editor.isVisible());

    // A opened later from the strip is not "fresh": Esc only closes it
    f.editor.showShot(f.shot);
    QTest::qWait(20);
    QTest::keyClick(f.canvas(), Qt::Key_Escape);
    EXPECT_NE(f.model.shot(f.shot), nullptr);
}

TEST(Translations, RussianLoadsForEditor)
{
    // Translations are tied to the class name with its namespace (mp::ShotEditor).
    // After the ps → mp rename the .ts contexts drifted apart from the code, and the UI became English.
    QTranslator tr;
    ASSERT_TRUE(tr.load(QStringLiteral(":/i18n/markpaste_ru.qm")));
    EXPECT_EQ(tr.translate("mp::ShotEditor", "Done"), QStringLiteral("Готово"));
    EXPECT_EQ(tr.translate("mp::StripWindow", "Paste set"), QStringLiteral("Вставить набор"));
    EXPECT_EQ(tr.translate("mp::ShotEditor", "Arrow"), QStringLiteral("Стрелка"));
}

namespace {
// Dragging in source coordinates. The editor window is larger than the shot, the scale is 1:1.
void dragSource(ShotCanvas *c, const QList<QPoint> &path)
{
    // The shot offset in the window: the source point (0,0) is at the center minus half the shot
    const QPoint origin((c->width() - 400) / 2, (c->height() - 300) / 2);
    QTest::mousePress(c, Qt::LeftButton, {}, origin + path.first());
    for (const QPoint &p : path.mid(1)) {
        QMouseEvent move(QEvent::MouseMove, QPointF(origin + p), c->mapToGlobal(QPointF(origin + p)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(c, &move);
    }
    QTest::mouseRelease(c, Qt::LeftButton, {}, origin + path.last());
}
} // namespace

TEST(ShotEditorUi, ArrowToolCreatesArrowMark)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    QTest::keyClick(f.canvas(), Qt::Key_A);
    ASSERT_EQ(f.canvas()->tool(), ShotCanvas::Tool::Arrow);
    dragSource(f.canvas(), {{50, 50}, {100, 80}, {150, 100}});
    const Shot *s = f.model.shot(f.shot);
    ASSERT_EQ(s->marks.size(), 1);
    EXPECT_EQ(s->marks[0].shape, MarkShape::Arrow);
    EXPECT_EQ(s->marks[0].points, (QList<QPoint>{{50, 50}, {150, 100}}));
}

TEST(ShotEditorUi, PenLoopIsHitOnlyByLine)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    QTest::keyClick(f.canvas(), Qt::Key_P);
    dragSource(f.canvas(), {{100, 100}, {200, 100}, {200, 200}, {100, 200}, {100, 105}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 1);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].shape, MarkShape::Freehand);
    f.canvas()->commitComment();

    // Inside the loop a new line is drawn, the old one is not dragged
    dragSource(f.canvas(), {{130, 150}, {170, 150}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 2);
    f.canvas()->commitComment();

    // By the line, the whole drawing is dragged
    const QList<QPoint> before = f.model.shot(f.shot)->marks[0].points;
    dragSource(f.canvas(), {{150, 100}, {160, 110}});
    const QList<QPoint> after = f.model.shot(f.shot)->marks[0].points;
    ASSERT_EQ(after.size(), before.size());
    EXPECT_EQ(after.first(), before.first() + QPoint(10, 10));
}

TEST(ShotEditorUi, PenAppliesToNewAndSelectedMarks)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    auto *picker = f.editor.findChild<PenPicker *>();
    ASSERT_NE(picker, nullptr);
    const MarkPen red{QColor(Qt::red), 5, LineStyle::Solid};
    picker->setPen(red);
    emit picker->penChanged(red);   // like a choice in the panel

    dragSource(f.canvas(), {{20, 20}, {120, 100}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 1);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen, red);   // a new mark uses the current pen

    // The selected mark is recolored with one edit
    const MarkPen dotted{QColor(Qt::blue), 2, LineStyle::Dot};
    emit picker->penChanged(dotted);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen, dotted);
    f.model.undoStack()->undo();
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen, red);
}

namespace {
QAction *frameAction(ShotEditor &editor, const QString &text)
{
    for (QToolButton *b : editor.findChildren<QToolButton *>())
        if (b->text() == QStringLiteral("Frame shape") && b->menu())
            for (QAction *a : b->menu()->actions())
                if (a->text() == text)
                    return a;
    return nullptr;
}
} // namespace

// Frame shape (ADR 0020): the choice in the "Mark" menu applies to new frames and to the selected one
TEST(ShotEditorUi, FrameShapeAppliesToNewAndSelectedMarks)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    QAction *oval = frameAction(f.editor, QStringLiteral("Oval"));
    QAction *rounded = frameAction(f.editor, QStringLiteral("Rounded rectangle"));
    ASSERT_NE(oval, nullptr);
    ASSERT_NE(rounded, nullptr);
    oval->trigger();
    EXPECT_EQ(f.canvas()->pen().frame, FrameShape::Ellipse);
    EXPECT_TRUE(oval->isChecked());

    dragSource(f.canvas(), {{20, 20}, {120, 100}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 1);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen.frame, FrameShape::Ellipse);

    // The frame just drawn is selected: its shape changes with one edit
    rounded->trigger();
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen.frame, FrameShape::Rounded);
    f.model.undoStack()->undo();
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen.frame, FrameShape::Ellipse);
}

TEST(ShotEditorUi, FrameShapeLeavesSelectedArrowAlone)
{
    EditorFixture f;
    const QUuid arrow = f.model.addLinearMark(f.shot, MarkShape::Arrow, {{10, 10}, {100, 80}});
    f.canvas()->editMark(arrow);
    const int steps = f.model.undoStack()->count();
    QAction *oval = frameAction(f.editor, QStringLiteral("Oval"));
    ASSERT_NE(oval, nullptr);
    oval->trigger();
    // An arrow has no frame shape, so no empty history step either
    EXPECT_EQ(f.model.undoStack()->count(), steps);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen.frame, FrameShape::Rect);
    EXPECT_EQ(f.canvas()->pen().frame, FrameShape::Ellipse);   // and new frames are ellipses
}

TEST(ShotEditorUi, OvalIsHitInsideButNotInItsCorner)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    f.model.addMark(f.shot, QRect(100, 100, 200, 100), {}, MarkPen{QColor(), 0, LineStyle::Solid, FrameShape::Ellipse});
    // A drag from a corner of the bounding rect misses the ellipse: a new frame, not a move
    dragSource(f.canvas(), {{108, 192}, {150, 150}, {180, 170}});
    EXPECT_EQ(f.model.shot(f.shot)->marks.size(), 2);
    // From the middle: the ellipse moves
    const QRect before = f.model.shot(f.shot)->marks[0].rect;
    dragSource(f.canvas(), {{200, 150}, {210, 160}});
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].rect, before.translated(10, 10));
}

// After Enter the choice of shape and color applies to the next mark: the drawn one does not change
TEST(ShotEditorUi, StyleChosenAfterEnterIsForNextMark)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    dragSource(f.canvas(), {{20, 20}, {120, 100}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 1);
    auto *edit = f.canvas()->findChild<QPlainTextEdit *>();
    ASSERT_TRUE(edit && edit->isVisible());
    edit->setFocus();
    QTest::keyClicks(edit, QStringLiteral("a"));
    QTest::keyClick(edit, Qt::Key_Return);
    const MarkPen drawn = f.model.shot(f.shot)->marks[0].pen;

    auto *picker = f.editor.findChild<PenPicker *>();
    MarkPen red = picker->pen();
    red.color = Qt::red;
    picker->setPen(red);
    emit picker->penChanged(red);
    for (QToolButton *b : f.editor.findChildren<QToolButton *>())
        if (b->text() == QStringLiteral("Frame shape") && b->menu())
            for (QAction *a : b->menu()->actions())
                if (a->text() == QStringLiteral("Oval"))
                    a->trigger();
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen, drawn);

    dragSource(f.canvas(), {{200, 150}, {300, 220}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 2);
    EXPECT_EQ(f.model.shot(f.shot)->marks[1].pen.color, QColor(Qt::red));
    EXPECT_EQ(f.model.shot(f.shot)->marks[1].pen.frame, FrameShape::Ellipse);
}

TEST(ShotEditorUi, SelectingMarkShowsItsPen)
{
    EditorFixture f;
    const MarkPen green{QColor(Qt::green), 8, LineStyle::Dash};
    const QUuid mark = f.model.addMark(f.shot, QRect(10, 10, 50, 40), {}, green);
    f.canvas()->editMark(mark);
    EXPECT_EQ(f.editor.findChild<PenPicker *>()->pen(), green);
    EXPECT_EQ(f.canvas()->pen(), green);
}

TEST(ShotEditorUi, EachShotStartsWithMarkTool)
{
    EditorFixture f;
    f.canvas()->setTool(ShotCanvas::Tool::Arrow);
    f.editor.showShot(f.shot);
    EXPECT_EQ(f.canvas()->tool(), ShotCanvas::Tool::Mark);
}

TEST(ShotEditorUi, PointToolPlacesPointByClick)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    QTest::keyClick(f.canvas(), Qt::Key_D);
    ASSERT_EQ(f.canvas()->tool(), ShotCanvas::Tool::Point);
    dragSource(f.canvas(), {{120, 90}});   // a click without dragging
    const Shot *s = f.model.shot(f.shot);
    ASSERT_EQ(s->marks.size(), 1);
    EXPECT_EQ(s->marks[0].shape, MarkShape::Point);
    EXPECT_EQ(s->marks[0].points, QList<QPoint>{QPoint(120, 90)});
    f.canvas()->commitComment();

    // A click on the point's badge picks it up rather than placing a new one; dragging moves it
    dragSource(f.canvas(), {{125, 92}, {160, 110}, {200, 130}});
    s = f.model.shot(f.shot);
    ASSERT_EQ(s->marks.size(), 1);
    EXPECT_EQ(s->marks[0].points, QList<QPoint>{QPoint(195, 128)});
}

// A tool key in the middle of a crop drag: the arrow used to take the first point
// from an empty list, and the program crashed
TEST(ShotEditorUi, SwitchingToolDuringDragIsSafe)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    ShotCanvas *c = f.canvas();
    c->setTool(ShotCanvas::Tool::Crop);
    const QPoint origin((c->width() - 400) / 2, (c->height() - 300) / 2);
    QTest::mousePress(c, Qt::LeftButton, {}, origin + QPoint(20, 20));
    c->setTool(ShotCanvas::Tool::Arrow);   // like the A key, the button is still held
    QMouseEvent move(QEvent::MouseMove, QPointF(origin + QPoint(120, 90)), c->mapToGlobal(QPointF(origin + QPoint(120, 90))),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(c, &move);
    QTest::mouseRelease(c, Qt::LeftButton, {}, origin + QPoint(120, 90));
    EXPECT_TRUE(f.model.shot(f.shot)->marks.isEmpty());
}

// The overlay was closed from outside (Alt+F4): the capture session must end with a cancel,
// otherwise the next capture will not start
TEST(CaptureOverlayUi, ClosingOverlayWindowCancelsSession)
{
    ScreenFrame frame;
    frame.screenName = QGuiApplication::primaryScreen()->name();
    frame.logicalGeometry = QGuiApplication::primaryScreen()->geometry();
    frame.image = noiseImage(200, 100);
    CaptureSession session({frame}, false);
    int cancelled = 0;
    QObject::connect(&session, &CaptureSession::cancelled, [&] { ++cancelled; });
    session.start();
    QTest::qWait(20);
    OverlayWidget *overlay = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets())
        if (auto *o = qobject_cast<OverlayWidget *>(w); o && o->isVisible())
            overlay = o;
    ASSERT_NE(overlay, nullptr);
    overlay->close();
    EXPECT_EQ(cancelled, 1);
}
