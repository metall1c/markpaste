# 0009. The first-start wizard is one window with real checks

- Status: accepted (2026-10-19)
- Spec items: APP-4, section 9 ("One wizard window for all permissions"), section 14 (the default key is taken).

## Decision

- **One window, four blocks:** system (environment, platform notes, an XWayland warning), capture, global hotkeys, paste; at the bottom, an autostart checkbox and a "Settings…" button. Not a step-by-step `QWizard`: the checks are independent, and the user sees everything at once.
- **When it is shown:** on the first normal start (`once/setupShown` in settings; before 2026-10-20 it was `general/setupShown`, which was not read back after a restart, see docs/problems.md) instead of a tray notification; later via the tray item "Setup check…" and the command `markpaste --setup`.
- **Capture** is a real shot of all monitors via `ICapture`, with the number of monitors, sizes and time. On Wayland the permission dialog appears right here if it is needed. A capture without the `Silent` flag gives a warning "the environment may show its own dialog on every shot".
- **Hotkeys**: the wizard shows the actual shortcuts from the system and asks to press each one. While the wizard is open, the controller passes the activation to it (`SetupWizard::hotkeyActivated`) instead of capturing or pasting. Not assigned: a warning and a "Change in system settings" button (on KDE). No `IHotkeys` at all: a hint to assign `markpaste --capture` / `--paste` in the keyboard settings.
- **Paste** is an end-to-end mode 1 check on the wizard itself: save the clipboard → input session (`IInputInjector::begin`, on Wayland the RemoteDesktop dialog the first time) → write a check text → 80 ms → Ctrl+V → 600 ms → close the session → restore the clipboard. Success means the text appeared in the wizard field. The delay before closing the session has the same reason as in `PasteOrchestrator` (docs/problems.md, 2026-10-19). Without background clipboard writes or Ctrl+V emulation the check does not run, and the wizard explains the "files + text" mode.
- The wizard is in `ui/` and works only through `Platform`, so it is covered by tests on stubs (`tests/test_setup_wizard.cpp`).

## Options considered

- **A step-by-step QWizard.** Extra "Next" clicks, while the checks do not depend on each other.
- **Check the paste in a third-party window** (ask to open a chat). Closer to real life, but the result cannot be checked programmatically; a field in the wizard itself gives an unambiguous answer.
- **Check whether the Print key is taken** (spec sections 9, 14). On Wayland the desktop assigns the shortcut, and the program sees only the result; on KDE the press is the check. For X11 and Windows, together with their hotkey implementations.

## Consequences

- After the update the wizard opens once also for those who have already used the program (there was no `general/setupShown` key before).
- If the wizard window did not get focus after the portal dialog (focus stealing prevention), the text does not appear: the wizard asks to click into the field and try again.
