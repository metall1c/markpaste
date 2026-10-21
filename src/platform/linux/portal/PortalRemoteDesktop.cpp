#include "PortalRemoteDesktop.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QLoggingCategory>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(lcPortal)

namespace mp::lx {

static const QString kIface = QStringLiteral("org.freedesktop.portal.RemoteDesktop");

namespace {
constexpr uint kDeviceKeyboard = 1;
constexpr uint kPersistUntilRevoked = 2;

// A delayed answer: on Wayland the keyboard state cannot be read without focus;
// the shortcut release arrives as the Deactivated signal of the GlobalShortcuts portal.
QFuture<Status> delayed(QObject *ctx, int ms)
{
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    QTimer::singleShot(ms, ctx, [promise] {
        promise->addResult(Status::success());
        promise->finish();
    });
    return promise->future();
}
} // namespace

PortalRemoteDesktop::PortalRemoteDesktop(QObject *parent)
    : IInputInjector(parent)
{
}

PortalRemoteDesktop::~PortalRemoteDesktop()
{
    end();
}

bool PortalRemoteDesktop::isAvailable() const
{
    return PortalBus::instance().hasInterface(kIface);
}

void PortalRemoteDesktop::finishBegin(const Status &status)
{
    m_starting = false;
    const auto waiters = std::exchange(m_beginWaiters, {});
    for (const auto &p : waiters) {
        p->addResult(status);
        p->finish();
    }
}

void PortalRemoteDesktop::fail(const QString &error)
{
    qCWarning(lcPortal) << "RemoteDesktop:" << error;
    end();
    finishBegin(Status::failure(error));
}

QFuture<Status> PortalRemoteDesktop::begin()
{
    if (!m_session.isEmpty() && !m_starting)
        return readyFuture(Status::success());

    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    m_beginWaiters.append(promise);
    if (m_starting)
        return promise->future();
    m_starting = true;

    PortalBus &bus = PortalBus::instance();

    // 1. CreateSession
    QVariantMap createOpts{{QStringLiteral("session_handle_token"), bus.newToken(QStringLiteral("rd"))}};
    bus.request(kIface, QStringLiteral("CreateSession"), {}, createOpts, {}, 10000)
        .then(this, [this](const PortalResponse &r) {
            if (!r.ok())
                return fail(QStringLiteral("CreateSession: ") + r.describe());
            m_session = r.results.value(QStringLiteral("session_handle")).toString();
            PortalBus::instance().connection().connect(
                PortalBus::kService, m_session, QStringLiteral("org.freedesktop.portal.Session"),
                QStringLiteral("Closed"), this, SLOT(onSessionClosed(QVariantMap)));

            // 2. SelectDevices: keyboard only, permission until revoked
            QVariantMap opts{
                {QStringLiteral("types"), kDeviceKeyboard},
                {QStringLiteral("persist_mode"), kPersistUntilRevoked},
            };
            if (!m_restoreToken.isEmpty())
                opts.insert(QStringLiteral("restore_token"), m_restoreToken);
            PortalBus::instance()
                .request(kIface, QStringLiteral("SelectDevices"), {QVariant::fromValue(QDBusObjectPath(m_session))},
                         opts, {}, 10000)
                .then(this, [this](const PortalResponse &r) {
                    if (!r.ok())
                        return fail(QStringLiteral("SelectDevices: ") + r.describe());

                    // 2a. Clipboard access strictly before Start (Clipboard portal specification)
                    if (m_requestClipboard) {
                        QDBusMessage msg = QDBusMessage::createMethodCall(
                            PortalBus::kService, PortalBus::kPath, QStringLiteral("org.freedesktop.portal.Clipboard"),
                            QStringLiteral("RequestClipboard"));
                        msg << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariantMap();
                        const QDBusMessage reply = PortalBus::instance().connection().call(msg, QDBus::Block, 5000);
                        if (reply.type() == QDBusMessage::ErrorMessage)
                            qCWarning(lcPortal) << "RequestClipboard failed:" << reply.errorMessage();
                    }

                    // 3. Start: this is where the permission dialog appears if there is no token or it did not fit.
                    // No timeout: a person may read the dialog for as long as they like.
                    PortalBus::instance()
                        .request(kIface, QStringLiteral("Start"),
                                 {QVariant::fromValue(QDBusObjectPath(m_session)), QString()}, {})
                        .then(this, [this](const PortalResponse &r) {
                            if (!r.ok())
                                return fail(QStringLiteral("Start: ") + r.describe());
                            const uint devices = r.results.value(QStringLiteral("devices")).toUInt();
                            m_clipboardEnabled = r.results.value(QStringLiteral("clipboard_enabled")).toBool();
                            const QString token = r.results.value(QStringLiteral("restore_token")).toString();
                            qCInfo(lcPortal) << "RemoteDesktop session started, devices =" << devices
                                             << "clipboard =" << m_clipboardEnabled
                                             << "restore token:" << (token.isEmpty() ? "none" : "received");
                            if (!(devices & kDeviceKeyboard))
                                return fail(QStringLiteral("keyboard access was not granted"));
                            if (!token.isEmpty() && token != m_restoreToken) {
                                m_restoreToken = token;
                                emit restoreTokenChanged(token);
                            }
                            finishBegin(Status::success());
                        });
                });
        });

    return promise->future();
}

void PortalRemoteDesktop::end()
{
    if (m_session.isEmpty())
        return;
    QDBusConnection bus = PortalBus::instance().connection();
    bus.disconnect(PortalBus::kService, m_session, QStringLiteral("org.freedesktop.portal.Session"),
                   QStringLiteral("Closed"), this, SLOT(onSessionClosed(QVariantMap)));
    QDBusMessage close = QDBusMessage::createMethodCall(PortalBus::kService, m_session,
                                                        QStringLiteral("org.freedesktop.portal.Session"),
                                                        QStringLiteral("Close"));
    bus.asyncCall(close);
    m_session.clear();
    m_clipboardEnabled = false;
    // Same as when the portal closes it: the portal clipboard (PortalClipboard) lives only in the session.
    // Without the signal it would consider itself the clipboard owner and in the next session would restore
    // stale content after a paste instead of what the user copied.
    emit sessionClosed();
}

void PortalRemoteDesktop::onSessionClosed(const QVariantMap &)
{
    qCInfo(lcPortal) << "RemoteDesktop session closed by the portal";
    m_session.clear();
    m_clipboardEnabled = false;
    emit sessionClosed();
}

QFuture<Status> PortalRemoteDesktop::waitModifiersReleased(std::chrono::milliseconds timeout)
{
    return delayed(this, int(timeout.count()));
}

QFuture<Status> PortalRemoteDesktop::key(int keycode, bool pressed)
{
    return PortalBus::instance().call(
        kIface, QStringLiteral("NotifyKeyboardKeycode"),
        {QVariant::fromValue(QDBusObjectPath(m_session)), QVariantMap(), keycode, uint(pressed ? 1 : 0)});
}

QFuture<Status> PortalRemoteDesktop::sendModifierMask()
{
    if (m_session.isEmpty())
        return readyFuture(Status::failure(QStringLiteral("RemoteDesktop session is not started")));
    // Ctrl↓ Ctrl↑: with Alt held this is not a shortcut, and Alt no longer counts as a lone press
    return key(kKeyLeftCtrl, true).then(this, [this](const Status &s) {
        key(kKeyLeftCtrl, false);
        return s;
    });
}

QFuture<Status> PortalRemoteDesktop::sendPaste()
{
    if (m_session.isEmpty())
        return readyFuture(Status::failure(QStringLiteral("RemoteDesktop session is not started")));

    // Ctrl↓ V↓ V↑ Ctrl↑ with pauses: some clients do not manage to process the modifier
    // if V arrives in the same frame.
    struct Step { int key; bool pressed; };
    static constexpr Step steps[] = {
        {kKeyLeftCtrl, true}, {kKeyV, true}, {kKeyV, false}, {kKeyLeftCtrl, false}};
    constexpr int kGapMs = 20;

    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    auto run = std::make_shared<std::function<void(int)>>();
    *run = [this, promise, run](int i) {
        if (i == int(std::size(steps))) {
            promise->addResult(Status::success());
            promise->finish();
            *run = nullptr;   // break the reference cycle
            return;
        }
        key(steps[i].key, steps[i].pressed).then(this, [this, promise, run, i](const Status &s) {
            if (!s.ok) {
                // Do not leave Ctrl held
                key(kKeyLeftCtrl, false);
                promise->addResult(s);
                promise->finish();
                *run = nullptr;
                return;
            }
            QTimer::singleShot(kGapMs, this, [run, i] {
                if (*run)
                    (*run)(i + 1);
            });
        });
    };
    (*run)(0);
    return promise->future();
}

} // namespace mp::lx
