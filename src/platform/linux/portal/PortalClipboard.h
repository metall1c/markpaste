#pragma once

#include "platform/Interfaces.h"

#include <QDBusObjectPath>
#include <QHash>
#include <QPointer>
#include <QStringList>

namespace mp::lx {

class PortalRemoteDesktop;

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
    void onSelectionTransfer(const QDBusObjectPath &session, const QString &mimeType, uint serial);
    void onSelectionOwnerChanged(const QDBusObjectPath &session, const QVariantMap &options);

private:
    QFuture<QByteArray> read(const QString &mimeType, int timeoutMs);
    QString sessionHandle() const;

    QPointer<PortalRemoteDesktop> m_session;
    ClipboardPayload m_payload;      // what we serve while the selection is ours
    bool m_owner = false;
    QStringList m_selectionMimes;    // types of the current foreign selection (SelectionOwnerChanged)
};

} // namespace mp::lx
