#pragma once

#include "platform/Interfaces.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>

namespace mp::lx {

// Глобальные клавиши через xdg-desktop-portal GlobalShortcuts (KDE, GNOME 48+, Hyprland).
//
// Программа только ПРЕДЛАГАЕТ сочетание (preferred_trigger); окончательно его назначает
// пользователь в системном диалоге или настройках (ТЗ разд. 9). Фактическое сочетание
// приходит в trigger_description. Все привязки делаются одним BindShortcuts: повторный
// вызов в той же сессии портал отклоняет.
class PortalGlobalShortcuts : public IHotkeys
{
    Q_OBJECT
public:
    explicit PortalGlobalShortcuts(QObject *parent = nullptr);
    ~PortalGlobalShortcuts() override;

    static bool isAvailable();

    // Совместимость с интерфейсом: копит запросы и отправляет их пачкой через bindAll().
    QFuture<BindResult> bind(Action action, const QString &preferred) override;
    QString name() const override { return QStringLiteral("xdg-desktop-portal GlobalShortcuts"); }

    QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) override;

    // Системные настройки клавиш программы (портал v2, ConfigureShortcuts)
    bool canOpenSystemSettings() const override;
    void openSystemSettings() override;
    bool systemAssignsTriggers() const override { return true; }

    static QString shortcutId(Action action);
    static bool actionFromId(const QString &id, Action *action);
    // Primary+Shift+V → CTRL+SHIFT+v (формат спецификации XDG shortcuts)
    static QString toPortalTrigger(const QString &portable);

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
