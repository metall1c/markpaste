#pragma once

#include "platform/common/QtClipboard.h"

namespace mp::win {

// Windows clipboard via QClipboard (OLE). Qt itself spreads the entry over Windows
// formats: the image into CF_DIB and CF_DIBV5, the file list into CF_HDROP, the text into
// CF_UNICODETEXT. Writes from the background: the window does not need focus for that.
//
// Qt does not put the "PNG" format itself (Chrome, Telegram and Office read it first: it has transparency and no
// loss), so we add the PNG bytes as they are. When reading (the clipboard snapshot before a
// paste) we take PNG and CF_DIB ourselves if Qt did not parse the image.
//
// "Not into history" (NFR-4, spec section 10) is three registered formats read by
// the Win+V clipboard history, cloud sync and clipboard monitor programs:
// ExcludeClipboardContentFromMonitorProcessing, CanIncludeInClipboardHistory = 0,
// CanUploadToCloudClipboard = 0.
//
// A busy clipboard. While another program keeps the clipboard open (rdpclip in an RDP session and clipboard
// managers open it after every change), OleSetClipboard does not write, and Qt only leaves
// a warning in the log. Without a check "Copied" would be a lie, and Ctrl+V would paste
// the previous clipboard content (docs/problems.md, 2026-10-20). So after writing we check that
// the clipboard is ours and retry until it is free; before the clipboard snapshot we wait the same way.
class WinClipboard : public QtClipboard
{
    Q_OBJECT
public:
    explicit WinClipboard(QObject *parent = nullptr);
    ~WinClipboard() override;
    QString name() const override { return QStringLiteral("Windows clipboard (OLE)"); }
    QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) override;
    QFuture<ClipboardPayload> snapshot() override;

    // QMimeData format for a registered Windows clipboard format with this name
    static QString windowsFormat(const QString &name);

    // How long to wait for another program to release the clipboard (about 2 s by default)
    void setBusyWait(int attempts, int intervalMs)
    {
        m_busyAttempts = attempts;
        m_busyIntervalMs = intervalMs;
    }

protected:
    bool keepEntry(const QString &mimeType) const override;
    void addPlatformFormats(QMimeData *md, const ClipboardPayload &payload, ClipboardFlags flags) const override;

private:
    ClipboardPayload readNow();
    bool clipboardFree() const;

    void *m_probeWindow = nullptr;   // HWND: open the clipboard for checks from our own window, not from NULL
    quint64 m_writeGeneration = 0;   // a new write cancels retries of the previous one still waiting for the clipboard
    int m_busyAttempts = 20;
    int m_busyIntervalMs = 100;
};

} // namespace mp::win
