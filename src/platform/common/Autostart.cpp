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

// The user's Run key: no administrator rights, like the per-user install (spec section 11).
// The value name is the app id, the value is the quoted program path (paths may contain spaces).
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
    // Turning it off in "Task Manager → Startup" does not delete the value but marks it in StartupApproved.
    // That is the user's decision in the system: the checkbox in the program shows it as "off".
    const QSettings approved(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
                                            "Explorer\\StartupApproved\\Run"),
                             QSettings::NativeFormat);
    const QByteArray state = approved.value(appId).toByteArray();
    if (!state.isEmpty() && (state.at(0) & 1))   // 0x03 means off, 0x02 means on
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
        // Turning it on from the program also removes the "off" mark set by Task Manager
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
    // GenericConfigLocation = $XDG_CONFIG_HOME (usually ~/.config)
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/")
         + appId + QStringLiteral(".desktop");
}

bool isAutostartEnabled(const QString &appId)
{
    const QString path = autostartFilePath(appId);
    if (!QFile::exists(path))
        return false;
    // Desktops turn autostart off without deleting the file: Hidden=true or X-GNOME-Autostart-enabled=false
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QString text = QString::fromUtf8(f.readAll());
    return !text.contains(QStringLiteral("\nHidden=true")) && !text.contains(QStringLiteral("\nX-GNOME-Autostart-enabled=false"));
}

// Escaping the Exec argument per the Desktop Entry specification: the path in quotes,
// inside the quotes " ` $ and \ are escaped.
static QString quoteExec(const QString &path)
{
    QString out;
    for (QChar c : path) {
        if (c == u'"' || c == u'`' || c == u'$' || c == u'\\')
            out += u'\\';
        out += c;
    }
    // Double backslash: in .desktop file values "\\" is a single "\"
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
        // The KDE tray and the portals must be ready by the time we start
        "X-KDE-autostart-phase=2\n")
        .arg(quoteExec(exe), appId);
    f.write(text.toUtf8());
    return f.commit();
}

#endif

} // namespace mp
