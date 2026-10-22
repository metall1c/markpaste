#pragma once

#include "platform/Interfaces.h"
#include "PortalBus.h"

namespace mp::lx {

// Ctrl+V emulation via the RemoteDesktop portal (keyboard only).
//
// The session is opened before a series of pastes and closed right after (spec section 9):
// while it is open, the desktop may show a remote control indicator.
// The first time the portal shows a permission dialog; with persist_mode=2 it returns a
// restore_token with which later sessions open without a dialog.
class PortalRemoteDesktop : public IInputInjector
{
    Q_OBJECT
public:
    explicit PortalRemoteDesktop(QObject *parent = nullptr);
    ~PortalRemoteDesktop() override;

    bool isAvailable() const override;
    QFuture<Status> begin() override;
    void end() override;
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    QFuture<Status> sendModifierMask() override;
    bool maskAddsCtrl() const override { return true; }
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("xdg-desktop-portal RemoteDesktop"); }

    void setRestoreToken(const QString &token) { m_restoreToken = token; }
    QString restoreToken() const { return m_restoreToken; }

    // Request clipboard access (Clipboard portal) in the same session. Needed before begin().
    void setRequestClipboard(bool enabled) { m_requestClipboard = enabled; }
    bool clipboardEnabled() const { return m_clipboardEnabled; }
    QString sessionHandle() const { return m_session; }

    // evdev codes (linux/input-event-codes.h). Keys are pressed by code, not by character:
    // with a Russian layout there is no "v" character (spec section 10).
    static constexpr int kKeyLeftCtrl = 29;
    static constexpr int kKeyV = 47;

signals:
    void restoreTokenChanged(const QString &token);
    void sessionClosed();

private slots:
    void onSessionClosed(const QVariantMap &details);

private:
    void fail(const QString &error);
    void finishBegin(const Status &status);
    QFuture<Status> key(int keycode, bool pressed);

    QString m_restoreToken;
    QString m_session;
    bool m_requestClipboard = false;
    bool m_clipboardEnabled = false;
    bool m_starting = false;
    QList<std::shared_ptr<QPromise<Status>>> m_beginWaiters;
};

} // namespace mp::lx
