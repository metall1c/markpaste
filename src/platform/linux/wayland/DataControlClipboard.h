#pragma once

#include "platform/Interfaces.h"

#include <QStringList>

namespace mp::lx {

class DataControlDevice;

// Clipboard via ext-data-control-v1: the selection can be read and changed without keyboard
// focus, i.e. from the background. Available in KWin (Plasma 6.7), Sway, Hyprland; not in Mutter (GNOME).
//
// The protocol itself is served by DataControlDevice in the WaylandConnection thread; this class is
// a thread-safe facade for the main thread. The clipboard data is served by the owner process
// on request, so while our source owns the selection the process must stay alive.
class DataControlClipboard : public IClipboard
{
    Q_OBJECT
public:
    // nullptr if the compositor does not support the protocol.
    static DataControlClipboard *create(QObject *parent = nullptr);
    ~DataControlClipboard() override;

    bool canWriteInBackground() const override { return true; }
    QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) override;
    QFuture<ClipboardPayload> snapshot() override;
    QString name() const override { return QStringLiteral("ext-data-control-v1"); }

    bool ownsSelection() const;
    QStringList currentMimeTypes() const;

private:
    explicit DataControlClipboard(DataControlDevice *device, QObject *parent);
    DataControlDevice *m_device;   // lives in the Wayland thread
};

} // namespace mp::lx
