# Actual platform matrix

What was checked on live systems: where, when and with what result. The expectations are in sections 9 and 12 of the spec.

Statuses: ✅ works · ❌ does not work · ⚠️ works with caveats · ❔ not checked.

## Fedora 44 KDE Plasma, Wayland (the main platform)

System: Plasma 6.7.5, KWin 6.7.5, Qt 6.11.2, xdg-desktop-portal 1.22.1 + xdg-desktop-portal-kde 6.7.5.

| Capability | Mechanism | Status | Date | Note |
| --- | --- | --- | --- | --- |
| Background clipboard: write | `ext_data_control_manager_v1` v1 | ✅ | 2026-10-19 | All 9 formats of mode 2 are visible in `wl-paste -l`, the image is served |
| Background clipboard: read (saving before a paste) | `ext_data_control_manager_v1` v1 | ✅ | 2026-10-19 | Text is saved and restored |
| Exclusion from the Klipper history | `x-kde-passwordManagerHint=secret` | ✅ | 2026-10-19 | The entry is not in `getClipboardHistoryMenu` |
| Ctrl+V emulation | RemoteDesktop portal v2, `NotifyKeyboardKeycode` | ✅ | 2026-10-19 | The first time a KDE dialog, the session in ~12 s including the click |
| A repeated session without a dialog | RemoteDesktop `persist_mode=2` + `restore_token` | ✅ | 2026-10-19 | The session opens in 16–18 ms |
| Sequential paste (mode 1), 3 images + text | data-control + RemoteDesktop | ✅ | 2026-10-19 | `pastetest --self-test`: PASS, 1.85 s for 3 images + text (400 ms delay). The receiver is a Qt window; chats not checked yet |
| Pasting into our own window while the clipboard is ours | data-control in a separate thread | ✅ | 2026-10-19 | `--self-test --in-process`: PASS (ADR 0002) |
| Pasting with a Russian layout | evdev KEY_V | ❔ | | |
| 50 runs in a row without losses | | ❔ | | Needs supervision: Ctrl+V goes to the active window |
| Clipboard via the portal | Clipboard portal v1 (xdg-desktop-portal 1.22.1, -kde 6.7.5) | ✅ | 2026-10-20 | `pastetest --portal-clipboard` with the user present: a RemoteDesktop+Clipboard dialog, `clipboard_enabled = 1`; SelectionRead reads the current clipboard, SetSelection/SelectionWrite: data-control reads what was written. After the session closes the written content disappears and the clipboard has the previous content again, which is why the clipboard is restored before the session closes (ADR 0010). Not checked on GNOME 50 |
| Screen capture | KWin ScreenShot2 v5, `CaptureScreen` + `native-resolution` | ✅ | 2026-10-19 | 2 monitors 1920×1080 in 37 ms, no dialog. KWin trust also works when `Exec` in the .desktop file is a symlink to the binary |
| Screen capture | Screenshot portal v2 | ❔ | | Implemented as the fallback, not checked |
| From the key to the overlay (NFR-1 ≤ 800 ms on Wayland) | KWin ScreenShot2, 2 monitors 1080p | ✅ | 2026-10-20 | The log of the user's real shots on 19–20.10, 14 of them: median 220 ms, 90 % within 379 ms, maximum 596 ms. ~40 ms without open windows, ~220 ms with the strip or the editor on screen (180 ms waiting for the compositor to hide them). 379 and 596 ms are the fallback Screenshot portal, see problems.md 2026-10-20. Not measured on 4K, there is no such monitor |
| Global hotkey: binding | GlobalShortcuts portal v2 | ✅ | 2026-10-19 | On the first BindShortcuts KDE showed a permission dialog; after confirmation Print and Ctrl+Alt+V (suggested by the program) were assigned |
| Global hotkey: press | `Activated` / `Deactivated` | ✅ | 2026-10-19 | Print starts our capture, Spectacle does not open. Ctrl+Alt+V starts the paste, the modifiers do not interfere with Ctrl+V |
| A new shortcut on update (Ctrl+Print, ADR 0022) | `BindShortcuts` with a new id | ⚠️ | 2026-10-20 | KDE shows "Global shortcuts request" and does not return `BindShortcuts` until answered. The dialog closed without an answer (the program was restarted) → the action is recorded without a key and is not asked again (problems.md). Ctrl+Print is not used by anything in KDE |
| "Capture without editor": activation | `Activated` for `capture-to-strip` | ✅ | 2026-10-20 | Via `kglobalaccel … invokeShortcut capture-to-strip` (the screen was locked): a KWin shot in 26 ms, the overlay. A physical Ctrl+Print press and the selection: ❔, waiting for the user |
| A new description for known ids | `BindShortcuts`, `description` | ✅ | 2026-10-20 | "Capture a screen area" → "Capture with editor", "Capture an area straight to the strip" → "Capture without editor": KDE updated the labels without a dialog, the keys stayed |
| Overlay above everything | layer-shell (LayerShellQt 6.7.5) | ✅ | 2026-10-19 | Checked by the user on 2 monitors at 100 % |
| Strip above windows, does not take focus | layer-shell, `KeyboardInteractivityNone` | ✅ | 2026-10-19 | A click on the strip does not move focus away from the chat window (the user) |
| Strip outline while moving | a transparent input-less layer-shell layer on each monitor | ❔ | 2026-10-19 | The layers are created (KWin sees 2 surfaces 1920×1080). The look and input passthrough wait for a manual check |
| Moving the strip by the header | layer-shell anchor + margins, on release | ✅ | 2026-10-19 | The user: works, but with a 3–5 s delay, fixed (problems.md). After the fix < 250 ms by KWin measurement, also to another monitor |
| The "Paste set" button in the strip, mode 1 | | ❔ | | It does not take focus, so it should work, but the button itself has not been pressed yet |
| Registering the app id | `org.freedesktop.host.portal.Registry` | ⚠️ | 2026-10-19 | Works only if the .desktop file is installed and the binary from `Exec` exists (see problems.md) |
| Passing files into a sandbox | FileTransfer portal | ❌ | 2026-10-19 | No such interface in xdg-desktop-portal 1.22.1 |

