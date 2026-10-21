# 0001. The main Linux platform is Fedora 44 KDE Plasma on Wayland

- Status: accepted (2026-10-19)
- Spec items: section 2 ("Linux: focus on Wayland"), section 9 ("Priority: Wayland on Fedora 44"), section 13 (stages)

## Context

The spec names three Fedora 44 editions on Wayland as the reference (GNOME 50, KDE Plasma 6.6, Sway) and considers GNOME the most important and the hardest. The user refined the priority: the main platform is Fedora 44 KDE Plasma on Wayland. The development machine runs the same system.

## Decision

- The Linux implementation is built and checked on KDE Plasma (Wayland) first. Then GNOME and Sway, X11 last.
- Work starts with Linux, not Windows (in the spec the "Core and Windows" stage came first). The core and the UI are still written without OS ties (NFR-11).
- The actual Plasma version on the machine is 6.7.5, not 6.6 as in the spec: Fedora 44 received an update.

## Checked right away (2026-10-19, Plasma 6.7.5, KWin 6.7.5, Qt 6.11.2, xdg-desktop-portal 1.22.1)

- KWin offers `ext_data_control_manager_v1` (v1) to ordinary clients, so the program can change the clipboard from the background. The spec's hypothesis for KDE is confirmed at the protocol level.
- KWin does **not** offer `zwp_virtual_keyboard_manager_v1` to ordinary clients. Ctrl+V is emulated through the RemoteDesktop portal.
- Portals: Screenshot v2, ScreenCast v5, GlobalShortcuts v2, RemoteDesktop v2, Clipboard v1, InputCapture v2. `org.freedesktop.host.portal.Registry` is present. FileTransfer is **not**.
- `org.kde.KWin.ScreenShot2` v5 is present and available only to "trusted" programs: the .desktop file must contain `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2` (Spectacle does this).
- `zwlr_layer_shell_v1` v5 is present, LayerShellQt 6.7.5 is in the repository.

## Consequences

- On KDE mode 1 (sequential paste) is the main one: data-control for the clipboard plus RemoteDesktop for Ctrl+V.
- Fast capture on KDE goes through KWin ScreenShot2, the fallback is the Screenshot portal.
- KDE-only solutions are kept behind the platform interfaces and `capabilities()`, so that GNOME and Sway can be added without changing the core.
