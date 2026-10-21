#pragma once

#include "platform/Interfaces.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>

namespace mp::lx {

// Global hotkeys via the xdg-desktop-portal GlobalShortcuts (KDE, GNOME 48+, Hyprland).
//
// The program only SUGGESTS a shortcut (preferred_trigger); the user assigns it for good
// in a system dialog or settings (spec section 9). The actual shortcut
// arrives in trigger_description. All bindings are made with one BindShortcuts: the portal
// rejects a repeated call in the same session.
class PortalGlobalShortcuts : public IHotkeys
{
    Q_OBJECT
public:
    explicit PortalGlobalShortcuts(QObject *parent = nullptr);
    ~PortalGlobalShortcuts() override;

    static bool isAvailable();

    // Interface compatibility: collects requests and sends them in one batch via bindAll().
    QFuture<BindResult> bind(Action action, const QString &preferred) override;
    QString name() const override { return QStringLiteral("xdg-desktop-portal GlobalShortcuts"); }

    QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) override;

    // The system shortcut settings for the program (portal v2, ConfigureShortcuts)
    bool canOpenSystemSettings() const override;
    void openSystemSettings() override;
    bool systemAssignsTriggers() const override { return true; }

    static QString shortcutId(Action action);
    static bool actionFromId(const QString &id, Action *action);
    // Primary+Shift+V → CTRL+SHIFT+v (XDG shortcuts specification format)
    static QString toPortalTrigger(const QString &portable);
    // trigger_description for the UI: GNOME gives "Press <Control><Alt>v" → "Ctrl+Alt+V";
    // KDE's "Ctrl+Alt+V" stays as is
    static QString displayTrigger(const QString &description);

private slots:
    void onActivated(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &options);
    void onDeactivated(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &options);
    void onShortcutsChanged(const QDBusMessage &message);

private:
    QFuture<Status> ensureSession();

    QString m_session;
    QFuture<Status> m_sessionFuture;
    bool m_sessionStarted = false;
};

} // namespace mp::lx
