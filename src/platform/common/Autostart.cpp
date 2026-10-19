#include "platform/Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace mp {

#ifdef Q_OS_WIN

bool isAutostartSupported() { return false; }
bool isAutostartEnabled(const QString &) { return false; }
bool setAutostartEnabled(const QString &, bool, const QString &) { return false; }
QString autostartFilePath(const QString &) { return {}; }

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
