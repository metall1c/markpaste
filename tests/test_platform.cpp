#include "platform/Autostart.h"

#include <QFile>
#include <QStandardPaths>

#include <gtest/gtest.h>

using namespace mp;

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
