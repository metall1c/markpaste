# 0026. GNOME: the strip stays mapped during a capture, "Always on Top" through the window menu, Print taken back from the GNOME screenshot tool

- Status: accepted (2026-10-21), after the user's first test on Ubuntu 26.04 GNOME
- Spec items: STR-1 (on GNOME the strip is an ordinary window), STR-5, section 9 ("Print Screen is often taken by the desktop … suggests turning off the system one"), APP-4 (the setup wizard)

## Context

Ubuntu 26.04 GNOME (Mutter, Wayland, no layer-shell) showed four problems with 0.2.1 test builds (problems.md 2026-10-21):

1. After a capture the strip came back in the middle of the screen, over the editor, although the user had moved it to a corner.
2. The strip is covered by any window.
3. The app grid showed no icon.
4. Print opened the GNOME screenshot tool, our Print shortcut never fired.

Wayland gives a program neither window positions nor "keep above". Before a capture the program hid its windows with `hide()`; Qt destroys the `xdg_toplevel` then, and the next `show()` is a new window that Mutter places where it wants.

## Decision

- **No unmapping for a capture.** Where the compositor places windows itself (Wayland without layer-shell), `StripWindow::hideForCapture()` keeps the strip shown but fully transparent: the frameless strip window has `WA_TranslucentBackground`, its background is not painted, and every child gets a `QGraphicsOpacityEffect` with opacity 0. `showAfterCapture()` removes the effects. The window keeps its place and its "Always on Top". Elsewhere (layer-shell, Windows, X11) it is `hide()` as before.
- **"Always on Top" from the GNOME window menu.** A right click on the strip header sends `xdg_toplevel.show_window_menu` for Qt's own toplevel; Mutter opens its window menu, which has "Always on Top". The tooltip of the header says so. Qt's `xdg_toplevel` comes from `QPlatformNativeInterface::nativeResourceForWindow("xdg_toplevel")` (private Qt Gui headers, `Qt6::GuiPrivate`), the seat and the serial from the public `QNativeInterface::QWaylandApplication` (Qt 6.5+). Only the request header of `xdg-shell` is generated; the interface symbols are Qt's. Code: `src/platform/linux/wayland/XdgWindowMenu.cpp`, `canShowWindowMenu()`/`showWindowMenu()` in `WindowRoles.h`.
- **Print back from GNOME.** `IHotkeys` got `desktopKeyHolder()`, `releaseDesktopKey()`, `restoreDesktopKeys()`. The portal implementation reads the GNOME screenshot keys with `gsettings` (`org.gnome.shell.keybindings` `show-screenshot-ui`, `screenshot`, `screenshot-window`, `show-screen-recording-ui`; the `gnome-settings-daemon` media keys of GNOME 41 and older) when `XDG_CURRENT_DESKTOP` has GNOME. The setup wizard shows "Print is taken by the GNOME screenshot tool…" and the button "Give Print to Markpaste": the key is removed from the GNOME value, the old value is saved in the program settings (`gnomeReleasedKeys`), and the program restarts with `--setup`. The program settings have "Give the freed keys back to the system".
- **Restart** (`restartWhenExited()` in `platform/Process.h`): `/bin/sh` waits until the old process is gone (the single-instance socket is free) and starts the program. A restart is needed because a GlobalShortcuts portal session cannot bind again, and a shortcut that GNOME did not grab at the first binding is not known to work later.
- **The icon**: the comment in `data/icons/app.svg` moved after the `<svg>` tag. Content sniffing needs `<svg` within the first 256 bytes; it was at byte 416, and the file was `application/xml`. A test checks the offset.

## Options considered

- **The whole program through XWayland on GNOME**: "above" and positions would work, but fractional scaling gets blurry, and the clipboard, hotkey and input paths change; too much risk. The user chose the window menu.
- **A hint only (Alt+Space → Always on Top)**: no code, but hard to discover; the hint is still in the tooltip.
- **An item in the strip's "…" menu that opens the window menu**: Mutter checks the serial of a pointer press on the toplevel; a click in a popup menu does not qualify, so only the right click on the header.
- **Moving the strip back to the remembered position after a capture**: impossible on Wayland, the program does not set positions of its windows.
- **A different default capture key on GNOME**: the user prefers Print; the wizard offers to take it instead.
- **Changing the GNOME setting silently on first start**: a system setting must not change without the user's click.

## Consequences

- The program uses private Qt Gui headers in one call. Distribution packages are rebuilt with each Qt update anyway; the Ubuntu 24.04 build (Qt 6.4) has no window menu (no public input serial there). `QT_NO_PRIVATE_MODULE_WARNING` is set.
- "Always on Top" lasts while the strip window exists: hiding the strip (tray, "hide when the set is empty") or a restart loses it, and GNOME places the strip anew.
- During a capture the transparent strip still exists in the compositor; the overlay covers it.
- Releasing Print changes the user's GNOME settings; the old value is kept and can be restored from the program settings. Uninstalling the program does not restore it.
- Checked: build and tests, `pastetest --gnome-keys` on a local copy of the GNOME schema (find, release, restore). On GNOME itself all four points wait for the user's check.
