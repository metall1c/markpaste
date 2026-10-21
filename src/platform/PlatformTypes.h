#pragma once

#include <QByteArray>
#include <QFuture>
#include <QImage>
#include <QKeySequence>
#include <QList>
#include <QPromise>
#include <QRect>
#include <QString>
#include <QStringList>

namespace mp {

// Result of an asynchronous platform operation. Errors are returned as values,
// not exceptions: on Wayland the user declining a portal dialog is a normal outcome.
struct Status {
    bool ok = true;
    QString error;

    static Status success() { return {}; }
    static Status failure(QString message) { return {false, std::move(message)}; }
    explicit operator bool() const { return ok; }
};

template <typename T>
QFuture<T> readyFuture(T value)
{
    QPromise<T> promise;
    promise.start();
    promise.addResult(std::move(value));
    promise.finish();
    return promise.future();
}

// --- Capture ---

struct ScreenFrame {
    QString screenName;     // output name: matches QScreen::name()
    QRect logicalGeometry;  // monitor position in logical desktop coordinates
    qreal scale = 1.0;      // physical pixels / logical
    QImage image;           // physical pixels
};

struct CaptureResult {
    Status status;
    QList<ScreenFrame> frames;
    // The program file was replaced while it was running (package update, rebuild): the system
    // no longer recognizes the process and does not allow fast capture. The shot may have come through
    // a fallback; the user should restart the program (docs/problems.md, 2026-10-20).
    bool programReplaced = false;
};

enum class CaptureCap {
    None = 0,
    WindowUnderCursor = 1 << 0,   // CAP-5
    Silent = 1 << 1,              // no dialog and no flash
};
Q_DECLARE_FLAGS(CaptureCaps, CaptureCap)
Q_DECLARE_OPERATORS_FOR_FLAGS(CaptureCaps)

// --- Hotkeys ---

enum class Action {
    Capture,
    PasteSet,
    NewShot,
    ClearSet,
    CaptureToStrip,   // shot straight to the strip, without the editor (Ctrl+Print)
};

struct BindResult {
    Status status;
    QString trigger;   // the actual shortcut; on Wayland the user confirms it
};

struct ActivationContext {
    QString activationToken;   // xdg-activation, if the platform provided one
};

// --- Clipboard ---

struct ClipboardEntry {
    QString mimeType;
    QByteArray data;
};

// One clipboard entry in several formats. Ordered from most to least preferred.
struct ClipboardPayload {
    QList<ClipboardEntry> entries;

    bool isEmpty() const { return entries.isEmpty(); }
    QStringList mimeTypes() const;
    const ClipboardEntry *find(const QString &mimeType) const;

    // Text with all accepted MIME type synonyms (text/plain;charset=utf-8, UTF8_STRING…).
    void addText(const QString &text);
    void addImage(const QByteArray &encoded, const QByteArray &mimeType = "image/png");
    static ClipboardPayload text(const QString &text);
    static ClipboardPayload image(const QByteArray &encoded, const QByteArray &mimeType = "image/png");
};

enum class ClipboardFlag {
    None = 0,
    ExcludeFromHistory = 1 << 0,   // not into history and not into the cloud (NFR-4)
};
Q_DECLARE_FLAGS(ClipboardFlags, ClipboardFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ClipboardFlags)

} // namespace mp
