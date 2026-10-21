# 0004. Strip: moving "on release" and visibility only with a non-empty set

- Status: accepted (2026-10-19)
- Spec items: STR-1, STR-5, APP-1, section 4 step 8

## Context

On KDE the user asked why the strip cannot be moved and why it is always open.

1. On KDE the strip is a layer-shell surface (role `LayerTop`, no keyboard). Only this way it stays above windows and does not take focus from the chat, which means the "Paste set" button works in mode 1. But the compositor does not let the user move such surfaces: the program sets the position with an edge anchor and margins.
2. The strip was shown on every start, even with an empty set.

## Decision

**Moving (STR-5).** The handle is the strip header (`⠿ Markpaste`). While the mouse button is held, pointer events go to our surface even outside it (implicit grab), with coordinates relative to the strip. On **release** we compute the point on the screen: the strip position we know (edge + margins + size, `assumedTopLeft()`) plus the release coordinates. This point selects the monitor, the nearest edge (left or right) and the height. The position (edge, offset from the top, monitor name) is kept in the `strip/*` settings. Changing the monitor recreates the surface (hide → `setScreen` → show): the output is set when the surface is created.

If the strip grew and went below the screen bottom, it moves up by itself.

**Outline while moving** (added 2026-10-19 at the user's request). While the button is held, the strip stays in place, so the pointer position on the screen is known exactly. During the drag a transparent layer-shell layer appears on every monitor (`LayerOverlay`, `Qt::WindowTransparentForInput`: an empty input region, no keyboard). It draws a dashed outline of the strip where it will end up after the release: already flush with the edge, at the right height and on the right monitor. The computation is shared with the move itself (`StripWindow::moveTarget`), so the outline and the result match. Only the old and new places of the outline are repainted. The layers are deleted after the release: the set of monitors may change by the next move. Debugging: the socket command `debug-ghost <x> <y> <w> <h>`.

Without layer-shell (GNOME) the strip is an ordinary window, and moving is `QWindow::startSystemMove()` done by the window manager.

**Visibility.**
- A start without a command and with an empty set shows only the tray icon (if "hide when the set is empty" is on); on the first start there is a "press Print" notification.
- The strip appears after a capture and hides when the set becomes empty (clearing, "clear after paste", deleting the last shot).
- Whether to hide the strip with an empty set is the "Hide the strip to the tray when the set is empty" setting ("Appearance" tab, `strip/hideWhenEmpty`, on by default; added 2026-10-20 at the user's request). When it is off, the strip does not hide when the set becomes empty and is shown on a normal start. Changing the setting with an empty set shows or hides the strip right away. Without a tray the strip does not hide regardless of the setting.
- The "—" button hides the strip to the tray. A click on the tray icon shows or hides the strip, an empty one included (at the user's request: at first a click with an empty set started a capture, which was unexpected). Capturing is Print or the tray menu.
- A repeated launch from the menu (`markpaste` or `--show`) shows the strip even with an empty set.
- Without a tray (GNOME without AppIndicator) the strip does not hide automatically: otherwise the program cannot be found (APP-1).

## Options considered

- **Smoothly moving the strip itself with the mouse.** Rejected. After each margin change, the next pointer events come relative to the old or the new position, and it is unknown whether the compositor has already applied the new margins. The error accumulates, and the strip would "run away" from the cursor.
- **Make the strip an ordinary window so that KWin moves it.** Rejected: "above windows" is lost, and a click on the strip takes focus from the chat, so the paste button stops working in mode 1.
- **Top and bottom edges.** Not done: the strip is vertical, at the top edge it would cover window titles. Can be added with a horizontal layout.

## Consequences

- The strip itself "jumps" on release, while its outline follows the cursor.
- The computation is correct as long as the compositor places the surface exactly by the anchor and margins. KWin does; to be checked on Sway.
