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
    // An upcoming GitLab release, garbage and an empty answer are not a new version
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "v9.0", "upcoming_release": true})", QStringLiteral("0.1.0")).isEmpty());
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "nightly"})", QStringLiteral("0.1.0")).isEmpty());
    EXPECT_TRUE(UpdateChecker::newerVersion("<html>404</html>", QStringLiteral("0.1.0")).isEmpty());
}

TEST(UpdateChecker, ReadsGitHubReleaseResponse)
{
    const QByteArray github = R"({"tag_name": "v0.3.0", "html_url": "https://github.com/o/markpaste/releases/tag/v0.3.0",
                                  "draft": false, "prerelease": false})";
    EXPECT_EQ(UpdateChecker::newerVersion(github, QStringLiteral("0.2.0")), QStringLiteral("0.3.0"));
    EXPECT_EQ(UpdateChecker::releasePage(github), QUrl(QStringLiteral("https://github.com/o/markpaste/releases/tag/v0.3.0")));
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "v0.3.0", "prerelease": true})", QStringLiteral("0.2.0")).isEmpty());
    EXPECT_TRUE(UpdateChecker::newerVersion(R"({"tag_name": "v0.3.0", "draft": true})", QStringLiteral("0.2.0")).isEmpty());
    // GitLab is still understood; a non-https URL is not opened
    EXPECT_EQ(UpdateChecker::releasePage(R"({"_links": {"self": "https://gitlab.example/r"}})"), QUrl(QStringLiteral("https://gitlab.example/r")));
    EXPECT_TRUE(UpdateChecker::releasePage(R"({"html_url": "file:///etc/passwd"})").isEmpty());
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
    EXPECT_TRUE(QFile::exists(path));   // below the threshold: stays in place

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

    // Stitching gives one image; "files + text" is shown as the same images and text
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
    // The window shows the author and a link to the source code
    bool author = false, homepage = false;
    for (QLabel *l : dlg.findChildren<QLabel *>()) {
        author |= l->text() == QStringLiteral("Andrey Che");
        homepage |= l->text().contains(QStringLiteral("https://example.org/markpaste"));
    }
    EXPECT_TRUE(author);
    EXPECT_TRUE(homepage);
}

TEST(AppIconFile, SvgIsRecognizedByContent)
{
    // Content sniffing (shared-mime-info, glycin in GNOME 49+) looks for "<svg" in the first 256 bytes.
    // Later than that the file is application/xml, and GNOME shows an empty icon (problems.md 2026-10-21).
    QFile f(QStringLiteral(MARKPASTE_SOURCE_DIR "/data/icons/app.svg"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const qsizetype at = f.read(256).indexOf("<svg");
    EXPECT_GE(at, 0);
}
