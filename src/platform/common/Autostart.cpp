#include "platform/Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace mp {

#ifdef Q_OS_WIN

// Ключ Run пользователя: без прав администратора, как и установка в профиль (ТЗ разд. 11).
// Имя значения — app id, значение — путь к программе в кавычках (в пути бывают пробелы).
static const QString kRunKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");

bool isAutostartSupported()
{
    return true;
}

QString autostartFilePath(const QString &appId)
{
    return kRunKey + u'\\' + appId;
}

bool isAutostartEnabled(const QString &appId)
{
    // Выключение в «Диспетчере задач → Автозагрузка» значение не удаляет, а помечает в StartupApproved.
    // Это решение пользователя в системе: галочка в программе его показывает как «выключено».
    const QSettings approved(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
                                            "Explorer\\StartupApproved\\Run"),
                             QSettings::NativeFormat);
    const QByteArray state = approved.value(appId).toByteArray();
    if (!state.isEmpty() && (state.at(0) & 1))   // 0x03 — выключено, 0x02 — включено
        return false;
    return !QSettings(kRunKey, QSettings::NativeFormat).value(appId).toString().isEmpty();
}

bool setAutostartEnabled(const QString &appId, bool enabled, const QString &executable)
{
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (!enabled) {
        run.remove(appId);
    } else {
        const QString exe = executable.isEmpty() ? QCoreApplication::applicationFilePath() : executable;
        run.setValue(appId, u'"' + QDir::toNativeSeparators(exe) + u'"');
        // Включение из программы снимает и отметку «выключено» из Диспетчера задач
        QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
                                 "Explorer\\StartupApproved\\Run"),
                  QSettings::NativeFormat)
            .remove(appId);
    }
    run.sync();
    return run.status() == QSettings::NoError;
}

#else

bool isAutostartSupported()
{
    return true;
}

QString autostartFilePath(const QString &appId)
{
    // GenericConfigLocation = $XDG_CONFIG_HOME (обычно ~/.config)
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/")
         + appId + QStringLiteral(".desktop");
}

bool isAutostartEnabled(const QString &appId)
{
    const QString path = autostartFilePath(appId);
    if (!QFile::exists(path))
        return false;
    // Окружения выключают автозапуск, не удаляя файл: Hidden=true или X-GNOME-Autostart-enabled=false
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QString text = QString::fromUtf8(f.readAll());
    return !text.contains(QStringLiteral("\nHidden=true")) && !text.contains(QStringLiteral("\nX-GNOME-Autostart-enabled=false"));
}

// Экранирование аргумента Exec по спецификации Desktop Entry: путь в кавычках,
// внутри кавычек экранируются " ` $ и \.
static QString quoteExec(const QString &path)
{
    QString out;
    for (QChar c : path) {
        if (c == u'"' || c == u'`' || c == u'$' || c == u'\\')
            out += u'\\';
        out += c;
    }
    // Двойная обратная косая: в значениях .desktop-файла «\\» — это одна «\»
    out.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    return u'"' + out + u'"';
}

bool setAutostartEnabled(const QString &appId, bool enabled, const QString &executable)
{
    const QString path = autostartFilePath(appId);
    if (!enabled)
        return !QFile::exists(path) || QFile::remove(path);

    const QString exe = executable.isEmpty() ? QCoreApplication::applicationFilePath() : executable;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const QString text = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Markpaste\n"
        "Comment=Capture, mark and paste screenshots with comments into any chat\n"
        "Comment[ru]=Снять, отметить и вставить снимки с комментариями в любой чат\n"
        "Exec=%1\n"
        "Icon=%2\n"
        "Terminal=false\n"
        "StartupNotify=false\n"
        "X-GNOME-Autostart-enabled=true\n"
        // Трей KDE и порталы должны быть готовы к моменту запуска
        "X-KDE-autostart-phase=2\n")
        .arg(quoteExec(exe), appId);
    f.write(text.toUtf8());
    return f.commit();
}

#endif

} // namespace mp
