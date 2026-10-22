#pragma once

#include "core/MessageBuilder.h"
#include "paste/PasteOrchestrator.h"
#include "platform/PlatformTypes.h"
#include "render/ShotRenderer.h"

#include <QString>

namespace mp {

// Program settings (APP-3, spec section 11). Stored in QSettings (INI) in the user's
// configuration directory. Shortcuts are stored in portable form (Primary+Alt+V).
struct Settings {
    // Hotkeys
    QString captureHotkey = QStringLiteral("Print");
    QString pasteHotkey = defaultPasteHotkey();   // Ctrl+V on Windows, else Ctrl+Alt+V; not Ctrl+Shift+V (PST-2)
    QString captureToStripHotkey = QStringLiteral("Primary+Print");   // shot straight to the strip (ADR 0022)

    // Capture
    bool quickCapture = true;          // CAP-4: confirm by releasing the mouse
    bool clearOnNextCapture = false;   // spec section 4, step 8

    // Paste
    PasteMode pasteMode = PasteMode::Sequential;
    bool restoreClipboard = true;
    bool clearAfterPaste = false;
    int imageDelayMs = 400;

    // Message
    QString templateLanguage = QStringLiteral("auto");   // auto / ru / en, separate from the UI language (NFR-10)
    QString shotHeader;    // empty means the language template
    QString markLine;
    QString intro;
    QString outro;
    bool textBeforeImages = false;
    bool captionsOnImage = false;      // EDT-10: comments as captions on the image
    bool markHalo = true;              // dark halo around mark lines (ADR 0021)

    // Appearance and images
    QColor accent = QColor(0xFF, 0x5A, 0x1F);
    int badgeSize = 28;                // number badge diameter, logical px
    QString uiLanguage = QStringLiteral("auto");   // auto / ru / en: UI language (APP-5), after a restart
    bool hideStripWhenEmpty = true;    // an empty set hides the strip to the tray (ADR 0004)
    bool keepSetOnRestart = false;     // NFR-5: the set is kept on disk and survives a restart
    bool checkUpdates = true;          // APP-7, NFR-4: the only network request
    MarkPen markPen;                   // the editor's last pen; empty fields mean defaults
    int maxLongSide = 4096;
    int jpegThresholdMb = 5;
    int jpegQuality = 85;

    static Settings load();
    void save() const;

    MessageTemplate messageTemplate() const;
    PasteOptions pasteOptions() const;
    RenderStyle renderStyle() const;
};

// Directory of the saved set (NFR-5): <AppLocalDataLocation>/session, on Linux
// ~/.local/share/markpaste/session, mode 0700
QString sessionDirectory();

} // namespace mp
