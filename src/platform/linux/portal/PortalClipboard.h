#pragma once

#include "platform/Interfaces.h"

#include <QDBusObjectPath>
#include <QHash>
#include <QPointer>
#include <QStringList>

namespace mp::lx {

class PortalRemoteDesktop;

// Буфер обмена через портал Clipboard (ТЗ разд. 9, 10). Запасной путь для окружений без
// ext-data-control-v1 — прежде всего GNOME (Mutter).
//
// Портал работает только внутри сессии RemoteDesktop, в которой до Start вызван
// RequestClipboard и которая ответила clipboard_enabled. Поэтому:
// - писать и читать можно только между IInputInjector::begin() и end();
// - needsInputSession() = true: оркестратор вставки открывает сессию раньше, чем
//   сохраняет буфер, и возвращает буфер до закрытия сессии.
// Запись: SetSelection(типы) → портал присылает SelectionTransfer(тип, serial) на каждый
// запрос получателя → SelectionWrite(serial) даёт fd → пишем данные → SelectionWriteDone.
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
    ClipboardPayload m_payload;      // что отдаём, пока selection наш
    bool m_owner = false;
    QStringList m_selectionMimes;    // типы текущего чужого selection (SelectionOwnerChanged)
};

} // namespace mp::lx