## Ubuntu 26.04 GNOME, Wayland

System: Ubuntu 26.04, GNOME (Mutter), the user's test machine; component versions not recorded. Markpaste 0.2.0 from `markpaste_0.2.0-1.ubuntu26.04_amd64.deb`, checked with the setup wizard on 2026-10-21.

| Capability | Mechanism | Status | Date | Note |
| --- | --- | --- | --- | --- |
| ext-data-control-v1 | | ❌ | 2026-10-21 | Not offered by Mutter, as the spec expected |
| Screen capture | Screenshot portal | ⚠️ | 2026-10-21 | 1 screen 1600×900 in 86 ms; the desktop may show its own dialog on each capture |
| Global hotkeys | GlobalShortcuts portal | ✅ | 2026-10-21 | Print, Ctrl+Alt+V, Ctrl+Print bound and activated |
| RemoteDesktop session with clipboard | RemoteDesktop + Clipboard portal | ✅ | 2026-10-21 | `devices = 1 clipboard = true`, restore token received |
| Strip: one header, moving | ordinary window, `startSystemMove()` | ❌ → fixed in 0.2.1 | 2026-10-21 | Two headers, ours did not move the strip: the program took the decorated window for a layer-shell panel (problems.md 2026-10-21). Recheck needed |
| Strip position after a capture | the strip stays mapped and transparent | ❌ → fixed in 0.2.1 | 2026-10-21 | Came back in the middle of the screen (problems.md). Recheck needed |
| Strip above other windows | GNOME window menu "Always on Top" (right click on the header) | ❌ → implemented in 0.2.1 | 2026-10-21 | No "on top" on Mutter for Wayland clients (STR-1). Recheck needed |
| Program icon in the app grid | `hicolor/scalable` SVG | ❌ → fixed in 0.2.1 | 2026-10-21 | The SVG was sniffed as `application/xml` (problems.md). Recheck needed |
| Print for Markpaste | GlobalShortcuts portal + releasing `show-screenshot-ui` | ❌ → implemented in 0.2.1 | 2026-10-21 | GNOME's screenshot tool takes Print first; the wizard frees it. Recheck needed |
| Paste test into the wizard field | Clipboard portal + RemoteDesktop | ❌ → fixed in 0.2.1 | 2026-10-21 | Our own window could not read the selection (problems.md 2026-10-21). Recheck needed; pasting into other apps not checked yet |

