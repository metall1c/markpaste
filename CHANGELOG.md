# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/); versions follow [Semantic Versioning](https://semver.org/). Before 0.2.0 the project was developed privately; versions 0.1.x were internal builds.

## [0.2.4] — 2026-10-22

Fixes for the static Linux build.

### Linux

- File dialogs (choosing the save folder) are the system ones again, through the desktop portal. 0.2.2–0.2.3 showed Qt's own plain dialog.
- `install.sh` from the archive:
  - it waits until the running Markpaste has really quit before replacing it;
  - updating over SSH or a text console works;
  - `sudo ./install.sh --system` also closes the copies other users are running;
  - a home folder with a space in its name no longer breaks the menu entry or KDE's capture without a dialog;
  - the icon and KDE menu caches are refreshed.
- The .rpm no longer carries Debian's changelog and copyright files.

## [0.2.3] — 2026-10-22

The static build: the Linux program now carries its own Qt 6.12.

### Linux

- **One .deb and one .rpm for all distributions** instead of a package per release: `markpaste_0.2.3-1_amd64.deb` for Debian 12+ and Ubuntu 22.04+, `markpaste-0.2.3-1.x86_64.rpm` for Fedora, openSUSE Tumbleweed and the like. They need only libraries every desktop has and keep working after distribution and Qt updates.
- A new archive for any distribution with glibc 2.35+, `markpaste-0.2.3-linux-x86_64.tar.gz`: `install.sh` inside installs it into `~/.local` (or `/usr/local`) with the menu entry. It replaces the archive built on Fedora, which needed Qt 6.11.
- The program uses its own look instead of the system style (Breeze); fcitx input does not work in it (ibus does). A package with the system Qt can still be built from source (docs/build.md).
- Ubuntu 24.04 with KDE: the program brings its own LayerShellQt, so the strip and the capture overlay should be layer-shell windows there too (not checked).

## [0.2.1] — 2026-10-22

Fixes for GNOME (checked on Ubuntu 26.04) and Ctrl+V on Windows.

### Windows

- The ordinary **Ctrl+V** is now the default paste-set key: it pastes the set while the set has shots. With an empty set, and in Markpaste's own windows, Ctrl+V is the ordinary paste. Existing installations keep their saved shortcut; type `Primary+V` in the settings to switch.

### GNOME

- The paste check in the setup wizard no longer hangs (the clipboard is served in its own thread).
- The strip is one frameless window with one header. It stays in place after a capture and when hidden: hiding minimizes it, and it comes back where you left it.
- **Always on Top** for the strip: right-click its header for the GNOME window menu.
- The app icon shows in the app grid.
- If Print opens the GNOME screenshot tool, the setup check frees it with one button. The settings can give the key back.
- "Copy set" works; the strip keeps its size as shots are added and removed.
- Shortcut labels are readable ("Ctrl+Alt+V" instead of "Press <Control><Alt>v").
- The tray menu starts with **Show set**: the Ubuntu tray opens the menu on a single click.

### Paste

- KDE: Alt+V no longer sometimes pastes nothing (and clears the set with "Clear the set after pasting"). The Ctrl the program presses to keep the chat's menu closed changed Alt+V into Ctrl+Alt+V and was taken for the release of the keys.
- Linux: the settings warn about paste shortcuts with Alt but without Ctrl, and refuse Ctrl+V (it would stop working in every program).

### Known limitations

- GNOME: the clipboard is empty after a paste (GNOME clears what was put into it during the paste).
- GNOME: a tray double click counts only within the GNOME double-click time; a slower one opens the menu.

## [0.2.0] — 2026-10-21

The first public release.

### Capture

- A global hotkey captures a screen area over a frozen screen: a magnifier, the size in pixels, arrow keys for pixel-precise adjustment, a click for the whole monitor.
- Two captures: **with editor** (Print) and **without editor**, straight to the strip (Ctrl+Print).
- Monitors with different scales: shots are kept in physical pixels.
- KDE Plasma: fast capture through KWin ScreenShot2 without a dialog, the Screenshot portal as a fallback. Windows: DXGI Desktop Duplication with GDI as a fallback (also in RDP sessions). X11: Qt capture.

### Marks and editing

- Numbered marks A1, A2…: frames (rectangle, rounded rectangle, ellipse), arrows, freehand lines and points, each with a comment.
- Every mark has its own color, width and line style; a thin dark halo keeps lines visible on any background (can be turned off).
- Blur (pixelation; the original pixels never leave the program), crop, unlimited undo and redo.
- Optional comment captions drawn on the image.

### Strip and paste

- Up to 26 shots (A–Z) in a strip at the screen edge: reorder by dragging, delete with the trash button on a thumbnail, move the strip to another edge or monitor.
- Sequential paste into the active chat with one hotkey (Ctrl+Alt+V): images one by one, then the text; the clipboard is restored afterwards. The program never presses Enter.
- Fallback modes: files + text in one clipboard entry, one stitched image + text, dragging the set into a chat.
- Message preview, saving the set to a folder (PNG + `message.md`), configurable message template; marks without a comment do not clutter the text.
- KDE Plasma and Sway/Hyprland on Wayland, GNOME through the Clipboard portal, X11 and Windows.

### Application

- A tray icon, a first-start setup check (capture, hotkeys, paste on your system), settings, autostart, Russian and English UI.
- On Wayland the hotkeys are owned by the desktop: "Change shortcuts…" opens the KDE dialog.
- Optionally keeps the set between restarts; a log file with rotation; a daily new-version check against GitHub Releases (the only network request, can be turned off).

### Packages

- Fedora 44 (.rpm); Ubuntu 24.04, Ubuntu 26.04 and Debian 13 (.deb, one per release); a Linux .tar.gz; a Windows 10/11 installer (per-user, no administrator rights) and a portable ZIP.

### Known limitations

- GNOME, Sway/Hyprland, a real X11 session and Windows 10 have been checked only partly.
- The Windows files are not signed (SmartScreen warns), and there is no auto-update yet.
- On Ubuntu 24.04 there is no Qt 6 LayerShellQt: on KDE there the strip is an ordinary window, and its paste button copies the set instead of pasting it (the paste hotkey works).
- KDE shows a "Remote control session started" notification on every paste (the portal's own notification).

[0.2.4]: https://github.com/metall1c/markpaste/releases/tag/v0.2.4
[0.2.3]: https://github.com/metall1c/markpaste/releases/tag/v0.2.3
[0.2.1]: https://github.com/metall1c/markpaste/releases/tag/v0.2.1
[0.2.0]: https://github.com/metall1c/markpaste/releases/tag/v0.2.0
