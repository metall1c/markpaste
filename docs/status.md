# Status against the spec: Linux and Windows

Updated: 2026-10-21 (GitHub, release 0.2.0). Priorities from the spec: **M**, required for 1.0; **S**, desirable; **C**, second round.
Marks: ✅ done and checked · 🟡 done, not checked or partial · ❌ not done.

## The main scenario on KDE Plasma (Wayland)

Works end to end and was checked by the user: Print → area → marks and comments → Ctrl+Alt+V. The set reaches Claude Code, ChatGPT, Claude and Telegram.

"Capture without editor" ([ADR 0022](decisions/0022-capture-to-strip.md), 2026-10-20, beyond the spec): Ctrl+Print → area → the shot goes straight to the strip, the editor does not open. The ordinary Print is called "Capture with editor" in the UI. Checked on X11 (Xvfb) and under Wine. On KDE the activation through the portal was checked; the physical key press is waiting for the user.

## Window styling

On 2026-10-20 the shot window and the strip were redone after an approved mock-up ([ADR 0019](decisions/0019-ui-style.md)). Changes: our own icons, tool groups, a primary button, comment cards, a hint line, an empty strip with a key hint, a rounded transparent strip on layer-shell. Checked: KDE Plasma 6.7.5 Wayland (`markpaste-uipreview --live`, dark Breeze), the light and dark palettes without a display, Windows 11 24H2 in VirtualBox (dark theme: shot, marks, comment, strip). On the same day the settings window ("pill" tabs, "Cancel" and "Save"), the wizard (step cards, result icons) and the preview were done in the same style. They were checked on KDE (`--live-dialogs`, Breeze Dark) and on Windows 11. The About window (version, author Andrey Che with e-mail, co-author Claude Opus 5.5, license, source code, system details with a "Copy details" button) opens from the strip's "⋯" menu, from the tray and with `markpaste --about`; checked on Windows 11. On 2026-10-21 each strip thumbnail got a trash button for deleting the shot in one click.

## Features (section 5)

| ID | Pr. | Status | What is left |
| --- | --- | --- | --- |
| CAP-1…4, CAP-7, CAP-8 | M | ✅ | — |
| CAP-6 different monitor scales | M | 🟡 | The code works in physical pixels, but only 2 monitors at 100 % were checked |
| CAP-5 window under the cursor | S | — | Windows and X11 only |
| EDT-1…8 | M | ✅ | Undo only from the editor: a shot deleted in the strip cannot be brought back without opening the editor. "Delete shot" in the editor closes the window, like Esc (since 2026-10-20). Frame shape: rectangle, rounded, ellipse ([ADR 0020](decisions/0020-frame-shape.md), 2026-10-20) |
| EDT-9 | C | 🟡 | Arrow, freehand (ADR 0006) and the point marker ([ADR 0015](decisions/0015-point-marker-and-captions.md)) are done; the user checked the point on KDE on 2026-10-20 |
| EDT-10 | C | 🟡 | The option "Also write comments on the image": caption plates next to the badges ([ADR 0015](decisions/0015-point-marker-and-captions.md)). Tests and uipreview; not checked in a live session |
| STR-1, STR-2, STR-4 | M | ✅ | The STR-2 counter is in the tooltip of the shot count in the header (since 2026-10-20, [ADR 0019](decisions/0019-ui-style.md)). STR-4: a trash button on each thumbnail (2026-10-21) |
| STR-3 reordering by dragging | S | 🟡 | A thumbnail can be dropped between others: the set is reordered, letters are recomputed, undone in one step; "Move up/down" stayed in the menu. Checked by the user on KDE on 2026-10-20 (the strip is layer-shell) |
| STR-5 moving the strip | S | ✅ | Left and right edges only |
| STR-6 dragging into a chat | M | 🟡 | Done, not checked in chats |
| PST-1, PST-3, PST-7, PST-8, PST-9 | M/S | ✅ | — |
| PST-2 starting a paste | M | ✅ | Intercepting the ordinary Ctrl+V is for Windows and X11 only per the spec |
| PST-4 "files + text" mode | M | 🟡 | Done, not checked in chats |
| PST-5 stitching | M | 🟡 | Done, not checked in chats |
| PST-6 message preview | S | 🟡 | A preview window: the same images and text as the paste gives ([ADR 0013](decisions/0013-set-on-disk.md)). A test and offscreen rendering; not checked in a live session |
| APP-1 tray | M | ✅ | Not checked on GNOME without a tray |
| APP-2 single instance | M | ✅ | — |
| APP-3 settings | M | 🟡 | The new settings were checked by the user on 2026-10-20. All the spec's groups are there: hotkeys, paste, message, appearance (color, badge size, UI language), images (longest side, JPEG threshold, quality), system (autostart, the set between restarts, version check). Missing: Ctrl+V interception, per-program delay profiles, the strip position in the window (moved with the mouse), theme (the system one is used) |
| APP-4 first-start wizard | M | 🟡 | Done (ADR 0009): capture check, hotkeys, an end-to-end Ctrl+V check, autostart. Tests on stubs; in a live KDE session the user went through it on 2026-10-20 without remarks |
| APP-5 Russian and English | M | ✅ | — |
| APP-6 saving the set to a folder | C | 🟡 | `A.png`, `B.png`… and `message.md` in a new folder ([ADR 0013](decisions/0013-set-on-disk.md)). A test; not checked in a live session |
| APP-7 new version notification | S | 🟡 | Once a day via the GitHub releases API ([ADR 0014](decisions/0014-logs-and-update-check.md), [ADR 0024](decisions/0024-github-home-and-releases.md)). Starts working with the first published release (0.2.0) |

