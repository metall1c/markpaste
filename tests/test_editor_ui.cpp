#include "TestUtil.h"
#include "ui/ShotEditor.h"

#include <QApplication>
#include <QPlainTextEdit>
#include <QTest>

#include <gtest/gtest.h>

using namespace ps;
using namespace ps::test;

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
}
