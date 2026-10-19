#pragma once

#include "PlatformTypes.h"

#include <QHash>
#include <QObject>

#include <chrono>

// Четыре платформенных интерфейса (ТЗ, разд. 8). Ядро и UI работают только через них.
// Все вызовы асинхронные: на Wayland за вызовом может стоять диалог портала.
// Ядро выбирает поведение по capabilities()/can*(), а не по названию ОС.

namespace ps {

class ICapture : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QFuture<CaptureResult> captureAllScreens() = 0;
    virtual CaptureCaps capabilities() const = 0;
    virtual QString name() const = 0;   // для логов и мастера первого запуска
};

class IHotkeys : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    // preferred — в переносимом виде (Primary+Shift+V, ТЗ разд. 9).
    // Фактическое сочетание может отличаться: на Wayland его подтверждает пользователь.
    virtual QFuture<BindResult> bind(Action action, const QString &preferred) = 0;
    // Все действия разом. На Wayland — один запрос и один диалог портала;
    // повторная привязка в той же сессии портала не разрешена.
    virtual QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) = 0;
    // Сочетания назначает система (Wayland): открыть её настройки клавиш программы.
    virtual bool canOpenSystemSettings() const { return false; }
    virtual void openSystemSettings() {}
    virtual QString name() const = 0;

signals:
    void activated(ps::Action action, const ps::ActivationContext &ctx);
    void released(ps::Action action);
};

class IClipboard : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool canWriteInBackground() const = 0;
    virtual QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) = 0;
    virtual QFuture<ClipboardPayload> snapshot() = 0;   // только текст и картинки (ТЗ разд. 10)
    virtual QString name() const = 0;
};

class IInputInjector : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool isAvailable() const = 0;
    // Подготовка перед серией вставок. На Wayland — открыть сессию RemoteDesktop
    // (первый раз — диалог разрешения, потом — по токену восстановления).
    virtual QFuture<Status> begin() = 0;
    virtual void end() = 0;
    virtual QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) = 0;
    virtual QFuture<Status> sendPaste() = 0;   // по коду клавиши V, не по символу
    virtual bool canDetectForegroundChange() const = 0;
    virtual QString name() const = 0;
};

} // namespace ps
