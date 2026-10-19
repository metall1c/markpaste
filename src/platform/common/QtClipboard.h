#pragma once

#include "platform/Interfaces.h"

namespace ps {

// Буфер через QClipboard. Переносимый запасной вариант: на Wayland без data-control
// Qt меняет буфер, только когда у нашего окна есть фокус клавиатуры, поэтому
// canWriteInBackground() = false, и последовательная вставка с ним не включается.
class QtClipboard : public IClipboard
{
    Q_OBJECT
public:
    explicit QtClipboard(bool backgroundWrites, QObject *parent = nullptr);

    bool canWriteInBackground() const override { return m_background; }
    QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) override;
    QFuture<ClipboardPayload> snapshot() override;
    QString name() const override { return QStringLiteral("QClipboard"); }

private:
    bool m_background;
};

} // namespace ps
