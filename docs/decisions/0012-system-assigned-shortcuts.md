# 0012. On Wayland shortcuts are changed only through the system

- Status: accepted (2026-10-20)

## Context

APP-3 (settings), spec section 9. The user changed the paste shortcut to Alt+V in the settings, but the old Ctrl+Alt+V kept working. Through the GlobalShortcuts portal the program only suggests a shortcut (`preferred_trigger`). KDE honors it at the first binding, after that it keeps its own value (`~/.config/kglobalshortcutsrc`) and ignores new suggestions (docs/problems.md, 2026-10-20).

## Decision

- `IHotkeys::systemAssignsTriggers()`: `true` for the portal, `false` for X11 and stubs.
- If the system assigns the shortcuts, the "Capture" and "Paste set" fields in settings are read-only and show the actual shortcuts from the portal (`trigger_description`). The "Change shortcuts…" button calls `ConfigureShortcuts` (portal v2), which opens the KDE dialog for the program's shortcuts.
- A change in the system arrives as the portal signal `ShortcutsChanged` → `IHotkeys::triggerChanged` → the controller updates the actual shortcuts (wizard, hints) without a restart.
- The `hotkeys/capture` and `hotkeys/paste` values in the program settings remain the suggestion for the first binding; the settings dialog does not change them.

## Options considered

- **Write the shortcut to KDE directly** via D-Bus `org.kde.kglobalaccel` or `kglobalshortcutsrc`. Rejected: KDE only, bypasses the portal and the user's consent, an internal format.
- **Recreate the binding under a new id** (`paste-set-2`) so that the portal accepts the new suggestion. Rejected: dead entries pile up in the KDE settings, and KDE may ask the user anyway.
- **Keep the field editable with an explanation.** That was the case, and the user did not understand that the value would not apply.

## Consequences

- On X11 (and later on Windows) the field is still editable, the shortcut applies after a restart.
- On GNOME the GlobalShortcuts portal is v1 without `ConfigureShortcuts`: there is no button, the field is read-only, changes are made in the GNOME settings. Not checked.
- Checked on KDE 6.7.5 (2026-10-20): after "Change shortcuts…" KDE sent `ShortcutsChanged` (`paste-set changed to "Ctrl+Alt+V, Alt+V"`), Alt+V works without a restart. KDE allows several shortcuts per action; the string is shown as is.
