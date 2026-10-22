#include "platform/Autostart.h"
#include "platform/PlatformTypes.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include <gtest/gtest.h>

using namespace mp;

TEST(Shortcuts, AltWithoutCtrlAndPlainPaste)
{
    EXPECT_TRUE(triggersHaveAltWithoutCtrl(QStringLiteral("Alt+V")));
    EXPECT_TRUE(triggersHaveAltWithoutCtrl(QStringLiteral("Ctrl+Alt+V, Alt+V")));   // KDE reports a list
    EXPECT_FALSE(triggersHaveAltWithoutCtrl(QStringLiteral("Primary+Alt+V")));
    EXPECT_FALSE(triggersHaveAltWithoutCtrl(QStringLiteral("Ctrl+Shift+V")));
    EXPECT_FALSE(triggersHaveAltWithoutCtrl(QString()));

    EXPECT_TRUE(triggersHavePlainPaste(QStringLiteral("Primary+V")));
    EXPECT_TRUE(triggersHavePlainPaste(QStringLiteral("Control+v")));
    EXPECT_TRUE(triggersHavePlainPaste(QStringLiteral("Ctrl+Alt+V, Ctrl+V")));
    EXPECT_FALSE(triggersHavePlainPaste(QStringLiteral("Ctrl+Shift+V")));
    EXPECT_FALSE(triggersHavePlainPaste(QStringLiteral("Ctrl+Alt+V")));
    EXPECT_FALSE(triggersHavePlainPaste(QStringLiteral("V")));

#ifdef Q_OS_WIN
    EXPECT_EQ(defaultPasteHotkey(), QStringLiteral("Primary+V"));
#else
    EXPECT_EQ(defaultPasteHotkey(), QStringLiteral("Primary+Alt+V"));
#endif
}

#ifndef Q_OS_WIN
TEST(Autostart, WritesAndRemovesDesktopFile)
{
    // Test mode: ~/.qttest/config instead of the real ~/.config
    QStandardPaths::setTestModeEnabled(true);
    const QString id = QStringLiteral("su._525.MarkpasteTest");
    const QString path = autostartFilePath(id);
    ASSERT_TRUE(path.contains(QStringLiteral("qttest")));
    QFile::remove(path);

    EXPECT_FALSE(isAutostartEnabled(id));
    ASSERT_TRUE(setAutostartEnabled(id, true, QStringLiteral("/opt/My Apps/mark\"paste")));
    EXPECT_TRUE(isAutostartEnabled(id));
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    // A quote inside the path: \" per the Exec specification, then \ is doubled as in any value
    EXPECT_TRUE(text.contains(QStringLiteral("Exec=\"/opt/My Apps/mark\\\\\"paste\"\n"))) << text.toStdString();
    EXPECT_TRUE(text.contains(QStringLiteral("Icon=su._525.MarkpasteTest\n")));

    ASSERT_TRUE(setAutostartEnabled(id, false));
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_TRUE(setAutostartEnabled(id, false));   // turning it off again is not an error
    QStandardPaths::setTestModeEnabled(false);
}
#else
TEST(Autostart, WritesAndRemovesRunKey)
{
    const QString id = QStringLiteral("su._525.MarkpasteTest");
    setAutostartEnabled(id, false);
    EXPECT_FALSE(isAutostartEnabled(id));
    ASSERT_TRUE(setAutostartEnabled(id, true, QStringLiteral("C:/Program Files/Mark paste/markpaste.exe")));
    EXPECT_TRUE(isAutostartEnabled(id));
    const QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                        QSettings::NativeFormat);
    // A path with spaces: quoted and with backslashes
    EXPECT_EQ(run.value(id).toString(), QStringLiteral("\"C:\\Program Files\\Mark paste\\markpaste.exe\""));
    ASSERT_TRUE(setAutostartEnabled(id, false));
    EXPECT_FALSE(isAutostartEnabled(id));
    EXPECT_TRUE(setAutostartEnabled(id, false));
}
#endif

#ifdef Q_OS_LINUX
#include "platform/linux/FdIo.h"
#include "platform/linux/GnomeKeybindings.h"
#include "platform/linux/portal/PortalGlobalShortcuts.h"

#include <QEventLoop>
#include <QFuture>

#include <fcntl.h>
#include <unistd.h>

namespace {
QByteArray awaitBytes(QFuture<QByteArray> f)
{
    QEventLoop loop;
    auto w = f.then(&loop, [&loop](const QByteArray &) { loop.quit(); });
    if (!f.isFinished())
        loop.exec();
    return f.result();
}
} // namespace

TEST(FdIo, ReadsAllUntilEof)
{
    int fds[2];
    ASSERT_EQ(::pipe2(fds, O_CLOEXEC), 0);
    ASSERT_EQ(::write(fds[1], "hello", 5), 5);
    ::close(fds[1]);
    QObject ctx;
    EXPECT_EQ(awaitBytes(lx::readAllAsync(fds[0], 1000, &ctx)), QByteArray("hello"));
}

