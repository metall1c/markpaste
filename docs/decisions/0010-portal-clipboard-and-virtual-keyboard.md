# 0010. Clipboard via the Clipboard portal (GNOME) and Ctrl+V via virtual-keyboard-v1 (Sway, Hyprland)

- Status: accepted (2026-10-19)
- Spec items: section 9 (matrix of mechanisms), section 10 (mode 1, clipboard restore PST-8), item 5 of the Linux version plan (docs/status.md).

## Context

Mode 1 needs two things from the background: writing the clipboard and pressing Ctrl+V. On KDE these are ext-data-control-v1 and the RemoteDesktop portal. GNOME (Mutter) has no data-control; Sway and Hyprland have no RemoteDesktop portal (xdg-desktop-portal-wlr and -hyprland do not implement it).

## Decision

### Clipboard: the Clipboard portal

- `PortalClipboard` (`src/platform/linux/portal/`). Works only inside a RemoteDesktop session: `RequestClipboard` is called before `Start`, and after `Start` the portal reports `clipboard_enabled`.
- Writing: `SetSelection(mime_types)` → for every receiver request the portal sends `SelectionTransfer(mime, serial)` → `SelectionWrite(serial)` gives an fd → the data is written (`writeAllAndClose`, 3 s) → `SelectionWriteDone(serial, ok)`. Since 2026-10-21 (0.2.1) all of this runs in a dedicated thread (`PortalSelectionServer`), not in the main one: Qt reads a paste into our own window synchronously in the main thread, and serving the transfer there deadlocked until Qt's timeout (problems.md 2026-10-21). The Klipper hint `x-kde-passwordManagerHint` is added, as with data-control.
- Reading (saving before a paste): the types of a foreign selection come from the `SelectionOwnerChanged` signal, the data from `SelectionRead(mime)`. Only text and images, as with data-control. If the signal has not come yet, we try `text/plain;charset=utf-8`.
- **A new property `IClipboard::needsInputSession()`.** It is `true` for the portal, and `PasteOrchestrator` and the wizard change the order: the input session first, then saving the clipboard; the clipboard is restored **before** the session closes, not after. On an error the clipboard write also goes out before `Session.Close` (D-Bus messages on one connection are processed in order).
- Selection in `PlatformFactory`: data-control → Clipboard portal (if both RemoteDesktop and Clipboard are present) → QClipboard without background writes. `MARKPASTE_CLIPBOARD=portal` forces the portal (for testing on KDE, where it is also present: xdg-desktop-portal-kde 6.7.5).

### Ctrl+V: virtual-keyboard-v1

- `VirtualKeyboardInjector` (`src/platform/linux/wayland/`): `zwp_virtual_keyboard_manager_v1` from our own Wayland connection (the `markpaste-wayland` thread, ADR 0002). No dialog and no session; `end()` does nothing.
- The keymap is US from libxkbcommon (`evdev/pc105/us`), passed via a memfd. Ctrl+V is sent by the KEY_V code (47), plus an explicit `modifiers()` with the Ctrl mask: wtype does this, and this way the modifier reaches even clients that do not track key state.
- After the keyboard is created, a roundtrip: a compositor that does not trust the client answers with the `unauthorized` protocol error (and drops the connection), so we learn about it right away rather than on the first paste.
- Selection in `PlatformFactory`: if the compositor provides the protocol, it is used (wlroots), otherwise the RemoteDesktop portal. If the clipboard goes through the Clipboard portal, only RemoteDesktop (the clipboard lives in its session). `MARKPASTE_INPUT=portal` forces the portal.
- The protocol XML is not in wayland-protocols: a copy of `virtual-keyboard-unstable-v1.xml` from wlroots 0.19 (MIT) is in `src/platform/linux/wayland/protocols/`.

## Checks

- virtual-keyboard-v1: a nested headless Sway 1.11 (wlroots 0.19) on this machine, `pastetest --self-test`: 3 images + text, PASS in 4 runs out of 4 (in the same process and with the receiver in a separate process). See docs/build.md for how to start it.
- The Clipboard portal on KDE (xdg-desktop-portal-kde 6.7.5) was **checked on 2026-10-20** with `pastetest --portal-clipboard`: after the dialog `clipboard_enabled = 1`, reading and writing work. Not checked on GNOME 50.
- Orchestrator: the `SessionBoundClipboard*` tests on stubs (order begin → snapshot → … → write → end).

## Options considered

- **Keep the RemoteDesktop session open all the time the program runs**, so that a portal clipboard would live after the paste too. Rejected: the desktop shows a remote control indicator all that time (spec section 9).
- **wtype/ydotool as external programs.** An extra dependency; ydotool needs uinput and root rights.
- **virtual-keyboard-v1 with the user's layout.** It cannot be found out without focus; US + key code gives Ctrl+V with any layout.

## Consequences and limitations

- A selection written through the portal lives only while the session is open: on KDE after `Session.Close` the written content disappears and the clipboard has the previous content again (step 7 of `pastetest --portal-clipboard`, 2026-10-20). That is why the clipboard is restored before the session closes. The consequence for GNOME: leaving something in the clipboard after an action (e.g. "Copy set") through the portal is impossible; QClipboard with focus is needed; not done yet.
- While our presses are in progress, the window in Sway gets the US layout; the next press on the real keyboard brings the user's layout back.
- libxkbcommon became a build dependency (Qt already has it at run time).