## Memory and speed measurements (NFR-1, NFR-2)

2026-10-20, Fedora 44, Qt 6.11.2, the `build/` build (RelWithDebInfo, the default in CMakeLists.txt, optimized).

**Tool:** `build/tools/bench/markpaste-bench`, without a screen (offscreen), with clipboard and Ctrl+V stubs. 3840×2160 shots that look like a screen (panels, text, buttons, a 900×600 "photo" inset with noise), PNG in memory as in the program (~1.7 MB per shot). Each shot has a frame with a comment and an arrow. The strip and the editor with the first shot are open.

| What | Spec target | Result | |
| --- | --- | --- | --- |
| A set of 26 4K shots + strip + editor, idle | ≤ 600 MB | **165–170 MB** RSS | ✅ |
| Peak while pasting 5 shots from such a set | ≤ 600 MB | **460 MB** | ✅ |
| The same 1.5 s after the paste, before the fix | — | **650–750 MB** and not going down | ❌ → fixed |
| After `releaseFreedMemory()` (the program calls it 2 s after a paste) | — | **165 MB** | ✅ |
| Overlay on 2×4K open (frames in memory) | — | +75 MB over the background | — |
| Pasting 5 shots + text up to the last Ctrl+V | ≤ 3 s | **1.77 s** (baking 0.22 s of it, the rest is the profile pauses, 80 + 400 ms per image); with the clipboard restored after 1 s, 3.17 s, but that is after the paste | ✅ |
| Encoding a 4K frame to PNG on capture | — | 330–340 ms, in the thread pool, the window does not wait | — |

**The program in the background in a live session** (`/proc/<pid>`, KDE Wayland, 2 monitors 1080p):
- Before the fix (the process had run for 1.5 h, with shots and pastes): RSS 189 MB, anonymous memory 89 MB.
- After the fix, 2026-10-20, 5 min after startup, 1 shot in the set: RSS 105 MB, of which 57 MB are libraries shared with other programs (Qt, KDE); own memory 48 MB (39 MB private dirty + 9 MB clean), PSS 52 MB. **CPU: 0 ticks in 30 s.** The target "up to 60 MB in the background" is met when counting own memory (PSS); by RSS it is not, but RSS includes libraries that are in memory anyway.
- The user pasting 3 shots 2×1080p (sampled every 0.5 s): before the paste RSS 133 MB (anonymous 43), during the paste 165 MB (anonymous 74), **after 6 s 108 MB (anonymous 18)**: memory comes back after the paste, lower than before it. The peak of the whole measurement is 203 MB with the capture overlay open (frames of two monitors in memory), then it drops.

**Found and fixed:** baking runs in the thread pool, and every thread has its own glibc malloc arena. Freed 4K frames (33 MB each unpacked) stay in the arenas, and RSS does not drop. `malloc_trim(0)` returns them to the system: 748 → 166 MB. The program calls it (`platform/Memory.h`) 2 s after a paste, a drag, a save to folder, closing the preview and clearing the set. Also the orchestrator no longer keeps the baked images until the next paste.

## Sway 1.11 (wlroots 0.19), nested headless on the same machine

A check without a separate system: `WLR_BACKENDS=headless sway` inside the KDE session, clients with `WAYLAND_DISPLAY=wayland-1` (docs/build.md). The portals here are KDE's, so only Wayland protocols are checked.

| Capability | Mechanism | Status | Date | Note |
| --- | --- | --- | --- | --- |
| Protocols | — | ✅ | 2026-10-19 | `zwp_virtual_keyboard_manager_v1` v1, `ext_data_control_manager_v1` v1, `zwlr_data_control_manager_v1` v2, `zwlr_layer_shell_v1` v4 are present |
| Ctrl+V emulation | `zwp_virtual_keyboard_v1`, US keymap | ✅ | 2026-10-19 | No dialog, the "session" in 2 ms |
| Sequential paste (mode 1), 3 images + text | data-control + virtual-keyboard | ✅ | 2026-10-19 | `pastetest --self-test`: PASS 4/4 (in the same process and in a separate one), 1.85 s |
| A real Sway and Hyprland session with their portals | | ❔ | | Capture (the -wlr portal), hotkeys (Sway has no GlobalShortcuts portal): not checked |