// The clipboard owner did not finish writing by the timeout: a fragment of an image or text
// must not come back to the clipboard after a paste
TEST(FdIo, TimeoutGivesNothingInsteadOfPartialData)
{
    int fds[2];
    ASSERT_EQ(::pipe2(fds, O_CLOEXEC), 0);
    ASSERT_EQ(::write(fds[1], "\x89PNG part", 9), 9);   // the writer "hung", its end is not closed
    QObject ctx;
    EXPECT_TRUE(awaitBytes(lx::readAllAsync(fds[0], 100, &ctx)).isEmpty());
    ::close(fds[1]);
}

TEST(FdIo, OverLimitGivesNothing)
{
    int fds[2];
    ASSERT_EQ(::pipe2(fds, O_CLOEXEC), 0);
    ASSERT_EQ(::write(fds[1], "0123456789", 10), 10);
    ::close(fds[1]);
    QObject ctx;
    EXPECT_TRUE(awaitBytes(lx::readAllAsync(fds[0], 1000, &ctx, 4)).isEmpty());
}
TEST(PortalGlobalShortcuts, TriggerInXdgFormat)
{
    using lx::PortalGlobalShortcuts;
    EXPECT_EQ(PortalGlobalShortcuts::toPortalTrigger(QStringLiteral("Print")), QStringLiteral("Print"));
    EXPECT_EQ(PortalGlobalShortcuts::toPortalTrigger(QStringLiteral("Primary+Print")), QStringLiteral("CTRL+Print"));
    EXPECT_EQ(PortalGlobalShortcuts::toPortalTrigger(QStringLiteral("Primary+Alt+V")), QStringLiteral("CTRL+ALT+v"));
}

TEST(PortalGlobalShortcuts, DisplayTriggerFromGnomeAndKde)
{
    using lx::PortalGlobalShortcuts;
    EXPECT_EQ(PortalGlobalShortcuts::displayTrigger(QStringLiteral("Press Print")), QStringLiteral("Print"));
    EXPECT_EQ(PortalGlobalShortcuts::displayTrigger(QStringLiteral("Press <Control><Alt>v")), QStringLiteral("Ctrl+Alt+V"));
    EXPECT_EQ(PortalGlobalShortcuts::displayTrigger(QStringLiteral("Press <Control>Print")), QStringLiteral("Ctrl+Print"));
    EXPECT_EQ(PortalGlobalShortcuts::displayTrigger(QStringLiteral("Ctrl+Alt+V")), QStringLiteral("Ctrl+Alt+V"));
    EXPECT_EQ(PortalGlobalShortcuts::displayTrigger(QString()), QString());
}

TEST(GnomeKeybindings, CanonicalFormMatchesGtkAndPortalTriggers)
{
    using namespace lx::GnomeKeybindings;
    EXPECT_EQ(canonical(QStringLiteral("Print")), QStringLiteral("print"));
    EXPECT_EQ(canonical(QStringLiteral("Ctrl+Print")), canonical(QStringLiteral("<Control>Print")));
    EXPECT_EQ(canonical(QStringLiteral("Primary+Print")), canonical(QStringLiteral("<Primary>Print")));
    EXPECT_EQ(canonical(QStringLiteral("Ctrl+Alt+V")), canonical(QStringLiteral("<Alt><Control>v")));
    EXPECT_NE(canonical(QStringLiteral("Print")), canonical(QStringLiteral("<Shift>Print")));
    EXPECT_EQ(canonical(QStringLiteral("<Super><Shift>s")), QStringLiteral("meta+shift+s"));
}

TEST(GnomeKeybindings, GVariantStringArrays)
{
    using namespace lx::GnomeKeybindings;
    EXPECT_EQ(parseStrv(QStringLiteral("['Print', '<Shift>Print']")),
              (QStringList{QStringLiteral("Print"), QStringLiteral("<Shift>Print")}));
    EXPECT_TRUE(parseStrv(QStringLiteral("@as []")).isEmpty());
    EXPECT_EQ(formatStrv({}), QStringLiteral("@as []"));
    EXPECT_EQ(formatStrv({QStringLiteral("<Shift>Print")}), QStringLiteral("['<Shift>Print']"));
    EXPECT_EQ(parseStrv(formatStrv({QStringLiteral("a'b")})), QStringList{QStringLiteral("a'b")});
}

TEST(PortalGlobalShortcuts, EveryActionHasOwnId)
{
    using lx::PortalGlobalShortcuts;
    QStringList ids;
    for (Action a : {Action::Capture, Action::PasteSet, Action::NewShot, Action::ClearSet, Action::CaptureToStrip}) {
        const QString id = PortalGlobalShortcuts::shortcutId(a);
        ASSERT_FALSE(id.isEmpty());
        EXPECT_FALSE(ids.contains(id)) << id.toStdString();
        ids << id;
        Action back = Action::Capture;
        ASSERT_TRUE(PortalGlobalShortcuts::actionFromId(id, &back));
        EXPECT_EQ(back, a);
    }
    // Ids of already bound shortcuts do not change: KDE keeps the assigned keys by them
    EXPECT_EQ(PortalGlobalShortcuts::shortcutId(Action::Capture), QStringLiteral("capture"));
    EXPECT_EQ(PortalGlobalShortcuts::shortcutId(Action::PasteSet), QStringLiteral("paste-set"));
}
#endif
