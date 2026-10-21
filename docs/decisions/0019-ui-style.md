# 0019. Window styling: our own icons and buttons, system colors, the accent from settings

- Status: accepted (2026-10-20; the settings, wizard and preview windows on the same day)
- Spec items: section 4 (step 3, the editing window), section 11 "Settings → Appearance" (accent color, dark/light theme from the system), STR-1, STR-2, EDT-1, NFR-9

## Context

The shot window and the strip looked "like a form". A comparison with Snapik (the user's screenshots, 2026-10-20) revealed problems:

- Font symbols stood in for icons (▭ ➚ ✎ ● ▦ ⛶ ↶ ↷). Their size and weight depend on the system font; on Windows ↶↷ came out tiny.
- The selected tool was painted in the system selection color. On Windows with a red accent "Mark" looked like an error.
- There was no primary button: "Delete shot", "New shot" and "Done" had the same weight.
- All elements were in one row, without groups.
- The shot lay on the same background as the window. The comments were a bare QListWidget.
- The strip had four identical buttons, three of them disabled with an empty set. A Breeze gear, a "💬" emoji.

The user approved a mock-up of the new look (the "Markpaste — new design" artifact).

## Decision

**Icons.** Our own set of line SVGs in `data/icons/ui/`: 24×24, line 1.8, color `currentColor`. They are embedded into the program as the `:/icons/ui/` resource. `iconPixmap()` (`src/ui/Theme.cpp`) substitutes the state color and draws the icon with `QSvgRenderer`; finished pixmaps are cached. A new dependency is Qt Svg:
- Fedora: `qt6-qtsvg-devel`;
- Debian/Ubuntu: `libqt6svg6-dev`;
- MinGW: `mingw64-qt6-qtsvg`. `Qt6Svg.dll` was in the ZIP before too: the `qsvgicon` plugin needs it.

The package dependency on the library is derived automatically: in .rpm from ELF, in .deb via shlibdeps. Three outlines follow Lucide (ISC), with a notice in `data/icons/ui/README.md`.

**Colors: `UiColors`** (`src/ui/Theme.h`).
- Background and text come from the system palette, so the light or dark theme follows the system. A theme change on the fly is picked up.
- Shades (the shot backdrop, the side panel, cards, borders, hints) are a mix of the window background and the text color. This way they suit any theme, not only Breeze.
- The hint color is adjusted to a contrast ≥ 4.5:1 with the background (NFR-9).
- The accent comes from settings, the same as for marks. For fills under white text (the selected tool, the primary button, letter circles) it is darkened the same way as the mark badge background (`badgeFillColor`, ADR 0003).
- The system selection color is left only on the selection handles on the canvas.

**Buttons paint themselves** (`src/ui/Controls.h`). On Breeze, Windows 11 and Fusion they look the same; the system style does not touch them:
- `FlatButton` (a QToolButton subclass): tools, undo and redo, trash, strip header buttons, pen. The button text remains its name for screen readers and tests, while the button itself shows the icon.
- `ActionButton` (a QPushButton subclass): primary (filled with the accent) or secondary (with a border), with an icon and a shortcut on the right: "Paste set Alt+V".
- `KeyHint` is "[Ctrl+Z] undo", `Rule` a separator, `Panel` the side panel background.
- `PillButton` is a "pill" tab: the selected one is tinted with the accent.
- `Card` is a rounded card with a border.
- `StatusLine` is a check result: a ✓, ⚠, ✗ or clock icon and text in the result's color. Green, yellow and red are picked separately for the dark and light themes.

In dialog windows buttons take keyboard focus: when reached by Tab an accent-colored frame appears around the button, after a mouse click it does not.

**Shot window.**
- Toolbar: icons in groups "mark, arrow, pen, point | blur, crop | color, width | undo, redo". On the right, the "Delete shot" trash, "New shot" and the primary "Done".
- The tool hint is in the tooltip: the name, the key and what it does.
- The shot lies on a dark backdrop, with a shadow and a thin border.
- On the right, comment cards with the mark badge in its color. The mark selected on the canvas is highlighted with an accent border. A mark without a comment is a dashed card "Add a comment: without it the mark is not in the text" (ADR 0018). The panel header says "in the text: N of M".
- At the bottom: the tool name with a short hint and three shortcuts (Ctrl+Z, Ctrl+Enter, Esc). The Esc label is "discard shot" or "close".

**Strip.**
- Header: the program icon, "Markpaste", the number of shots, the "Clear" trash, "⋯" (preview, save to folder, settings), "Hide to tray".
- Empty set: a dashed frame "The set is empty · [Print] capture an area" and one primary button "New shot".
- With shots: the primary "Paste set" with its shortcut, below it "New shot" and a "Copy set" icon. If the primary button copies by itself (Wayland without layer-shell), the copy icon is hidden.
- The shortcuts on the strip are the actually assigned ones (`m_boundTriggers`). From the KDE list "Ctrl+Alt+V, Alt+V" the shortest is taken.
- Thumbnails are full-width rounded cards. The letter is in a circle, the number of comments has an icon. The open shot has an accent outline. In the top-right corner of a thumbnail there is a "Delete shot" trash (STR-4): it deletes right away, without confirmation, because the deletion goes through the undo stack (EDT-8) and is undone with Ctrl+Z in the editor. The trash lies on a dark semi-transparent backing, like the comment counter, so that it does not get lost on a light shot; on hover the backing turns red. The same deletion stays in the right-click menu.
- Strip messages are on a rounded plate, not on the system selection color.
- On layer-shell (KDE, Sway) the strip became a transparent surface, as on Windows before: rounded corners, `StripWindow::paintEvent` paints the background with its border. An ordinary window (GNOME, X11) is still opaque, in the system frame.

**Settings, wizard and preview windows.**
- Fields, lists and checkboxes stay system ones: the system style does not get in the way there. `useAccentSelection` only recolors the selection, checkboxes and focus frame in these windows with the accent. On Windows 11 `QPalette::Accent` is also set for this, only when built with Qt ≥ 6.6; Linux does not have it.
- Settings:
  - instead of a `QTabWidget`, a row of "pill" tabs above the pages;
  - at the bottom "Cancel" and the primary "Save" (on Enter) instead of "OK/Cancel";
  - the mark color is a button with a color circle and an explanation that the same color marks the selected tool, primary buttons and shot letters;
  - system details are on a card, platform notes with ⚠;
  - the "Snap" tab is called "Capture", a noun like the others.
- Wizard:
  - the title "System check";
  - steps are cards with a number in a circle instead of a `QGroupBox`;
  - check results are `StatusLine`;
  - at the bottom "Settings…" and the primary "Done".
- Preview:
  - the summary "The chat will get…" in bold;
  - images with rounded corners and a border;
  - the message text on a card;
  - at the bottom "Copy text", "Save to folder…" and the primary "Close".

**Checking the look.** `markpaste-uipreview [--dark] DIR` renders the windows without a display (`--dark` is the Breeze Dark palette). `markpaste-uipreview --live 6 FILE.png` shows the strip and the shot window in a real session and captures the whole screen via spectacle. `--live-dialogs 0 PREFIX` shows three windows one after another and captures only the active window (`spectacle -a`): on Wayland windows cannot be arranged, and a capture of the whole screen includes other windows.

## Options considered

- **Editing right on the frozen screen, as in Snapik** (a toolbar under the selection). Rejected for several reasons:
  - The spec (section 4, step 3) describes a separate window.
  - On Wayland the overlay covers all monitors, and nothing else is visible while editing.
  - On GNOME there are risks with overlay focus.
  - A shot opened from the strip is edited in a window anyway, so two editors would have to be maintained.

  Almost everything that makes Snapik's look modern (icons, the accent and a primary button) worked in the window too.
- **Our own dark style on all systems** (Fusion + our own palette, as in Snapik). The windows would stop following the system's light theme. Qt 6.4 has no `QStyleHints::colorScheme`, so theme changes would have to be tracked by hand.
- **Style sheets (QSS) for buttons.** Mixed shades would have to be rebuilt into a string on every theme and accent change. The Windows 11 style and QSS conflict in places. QSS remains only on the comment field (an accent-colored border).
- **Icons from the theme (Breeze) or drawn in code.** Windows has no theme, and other DEs have different icons. Drawing 17 icons in code is a lot of code without a gain when Qt Svg is already present on the systems and in the ZIP.
- **Labels on tools.** Compared in the mock-up. The toolbar becomes wider than 1100 px and looks busy. The names moved to the tooltip and the hint line.

## Consequences and limitations

- A new build dependency: Qt Svg.
- STR-2: the full counter "2 shots · 3 comments" is shown in the tooltip of the shot count in the strip header rather than as a separate line (`docs/deviations.md`).
- The shot window is no narrower than ~800 px: that is how much the toolbar takes.
- The strip got wider: ~234 px instead of ~200, the thumbnail is 212 px.
- Buttons are drawn in code, so the system theme affects only colors, not shapes. There is no focus frame: the buttons of the shot window and the strip did not take focus before either (`Qt::NoFocus`), everything is reachable with keys (NFR-9 in docs/status.md).