## X11 (Xvfb 21.1.24, no window manager, on the same machine)

`Xvfb :97`, clients with `DISPLAY=:97 QT_QPA_PLATFORM=xcb`, presses via xdotool (XTEST). The X11 mechanisms are checked, not a desktop environment.

| Capability | Mechanism | Status | Date | Note |
| --- | --- | --- | --- | --- |
| Global hotkeys | `XGrabKey` on the root window | ✅ | 2026-10-19 | Print and Ctrl+Alt+V; holding gives one activation; without Alt it does not fire |
| Print and Ctrl+Print on one key | `XGrabKey`, `owner_events = 0` | ✅ | 2026-10-20 | `pastetest --x11-hotkeys`: Print, Ctrl+Print, Shift+Print (does not fire). The program with xfwm4: Ctrl+Print → a shot in the strip without the editor; Print → the editor; Ctrl+Print with the editor open → a shot in the strip, the editor stayed on its shot. Before the fix every second press was lost (problems.md) |
| A shortcut taken by another program | BadAccess | ✅ | 2026-10-19 | The error "already used by another program" |
| Ctrl+V emulation | XTEST | ✅ | 2026-10-19 | |
| Sequential paste (mode 1), 3 images + text | QClipboard + XTEST | ✅ | 2026-10-19 | `pastetest --self-test`: PASS 3/3 |
| Wizard: capture and paste | | ✅ | 2026-10-19 | Capture 1280×800 in 5 ms; Ctrl+V reached the field |
| A real X11 session (KDE/Xfce), a Russian layout | | ❔ | | |

## Windows 11 24H2 (VirtualBox 7.2, console)

Since 0.2.0 the user works with the Windows 11 version daily (reported 2026-10-21), so Windows 11 is a release platform; the table below is the developer's own check in a VM.

Build 26100.9711, VBoxSVGA without 3D, one monitor 1920×967, 100 %. The portable ZIP (MinGW, Qt 6.11.2). Controlled with `tools/dev/winvm/vm.sh` (docs/build.md): keys are VirtualBox scancodes (for Windows this is a real keyboard, not `SendInput`), the mouse is `SendInput` from the user's session.

| Capability | Mechanism | Status | Date | Note |
| --- | --- | --- | --- | --- |
| `markpaste_windows_tests` | | ✅ | 2026-10-20 | 11 of 11: OLE clipboard formats, a busy clipboard (write and snapshot), CF_DIB → PNG, `RegisterHotKey`, the hook, capture |
| `markpaste_core_tests`, `markpaste_ui_tests` | offscreen | ✅ | 2026-10-20 | 63 and 36. Needs `qoffscreen.dll`, which is not in the ZIP (build.md) |
| Clipboard busy in another program | retrying `OleSetClipboard` | ✅ | 2026-10-20 | A script holds the clipboard for 10 s → "The clipboard is busy", the clipboard keeps the old text; 4 s → the write succeeded after it was released. Before the fix, "Set copied" with the old clipboard (problems.md) |
| Ctrl+Alt+V with the Markpaste settings open | `QApplication::activeWindow` | ✅ | 2026-10-20 | "Click into the chat window first…", no keys are pressed. From an open editor the paste goes into Paint, as before |
| The strip at the bottom of the screen | `availableGeometry` | ✅ | 2026-10-20 | Does not go over the taskbar, also when it grows because of a message and the "Stop" button; a long message wraps instead of pushing the strip past the edge |
| Screen capture | DXGI Desktop Duplication | ✅ | 2026-10-20 | 29–42 ms per 1920×967 frame; the image is correct |
| From the key to the overlay (NFR-1 ≤ 300 ms) | DXGI | ✅ | 2026-10-20 | 62 ms without program windows on screen, 222–234 ms with the strip/wizard (waiting for them to hide) |
| Print Screen | the `WH_KEYBOARD_LL` hook | ✅ | 2026-10-20 | Fires, the Snipping Tool does not open (the Windows setting was not changed) |
| The overlay gets the keyboard after Print | `forceActivate` | ✅ | 2026-10-20 | Esc closes the overlay right away |
| Ctrl+Alt+V → pasting into Notepad | `RegisterHotKey` + `SendInput` | ✅ | 2026-10-20 | "Shot A / A1 button here" |
| The "Paste set" button on the strip → Paint | `WS_EX_NOACTIVATE` | ✅ | 2026-10-20 | The target is `mspaint.exe`, an image with the A1 mark was pasted |
| Stopping on a window change | `GetForegroundWindow` | ✅ | 2026-10-20 | A console window took focus before the paste → "The active window changed: paste stopped", nothing extra was pasted |
| First-start wizard | | ✅ | 2026-10-20 | Capture check ✓ (1 screen, 63 ms), Print ✓ |
| A shot in an RDP session | GDI | ❔ | | It was a black screen on Windows Server 2022 over RDP (problems.md, 2026-10-20); fixed, not checked |
| The Win+V history, pasting into chats, UIPI, HDR, different scales | | ❔ | | |

