#include "PortalGlobalShortcuts.h"

#include "PortalBus.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcPortal)

namespace {

// a(sa{sv}): the list of shortcuts in BindShortcuts and its reply
struct PortalShortcut {
    QString id;
    QVariantMap properties;
};
using PortalShortcutList = QList<PortalShortcut>;

QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s)
{
    arg.beginStructure();
    arg << s.id << s.properties;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s)
{
    arg.beginStructure();
    arg >> s.id >> s.properties;
    arg.endStructure();
    return arg;
}

} // namespace

Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(PortalShortcutList)

namespace mp::lx {

static const QString kIface = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");

PortalGlobalShortcuts::PortalGlobalShortcuts(QObject *parent)
    : IHotkeys(parent)
{
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<PortalShortcutList>();

    QDBusConnection bus = PortalBus::instance().connection();
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("Activated"), this,
                SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("Deactivated"), this,
                SLOT(onDeactivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("ShortcutsChanged"), this,
                SLOT(onShortcutsChanged(QDBusMessage)));
}

PortalGlobalShortcuts::~PortalGlobalShortcuts()
{
    if (!m_session.isEmpty()) {
        QDBusMessage close = QDBusMessage::createMethodCall(PortalBus::kService, m_session,
                                                            QStringLiteral("org.freedesktop.portal.Session"),
                                                            QStringLiteral("Close"));
        PortalBus::instance().connection().asyncCall(close);
    }
}

bool PortalGlobalShortcuts::isAvailable()
{
    return PortalBus::instance().hasInterface(kIface);
}

QString PortalGlobalShortcuts::shortcutId(Action action)
{
    switch (action) {
    case Action::Capture: return QStringLiteral("capture");
    case Action::PasteSet: return QStringLiteral("paste-set");
    case Action::NewShot: return QStringLiteral("new-shot");
    case Action::ClearSet: return QStringLiteral("clear-set");
    case Action::CaptureToStrip: return QStringLiteral("capture-to-strip");
    }
    return {};
}

bool PortalGlobalShortcuts::actionFromId(const QString &id, Action *action)
{
    for (Action a : {Action::Capture, Action::PasteSet, Action::NewShot, Action::ClearSet, Action::CaptureToStrip})
        if (shortcutId(a) == id) {
            *action = a;
            return true;
        }
    return false;
}

QString PortalGlobalShortcuts::toPortalTrigger(const QString &portable)
{
    // XDG shortcuts specification: CTRL, ALT, SHIFT, NUM, LOGO modifiers and the xkb key name.
    QStringList out;
    const QStringList parts = portable.split(u'+', Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        const QString lower = p.toLower();
        const bool last = i == parts.size() - 1;
        if (!last && (lower == u"primary" || lower == u"ctrl" || lower == u"control"))
            out << QStringLiteral("CTRL");
        else if (!last && lower == u"alt")
            out << QStringLiteral("ALT");
        else if (!last && lower == u"shift")
            out << QStringLiteral("SHIFT");
        else if (!last && (lower == u"meta" || lower == u"super" || lower == u"logo"))
            out << QStringLiteral("LOGO");
        else if (lower == u"print" || lower == u"printscreen")
            out << QStringLiteral("Print");
        else
            out << (p.size() == 1 ? lower : p);
    }
    return out.join(u'+');
}

QString PortalGlobalShortcuts::displayTrigger(const QString &description)
{
    QString d = description.trimmed();
    if (d.startsWith(QStringLiteral("Press "), Qt::CaseInsensitive))
        d = d.mid(6).trimmed();
    if (!d.contains(u'<'))
        return d;
    // GTK accelerator: <Control><Alt>v
    static const QList<QPair<QString, QString>> mods{
        {QStringLiteral("control"), QStringLiteral("Ctrl")}, {QStringLiteral("primary"), QStringLiteral("Ctrl")},
        {QStringLiteral("ctrl"), QStringLiteral("Ctrl")},    {QStringLiteral("alt"), QStringLiteral("Alt")},
        {QStringLiteral("shift"), QStringLiteral("Shift")},  {QStringLiteral("super"), QStringLiteral("Meta")},
        {QStringLiteral("meta"), QStringLiteral("Meta")},    {QStringLiteral("logo"), QStringLiteral("Meta")}};
    QStringList out;
    int i = 0;
    while (i < d.size() && d[i] == u'<') {
        const int close = d.indexOf(u'>', i);
        if (close < 0)
            break;
        const QString mod = d.mid(i + 1, close - i - 1).toLower();
        QString name = mod;
        for (const auto &[gtk, qt] : mods)
            if (mod == gtk)
                name = qt;
        if (!out.contains(name))
            out << name;
        i = close + 1;
    }
    QString key = d.mid(i).trimmed();
    if (key.size() == 1)
        key = key.toUpper();
    if (!key.isEmpty())
        out << key;
    return out.join(u'+');
}

QFuture<Status> PortalGlobalShortcuts::ensureSession()
{
    if (m_sessionStarted)
        return m_sessionFuture;
    m_sessionStarted = true;
    PortalBus &bus = PortalBus::instance();
    const QVariantMap opts{{QStringLiteral("session_handle_token"), bus.newToken(QStringLiteral("gs"))}};
    m_sessionFuture = bus.request(kIface, QStringLiteral("CreateSession"), {}, opts, {}, 10000)
                          .then(this, [this](const PortalResponse &r) {
                              if (!r.ok()) {
                                  m_sessionStarted = false;   // the next attempt creates the session again
                                  return Status::failure(QStringLiteral("GlobalShortcuts.CreateSession: ") + r.describe());
                              }
                              m_session = r.results.value(QStringLiteral("session_handle")).toString();
                              return Status::success();
                          });
    return m_sessionFuture;
}

QFuture<BindResult> PortalGlobalShortcuts::bind(Action action, const QString &preferred)
{
    return bindAll({{action, preferred}}).then([action](const QHash<Action, BindResult> &all) {
        return all.value(action, BindResult{Status::failure(QStringLiteral("not bound")), {}});
    });
}

QFuture<QHash<Action, BindResult>> PortalGlobalShortcuts::bindAll(const QHash<Action, QString> &preferred)
{
    return ensureSession()
        .then(this, [this, preferred](const Status &s) -> QFuture<QHash<Action, BindResult>> {
            QHash<Action, BindResult> failed;
            if (!s.ok) {
                for (auto it = preferred.cbegin(); it != preferred.cend(); ++it)
                    failed.insert(it.key(), BindResult{s, {}});
                return readyFuture(failed);
            }

            PortalShortcutList list;
            for (auto it = preferred.cbegin(); it != preferred.cend(); ++it) {
                PortalShortcut sc;
                sc.id = shortcutId(it.key());
                switch (it.key()) {
                case Action::Capture: sc.properties[QStringLiteral("description")] = tr("Shot with editor"); break;
                case Action::PasteSet: sc.properties[QStringLiteral("description")] = tr("Paste the set into the chat"); break;
                case Action::NewShot: sc.properties[QStringLiteral("description")] = tr("New shot"); break;
                case Action::ClearSet: sc.properties[QStringLiteral("description")] = tr("Clear the set"); break;
                case Action::CaptureToStrip:
                    sc.properties[QStringLiteral("description")] = tr("Shot without editor");
                    break;
                }
                if (!it.value().isEmpty())
                    sc.properties[QStringLiteral("preferred_trigger")] = toPortalTrigger(it.value());
                list << sc;
            }

            // The BindShortcuts reply may come after a dialog: no timeout.
            return PortalBus::instance()
                .request(kIface, QStringLiteral("BindShortcuts"),
                         {QVariant::fromValue(QDBusObjectPath(m_session)), QVariant::fromValue(list), QString()}, {})
                .then(this, [preferred](const PortalResponse &r) {
                    QHash<Action, BindResult> result;
                    for (auto it = preferred.cbegin(); it != preferred.cend(); ++it)
                        result.insert(it.key(), BindResult{Status::failure(QStringLiteral("BindShortcuts: ") + r.describe()), {}});
                    if (!r.ok())
                        return result;
                    PortalShortcutList bound;
                    const QVariant v = r.results.value(QStringLiteral("shortcuts"));
                    v.value<QDBusArgument>() >> bound;
                    for (const PortalShortcut &sc : bound) {
                        Action a;
                        if (!actionFromId(sc.id, &a))
                            continue;
                        const QString trigger = displayTrigger(sc.properties.value(QStringLiteral("trigger_description")).toString());
                        result.insert(a, BindResult{Status::success(), trigger});
                        qCInfo(lcPortal) << "shortcut" << sc.id << "bound to" << (trigger.isEmpty() ? QStringLiteral("<none>") : trigger);
                    }
                    return result;
                });
        })
        .unwrap();
}

bool PortalGlobalShortcuts::canOpenSystemSettings() const
{
    return !m_session.isEmpty() && PortalBus::instance().interfaceVersion(kIface) >= 2;
}

void PortalGlobalShortcuts::openSystemSettings()
{
    if (m_session.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                      QStringLiteral("ConfigureShortcuts"));
    msg << QVariant::fromValue(QDBusObjectPath(m_session)) << QString() << QVariantMap();
    PortalBus::instance().connection().asyncCall(msg);
}

void PortalGlobalShortcuts::onActivated(const QDBusObjectPath &session, const QString &id, qulonglong,
                                        const QVariantMap &options)
{
    Action a;
    if (session.path() != m_session || !actionFromId(id, &a))
        return;
    ActivationContext ctx;
    ctx.activationToken = options.value(QStringLiteral("activation_token")).toString();
    emit activated(a, ctx);
}

void PortalGlobalShortcuts::onShortcutsChanged(const QDBusMessage &message)
{
    // ShortcutsChanged(o session_handle, a(sa{sv}) shortcuts): the user changed a shortcut
    // in the system settings (in KDE, "Shortcuts" or the ConfigureShortcuts dialog)
    const QList<QVariant> args = message.arguments();
    if (args.size() < 2 || args[0].value<QDBusObjectPath>().path() != m_session)
        return;
    PortalShortcutList list;
    args[1].value<QDBusArgument>() >> list;
    for (const PortalShortcut &sc : list) {
        Action a;
        if (!actionFromId(sc.id, &a))
            continue;
        const QString trigger = displayTrigger(sc.properties.value(QStringLiteral("trigger_description")).toString());
        qCInfo(lcPortal) << "shortcut" << sc.id << "changed to" << (trigger.isEmpty() ? QStringLiteral("<none>") : trigger);
        emit triggerChanged(a, trigger);
    }
}

void PortalGlobalShortcuts::onDeactivated(const QDBusObjectPath &session, const QString &id, qulonglong,
                                          const QVariantMap &)
{
    Action a;
    if (session.path() != m_session || !actionFromId(id, &a))
        return;
    emit released(a);
}

} // namespace mp::lx
