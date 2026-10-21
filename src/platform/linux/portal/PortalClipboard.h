#pragma once

#include "platform/Interfaces.h"

#include <QDBusObjectPath>
#include <QHash>
#include <QMutex>
#include <QPointer>
#include <QStringList>
#include <QThread>

#include <memory>

namespace mp::lx {

class PortalRemoteDesktop;

// Serves SelectionTransfer requests while our selection is set. Lives in its own thread.
//
// Why a thread: when the paste target is our own window (the setup wizard test field, a mark
// comment), Qt reads the Wayland offer synchronously in the main thread and waits on the pipe.
// The data for that pipe comes only after SelectionTransfer → SelectionWrite, and if those were
// handled in the main thread too, both sides would wait until Qt's read timeout and the paste
// would come out empty ("QWaylandDataOffer: timeout reading from pipe", see docs/problems.md).
class PortalSelectionServer : public QObject
{
    Q_OBJECT
public:
    // Both are thread-safe. clear() drops the data when the selection is no longer ours.
    void setSelection(const QString &session, const ClipboardPayload &payload);
    void clear();

private slots:
    void onSelectionTransfer(const QDBusObjectPath &session, const QString &mimeType, uint serial);

private:
    QMutex m_mutex;
    QString m_session;
    ClipboardPayload m_payload;
};

// Clipboard via the Clipboard portal (spec sections 9, 10). A fallback for environments without
// ext-data-control-v1, above all GNOME (Mutter).
//
// The portal works only inside a RemoteDesktop session in which RequestClipboard was called
// before Start and which answered clipboard_enabled. Therefore:
// - reading and writing are possible only between IInputInjector::begin() and end();
// - needsInputSession() = true: the paste orchestrator opens the session before
//   saving the clipboard and restores the clipboard before closing the session.
// Writing: SetSelection(types) → the portal sends SelectionTransfer(type, serial) for every
// receiver request → SelectionWrite(serial) gives an fd → we write the data → SelectionWriteDone.
class PortalClipboard : public IClipboard
{
    Q_OBJECT
public:
    explicit PortalClipboard(PortalRemoteDesktop *session, QObject *parent = nullptr);
    ~PortalClipboard() override;

    static bool isAvailable();

    bool canWriteInBackground() const override { return true; }
    bool needsInputSession() const override { return true; }
    QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) override;
    QFuture<ClipboardPayload> snapshot() override;
    QString name() const override { return QStringLiteral("xdg-desktop-portal Clipboard"); }

private slots:
    void onSelectionOwnerChanged(const QDBusObjectPath &session, const QVariantMap &options);

private:
    QFuture<QByteArray> read(const QString &mimeType, int timeoutMs);
    QString sessionHandle() const;

    QPointer<PortalRemoteDesktop> m_session;
    ClipboardPayload m_payload;      // what we serve while the selection is ours
    bool m_owner = false;
    QThread m_serverThread;
    std::unique_ptr<PortalSelectionServer> m_server;
    QStringList m_selectionMimes;    // types of the current foreign selection (SelectionOwnerChanged)
};

} // namespace mp::lx