## Windows (under Wine 11.0 on Fedora 44)

The MinGW cross-build, Qt 6.11.2. **This is not Windows**: Wine has no DXGI, UIPI, Snipping Tool, HDR or different monitor scales; real Windows 11 is in the section above ([ADR 0016](decisions/0016-windows-platform.md), [ADR 0017](decisions/0017-windows-cross-build.md)).

| Capability | Mechanism | Status | Date | Note |
| --- | --- | --- | --- | --- |
| Core and window tests | `markpaste_core_tests`, `markpaste_ui_tests` (offscreen) | ✅ | 2026-10-20 | 63 and 33 tests, as on Linux (except the Linux-only ones) |
| Clipboard: formats | QClipboard (OLE) + our own | ✅ | 2026-10-20 | `markpaste_windows_tests`: CF_DIB/CF_DIBV5, `PNG`, CF_HDROP, CF_UNICODETEXT, `ExcludeClipboardContentFromMonitorProcessing`, `CanIncludeInClipboardHistory` = 0, `CanUploadToCloudClipboard` = 0; no marks on restore. The Win+V history was not checked on real Windows |
| Clipboard: another program's image before a paste | `PNG` / `CF_DIB` → PNG | ✅ | 2026-10-20 | Qt under Wine does not parse `CF_DIB`, so we read it ourselves (problems.md) |
| Global hotkeys | `RegisterHotKey` | ✅ | 2026-10-20 | Ctrl+Alt+V via xdotool in Xvfb: fires, waits for the release, pastes. A taken shortcut gives "already used by another program" |
| Print Screen | the `WH_KEYBOARD_LL` hook in its own thread | ✅ | 2026-10-20 | Fires and opens the overlay. Taking it from the Windows 11 Snipping Tool was not checked |
| Ctrl+Print: capture without editor (ADR 0022) | the same hook, exact modifiers | ✅ | 2026-10-20 | Xvfb + xfwm4, focus in Wine's Notepad: Ctrl+Print → A in the strip without the editor, Print → editor B, Ctrl+Print with the editor open → C in the strip, the editor stayed on B. Not checked on Windows 11 |
| Ctrl+V | `SendInput` | ✅ | 2026-10-20 | Wizard: "Ctrl+V reached the window"; the set was pasted into Wine's Notepad (text; Notepad does not accept images). Wine has no UIPI |
| The "Paste set" button on the strip (mode 1) | the strip with `WS_EX_NOACTIVATE` | ✅ | 2026-10-20 | A click on Notepad, then on the strip button: the paste target is `notepad.exe`, the text is in place |
| Moving the strip to an edge (STR-5) | `startSystemMove` + `WM_EXITSIZEMOVE` | ✅ | 2026-10-20 | With xfwm4: dragged right → left, the strip snapped to the left edge at the release height; the position is in the registry |
| Screen capture | DXGI Desktop Duplication | ❔ | | Not implemented in Wine (`0x887a0004`); in Xvfb Wine's dxgi.dll crashes (problems.md) |
| Screen capture | GDI BitBlt (fallback) | ✅ | 2026-10-20 | 2 monitors 1920×1080 on the desktop: 20–24 ms per monitor, the overlay 102 ms after the command. Xvfb 1600×900: 9 ms, the overlay after 21 ms |
| First-start wizard | | ✅ | 2026-10-20 | All four checks are green. The intro text mentions Wayland; to be fixed for Windows |
| Autostart | the `Run` key | ✅ | 2026-10-20 | The test writes and deletes the value; logging in was not checked |
| Portable ZIP | `cpack` | ✅ | 2026-10-20 | The directory from the ZIP starts without Qt paths: capture and the overlay work |
| Setup.exe (ADR 0023) | NSIS 3.11, per-user | ✅ | 2026-10-20 | A clean Wine prefix: `/S` and the wizard (Russian with `LANG=ru_RU`) install into `%LOCALAPPDATA%\Programs\Markpaste`, a Start menu shortcut, an entry in `HKCU\…\Uninstall`, the program starts. Installing over a running program closes it (`command: quit` in the log). Uninstalling closes the program and removes the files, the shortcut, the entry and autostart. Not checked on Windows 11 |

