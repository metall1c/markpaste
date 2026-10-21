#pragma once

#include "PlatformTypes.h"

#include <QHash>
#include <QObject>

#include <chrono>

// Four platform interfaces (spec, section 8). The core and the UI work only through them.
// All calls are asynchronous: on Wayland a portal dialog may stand behind a call.
// The core picks its behavior by capabilities()/can*(), not by the OS name.

namespace mp {

class ICapture : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QFuture<CaptureResult> captureAllScreens() = 0;
    virtual CaptureCaps capabilities() const = 0;
    virtual QString name() const = 0;   // for logs and the first-start wizard
};

class IHotkeys : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    // preferred is in portable form (Primary+Shift+V, spec section 9).
    // The actual shortcut may differ: on Wayland the user confirms it.
    virtual QFuture<BindResult> bind(Action action, const QString &preferred) = 0;
    // All actions at once. On Wayland, one request and one portal dialog;
    // binding again in the same portal session is not allowed.
    virtual QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) = 0;
    // Shortcuts are assigned by the system (Wayland): open its shortcut settings for the program.
    virtual bool canOpenSystemSettings() const { return false; }
    virtual void openSystemSettings() {}
    // true: the shortcut from the program settings is only suggested at the first binding, and after that
    // the system keeps and changes it (GlobalShortcuts portal). The settings field is then read-only.
    virtual bool systemAssignsTriggers() const { return false; }
    virtual QString name() const = 0;

signals:
    void activated(mp::Action action, const mp::ActivationContext &ctx);
    void released(mp::Action action);
    // The user changed the shortcut in the system settings
    void triggerChanged(mp::Action action, const QString &trigger);
};

class IClipboard : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool canWriteInBackground() const = 0;
    // Reading and writing are possible only while an input session is open (IInputInjector::begin…end):
    // the Clipboard portal works inside a RemoteDesktop session.
    virtual bool needsInputSession() const { return false; }
    virtual QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) = 0;
    virtual QFuture<ClipboardPayload> snapshot() = 0;   // text and images only (spec section 10)
    virtual QString name() const = 0;
};

class IInputInjector : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool isAvailable() const = 0;
    // Preparation before a series of pastes. On Wayland, open a RemoteDesktop session
    // (the first time a permission dialog, later by the restore token).
    virtual QFuture<Status> begin() = 0;
    virtual void end() = 0;
    virtual QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) = 0;
    virtual QFuture<Status> sendPaste() = 0;   // by the V key code, not by the character
    // Press and release a "mask" key while the user still holds the Alt of the hotkey.
    // The system takes the V of Alt+V, and the active window sees a lone Alt press: Electron
    // (Mattermost, Slack), Firefox and Windows programs open their menu on it, and our Ctrl+V
    // goes to the menu (docs/problems.md, 2026-10-20). A press between Alt↓ and Alt↑ prevents that.
    virtual QFuture<Status> sendModifierMask() { return readyFuture(Status::success()); }
    virtual bool canDetectForegroundChange() const = 0;
    virtual QString name() const = 0;
};

} // namespace mp
