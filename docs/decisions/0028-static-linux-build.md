# 0028. A static Linux build with its own Qt: one archive for any distribution

- Status: accepted (2026-10-22), at the user's request
- Spec items: section 11 (Linux packages with system Qt; AppImage and Flatpak in the second round), section 7 ("on Linux only system packages"), NFR-3

## Context

A build against the distribution's Qt runs only with that Qt. Within Qt 6 the binary compatibility goes forward, so a public-API build survives a Qt update, but since 0.2.1 the program also uses private Qt API (the GNOME window menu, ADR 0026; the xdg-activation token). The Fedora binary needs `Qt_6.11_PRIVATE_API`: a Qt 6.12 update inside Fedora 44 would make dnf hold back Qt or remove markpaste, and on Debian/Ubuntu a .deb would stop starting after a distribution upgrade. The `.tar.gz` of 0.2.0–0.2.1 was the Fedora build and needed exactly Qt 6.11.

The user asked for a build that is as static as possible, with the newest Qt, at the price of size.

## Decision

- **`tools/static/`**: a `Dockerfile` (Debian 12) and `build-qt.sh` build a static **Qt 6.12.0** (qtbase, qtsvg, linguist tools, qttranslations; the Wayland client is part of qtbase since 6.10) and a static **LayerShellQt 6.8.1** into `/opt/qt` of the image. `tools/static/build.sh` (or `build-in-container.sh static`, the CI target `static`) builds Markpaste with it, runs the core and UI tests and packs `dist/markpaste-X.Y.Z-linux-x86_64.tar.gz`.
- **Debian 12 as the base**: glibc 2.36, the binary needs at most `GLIBC_2.35`. It runs on Debian 12+, Ubuntu 22.04+, Fedora 36+, and current Arch and openSUSE Tumbleweed.
- **What stays shared** (every graphical desktop has it, the ABI is stable for decades): glibc, libstdc++, libwayland-client/cursor, libxcb, libxkbcommon(-x11), fontconfig, freetype, libdbus-1. Everything else is inside: Qt, its platform plugins (wayland, xcb, offscreen), xdg-shell, the Adwaita/Bradient decorations, ibus and compose input, the xdg-desktop-portal theme, SVG, the xcb extensions and xcb-util (`libxcb-cursor0` is missing on many desktops), harfbuzz, png, pcre2, zlib.
- **OpenGL is off** (`INPUT_opengl=no`): the program draws with QPainter, Wayland gets shm buffers, so there is no libGL/libEGL dependency. **OpenSSL is loaded at run time**: only the update check needs it, and every distribution has `libssl.so.3`.
- **Plugins**: a static Qt links only its default plugins (xcb, imageformats). `markpaste_import_qt_plugins()` in CMake adds wayland, offscreen (tests), ibus/compose and the portal theme, and drops the shells and formats we never use.
- **LayerShellQt**: only the static `LayerShellQtInterface`; `Window::get()` installs the shell integration itself, so the plugin and the QML part (it would need QtQml) are cut out of its CMake.
- **The Qt translations** (`qtbase_ru.qm`) go into the resources: a static Qt has no translations directory on the user's machine. `main.cpp` looks in the Qt directory first, then in `:/i18n`.
- **The archive**: `markpaste`, `install.sh`, `share/` (.desktop, icon, AppStream), `LICENSE`. `install.sh` installs into `~/.local` (`--system`: `/usr/local`) and writes the absolute `Exec` path into the .desktop file: KWin lets a program into ScreenShot2 only when `Exec` matches its executable. A running copy is quit and restarted (a replaced file breaks the KWin check, docs/problems.md 2026-10-20).
- The `.tar.gz` of the Fedora build is no longer released; the .rpm and the .deb files stay as they are, with the system Qt.

## Options considered

- **AppImage with the shared Qt libraries** (linuxdeploy). Simpler: no Qt build. Rejected for now: it needs FUSE, which is not installed by default on several systems (Ubuntu 24.04), and KWin's Exec check does not work with the AppImage mount path that changes on every start.
- **Flatpak.** The spec's second round. The sandbox changes the paths of temporary PNGs and the portal app id; more work than an archive.
- **A fully static binary (static glibc).** Not possible in practice: NSS, `dlopen` of OpenSSL, GPU and input drivers need the dynamic loader.
- **Building libxkbcommon, fontconfig, libwayland statically too.** They read system data (XKB keymaps, font configs, the compositor protocol), and libxcb's users must share one copy; linking them statically only adds version mismatch risks.
- **Debian 11 or Ubuntu 22.04 as the base, for older glibc.** Qt 6.12 needs CMake 3.25 and a C++17/20 compiler; Debian 12 has them (CMake 3.31 is still taken from Kitware for ECM 6.31). Older systems are out of the spec's range anyway.
- **Making the .deb and .rpm from the static binary too** (one package per format for all releases). Possible with the same image; not done yet: it is the user's call whether the main platform (Fedora KDE) should lose the system Qt integration (Breeze style, KDE platform theme, fcitx).

## Consequences

- The binary is 29 MB, the archive 12 MB (NFR-3 speaks about packages without a bundled Qt; this archive is an addition to them).
- Breeze and other system styles and the KDE platform theme are not loaded: the program has its own style anyway (ADR 0019), file dialogs go through the portal theme. fcitx input does not work (its Qt plugin is not built); ibus works.
- Qt security fixes reach users only with a new Markpaste release; the Qt version is bumped in the Dockerfile.
- The image build (Qt from source) takes 6 minutes on 16 cores, about 30 minutes on a GitHub runner, on every CI run (no layer cache yet).
- Qt 6.12 itself also registers the app id with the portal and warns "Connection already associated with an application ID": we registered first, so the warning is harmless.
- Checked: Ubuntu 26.04 GNOME Wayland (the whole cycle: portal capture, editor, comment, paste into Text Editor, update check over HTTPS, `install.sh` install, update and uninstall); Fedora 44 KDE Wayland (start: KWin ScreenShot2, GlobalShortcuts, ext-data-control); X11 under Xvfb in bare containers of Debian 12, Ubuntu 22.04, Ubuntu 24.04, Fedora 44, Arch, openSUSE Tumbleweed (all libraries found, the program runs). See docs/platform-matrix.md.
