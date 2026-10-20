#include "TestUtil.h"
#include "app/LogFile.h"
#include "app/UpdateChecker.h"
#include "ui/AboutDialog.h"
#include "ui/PreviewDialog.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <QLabel>

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

TEST(UpdateChecker, ComparesTagWithCurrentVersion)
{
    EXPECT_EQ(UpdateChecker::newerVersion(R"({"tag_name": "v0.2.0"})", QStringLiteral("0.1.0")), QStringLiteral("0.2.0"));
    EXPECT_EQ(UpdateChecker::newerVersion(R"({"tag_name": "0.10.1"})", QStringLiteral("0.9.3")), QStringLiteral("0.10.1"));
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "v0.1.0"})", QStringLiteral("0.1.0")).isEmpty());
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "v0.0.9"})", QStringLiteral("0.1.0")).isEmpty());
    // Будущий релиз в GitLab, мусор и пустой ответ — не новая версия
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "v9.0", "upcoming_release": true})", QStringLiteral("0.1.0")).isEmpty());
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "nightly"})", QStringLiteral("0.1.0")).isEmpty());
    EXPECT_TRUE(UpdateChecker::newerVersion("<html>404</html>", QStringLiteral("0.1.0")).isEmpty());
}

TEST(LogFile, RotationKeepsLimitedNumberOfFiles)
{
    QTemporaryDir tmp;
    const QString path = tmp.filePath(QStringLiteral("markpaste.log"));
    auto write = [&](const QByteArray &text) {
        QFile f(path);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(text);
    };
    write("small");
    LogFile::rotate(path, 100, 2);
    EXPECT_TRUE(QFile::exists(path));   // меньше порога — на месте

    for (const char *gen : {"first", "second", "third"}) {
        write(QByteArray(gen).repeated(50));
        LogFile::rotate(path, 100, 2);
    }
    EXPECT_FALSE(QFile::exists(path));
    QFile one(tmp.filePath(QStringLiteral("markpaste.1.log")));
    QFile two(tmp.filePath(QStringLiteral("markpaste.2.log")));
    ASSERT_TRUE(one.open(QIODevice::ReadOnly));
    ASSERT_TRUE(two.open(QIODevice::ReadOnly));
    EXPECT_TRUE(one.readAll().startsWith("third"));
    EXPECT_TRUE(two.readAll().startsWith("second"));
    EXPECT_FALSE(QFile::exists(tmp.filePath(QStringLiteral("markpaste.3.log"))));
}

TEST(PreviewDialog, ShowsImagesAndTextInPasteOrder)
{
    CaptureSet set;
    for (int i = 0; i < 2; ++i) {
        Shot s = makeShot(80, 60);
        s.id = QUuid::createUuid();
        s.marks << Mark{QUuid::createUuid(), QRect(5, 5, 20, 20), QStringLiteral("note %1").arg(i)};
        set.shots << s;
    }
    PasteOptions opt;
    opt.messageTemplate = MessageTemplate::defaultFor(QLocale(QLocale::English));

    PreviewDialog dlg;
    dlg.showSet(set, opt);
    ASSERT_TRUE(QTest::qWaitFor([&] { return dlg.isReady(); }, 5000));
    EXPECT_EQ(dlg.imageCount(), 2);
    EXPECT_EQ(dlg.messageText(), QStringLiteral("Shot A\nA1 note 0\n\nShot B\nB1 note 1"));

    // Склейка — одна картинка; «файлы + текст» показываются как те же картинки и текст
    opt.mode = PasteMode::Combined;
    dlg.showSet(set, opt);
    ASSERT_TRUE(QTest::qWaitFor([&] { return dlg.isReady(); }, 5000));
    EXPECT_EQ(dlg.imageCount(), 1);
    opt.mode = PasteMode::FilesAndText;
    dlg.showSet(set, opt);
    ASSERT_TRUE(QTest::qWaitFor([&] { return dlg.isReady(); }, 5000));
    EXPECT_EQ(dlg.imageCount(), 2);
}

TEST(AboutDialog, DetailsCarryVersionAndSystem)
{
    AboutInfo info{QStringLiteral("9.8.7"), QStringLiteral("https://example.org/markpaste"), QStringLiteral("Test env"),
                   QStringLiteral("Cap"), QStringLiteral("Keys"), QStringLiteral("Clip"), QString()};
    AboutDialog dlg(info);
    const QString d = dlg.details();
    EXPECT_TRUE(d.startsWith(QStringLiteral("Markpaste 9.8.7\n")));
    EXPECT_TRUE(d.contains(QStringLiteral("Environment: Test env")));
    EXPECT_TRUE(d.contains(QStringLiteral("Capture: Cap")));
    EXPECT_TRUE(d.contains(QStringLiteral("Qt ")));
    // В окне — автор и ссылка на исходный код
    bool author = false, homepage = false;
    for (QLabel *l : dlg.findChildren<QLabel *>()) {
        author |= l->text() == QStringLiteral("Andrey Che");
        homepage |= l->text().contains(QStringLiteral("https://example.org/markpaste"));
    }
    EXPECT_TRUE(author);
    EXPECT_TRUE(homepage);
}
