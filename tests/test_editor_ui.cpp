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
    edit->insertPlainText(QStringLiteral("кнопку ярче"));   // keyClicks не умеет кириллицу
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
    EXPECT_TRUE(f.canvas()->selectedMark().isNull());   // Enter снимает выделение
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
    EditorFixture f;   // открыт как из ленты (не свежий)
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
    EXPECT_NE(f.model.shot(f.shot), nullptr);   // снимок жив
    EXPECT_EQ(f.done, 0);
}

TEST(ShotEditorUi, DoneMakesShotNotFresh)
{
    FreshFixture f;
    QTest::keyClick(f.canvas(), Qt::Key_Return, Qt::ControlModifier);
    EXPECT_FALSE(f.editor.isFreshShot());
}

// Снимок удалили кнопкой «Удалить снимок», отменой (Ctrl+Z) или из ленты — редактор закрывается,
// как по Esc, и не открывает сам другой снимок ленты (docs/problems.md, 2026-10-20).
TEST(ShotEditorUi, RemovedShotClosesEditor)
{
    EditorFixture f;                                    // A принят
    const QUuid b = f.model.addShot(makeShot(400, 300));
    f.editor.showShot(b, true);                         // B только что снят
    QTest::qWait(20);
    f.model.removeShot(b);                              // Ctrl+Z или удаление из ленты
    EXPECT_FALSE(f.editor.isVisible());
    EXPECT_FALSE(f.editor.isFreshShot());
    EXPECT_NE(f.model.shot(f.shot), nullptr);           // A на месте
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

    // Открытый потом из ленты A не «свежий»: Esc его только закрывает
    f.editor.showShot(f.shot);
    QTest::qWait(20);
    QTest::keyClick(f.canvas(), Qt::Key_Escape);
    EXPECT_NE(f.model.shot(f.shot), nullptr);
}

TEST(Translations, RussianLoadsForEditor)
{
    // Переводы привязаны к имени класса с пространством имён (mp::ShotEditor).
    // После переименования ps → mp контексты в .ts разошлись с кодом, и интерфейс стал английским.
    QTranslator tr;
    ASSERT_TRUE(tr.load(QStringLiteral(":/i18n/markpaste_ru.qm")));
    EXPECT_EQ(tr.translate("mp::ShotEditor", "Done"), QStringLiteral("Готово"));
    EXPECT_EQ(tr.translate("mp::StripWindow", "Paste set"), QStringLiteral("Вставить набор"));
    EXPECT_EQ(tr.translate("mp::ShotEditor", "Arrow"), QStringLiteral("Стрелка"));
}

namespace {
// Перетаскивание в координатах исходника. Окно редактора крупнее снимка, масштаб 1:1.
void dragSource(ShotCanvas *c, const QList<QPoint> &path)
{
    // Смещение снимка в окне: точка исходника (0,0) лежит в центре минус половина снимка
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

    // Внутри обвода рисуется новая линия, а не тащится старая
    dragSource(f.canvas(), {{130, 150}, {170, 150}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 2);
    f.canvas()->commitComment();

    // За линию — перетаскивается весь рисунок
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
    emit picker->penChanged(red);   // как выбор в панели

    dragSource(f.canvas(), {{20, 20}, {120, 100}});
    ASSERT_EQ(f.model.shot(f.shot)->marks.size(), 1);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen, red);   // новая метка — текущим пером

    // Выделенная метка перекрашивается одной правкой
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

// Форма рамки (ADR 0020): выбор в меню у «Метки» — для новых рамок и для выделенной
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

    // Только что нарисованная рамка выделена: форма меняется у неё одной правкой
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
    // У стрелки формы рамки нет — пустого шага в истории тоже
    EXPECT_EQ(f.model.undoStack()->count(), steps);
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].pen.frame, FrameShape::Rect);
    EXPECT_EQ(f.canvas()->pen().frame, FrameShape::Ellipse);   // а новые рамки — овалы
}

TEST(ShotEditorUi, OvalIsHitInsideButNotInItsCorner)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    f.model.addMark(f.shot, QRect(100, 100, 200, 100), {}, MarkPen{QColor(), 0, LineStyle::Solid, FrameShape::Ellipse});
    // Перетаскивание из угла охватывающего прямоугольника — мимо овала: новая рамка, а не сдвиг
    dragSource(f.canvas(), {{108, 192}, {150, 150}, {180, 170}});
    EXPECT_EQ(f.model.shot(f.shot)->marks.size(), 2);
    // Из середины — сдвиг овала
    const QRect before = f.model.shot(f.shot)->marks[0].rect;
    dragSource(f.canvas(), {{200, 150}, {210, 160}});
    EXPECT_EQ(f.model.shot(f.shot)->marks[0].rect, before.translated(10, 10));
}

// После Enter выбор формы и цвета — для следующей метки: нарисованная не меняется
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
    dragSource(f.canvas(), {{120, 90}});   // щелчок без перетаскивания
    const Shot *s = f.model.shot(f.shot);
    ASSERT_EQ(s->marks.size(), 1);
    EXPECT_EQ(s->marks[0].shape, MarkShape::Point);
    EXPECT_EQ(s->marks[0].points, QList<QPoint>{QPoint(120, 90)});
    f.canvas()->commitComment();

    // Щелчок по значку точки берёт её, а не ставит новую; перетаскивание двигает
    dragSource(f.canvas(), {{125, 92}, {160, 110}, {200, 130}});
    s = f.model.shot(f.shot);
    ASSERT_EQ(s->marks.size(), 1);
    EXPECT_EQ(s->marks[0].points, QList<QPoint>{QPoint(195, 128)});
}

// Клавиша инструмента посреди перетаскивания обрезки: раньше стрелка брала первую точку
// из пустого списка и программа падала
TEST(ShotEditorUi, SwitchingToolDuringDragIsSafe)
{
    EditorFixture f;
    f.editor.resize(900, 600);
    QTest::qWait(20);
    ShotCanvas *c = f.canvas();
    c->setTool(ShotCanvas::Tool::Crop);
    const QPoint origin((c->width() - 400) / 2, (c->height() - 300) / 2);
    QTest::mousePress(c, Qt::LeftButton, {}, origin + QPoint(20, 20));
    c->setTool(ShotCanvas::Tool::Arrow);   // как клавиша A, кнопка ещё зажата
    QMouseEvent move(QEvent::MouseMove, QPointF(origin + QPoint(120, 90)), c->mapToGlobal(QPointF(origin + QPoint(120, 90))),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(c, &move);
    QTest::mouseRelease(c, Qt::LeftButton, {}, origin + QPoint(120, 90));
    EXPECT_TRUE(f.model.shot(f.shot)->marks.isEmpty());
}

// Оверлей закрыли извне (Alt+F4): сеанс захвата должен завершиться отменой,
// иначе следующий захват не запустится
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
