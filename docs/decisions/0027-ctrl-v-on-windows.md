# 0027. Ctrl+V pastes the set on Windows; Linux warns about Alt without Ctrl

- Status: accepted (2026-10-22)
- Spec items: PST-2 ("intercepting the ordinary Ctrl+V with a non-empty set and the option on (Windows and X11 only)"; the default key is Ctrl+Alt+V), section 9 ("Own key presses", "Intercepting the ordinary Ctrl+V"), section 10 ("Released modifiers"), APP-3, APP-4

## Context

The user works daily on Windows 11 and Fedora KDE. Snapik pastes the set with the ordinary Ctrl+V on Windows, and the user wants the same as the Windows default. On Linux the default stays Ctrl+Alt+V.

On Linux a separate key is needed. A global shortcut belongs to the compositor (GlobalShortcuts portal, XGrabKey): the press is consumed and cannot be handed back to the active window. The program also cannot tell its own emulated Ctrl+V from the user's. Ctrl+V as a global shortcut would stop paste in every program and catch our own Ctrl+V.

Alt+V on Linux has its own weak spot (problems.md 2026-10-21, "KDE: Alt+V sometimes pastes nothing"). The modifier mask that keeps the chat from opening its menu on the Alt release is a Ctrl tap there (portal, virtual keyboard, XTest). While V is held, this turns Alt+V into Ctrl+Alt+V, and the compositor may report the hotkey as released early. `PasteOrchestrator` now settles releases for 150 ms. This is not checked for every desktop, and not for the case where only Alt+V is bound.

## Decision

- **Windows: Ctrl+V through the `WH_KEYBOARD_LL` hook, conditionally.** A binding that is exactly Ctrl+V (`WinHotkeys`, `isPlainPaste`) goes through the hook rather than `RegisterHotKey`, as a conditional trigger. The hook swallows the press only while it is armed and no window of our process is in the foreground. Otherwise the press goes on untouched: an empty set, a comment field in the editor, the settings. Our own `SendInput` presses carry `LLKHF_INJECTED` and are skipped, as before. The release comes from the hook (the V key-up); `SendInput` still waits for the user's Ctrl to be released, up to 1.5 s.
- **Arming**: `IHotkeys::setPasteKeyArmed(bool)` and `canInterceptPasteKey()`. `AppController` arms the key when the set has shots, on every model change and after binding.
- **Defaults**: `defaultPasteHotkey()` (`platform/PlatformTypes`) gives `Primary+V` on Windows and `Primary+Alt+V` elsewhere. Settings are stored with all values on the first save, so existing installations keep their saved Ctrl+Alt+V. Switching means typing `Primary+V` in the settings.
- **No separate "intercept Ctrl+V" option** (the spec has one). Ctrl+V is used only when it is the paste shortcut, so the shortcut field is the option.
- **Settings note under "Paste the set"** (`SettingsDialog::updatePasteKeyNote`):
  - Ctrl+V where it can be intercepted: an "ok" note explaining when Ctrl+V pastes the set.
  - Ctrl+V where it cannot be intercepted (Linux): an error, and Save is disabled.
  - Alt without Ctrl where the mask is a Ctrl tap (`IInputInjector::maskAddsCtrl()`, true for the three Linux injectors): a warning that it may paste unreliably, with Ctrl+Alt+V suggested.
  - Where the desktop assigns the shortcuts (portal), the note is about the actual bound shortcut.
- **Setup wizard**: a conditional Ctrl+V cannot be tested with an empty set, so it counts as working ("pastes the set while it has shots"). Alt without Ctrl on Linux gets a short hint.
- Shortcut parsing for these checks is shared: `triggersHaveAltWithoutCtrl()`, `triggersHavePlainPaste()`. They accept KDE's lists ("Ctrl+Alt+V, Alt+V") and the portable form. `AppController` now uses the first one to decide on the mask too.

## Options considered

- **Ctrl+V on X11 as well** (the spec allows it: `XGrabKey` while the set has shots, released around our own XTest press). Not done now: X11 is the last platform in priority, and a grab cannot be limited to "not in our own windows" without extra work. The settings refuse Ctrl+V there.
- **Re-injecting Ctrl+V on Wayland when the set is empty**: the program does not know the active window, and our own press would trigger the shortcut again. Rejected.
- **A Windows mask key for Linux too (0xE8)**: Linux has no such unused key that every compositor and toolkit ignores; Ctrl is what the portal and the virtual keyboard can send.
- **Forbidding Alt without Ctrl on Linux**: too strict, it mostly works and the user may prefer it. A warning instead.

## Consequences and limitations

- With shots in the set, Ctrl+V on Windows pastes the set in every program, even when the user wanted to paste something else. To paste the clipboard as usual, clear the set or use the "Paste" menu of the program. This is the Snapik behavior.
- The hook answers instantly (an atomic flag and `GetForegroundWindow`); it is installed while Print or Ctrl+V is bound.
- Checked on Windows 11 24H2 (VirtualBox) on 2026-10-22:
  - with an empty set, Ctrl+V pasted text in Notepad;
  - in the open editor, Ctrl+V did not start a paste;
  - with one shot, Ctrl+V pasted it into Paint;
  - after clearing the set, Ctrl+V was the ordinary paste again, and the clipboard was restored.
