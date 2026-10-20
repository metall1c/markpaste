#pragma once

#include "PlatformTypes.h"

#include <QHash>
#include <QObject>

#include <chrono>

// Четыре платформенных интерфейса (ТЗ, разд. 8). Ядро и UI работают только через них.
// Все вызовы асинхронные: на Wayland за вызовом может стоять диалог портала.
// Ядро выбирает поведение по capabilities()/can*(), а не по названию ОС.

namespace mp {

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
    // true — сочетание из настроек программы только предлагается при первой привязке, а дальше
    // его хранит и меняет система (портал GlobalShortcuts). Поле в настройках тогда только для чтения.
    virtual bool systemAssignsTriggers() const { return false; }
    virtual QString name() const = 0;

signals:
    void activated(mp::Action action, const mp::ActivationContext &ctx);
    void released(mp::Action action);
    // Пользователь поменял сочетание в настройках системы
    void triggerChanged(mp::Action action, const QString &trigger);
};

class IClipboard : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool canWriteInBackground() const = 0;
    // Писать и читать можно только при открытой сессии ввода (IInputInjector::begin…end):
    // портал Clipboard работает внутри сессии RemoteDesktop.
    virtual bool needsInputSession() const { return false; }
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

} // namespace mp
