#include "PortalBus.h"

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QTimer>

#include <memory>

Q_LOGGING_CATEGORY(lcPortal, "pointshot.portal")

namespace ps::lx {

const QString PortalBus::kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString PortalBus::kPath = QStringLiteral("/org/freedesktop/portal/desktop");

QString PortalResponse::describe() const
{
    if (!error.isEmpty())
        return error;
    switch (code) {
    case 0: return QStringLiteral("ok");
    case 1: return QStringLiteral("cancelled by user");
    default: return QStringLiteral("portal error (code %1)").arg(code);
    }
}

namespace {

// Ждёт сигнал Response на пути Request и выполняет обещание.
class RequestWaiter : public QObject
{
    Q_OBJECT
public:
    explicit RequestWaiter(QDBusConnection bus, QString path)
        : m_bus(std::move(bus)), m_path(std::move(path))
    {
        m_promise.start();
    }

    bool subscribe()
    {
        return m_bus.connect(PortalBus::kService, m_path, QStringLiteral("org.freedesktop.portal.Request"),
                             QStringLiteral("Response"), this, SLOT(onResponse(uint, QVariantMap)));
    }

    QFuture<PortalResponse> future() { return m_promise.future(); }

    void fail(const QString &error)
    {
        PortalResponse r;
        r.code = 2;
        r.error = error;
        finish(r);
    }

    void startTimeout(int ms)
    {
        if (ms > 0)
            QTimer::singleShot(ms, this, [this] {
                // Закрываем Request, чтобы диалог не остался висеть.
                QDBusMessage close = QDBusMessage::createMethodCall(
                    PortalBus::kService, m_path, QStringLiteral("org.freedesktop.portal.Request"),
                    QStringLiteral("Close"));
                m_bus.asyncCall(close);
                fail(QStringLiteral("portal request timed out"));
            });
    }

public slots:
    void onResponse(uint code, const QVariantMap &results)
    {
        PortalResponse r;
        r.code = code;
        r.results = results;
        finish(r);
    }

private:
    void finish(const PortalResponse &r)
    {
        if (m_done)
            return;
        m_done = true;
        m_bus.disconnect(PortalBus::kService, m_path, QStringLiteral("org.freedesktop.portal.Request"),
                         QStringLiteral("Response"), this, SLOT(onResponse(uint, QVariantMap)));
        m_promise.addResult(r);
        m_promise.finish();
        deleteLater();
    }

    QDBusConnection m_bus;
    QString m_path;
    QPromise<PortalResponse> m_promise;
    bool m_done = false;
};

} // namespace

PortalBus &PortalBus::instance()
{
    static PortalBus bus;
    return bus;
}

PortalBus::PortalBus()
    : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("pointshot-portal")))
{
    if (!m_bus.isConnected())
        qCWarning(lcPortal) << "cannot connect to session bus:" << m_bus.lastError().message();
}

Status PortalBus::registerApp(const QString &appId)
{
    if (!m_appId.isEmpty())
        return Status::success();
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath,
                                                      QStringLiteral("org.freedesktop.host.portal.Registry"),
                                                      QStringLiteral("Register"));
    msg << appId << QVariantMap();
    const QDBusMessage reply = m_bus.call(msg, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(lcPortal) << "host Registry.Register failed:" << reply.errorName() << reply.errorMessage();
        return Status::failure(reply.errorMessage());
    }
    m_appId = appId;
    qCInfo(lcPortal) << "registered with portals as" << appId;
    return Status::success();
}

bool PortalBus::hasInterface(const QString &interface) const
{
    return interfaceVersion(interface) > 0;
}

uint PortalBus::interfaceVersion(const QString &interface) const
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath,
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    msg << interface << QStringLiteral("version");
    const QDBusMessage reply = m_bus.call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return 0;
    return reply.arguments().first().value<QDBusVariant>().variant().toUInt();
}

QString PortalBus::newToken(const QString &prefix)
{
    return QStringLiteral("%1_%2_%3").arg(prefix).arg(++m_counter).arg(QRandomGenerator::global()->generate());
}

QString PortalBus::requestPath(const QString &token) const
{
    // :1.234 → 1_234 (спецификация org.freedesktop.portal.Request)
    QString sender = m_bus.baseService();
    sender.remove(0, 1);
    sender.replace(u'.', u'_');
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

QFuture<PortalResponse> PortalBus::request(const QString &interface, const QString &method,
                                           QVariantList argsBeforeOptions, QVariantMap options,
                                           QVariantList argsAfterOptions, int timeoutMs)
{
    const QString token = newToken(QStringLiteral("ps"));
    options.insert(QStringLiteral("handle_token"), token);

    auto *waiter = new RequestWaiter(m_bus, requestPath(token));
    waiter->setParent(this);
    QFuture<PortalResponse> future = waiter->future();
    if (!waiter->subscribe()) {
        waiter->fail(QStringLiteral("cannot subscribe to portal Response signal"));
        return future;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, interface, method);
    QVariantList args = argsBeforeOptions;
    args << options;
    args << argsAfterOptions;
    msg.setArguments(args);

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), waiter);
    connect(watcher, &QDBusPendingCallWatcher::finished, waiter, [waiter, interface, method](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        w->deleteLater();
        if (reply.isError())
            waiter->fail(QStringLiteral("%1.%2: %3").arg(interface, method, reply.error().message()));
    });
    waiter->startTimeout(timeoutMs);
    return future;
}

QFuture<Status> PortalBus::call(const QString &interface, const QString &method, const QVariantList &args)
{
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, interface, method);
    msg.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [promise, interface, method](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<> reply = *w;
        w->deleteLater();
        promise->addResult(reply.isError()
                               ? Status::failure(QStringLiteral("%1.%2: %3").arg(interface, method, reply.error().message()))
                               : Status::success());
        promise->finish();
    });
    return promise->future();
}

} // namespace ps::lx

#include "PortalBus.moc"
