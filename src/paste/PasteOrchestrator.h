#pragma once

#include "core/CaptureSet.h"
#include "core/MessageBuilder.h"
#include "platform/Interfaces.h"
#include "render/ShotRenderer.h"

#include <QObject>
#include <QPointer>

namespace mp {

enum class PasteMode {
    Sequential = 1,     // mode 1: images one by one, then the text, Ctrl+V on the user's behalf
    FilesAndText = 2,   // mode 2: one clipboard entry with files, text and the first image
    Combined = 4,       // mode 4: stitched into one image + text (sequentially)
};

// Application profile delays (spec section 10). Starting values, refined on the prototype.
struct PasteTiming {
    int settleMs = 80;            // clipboard write → Ctrl+V
    int afterImageMs = 400;       // after the Ctrl+V of an image
    int afterTextMs = 400;        // after the last Ctrl+V, before closing the input session
    int restoreAfterMs = 1000;    // then the clipboard is restored (PST-8)
    int releaseTimeoutMs = 1500;  // how long to wait for the hotkey release
    int afterReleaseMs = 50;
    // A release followed by a new press within this time is a chord change, not a release: the Ctrl mask
    // turns a held Alt+V into Ctrl+Alt+V, and KDE reports "released" + "pressed" (problems.md 2026-10-21)
    int releaseSettleMs = 150;
};

struct PasteOptions {
    PasteMode mode = PasteMode::Sequential;
    MessageTemplate messageTemplate;
    RenderStyle style;
    EncodeOptions encode;
    PasteTiming timing;
    bool restoreClipboard = true;
    bool waitHotkeyRelease = false;   // the paste was started by a hotkey: wait for its release
    bool maskAltTap = false;          // the shortcut has Alt without Ctrl: suppress a lone Alt (sendModifierMask)
};

// Pasting the set (PST-1…PST-9). Works only through IClipboard and IInputInjector,
// knows nothing about the OS. Never presses Enter (PST-9).
//
// Sequential paste is a state machine on futures and timers in the main thread:
// bake everything (thread pool) → save the clipboard → open the input session → wait for
// the keys to be released → [write an image → pause → Ctrl+V → pause] × N → text →
// pause → close the session → pause → restore the clipboard.
class PasteOrchestrator : public QObject
{
    Q_OBJECT
public:
    PasteOrchestrator(IClipboard *clipboard, IInputInjector *injector, QObject *parent = nullptr);

    bool isRunning() const { return m_running; }
    // Which modes are possible on this platform right now.
    bool canSequential() const;

    void start(const CaptureSet &set, const PasteOptions &options);
    // Cancel ("Stop"). Immediately unless Ctrl+V is being pressed: waiting for the clipboard or the input session
    // may never end (the compositor restarted), and the paste must not hang.
    void cancel();

    // Report that the hotkey was released (IHotkeys::released) or activated again while the paste
    // is waiting for the release (IHotkeys::activated): the keys are held again, wait for the next release.
    void hotkeyReleased();
    void hotkeyPressed();

    // Prepared items for the modes, public for drag and drop and tests.
    struct Item {
        ClipboardPayload payload;
        QString what;   // for progress and logs: "A", "B", "text"
    };
    static QList<Item> bakeSequence(const CaptureSet &set, const PasteOptions &options);
    static ClipboardPayload filesAndTextPayload(const CaptureSet &set, const PasteOptions &options, QStringList *files);

signals:
    void progress(int done, int total);
    void finished(mp::Status status, mp::PasteMode mode);

private:
    void runSequence(QList<Item> items);
    // Open the input session once per paste; whoever asks while it is opening waits
    void openSession(std::function<void()> next);
    void step();
    void finish(const Status &status);
    void after(int ms, std::function<void()> fn);

    QPointer<IClipboard> m_clipboard;
    QPointer<IInputInjector> m_injector;
    PasteOptions m_options;
    QList<Item> m_items;
    int m_index = 0;
    ClipboardPayload m_saved;
    bool m_running = false;
    bool m_cancelled = false;
    bool m_keysInFlight = false;   // Ctrl↓ V↓ V↑ Ctrl↑ in progress: cancellation waits until it ends
    bool m_sessionOpen = false;
    bool m_sessionOpening = false;
    QList<std::function<void()>> m_sessionWaiters;
    bool m_released = false;
    quint64 m_releaseEpoch = 0;   // a newer release or press cancels the pending settle of an older release
    bool m_firstKeysSent = false;
    std::function<void()> m_onReleased;
    quint64 m_generation = 0;   // guards against delayed callbacks of an earlier run
};

} // namespace mp
