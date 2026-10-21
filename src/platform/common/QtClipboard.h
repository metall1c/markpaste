#pragma once

#include "platform/Interfaces.h"

class QMimeData;

namespace mp {

// Clipboard via QClipboard. A portable fallback: on Wayland without data-control
// Qt changes the clipboard only while our window has keyboard focus, so
// canWriteInBackground() = false, and sequential paste is not enabled with it.
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
    // Data for QClipboard::setMimeData: the set's entries, the image also as a QImage, system formats
    QMimeData *makeMimeData(const ClipboardPayload &payload, ClipboardFlags flags) const;
    // Whether to put an entry of this MIME type into the clipboard (Windows needs no X11 names: UTF8_STRING…)
    virtual bool keepEntry(const QString &mimeType) const;
    // System-specific formats on top of the common ones: "not into history" marks, PNG for Windows…
    virtual void addPlatformFormats(QMimeData *md, const ClipboardPayload &payload, ClipboardFlags flags) const;

private:
    bool m_background;
};

} // namespace mp
