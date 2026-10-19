#include "PortalGlobalShortcuts.h"

#include "PortalBus.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcPortal)

namespace {

// a(sa{sv}) — список сочетаний в BindShortcuts и ответе
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
    }
    return {};
}

bool PortalGlobalShortcuts::actionFromId(const QString &id, Action *action)
{
    for (Action a : {Action::Capture, Action::PasteSet, Action::NewShot, Action::ClearSet})
        if (shortcutId(a) == id) {
            *action = a;
            return true;
        }
    return false;
}

QString PortalGlobalShortcuts::toPortalTrigger(const QString &portable)
{
    // Спецификация XDG shortcuts: модификаторы CTRL, ALT, SHIFT, NUM, LOGO и имя клавиши по xkb.
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
                                  m_sessionStarted = false;   // следующая попытка создаст сессию заново
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
                case Action::Capture: sc.properties[QStringLiteral("description")] = tr("Capture screen area"); break;
                case Action::PasteSet: sc.properties[QStringLiteral("description")] = tr("Paste the set into the chat"); break;
                case Action::NewShot: sc.properties[QStringLiteral("description")] = tr("New shot"); break;
                case Action::ClearSet: sc.properties[QStringLiteral("description")] = tr("Clear the set"); break;
                }
                if (!it.value().isEmpty())
                    sc.properties[QStringLiteral("preferred_trigger")] = toPortalTrigger(it.value());
                list << sc;
            }

            // Ответ BindShortcuts может прийти после диалога: таймаута нет.
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
                        const QString trigger = sc.properties.value(QStringLiteral("trigger_description")).toString();
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

void PortalGlobalShortcuts::onDeactivated(const QDBusObjectPath &session, const QString &id, qulonglong,
                                          const QVariantMap &)
{
    Action a;
    if (session.path() != m_session || !actionFromId(id, &a))
        return;
    emit released(a);
}

} // namespace mp::lx
