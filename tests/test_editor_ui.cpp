#include "TestUtil.h"
#include "ui/ShotEditor.h"

#include <QApplication>
#include <QPlainTextEdit>
#include <QTest>
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

TEST(Translations, RussianLoadsForEditor)
{
    // Переводы привязаны к имени класса с пространством имён (mp::ShotEditor).
    // После переименования ps → mp контексты в .ts разошлись с кодом, и интерфейс стал английским.
    QTranslator tr;
    ASSERT_TRUE(tr.load(QStringLiteral(":/i18n/markpaste_ru.qm")));
    EXPECT_EQ(tr.translate("mp::ShotEditor", "Done"), QStringLiteral("Готово"));
    EXPECT_EQ(tr.translate("mp::StripWindow", "Paste set"), QStringLiteral("Вставить набор"));
}
