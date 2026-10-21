#pragma once

#include "platform/PlatformTypes.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

namespace mp::lx {

struct PortalResponse {
    // 0 = success, 1 = cancelled by the user, 2 = another error (Request specification).
    // For D-Bus errors and timeouts we use code = 2 and fill in error.
    uint code = 2;
    QVariantMap results;
    QString error;

    bool ok() const { return code == 0; }
    QString describe() const;
};

// A separate session bus connection for all xdg-desktop-portal calls.
//
// Why separate: the portal identifies the program by its connection (sender). Registering the
// app id via org.freedesktop.host.portal.Registry must be the FIRST portal call
// on the connection, and Qt's shared connection (QDBusConnection::sessionBus) may already
// have been used by Qt itself by then, e.g. the platform theme reads the Settings portal.
// All of the program's portal calls must go through this connection, otherwise permissions
// and restore tokens will not be tied to the app id.
class PortalBus : public QObject
{
    Q_OBJECT
public:
    static PortalBus &instance();

    QDBusConnection connection() const { return m_bus; }
    bool isConnected() const { return m_bus.isConnected(); }

    // Register the app id. A repeated call does nothing.
    Status registerApp(const QString &appId);
    QString appId() const { return m_appId; }
    bool hasInterface(const QString &interface) const;
    uint interfaceVersion(const QString &interface) const;

    // Call a portal method that answers through a Request object.
    // handle_token is added to options; the Response subscription is made before the call (otherwise a race).
    // timeoutMs = 0 means wait forever (a permission dialog may stay open for a long time).
    QFuture<PortalResponse> request(const QString &interface, const QString &method,
                                    QVariantList argsBeforeOptions, QVariantMap options,
                                    QVariantList argsAfterOptions = {}, int timeoutMs = 0);

    // A plain call without a Request (e.g. NotifyKeyboardKeycode).
    QFuture<Status> call(const QString &interface, const QString &method, const QVariantList &args);

    QString newToken(const QString &prefix);

    static const QString kService;
    static const QString kPath;

private:
    PortalBus();
    QString requestPath(const QString &token) const;

    QDBusConnection m_bus;
    QString m_appId;
    quint64 m_counter = 0;
};

} // namespace mp::lx
