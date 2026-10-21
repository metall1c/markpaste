# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/); versions follow [Semantic Versioning](https://semver.org/). Before 0.2.0 the project was developed privately; versions 0.1.x were internal builds.

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

- GNOME, Sway/Hyprland and a real X11 session have been checked only partly; Windows is a beta.
- The Windows files are not signed (SmartScreen warns), and there is no auto-update yet.
- On Ubuntu 24.04 there is no Qt 6 LayerShellQt: on KDE there the strip is an ordinary window, and its paste button copies the set instead of pasting it (the paste hotkey works).
- KDE shows a "Remote control session started" notification on every paste (the portal's own notification).

[0.2.0]: https://github.com/metall1c/markpaste/releases/tag/v0.2.0
