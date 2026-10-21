#include "TestUtil.h"
#include "core/CaptureSetModel.h"
#include "ui/StripWindow.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDropEvent>
#include <QLayout>
#include <QMenu>
#include <QMimeData>
#include <QScrollArea>
#include <QSettings>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

namespace {

struct StripFixture {
    CaptureSetModel model;
    QList<QUuid> ids;
    std::unique_ptr<StripWindow> strip;

    StripFixture()
    {
        for (int i = 0; i < 3; ++i)
            ids << model.addShot(makeShot(200, 100));
        strip = std::make_unique<StripWindow>(&model);
        strip->show();
        QApplication::processEvents();
    }

    QWidget *list() const { return strip->findChild<QScrollArea *>()->widget(); }

    QList<QUuid> order() const
    {
        QList<QUuid> out;
        for (const Shot &s : model.set().shots)
            out << s.id;
        return out;
    }

    // Drop a shot at a point of the list the way QDrag would
    void drop(const QByteArray &payload, int y)
    {
        QMimeData data;
        data.setData(QString::fromLatin1(StripWindow::kShotMime), payload);
        QDropEvent e(QPointF(20, y), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
        // QApplication::notify lets a Drop through only inside a real QDrag, so deliver it directly
        static_cast<QObject *>(list())->event(&e);
        QTest::qWait(20);
    }
};

} // namespace

TEST(StripUi, DropIndexFollowsThumbnailMiddles)
{
    StripFixture f;
    EXPECT_EQ(f.strip->dropIndexAt(0), 0);
    EXPECT_EQ(f.strip->dropIndexAt(f.list()->height() + 100), 3);
}

TEST(StripUi, DroppingShotReordersSet)
{
    StripFixture f;
    // A to the end: A B C → B C A
    f.drop(StripWindow::shotDragData(f.ids[0]), f.list()->height() - 1);
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[1], f.ids[2], f.ids[0]}));
    // A (now last) to the start: → A B C
    f.drop(StripWindow::shotDragData(f.ids[0]), 1);
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[0], f.ids[1], f.ids[2]}));
    // The reorder is undone in one step
    f.model.undoStack()->undo();
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[1], f.ids[2], f.ids[0]}));
}

TEST(StripUi, DropOnOwnPlaceOrFromOtherProcessChangesNothing)
{
    StripFixture f;
    const int undoBefore = f.model.undoStack()->count();
    f.drop(StripWindow::shotDragData(f.ids[1]), f.strip->dropLineY(1) + 1);   // B onto its own place
    f.drop("1 " + f.ids[0].toByteArray(QUuid::WithoutBraces), f.list()->height() - 1);   // a foreign pid
    EXPECT_EQ(f.order(), f.ids);
    EXPECT_EQ(f.model.undoStack()->count(), undoBefore);
}

namespace {
// Choose a thumbnail context menu item the way a person does: the menu is open
// in its own exec(), the item is chosen with Enter.
void chooseFromThumbMenu(StripFixture &f, int thumbIndex, const QString &actionText)
{
    QWidget *thumb = f.list()->layout()->itemAt(thumbIndex)->widget();
    ASSERT_NE(thumb, nullptr);
    bool chosen = false;
    QTimer::singleShot(50, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        ASSERT_NE(menu, nullptr);
        for (QAction *a : menu->actions())
            if (a->text() == actionText) {
                menu->setActiveAction(a);
                QTest::keyClick(menu, Qt::Key_Return);
                chosen = true;
                return;
            }
    });
    QTest::mouseClick(thumb, Qt::RightButton, {}, QPoint(20, 20));
    QTest::qWait(20);
    EXPECT_TRUE(chosen);
}
} // namespace

// A menu item changes the set → the strip rebuilds and deletes the thumbnail while its menu
// is still in exec(). This used to crash the program: the menu is the thumbnail's child object on the stack.
TEST(StripUi, ThumbMenuDeleteDoesNotCrash)
{
    StripFixture f;
    chooseFromThumbMenu(f, 1, QStringLiteral("Delete"));
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[0], f.ids[2]}));
}

// The trash button on a thumbnail deletes the shot right away, without confirmation; undone in one step
TEST(StripUi, ThumbTrashButtonDeletesShot)
{
    StripFixture f;
    QWidget *thumb = f.list()->layout()->itemAt(1)->widget();
    ASSERT_NE(thumb, nullptr);
    QAbstractButton *trash = nullptr;
    for (auto *b : thumb->findChildren<QAbstractButton *>())
        if (b->text() == QStringLiteral("Delete shot"))
            trash = b;
    ASSERT_NE(trash, nullptr);
    EXPECT_TRUE(trash->isVisible());
    EXPECT_GT(trash->x(), thumb->width() / 2);   // in the top-right corner
    QTest::mouseClick(trash, Qt::LeftButton);
    QTest::qWait(20);
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[0], f.ids[2]}));
    f.model.undoStack()->undo();
    EXPECT_EQ(f.order(), f.ids);
}

TEST(StripUi, ThumbMenuMoveDoesNotCrash)
{
    StripFixture f;
    chooseFromThumbMenu(f, 0, QStringLiteral("Move down"));
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[1], f.ids[0], f.ids[2]}));
}

// A long message ("Click into the chat window first…") widened the strip: on Windows it
// went past the right screen edge, and the text was cut off
TEST(StripUi, LongMessageWrapsInsteadOfWideningStrip)
{
    StripFixture f;
    f.strip->adjustSize();
    QApplication::processEvents();
    const int width = f.strip->width();
    f.strip->showMessage(QStringLiteral("Click into the chat first: a Markpaste window is active, "
                                        "and this text is long enough to need several lines"));
    QApplication::processEvents();
    EXPECT_EQ(f.strip->width(), width);
}

#ifdef Q_OS_WIN
// A strip the program places itself (Windows) grew because of a message: its bottom stays in the work area
TEST(StripUi, GrownStripStaysInsideWorkArea)
{
    // Without an organization name QSettings saves nothing on Windows
    const QString org = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(QStringLiteral("markpaste-tests"));
    QSettings().setValue(QStringLiteral("strip/offset"), 100000);   // "at the very bottom"
    StripFixture f;
    f.strip->hide();
    f.strip->present();
    QApplication::processEvents();
    f.strip->present();   // the size has settled: snap flush to the bottom
    QApplication::processEvents();
    const QRect area = f.strip->screen()->availableGeometry();
    ASSERT_EQ(f.strip->geometry().bottom(), area.bottom());
    f.strip->showMessage(QStringLiteral("One more line\nand another\nand a third"));
    f.strip->setProgress(0, 3);
    QApplication::processEvents();
    EXPECT_LE(f.strip->geometry().bottom(), area.bottom());
    QSettings().remove(QStringLiteral("strip"));
    QCoreApplication::setOrganizationName(org);
}
#endif
