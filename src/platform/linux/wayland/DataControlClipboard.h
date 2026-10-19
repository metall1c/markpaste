#pragma once

#include "platform/Interfaces.h"

#include <QStringList>

namespace ps::lx {

class DataControlDevice;

// Буфер обмена через ext-data-control-v1: читать и менять selection можно без фокуса
// клавиатуры, то есть из фона. Есть в KWin (Plasma 6.7), Sway, Hyprland; нет в Mutter (GNOME).
//
// Сам протокол обслуживает DataControlDevice в потоке WaylandConnection, этот класс —
// потокобезопасный фасад для главного потока. Данные буфера отдаёт процесс-владелец
// по запросу, поэтому пока наш источник владеет selection, процесс должен жить.
class DataControlClipboard : public IClipboard
{
    Q_OBJECT
public:
    // nullptr, если композитор не поддерживает протокол.
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
    DataControlDevice *m_device;   // живёт в потоке Wayland
};

} // namespace ps::lx
