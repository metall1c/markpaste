# 0022. Capture without editor (straight to the strip) on Ctrl+Print

- Status: accepted (2026-10-20)
- Spec items: section 4 (steps 1–5, 8), section 11 "Settings → Hotkeys", CAP-1, CAP-7, APP-4

## Context

The user's request: on Ctrl+Print an area shot must go to the strip without the editor and without comments. This way a set of several screens is collected quickly, and marks, if needed, are added later from the strip. The spec has no such key: section 11 lists capture, paste set, new shot and clear.

## Decision

- **A separate action `Action::CaptureToStrip`.** The default shortcut is `Primary+Print` (Ctrl+Print), the setting `hotkeys/captureToStrip`. The GlobalShortcuts portal id is `capture-to-strip`.
- **Names for the user** (since 0.1.3, at the user's request): "Capture with editor" (Print) and "Capture without editor" (Ctrl+Print). The same in settings (the "Capture" tab, both fields side by side), the wizard, the tray menu and the descriptions in KDE "Shortcuts". The internal names (`Action::CaptureToStrip`, `--capture-to-strip`, id `capture-to-strip`) did not change: KDE keeps the assigned key by the id. At first they were "Capture to strip" and "Capture", which did not show how the two actions differ.
- **Behavior.** The same overlay as for Print. After the selection the shot is added to the set, the strip is shown, the editor does not open. If the editor was open, it returns to its shot, as after a cancelled capture.
- **The command `markpaste --capture-to-strip`** is for environments without global hotkeys (Sway, GNOME 46–47): it is bound in the system settings, like `--capture`.
- **The first-start wizard** shows this key if it is assigned, but it does not affect the hotkey check result: it is an extra one.
- **"Start a new set on the next capture"** now clears the set only if it was pasted or copied and not changed after that (the `AppController::m_setDelivered` flag). Before, a closed editor was enough. With shots in a row without the editor the set would be cleared on every shot. Step 8 of spec section 4 describes exactly clearing after a paste.
- **Platforms:**
  - on Windows both keys are caught by one `WH_KEYBOARD_LL` hook: modifiers are compared exactly, Print does not fire on Ctrl+Print and vice versa;
  - on X11 two bindings on one key did not work at first; this was fixed (problems.md, 2026-10-20);
  - in the GlobalShortcuts portal it is a third `id` in the same `BindShortcuts`.

## Options considered

- **A modifier during the selection** (e.g. release the mouse with Shift for no editor). Rejected: a separate shortcut was asked for; nobody would find another gesture in the overlay.
- **A setting "do not open the editor after a capture".** Rejected: it changes the behavior of Print entirely. Both paths are needed at once: usually with marks, sometimes quickly.
- **Use `Action::NewShot`.** Rejected: "New shot" in the spec is the same operation as Print (the strip button). The new action has a different meaning, and the id `new-shot` may already have got into the KDE settings.
- **Straight to the clipboard, like ordinary screenshot programs.** Rejected: "to the strip" was asked for. A single shot is pasted with Ctrl+Alt+V anyway.

## Consequences

- **KDE: after a program update the "Global shortcuts request" dialog appears once.** For new `id`s KDE asks for confirmation, and `BindShortcuts` does not return until the answer. While the dialog is open, the program does not know its shortcuts (the button hints are empty). Whether the old Print and Ctrl+Alt+V fire during that time was not checked.
  - If the dialog closed without an answer (the program exited while it was open), KDE remembers the action without a key: `capture-to-strip=none,Ctrl+Print,…` in `kglobalshortcutsrc`. It does not ask again.
  - The key can then be assigned with the "Change shortcuts…" button in settings or in KDE "Shortcuts" (problems.md, 2026-10-20).
- On a fresh install all three shortcuts are confirmed in one dialog at the first start.
- Changing the `description` of already known ids does not trigger the dialog: KDE 6.7.5 updated the labels in `kglobalshortcutsrc` on the next `BindShortcuts`, the keys stayed (checked 2026-10-20).
- Not checked on GNOME 50 and in a real Windows 11 session (platform-matrix.md).