## Non-functional (section 6)

| ID | Status | What is left |
| --- | --- | --- |
| NFR-1 speed | 🟡 | To the overlay: a median of 220 ms by the log of real shots (2×1080p, KWin). Pasting 5 4K shots up to the last Ctrl+V: 1.77 s (`markpaste-bench`). The overlay on 4K was not measured, there is no such monitor ([platform-matrix.md](platform-matrix.md#memory-and-speed-measurements-nfr-1-nfr-2)) |
| NFR-2 memory | 🟡 | 26 4K shots with the strip and the editor: 170 MB, the paste peak 460 MB (target 600). "Stuck" memory after a paste was found and fixed (750 → 165 MB, `malloc_trim`). CPU in the background: 0. In a live session: own memory in the background 48 MB (PSS 52, target 60), after pasting 3 shots back to 108 MB RSS within 6 s ([platform-matrix.md](platform-matrix.md#memory-and-speed-measurements-nfr-1-nfr-2)) |
| NFR-3 package sizes | 🟡 | 0.2.0 packages: see docs/release.md and platform-matrix.md. The Windows installer is 23.8 MB (below 40) |
| NFR-5 "survive a restart" | 🟡 | A checkbox in settings, off by default; `~/.local/share/markpaste/session`, 0700 ([ADR 0013](decisions/0013-set-on-disk.md)). Format and permission tests; a restart in a live session was not checked |
| NFR-8 reliability | 🟡 | Errors are shown, but failure scenarios (declining a portal dialog, a KWin crash) were not checked |
| NFR-9 keyboard accessibility | 🟡 | The strip has no keyboard (by design, so as not to take focus); everything is reachable with hotkeys and the tray |
| NFR-12 one-command build | 🟡 | On Fedora, yes; Ubuntu 24.04 (Qt 6.4): see the 0.2.0 .deb build in docs/build.md |
| Log file with rotation (sections 7, 11) | ✅ | `~/.local/share/markpaste/logs/markpaste.log`, 1 MB × 3 files, plus journald ([ADR 0014](decisions/0014-logs-and-update-check.md)) |

## Other Linux environments

| Environment | Status | What is wrong |
| --- | --- | --- |
| GNOME 50 | ❌ not checked | No data-control: mode 1 goes through the Clipboard portal inside a RemoteDesktop session, which is **implemented** (ADR 0010) and was checked on KDE with `pastetest --portal-clipboard` (2026-10-20), not on GNOME. Fallback modes 2–4 exist but were not checked. Overlay focus on GNOME was not checked |
| Sway, Hyprland | 🟡 | Ctrl+V via `virtual-keyboard-v1` (ADR 0010): mode 1 passes the self-test in a nested headless Sway 1.11. Not checked in a real Sway/Hyprland session; Sway has no global hotkeys (no GlobalShortcuts portal), so bind `markpaste --capture` in the Sway config |
| X11 | 🟡 | Global hotkeys (`XGrabKey`), Ctrl+V (XTEST), waiting for modifier release are done (ADR 0011) and checked on Xvfb: the paste self-test, hotkeys, the wizard; the whole program with xfwm4 (2026-10-20, when the loss of every second press was also fixed, problems.md). A real X11 session was not checked. Capture is `QScreen::grabWindow`, without MIT-SHM |
| Sandboxed programs (Flatpak, Snap) | ❌ | The FileTransfer portal is not implemented (this Fedora does not have it either) |
| Screenshot portal (fallback capture) | 🟡 | Implemented, not checked |

## Checks from section 12 that have not been done yet

- 50 sequential pastes in a row without losses.
- Pasting with a Russian keyboard layout.
- Monitors with different and fractional scales (125 %, 150 %).
- A repeated start after a reboot without repeated dialogs.
- A network monitor: no outgoing requests.

## Build and distribution (section 11)

| What | Status |
| --- | --- |
| .rpm and .deb packages (CPack) | 🟡 — built for 0.2.0 (docs/release.md); not installed on a clean system |
| Installing the .desktop file, icon and metainfo with `cmake --install` | ✅ — the icon `data/icons/app.svg` repeats the one drawn in code |
| Autostart (`~/.config/autostart`) | 🟡 — a checkbox in settings ("System" tab), the file is written and deleted (a test); logging in with autostart was not checked |
| CI: build on Fedora 44 and Ubuntu 24.04, tests, portals via python-dbusmock | 🟡 — GitHub Actions build and tests on Fedora 44 and Ubuntu 24.04 (`.github/workflows/ci.yml`); no portal tests |
| LICENSE, README, CONTRIBUTING | 🟡 — LICENSE (GPL-3.0-or-later), README in English and Russian with a user guide; no CONTRIBUTING |

## Windows (started 2026-10-20)

The Linux version is ready; the Windows version is cross-compiled on Fedora and checked under Wine ([ADR 0016](decisions/0016-windows-platform.md), [ADR 0017](decisions/0017-windows-cross-build.md)). The core and the UI are shared with Linux; only the implementations in `src/platform/windows/` are new. On real Windows 11 (VirtualBox) on 2026-10-20 the main scenario passed: Print → area → mark → Ctrl+Alt+V into Notepad, the strip button → an image into Paint (platform-matrix.md). A user on Windows Server 2022 over RDP got a black shot; fixed, waiting for a check. Code review (2026-10-20): fixed writing into a busy clipboard, pasting into our own window, the strip over the taskbar and beyond the screen edge (problems.md); on Windows 11 all three test suites pass: core 63, windows 36, platform 11.

| What | Status | What is left |
| --- | --- | --- |
| Capture (CAP-1…4, CAP-8) | 🟡 | DXGI Desktop Duplication + GDI per monitor; in RDP GDI only. DXGI was checked on Windows 11 in VirtualBox. Left: RDP, HDR, different scales (CAP-6), a rotated monitor |
| Global hotkeys (CAP-7, PST-2) | 🟡 | `RegisterHotKey`, Print Screen via a hook in its own thread. Work under Wine. On Windows 11, taking Print from the Snipping Tool |
| Clipboard (modes 1 and 2, NFR-4) | 🟡 | OLE + `PNG` + "not into history" marks; a clipboard busy in another program is retried, then an honest error (checked on Windows 11). Left: the Win+V history, pasting into chats |
| Ctrl+V (mode 1) | 🟡 | `SendInput`, waiting for modifiers, stopping on a window change, a UIPI check. Pasting into Notepad works under Wine |
| Strip (STR-1, STR-5) | 🟡 | A frameless non-activating window, moved to an edge within the work area (does not cover the taskbar). Works on Windows 11 and under Wine; different monitor scales not checked |
| Overlay: focus after Print | ✅ | `forceActivate` (`AttachThreadInput`): on Windows 11 Esc closes the overlay right away |
| Autostart, settings, log, NFR-5 | 🟡 | The `Run` registry key, settings in the registry, `%LOCALAPPDATA%`. A test under Wine; logging in was not checked |
| First-start wizard | 🟡 | Works under Wine. No hint about the Snipping Tool on Print |
| Portable ZIP | 🟡 | 35 MB, Fedora's Qt with ICU. Starts under Wine without Qt paths |
| Intercepting the ordinary Ctrl+V (PST-2) | ❌ | A hook while the set is non-empty, skipping our own presses |
| Per-program delay profiles (section 10) | ❌ | The target process name is already written to the log |
| Window under the cursor (CAP-5) | ❌ | |
| UIPI → automatically the "files + text" mode | ❌ | Currently an error with a hint |
| Setup.exe | 🟡 | NSIS, per-user without administrator rights, 23.8 MB ([ADR 0023](decisions/0023-windows-installer-nsis.md)). Checked under Wine; not installed on Windows 11 |
| Auto-update (Velopack, APP-7), SignPath signing | ❌ | |
| A CI build on Windows (MSVC, Qt from aqtinstall) | ❌ | Needed for the release and NFR-3 |

## Plan

The order was suggested on 2026-10-19; the user may change it.

1. **Checks on KDE** (cheap, the findings affect the rest): the "files + text" and stitching modes in chats, dragging from the strip, a Russian layout, 50 pastes in a row, different monitor scales.
2. ✅ **New mark shapes: arrow and freehand**, done on 2026-10-19, see [ADR 0006](decisions/0006-arrow-and-freehand-marks.md). Each mark has its own color, width and line style, done on 2026-10-19 at the user's request, [ADR 0007](decisions/0007-mark-pen.md); every shot opens with the "Mark" tool.
3. ✅ **The .rpm package, autostart, installing the .desktop file and icon**, done on 2026-10-19 (docs/build.md, "The .rpm package" section). Left: install the rpm on a clean system.
4. ✅ **First-start wizard** (APP-4), done on 2026-10-19, [ADR 0009](decisions/0009-setup-wizard.md). Passed in a live KDE session on 2026-10-20; found and fixed: it opened on every start (docs/problems.md).
5. 🟡 **GNOME 50**: the Clipboard portal for mode 1 was implemented on 2026-10-19 and checked on KDE on 2026-10-20 (`pastetest --portal-clipboard`); GNOME 50 is left; checking the fallback modes and overlay focus needs a GNOME system. **Sway and Hyprland**: `virtual-keyboard-v1` is done, the self-test in a nested Sway passes (ADR 0010).
6. 🟡 **X11**, done on 2026-10-19 (ADR 0011), checked on Xvfb. Left: a real X11 session, a 4K capture measurement.
7. **The KDE notification "Remote control session started" on every paste**: open; the solution (keep the RemoteDesktop session for a few minutes) is described in docs/problems.md (2026-10-20), not implemented yet.
8. 🟡 **Building packages in containers** (the user's decision on 2026-10-20: after the Linux features are done). `tools/package/build-in-container.sh <image>` → `build-pkg/<system>/`; Fedora 44 and Ubuntu 24.04 are done for 0.2.0 (docs/build.md), Debian 12 is left. The Ubuntu build with Qt 6.4 checks that the code uses no newer API (spec section 7).
9. **Windows** (the user's decision on 2026-10-20: the Linux version is ready, moving on to Windows). Done: the platform (capture, hotkeys, clipboard, Ctrl+V, strip, autostart), cross-compilation on Fedora, tests and an end-to-end check under Wine, the portable ZIP ([ADR 0016](decisions/0016-windows-platform.md), [ADR 0017](decisions/0017-windows-cross-build.md)). Setup.exe on NSIS ([ADR 0023](decisions/0023-windows-installer-nsis.md)). Next: checking on real Windows 10/11 (ZIP and Setup.exe), Ctrl+V interception, delay profiles, CAP-5, a Snipping Tool hint, auto-update (Velopack), CI on Windows.
10. **Publishing on GitHub** (2026-10-21): the repository `github.com/metall1c/markpaste`, documentation and code comments in English, README in English and Russian, the first release 0.2.0 ([ADR 0024](decisions/0024-github-home-and-releases.md), docs/release.md).
