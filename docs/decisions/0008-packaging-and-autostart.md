# 0008. The .rpm package via CPack, the icon and autostart

- Status: accepted (2026-10-19)
- Spec items: section 11 ("Build and distribution", autostart, ID), APP-3, NFR-3.

## Decision

- **Packages come from CPack** in the same CMake: RPM (if `rpmbuild` is present), DEB (if `dpkg-shlibdeps` is present), always TGZ. There is no separate .spec: for OBS (spec section 11) it will be needed later and can call the same `cmake --install`.
- **Dependencies** are automatic from the `.so` files (rpm `AUTOREQ`, `dpkg-shlibdeps`), plus `xdg-desktop-portal` by hand: without it there is no capture on GNOME, no hotkeys and no Ctrl+V.
- **An absolute `Exec`** in the .desktop file, from `CMAKE_INSTALL_FULL_BINDIR`: KWin compares it with the process executable (`X-KDE-DBUS-Restricted-Interfaces`). That is why the package is built with `-DCMAKE_INSTALL_PREFIX=/usr`, and `CPACK_PACKAGING_INSTALL_PREFIX` is taken from it too, otherwise the path in the package and in Exec would differ.
- **The icon** is the SVG `data/icons/app.svg`, installed as `hicolor/scalable/apps/su._525.Markpaste.svg`. It repeats the icon that `AppIcon.cpp` draws in code for the tray and windows; the program does not read the icon file, it is needed by application menus and software centers.
- **AppStream metainfo**: `share/metainfo/su._525.Markpaste.metainfo.xml`, so that the package is visible in Discover and GNOME Software.
- **Autostart** is the file `$XDG_CONFIG_HOME/autostart/su._525.Markpaste.desktop` (XDG Autostart specification), not a value in the program settings: this way the system itself sees it and can turn it off (KDE "Autostart", GNOME Tweaks). A file without `Hidden=true` and without `X-GNOME-Autostart-enabled=false` counts as enabled. `Exec` is the path of the current process. The code is `src/platform/common/Autostart.cpp`; for Windows there was a stub (the Run key came with the Windows version).
- **License**: GPL-3.0-or-later (spec section 11, CLAUDE.md), the `LICENSE` file.

## Options considered

- **Our own .spec for rpm.** Controls the package more precisely, but duplicates the install rules and does not give a .deb. CPack is enough for a manual build.
- **A relative `Exec=markpaste`.** Simpler for relocatable packages, but then KWin does not recognize the program and does not let it into ScreenShot2 without a dialog.
- **Autostart through the Background portal** (`RequestBackground` with `autostart: true`). Needed for Flatpak; for a package from a repository a file in `~/.config/autostart` is simpler and works in all environments.

## Consequences

- The rpm is tied to the Qt version of the build; build it separately for each Fedora release.
- If autostart is enabled from a developer build, the `Exec` gets the path to `build/`.
- GPG package signing and a repository (OBS) are not done yet.
