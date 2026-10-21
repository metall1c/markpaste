#include "GnomeKeybindings.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

Q_LOGGING_CATEGORY(lcGnomeKeys, "markpaste.gnomekeys")

namespace mp::lx::GnomeKeybindings {

namespace {

struct Known {
    const char *schema;
    const char *key;
    const char *description;
};

// Screenshot keys of GNOME Shell 42+ and of gnome-settings-daemon before it (GNOME 41 and older)
const Known kKnown[] = {
    {"org.gnome.shell.keybindings", "show-screenshot-ui", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME screenshot tool")},
    {"org.gnome.shell.keybindings", "screenshot", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME screen screenshot")},
    {"org.gnome.shell.keybindings", "screenshot-window", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME window screenshot")},
    {"org.gnome.shell.keybindings", "show-screen-recording-ui", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME screen recording")},
    {"org.gnome.settings-daemon.plugins.media-keys", "screenshot", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME screenshot")},
    {"org.gnome.settings-daemon.plugins.media-keys", "window-screenshot", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME window screenshot")},
    {"org.gnome.settings-daemon.plugins.media-keys", "area-screenshot", QT_TRANSLATE_NOOP("GnomeKeybindings", "the GNOME area screenshot")},
};

const QString kSettingsGroup = QStringLiteral("gnomeReleasedKeys");

// gsettings <args>; nullopt-like: ok = false if the tool or the schema/key is missing
QString gsettings(const QStringList &args, bool *ok)
{
    QProcess p;
    p.start(QStringLiteral("gsettings"), args);
    *ok = p.waitForFinished(3000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    if (!*ok)
        return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

} // namespace

bool isGnome()
{
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':', Qt::SkipEmptyParts);
    return std::any_of(desktops.cbegin(), desktops.cend(),
                       [](const QString &d) { return d.compare(QStringLiteral("GNOME"), Qt::CaseInsensitive) == 0; });
}

QString canonical(const QString &triggerOrAccel)
{
    QString s = triggerOrAccel.trimmed();
    QStringList mods;
    QString key;
    auto addMod = [&mods](QString m) {
        m = m.toLower();
        if (m == u"primary" || m == u"control" || m == u"ctrl")
            m = QStringLiteral("ctrl");
        else if (m == u"super" || m == u"meta" || m == u"logo")
            m = QStringLiteral("meta");
        if (!mods.contains(m))
            mods << m;
    };
    if (s.contains(u'<')) {   // GTK accelerator
        static const QRegularExpression re(QStringLiteral("<([^>]+)>"));
        auto it = re.globalMatch(s);
        int end = 0;
        while (it.hasNext()) {
            const auto m = it.next();
            addMod(m.captured(1));
            end = m.capturedEnd();
        }
        key = s.mid(end);
    } else {
        const QStringList parts = s.split(u'+', Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            if (i + 1 < parts.size())
                addMod(parts[i].trimmed());
            else
                key = parts[i].trimmed();
        }
    }
    key = key.toLower();
    if (key == u"printscreen" || key == u"sysrq")
        key = QStringLiteral("print");
    if (key.isEmpty())
        return {};
    std::sort(mods.begin(), mods.end());
    mods << key;
    return mods.join(u'+');
}

QStringList parseStrv(const QString &gvariant)
{
    QStringList out;
    static const QRegularExpression re(QStringLiteral("'((?:[^'\\\\]|\\\\.)*)'"));
    auto it = re.globalMatch(gvariant);
    static const QRegularExpression escape(QStringLiteral("\\\\(.)"));
    while (it.hasNext())
        out << it.next().captured(1).replace(escape, QStringLiteral("\\1"));
    return out;
}

QString formatStrv(const QStringList &list)
{
    if (list.isEmpty())
        return QStringLiteral("@as []");
    QStringList quoted;
    for (const QString &s : list)
        quoted << u'\'' + QString(s).replace(u'\'', QStringLiteral("\\'")) + u'\'';
    return u'[' + quoted.join(QStringLiteral(", ")) + u']';
}

QList<Holder> holders(const QString &trigger)
{
    QList<Holder> out;
    const QString want = canonical(trigger);
    if (!isGnome() || want.isEmpty())
        return out;
    for (const Known &k : kKnown) {
        bool ok = false;
        const QString value = gsettings({QStringLiteral("get"), QLatin1String(k.schema), QLatin1String(k.key)}, &ok);
        if (!ok)
            continue;   // no such schema or key in this GNOME version
        const QStringList accels = parseStrv(value);
        if (std::any_of(accels.cbegin(), accels.cend(), [&want](const QString &a) { return canonical(a) == want; }))
            out << Holder{QLatin1String(k.schema), QLatin1String(k.key), accels,
                          QCoreApplication::translate("GnomeKeybindings", k.description)};
    }
    return out;
}

Status release(const QString &trigger)
{
    const QString want = canonical(trigger);
    const QList<Holder> list = holders(trigger);
    if (list.isEmpty())
        return Status::success();
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    for (const Holder &h : list) {
        const QString id = h.schema + u'|' + h.key;   // '/' would make a QSettings subgroup
        if (!settings.contains(id))   // the value before the program's first change
            settings.setValue(id, formatStrv(h.accels));
        QStringList rest;
        for (const QString &a : h.accels)
            if (canonical(a) != want)
                rest << a;
        bool ok = false;
        gsettings({QStringLiteral("set"), h.schema, h.key, formatStrv(rest)}, &ok);
        qCInfo(lcGnomeKeys) << "released" << trigger << "from" << id << (ok ? "ok" : "FAILED");
        if (!ok)
            return Status::failure(QStringLiteral("gsettings set %1 %2 failed").arg(h.schema, h.key));
    }
    return Status::success();
}

bool hasReleased()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    return !settings.childKeys().isEmpty();
}

Status restore()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    Status result = Status::success();
    for (const QString &id : settings.childKeys()) {
        const int slash = id.indexOf(u'|');
        bool ok = false;
        gsettings({QStringLiteral("set"), id.left(slash), id.mid(slash + 1), settings.value(id).toString()}, &ok);
        qCInfo(lcGnomeKeys) << "restored" << id << (ok ? "ok" : "FAILED");
        if (ok)
            settings.remove(id);
        else
            result = Status::failure(QStringLiteral("gsettings set %1 failed").arg(id));
    }
    return result;
}

} // namespace mp::lx::GnomeKeybindings