## Release packages (0.2.0, 2026-10-21)

Built with `tools/package/build-in-container.sh` (ADR 0025), checked in clean containers: the package installs with the system package manager and `markpaste --version` prints the version (offscreen). This checks dependencies and files, not the desktop session.

| Package | Built on | Installs on | Does not install on | Checks |
| --- | --- | --- | --- | --- |
| `markpaste-0.2.0-1.fc44.x86_64.rpm` | fedora:44 (Qt 6.11.2, LayerShellQt 6.7) | Fedora 44 | — | rpmlint 0 errors, 0 warnings; the main platform for daily use |
| `markpaste_0.2.0-1.ubuntu24.04_amd64.deb` | ubuntu:24.04 (Qt 6.4.2, no Qt 6 LayerShellQt) | Ubuntu 24.04 | Ubuntu 25.10, 26.04, Debian 12, 13 | lintian clean |
| `markpaste_0.2.0-1.ubuntu26.04_amd64.deb` | ubuntu:26.04 (LayerShellQt for Qt 6) | Ubuntu 26.04 | — | lintian clean |
| `markpaste_0.2.0-1.debian13_amd64.deb` | debian:13 (Qt 6.8, LayerShellQt 6.3) | Debian 13 | — | lintian clean |
| `Markpaste-0.2.0-win64-setup.exe` | fedora:44, MinGW 16.2.1, Qt 6.11.2 | a clean Wine 11 prefix (`/S`) | — | installs into `%LOCALAPPDATA%\Programs\Markpaste`, the "Apps" entry has version 0.2.0 and the GitHub URL; the same toolchain as the build tested under Wine (core 71, UI 44, windows 11 tests) |

## Chat compatibility

| Chat | Environment | Mode | Started by | Result | Date | Note |
| --- | --- | --- | --- | --- | --- | --- |
| Claude Code (CLI) in Konsole | Fedora 44 KDE, Wayland | 1 | Ctrl+Alt+V | ✅ 2 shots + text, the order is right, the text matched | 2026-10-19 | The images came as `[Image #1]`, `[Image #2]`, then the text. A 400 ms delay is enough |
| ChatGPT in a browser | Fedora 44 KDE, Wayland | 1 | Ctrl+Alt+V | ✅ works correctly (the user) | 2026-10-19 | The browser was not specified. A 400 ms delay is enough |
| Claude in a browser | Fedora 44 KDE, Wayland | 1 | Ctrl+Alt+V | ✅ works correctly (the user) | 2026-10-19 | The browser was not specified |
| Telegram Desktop | Fedora 44 KDE, Wayland | 1 | Ctrl+Alt+V | ✅ works correctly (the user) | 2026-10-19 | The package variant (RPM or Flathub) was not specified |
| Cursor | | | | ❔ | | |
