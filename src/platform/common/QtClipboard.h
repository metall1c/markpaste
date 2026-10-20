#pragma once

#include "platform/Interfaces.h"

class QMimeData;

namespace mp {

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

protected:
    // Данные для QClipboard::setMimeData: записи набора, картинка ещё и как QImage, форматы системы
    QMimeData *makeMimeData(const ClipboardPayload &payload, ClipboardFlags flags) const;
    // Класть ли в буфер запись этого MIME-типа (Windows не нужны имена X11: UTF8_STRING…)
    virtual bool keepEntry(const QString &mimeType) const;
    // Форматы под конкретную систему поверх общих: пометки «не в историю», PNG для Windows…
    virtual void addPlatformFormats(QMimeData *md, const ClipboardPayload &payload, ClipboardFlags flags) const;

private:
    bool m_background;
};

} // namespace mp
