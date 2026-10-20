#include "TestUtil.h"
#include "core/CaptureSetModel.h"
#include "ui/StripWindow.h"

#include <QApplication>
#include <QDropEvent>
#include <QLayout>
#include <QMenu>
#include <QMimeData>
#include <QScrollArea>
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

    // Бросить снимок в точку списка так, как это сделал бы QDrag
    void drop(const QByteArray &payload, int y)
    {
        QMimeData data;
        data.setData(QString::fromLatin1(StripWindow::kShotMime), payload);
        QDropEvent e(QPointF(20, y), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
        // QApplication::notify пропускает Drop только внутри настоящего QDrag — отдаём напрямую
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
    // A в конец: A B C → B C A
    f.drop(StripWindow::shotDragData(f.ids[0]), f.list()->height() - 1);
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[1], f.ids[2], f.ids[0]}));
    // A (теперь последний) в начало: → A B C
    f.drop(StripWindow::shotDragData(f.ids[0]), 1);
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[0], f.ids[1], f.ids[2]}));
    // Перестановка отменяется одним шагом
    f.model.undoStack()->undo();
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[1], f.ids[2], f.ids[0]}));
}

TEST(StripUi, DropOnOwnPlaceOrFromOtherProcessChangesNothing)
{
    StripFixture f;
    const int undoBefore = f.model.undoStack()->count();
    f.drop(StripWindow::shotDragData(f.ids[1]), f.strip->dropLineY(1) + 1);   // B на своё место
    f.drop("1 " + f.ids[0].toByteArray(QUuid::WithoutBraces), f.list()->height() - 1);   // чужой pid
    EXPECT_EQ(f.order(), f.ids);
    EXPECT_EQ(f.model.undoStack()->count(), undoBefore);
}

namespace {
// Выбрать пункт контекстного меню миниатюры так, как это делает человек: меню открыто
// своим exec(), пункт выбирается клавишей Enter.
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

// Пункт меню меняет набор → лента перестраивается и удаляет миниатюру, пока её меню
// ещё в exec(). Раньше это роняло программу: меню — дочерний объект миниатюры на стеке.
TEST(StripUi, ThumbMenuDeleteDoesNotCrash)
{
    StripFixture f;
    chooseFromThumbMenu(f, 1, QStringLiteral("Delete"));
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[0], f.ids[2]}));
}

TEST(StripUi, ThumbMenuMoveDoesNotCrash)
{
    StripFixture f;
    chooseFromThumbMenu(f, 0, QStringLiteral("Move down"));
    EXPECT_EQ(f.order(), (QList<QUuid>{f.ids[1], f.ids[0], f.ids[2]}));
}
