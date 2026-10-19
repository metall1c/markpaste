#pragma once

#include "platform/PlatformTypes.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

namespace ps::lx {

struct PortalResponse {
    // 0 — успех, 1 — пользователь отменил, 2 — другая ошибка (спецификация Request).
    // Для ошибок D-Bus и таймаута используем code = 2 и заполняем error.
    uint code = 2;
    QVariantMap results;
    QString error;

    bool ok() const { return code == 0; }
    QString describe() const;
};

// Отдельное подключение к сессионной шине для всех вызовов xdg-desktop-portal.
//
// Почему отдельное: портал узнаёт программу по подключению (sender). Регистрация
// app id через org.freedesktop.host.portal.Registry должна быть ПЕРВЫМ вызовом порталов
// на подключении, а общее подключение Qt (QDBusConnection::sessionBus) к этому моменту
// уже мог использовать сам Qt — например, платформенная тема читает портал Settings.
// Все порталы программы обязаны идти через это подключение, иначе разрешения
// и токены восстановления не свяжутся с app id.
class PortalBus : public QObject
{
    Q_OBJECT
public:
    static PortalBus &instance();

    QDBusConnection connection() const { return m_bus; }
    bool isConnected() const { return m_bus.isConnected(); }

    // Регистрация app id. Повторный вызов ничего не делает.
    Status registerApp(const QString &appId);
    QString appId() const { return m_appId; }
    bool hasInterface(const QString &interface) const;
    uint interfaceVersion(const QString &interface) const;

    // Вызов метода портала, который отвечает через объект Request.
    // В options добавляется handle_token, подписка на Response — до вызова (иначе гонка).
    // timeoutMs = 0 — ждать бесконечно (диалог разрешения может висеть долго).
    QFuture<PortalResponse> request(const QString &interface, const QString &method,
                                    QVariantList argsBeforeOptions, QVariantMap options,
                                    QVariantList argsAfterOptions = {}, int timeoutMs = 0);

    // Обычный вызов без Request (например, NotifyKeyboardKeycode).
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

} // namespace ps::lx
