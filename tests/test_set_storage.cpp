#include "TestUtil.h"
#include "core/SetStorage.h"
#include "render/ShotRenderer.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

static CaptureSet sampleSet()
{
    Shot a = makeShot(120, 80);
    a.id = QUuid::createUuid();
    a.devicePixelRatio = 1.5;
    a.crop = QRect(10, 5, 100, 70);
    a.marks << Mark{QUuid::createUuid(), QRect(20, 20, 30, 20), QStringLiteral("Кнопка\nярче")};
    Mark arrow{QUuid::createUuid(), QRect(), QStringLiteral("сюда"), MarkShape::Arrow, {QPoint(30, 60), QPoint(80, 30)}};
    arrow.rect = QRect(30, 30, 51, 31);
    arrow.pen = MarkPen{QColor(0x1e, 0x88, 0xe5), 5, LineStyle::Dash};
    a.marks << arrow;
    a.redactions << Redaction{QUuid::createUuid(), QRect(60, 10, 20, 10)};

    Shot b = makeShot(50, 40);
    b.id = QUuid::createUuid();
    b.marks << Mark{QUuid::createUuid(), QRect(1, 1, 10, 10), QString(), MarkShape::Freehand,
                    {QPoint(1, 1), QPoint(5, 8), QPoint(10, 3)}, MarkPen{QColor(), 0, LineStyle::Dot}};
    b.marks << Mark{QUuid::createUuid(), QRect(20, 20, 1, 1), QStringLiteral("точка"), MarkShape::Point, {QPoint(20, 20)}};
    CaptureSet set;
    set.shots << a << b;
    return set;
}

TEST(SetStorage, JsonRoundTripKeepsEverythingButPixels)
{
    const CaptureSet set = sampleSet();
    QString error;
    const auto back = SetStorage::fromJson(SetStorage::toJson(set), &error);
    ASSERT_TRUE(back) << error.toStdString();
    CaptureSet expected = set;
    for (Shot &s : expected.shots)
        s.sourcePng.clear();
    EXPECT_EQ(*back, expected);
}

TEST(SetStorage, SessionRoundTripAndPrivatePermissions)
{
    QTemporaryDir tmp;
    const QString dir = tmp.filePath(QStringLiteral("session"));
    const CaptureSet set = sampleSet();
    QString error;
    ASSERT_TRUE(SetStorage::saveSession(set, dir, &error)) << error.toStdString();

#ifndef Q_OS_WIN
    // На Windows права задаёт ACL каталога профиля (%LOCALAPPDATA% — только владелец), а
    // QFile::permissions о нём не знает (NFR-5)
    EXPECT_EQ(QFile::permissions(dir) & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::ExeOther), 0);
    EXPECT_EQ(QFile::permissions(dir + QStringLiteral("/set.json"))
                  & (QFileDevice::ReadGroup | QFileDevice::ReadOther), 0);
#endif

    const auto back = SetStorage::loadSession(dir, &error);
    ASSERT_TRUE(back) << error.toStdString();
    EXPECT_EQ(*back, set);
}

TEST(SetStorage, RemovedShotSourceIsDeletedOnNextSave)
{
    QTemporaryDir tmp;
    CaptureSet set = sampleSet();
    ASSERT_TRUE(SetStorage::saveSession(set, tmp.path()));
    EXPECT_EQ(QDir(tmp.path()).entryList({QStringLiteral("*.png")}).size(), 2);
    set.shots.removeFirst();
    ASSERT_TRUE(SetStorage::saveSession(set, tmp.path()));
    EXPECT_EQ(QDir(tmp.path()).entryList({QStringLiteral("*.png")}),
              QStringList{set.shots.first().id.toString(QUuid::WithoutBraces) + QStringLiteral(".png")});
}

TEST(SetStorage, MissingSessionIsEmptySetAndBrokenJsonIsError)
{
    QTemporaryDir tmp;
    auto empty = SetStorage::loadSession(tmp.filePath(QStringLiteral("none")));
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->isEmpty());

    QFile f(tmp.filePath(QStringLiteral("set.json")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("{ not json");
    f.close();
    QString error;
    EXPECT_FALSE(SetStorage::loadSession(tmp.path(), &error));
    EXPECT_FALSE(error.isEmpty());

    EXPECT_FALSE(SetStorage::fromJson(R"({"version": 99, "shots": []})"));
}

TEST(SetStorage, ShotWithoutSourceFileIsSkipped)
{
    QTemporaryDir tmp;
    const CaptureSet set = sampleSet();
    ASSERT_TRUE(SetStorage::saveSession(set, tmp.path()));
    QFile::remove(tmp.filePath(set.shots[0].id.toString(QUuid::WithoutBraces) + QStringLiteral(".png")));
    const auto back = SetStorage::loadSession(tmp.path());
    ASSERT_TRUE(back);
    ASSERT_EQ(back->shots.size(), 1);
    EXPECT_EQ(back->shots[0].id, set.shots[1].id);
}

TEST(SetStorage, ExportWritesBakedPngsAndMarkdown)
{
    QTemporaryDir tmp;
    const CaptureSet set = sampleSet();
    MessageTemplate tpl = MessageTemplate::defaultFor(QLocale(QLocale::English));
    QString error;
    const QStringList files = SetStorage::exportSet(set, RenderStyle(), tpl, tmp.path(), &error);
    ASSERT_EQ(files.size(), 3) << error.toStdString();

    // Картинка — запечённая: размер обрезки, а не исходника
    const QImage a(tmp.filePath(QStringLiteral("A.png")));
    EXPECT_EQ(a.size(), set.shots[0].crop.size());
    EXPECT_EQ(a, renderShot(set.shots[0], 0, RenderStyle()).convertToFormat(a.format()));

    QFile md(tmp.filePath(QStringLiteral("message.md")));
    ASSERT_TRUE(md.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(md.readAll());
    EXPECT_EQ(text, QStringLiteral("![A](A.png)\n![B](B.png)\n\nShot A\nA1 Кнопка\nярче\nA2 сюда\n\nShot B\nB2 точка\n"));

    tpl.textBeforeImages = true;
    EXPECT_TRUE(SetStorage::exportMarkdown(set, tpl).endsWith(QStringLiteral("B2 точка\n\n![A](A.png)\n![B](B.png)\n")));
}
