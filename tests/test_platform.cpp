#include "platform/Autostart.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include <gtest/gtest.h>

using namespace mp;

#ifndef Q_OS_WIN
TEST(Autostart, WritesAndRemovesDesktopFile)
{
    // Тестовый режим: ~/.qttest/config вместо настоящего ~/.config
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
    // Кавычка внутри пути: \" по спецификации Exec, затем \ удваивается как в любом значении
    EXPECT_TRUE(text.contains(QStringLiteral("Exec=\"/opt/My Apps/mark\\\\\"paste\"\n"))) << text.toStdString();
    EXPECT_TRUE(text.contains(QStringLiteral("Icon=su._525.MarkpasteTest\n")));

    ASSERT_TRUE(setAutostartEnabled(id, false));
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_TRUE(setAutostartEnabled(id, false));   // повторное выключение — не ошибка
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
    // Путь с пробелами — в кавычках и с обратными косыми
    EXPECT_EQ(run.value(id).toString(), QStringLiteral("\"C:\\Program Files\\Mark paste\\markpaste.exe\""));
    ASSERT_TRUE(setAutostartEnabled(id, false));
    EXPECT_FALSE(isAutostartEnabled(id));
    EXPECT_TRUE(setAutostartEnabled(id, false));
}
#endif

#ifdef Q_OS_LINUX
#include "platform/linux/FdIo.h"
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

// Владелец буфера не дописал данные к таймауту: обрывок картинки или текста
// не должен вернуться в буфер после вставки
TEST(FdIo, TimeoutGivesNothingInsteadOfPartialData)
{
    int fds[2];
    ASSERT_EQ(::pipe2(fds, O_CLOEXEC), 0);
    ASSERT_EQ(::write(fds[1], "\x89PNG part", 9), 9);   // писатель «завис», конец не закрыт
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
    // Id уже привязанных сочетаний не меняются: по ним KDE хранит назначенные клавиши
    EXPECT_EQ(PortalGlobalShortcuts::shortcutId(Action::Capture), QStringLiteral("capture"));
    EXPECT_EQ(PortalGlobalShortcuts::shortcutId(Action::PasteSet), QStringLiteral("paste-set"));
}
#endif
