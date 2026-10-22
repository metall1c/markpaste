# Problem log

Each entry: date, environment, symptom, cause, solution, links. New entries go on top. Unsolved ones are recorded too, with the status "open".

---

## 2026-10-22 · Static build: Qt's own file dialog instead of the system one

- **Environment:** the static build 0.2.2–0.2.3 (Qt 6.12.0, the `xdgdesktopportal` theme plugin linked), Fedora 44 KDE Plasma, Wayland; GNOME the same way. Found in a code review.
- **Symptom:** "Choose folder" in the preview window opens Qt's plain widget dialog, not the KDE or GTK one (0.2.1 with the system Qt opened the KDE dialog). `QT_LOGGING_RULES='qt.qpa.theme*=true' markpaste --version` says `Successfully created platform theme "kde"`.
- **Cause:** Qt adds `xdgdesktopportal` to its theme list only inside flatpak or snap, or when `QT_QPA_PLATFORMTHEME` names it. Otherwise it takes the platform's names (`kde`, `generic`): the built-in themes, which have no native dialogs, and the static Qt has neither plasma-integration nor gtk3. Linking the plugin was not enough.
- **Solution:** 0.2.4: `mp::prepareProcess()` sets `QT_QPA_PLATFORMTHEME=xdgdesktopportal` before `QApplication` when the variable is empty, and `finishProcessSetup()` removes it afterwards (programs we start do not inherit it). The portal theme keeps `kde`/`gnome` as its base for everything else. Checked on Fedora 44 KDE: the theme log says `"xdgdesktopportal"`, `dbus-monitor` shows `org.freedesktop.portal.FileChooser.OpenFile` for `getExistingDirectory`; start-up time is the same (0.02 s for `--version`). Pitfall when testing: the theme reads the FileChooser version asynchronously, so a dialog opened before the event loop runs is still Qt's own. ADR 0028.
- **Status:** solved.

## 2026-10-22 · Static Qt: the Wayland compositor is built although `FEATURE_wayland_server=OFF`

- **Environment:** `tools/static/build-qt.sh`, Qt 6.12.0, the qtwayland step.
- **Symptom:** the image has `libQt6WaylandCompositor.a`, `libQt6WlShellIntegration.a` and the wl-shell, fullscreen-shell, ivi-shell and qt-shell plugins; `qtgui-config_p.h` has `QT_FEATURE_wayland_server 1`.
- **Cause:** since Qt 6.10 the Wayland features are defined in qtbase (`src/gui/configure.cmake`); a module cannot change a feature of the module it depends on, so the switch in the qtwayland step did nothing.
- **Solution:** 0.2.4: `wayland_server`, `wayland_client_wl_shell` and `wayland_client_fullscreen_shell_v1` are off in the qtbase step, `wayland_client_ivi_shell` and `wayland_client_qt_shell` in the qtwayland step. The image keeps only xdg-shell and the decorations; it is 1.11 GB instead of 1.24 GB (also because `tools/static/sources/` is no longer copied into it).
- **Status:** solved.

## 2026-10-22 · Static archive: `install.sh` leaves an old copy running or reports a restart that did not happen

- **Environment:** `install.sh` of 0.2.2–0.2.3. Found in a code review; the fixes checked in a test `HOME` with a space in its name, the installed copy running offscreen.
- **Symptoms and causes:**
  - `sudo ./install.sh --system`: `pgrep -u "$(id -u)"` looked for root's processes, so the user's running copy stayed on the replaced (deleted) file, and KWin refused it ScreenShot2.
  - Over SSH or on a text console `markpaste --quit` could not open a display and failed silently; the script still said "Restarted the running copy".
  - `--quit` returns as soon as the command is written, so the new copy could start while the old one was still listening, hand it `show` and exit: nothing left running.
  - `Exec=` was written without quotes: a space in `$HOME` broke the menu entry and the KWin check.
  - `gtk-update-icon-cache` without `-t` always failed (no `index.theme` in these hicolor directories); KDE's cache was not refreshed.
- **Solution:** 0.2.4, see ADR 0028 ("The archive"): copies are found by `/proc/<pid>/exe`, `--quit` runs offscreen as the copy's owner, the script waits up to 5 s for each one to exit, restarts only the current user's copy and only with a display, quotes `Exec`, runs `gtk-update-icon-cache -t` and `kbuildsycoca6`. Checked: fresh install, update with a running copy without a display ("closed, start it again"), update with a display (restarted, the new process runs the new file), `--uninstall` with a running copy; `desktop-file-validate` accepts the quoted `Exec`. Not checked: `sudo … --system` with another user's copy running.
- **Status:** solved.

## 2026-10-22 · Static Qt: crash in the tray code without libdbus

- **Environment:** the static build (Qt 6.12.0, `FEATURE_dbus_linked=OFF`), a bare `fedora:44` container with Xvfb, no `dbus-libs`.
- **Symptom:** `markpaste --about` exits with SIGSEGV. gdb: `QDBusAbstractInterface::callWithArgumentList` ← `QDBusConnectionInterface::isServiceRegistered` ← `QDBusMenuConnection` ← `QGenericUnixTheme::shouldUseDBusTray` ← `QSystemTrayIcon::isSystemTrayAvailable`.
- **Cause:** with `dbus_linked=OFF` Qt loads `libdbus-1.so.3` itself; when it is missing, the D-Bus tray check calls through an invalid connection.
- **Solution:** `tools/static/build-qt.sh` links libdbus (`FEATURE_dbus_linked=ON`): without it the loader now says which library is missing. Every desktop has libdbus (portals need it). ADR 0028.
- **Status:** solved.

## 2026-10-22 · Static Qt: LayerShellQt does not compile against Qt 6.12 on Debian 12

- **Environment:** `debian:12` (libwayland 1.21), Qt 6.12.0 built from source, LayerShellQt 6.8.1.
- **Symptom:** `qwayland-wayland.h:1317: error: 'wl_fixes' in namespace '::' does not name a type`.
- **Cause:** since Qt 6.10 the Wayland client is in qtbase and generates the core protocol from its own newer `wayland.xml` (with `wl_fixes`). LayerShellQt includes the system `<wayland-client.h>` first; the system `wayland-client-protocol.h` has the same include guard as Qt's generated one, so Qt's is skipped.
- **Solution:** LayerShellQt is built with `-include <Qt>/QtWaylandClient/private/wayland-wayland-client-protocol.h`, so Qt's header goes first (`tools/static/build-qt.sh`). Debian 12 also lacks `ext-data-control-v1` (wayland-protocols 1.31): the image installs wayland-protocols 1.47; ECM 6.31 needs CMake 3.30: the image takes CMake 3.31.8 from Kitware.
- **Status:** solved.

## 2026-10-21 · GitHub renames .deb assets: "~" becomes "."

- **Environment:** `gh release create` with `markpaste_0.2.0-1~ubuntu24.04_amd64.deb` and the other .deb files.
- **Symptom:** the release shows `markpaste_0.2.0-1.ubuntu24.04_amd64.deb`; the names in `SHA256SUMS` and the README no longer match the downloads, `sha256sum -c` would fail.
- **Cause:** GitHub Releases replaces characters it does not allow in asset names, `~` among them.
- **Solution:** `build-in-container.sh` names the .deb files with a dot from the start (the package version inside stays `0.2.0-1~ubuntu24.04`); `SHA256SUMS`, the README and the docs use the dotted names.
- **Status:** solved.

## 2026-10-21 · Debian 13: the build fails with LayerShellQt 6.3

- **Environment:** `debian:13` container, Qt 6.8, LayerShellQt 6.3.4 (`liblayershellqtinterface-dev`). Found by the first release build for Debian (ADR 0025).
- **Symptom:** `WindowRoles.cpp:60: error: 'class LayerShellQt::Window' has no member named 'setScreen'; did you mean 'setScope'?`
- **Cause:** `Window::setScreen` and `Window::screen` appeared in LayerShellQt 6.4. Fedora 44 has 6.7, so it was never noticed. Older versions take the surface output from `QWindow::screen()` (`ScreenFromQWindow`, their default).
- **Solution:** CMake defines `MARKPASTE_LAYERSHELL_SET_SCREEN` for LayerShellQt 6.4+; otherwise `setLayerScreen`/`layerScreen` in `platform/common/WindowRoles.cpp` use `QWindow::setScreen`/`QWindow::screen`. The Debian 13 package builds and passes the tests. The overlay and the strip on a real Debian 13 KDE session were not checked.
- **Status:** solved in the build, not checked on a live Debian 13.

## 2026-10-21 · A .deb does not install on other Ubuntu and Debian releases

- **Environment:** the .deb built on Ubuntu 24.04; clean `ubuntu:25.10`, `ubuntu:26.04`, `debian:12`, `debian:13` containers.
- **Symptom:** `apt install ./markpaste_0.2.0_amd64.deb` fails: "Depends: libqt6concurrent6t64 (>= 6.1.2) but none of the choices are installable" (Ubuntu 26.04, Debian 13), "Depends: libqt6widgets6t64 … is not installable" (Debian 12).
- **Cause:** `dpkg-shlibdeps` writes the library package names of the build system, and they differ between releases: Ubuntu 24.04 renamed the Qt libraries for the 64-bit time_t transition (`…t64`), Debian 12 has the old names, and later releases changed them again.
- **Solution:** one .deb per release (Ubuntu 24.04, Ubuntu 26.04, Debian 13) with the release in the Debian revision, `markpaste_0.2.0-1.ubuntu24.04_amd64.deb`; each installs in a clean container of its release. lintian then wanted `changelog.Debian.gz` instead of `changelog.gz` (a package with a revision is not "native"), which is now generated.
- **Status:** solved. No package for Debian 12.

## 2026-10-21 · lintian errors in the first .deb

- **Environment:** lintian in `ubuntu:24.04`, the CPack .deb.
- **Symptom:** `E: description-synopsis-is-duplicated`, `E: no-copyright-file`, `E: no-changelog`, `W: no-manual-page`.
- **Cause:** `CPACK_DEBIAN_PACKAGE_DESCRIPTION` repeated the summary that CPack already uses as the synopsis; the package had no Debian `copyright`, changelog or man page.
- **Solution:** the explicit description is removed; `data/markpaste.1`, `data/copyright.in` (DEP-5) and a generated changelog are installed, compressed with `file(ARCHIVE_CREATE … FORMAT raw COMPRESSION GZip)`. lintian is clean, rpmlint gives 0 errors and 0 warnings (the man page also removed its only warning).
- **Status:** solved.

## 2026-10-21 · English UI on a Russian system: sizes in the preview are in Russian ("79,7 кБ")

- **Environment:** Fedora 44, `LANG=ru_RU.UTF-8`, the interface language set to English in settings. Found while making the English README screenshots.
- **Symptom:** the message preview says "This goes to the chat: 2 image(s) (79,7 кБ)": English text with Russian units and a decimal comma.
- **Cause:** `PreviewDialog` formatted sizes with `QLocale()`, the system locale, while the UI language comes from settings and may differ from it.
- **Solution:** the UI locale is set once at startup (`mp::setUiLocale` in `main.cpp`, the same locale the translations are loaded for) and `PreviewDialog` formats sizes with `uiLocale()`. `QLocale::setDefault` was not used on purpose: the "Like the system" message language must keep following the system.
- **Status:** solved.

## 2026-10-20 · KDE: the new "Capture to strip" shortcut (now "Capture without editor") was left without a key

- **Environment:** Fedora 44 KDE Plasma 6.7.5, Wayland, xdg-desktop-portal 1.22.1 + -kde 6.7.5, GlobalShortcuts v2. The program 0.1.1 → 0.1.2, a third id `capture-to-strip` was added to `BindShortcuts` (ADR 0022).
- **Symptom:** after the update the log has no `shortcut … bound` lines: the `BindShortcuts` answer does not come. On the screen there is the KDE window "Global shortcuts request" (visible via `qdbus --literal org.kde.KWin /WindowsRunner org.kde.krunner1.Match комбинац`). The program was restarted without answering the dialog: it closed, and the new instance immediately got `capture-to-strip bound to "<none>"`.
- **Cause:** for `id`s KDE does not know yet, it asks the user and holds `BindShortcuts` until the answer. When the portal session closes together with the program, KDE records the action without a key: `capture-to-strip=none,Ctrl+Print,<description>` (current, default, description). After that the action counts as known, and there is no dialog anymore.
- **Solution:** on an update, answer the KDE dialog without restarting the program. If the key is already lost, use "Change shortcuts…" in the Markpaste settings or KDE "Shortcuts". For a developer without a mouse:
  `gdbus call --session --dest org.kde.kglobalaccel --object-path /kglobalaccel --method org.kde.KGlobalAccel.setForeignShortcut "['su._525.Markpaste','capture-to-strip','Markpaste','Capture without editor']" "[83886089]"` (83886089 = `Qt::CTRL | Qt::Key_Print`). The program received `ShortcutsChanged` and picked up Ctrl+Print without a restart.
- **Status:** worked around. A warning is in ADR 0022.

## 2026-10-20 · X11: every second press of Print (and Ctrl+Print) did not fire

- **Environment:** Xvfb 21.1.24 + xfwm4, Qt 6.11.2 (xcb), presses via xdotool. Found while checking Ctrl+Print (ADR 0022).
- **Symptom:** Ctrl+Print → a shot, then Ctrl+Print again: nothing. There is no `hotkey activated` in the log. It fires again only after another shortcut on the same key is pressed.
- **Cause:** `XGrabKey` was called with `owner_events = 1`. While the shortcut is held, X gives its events to our program's window if it has focus, rather than to the root window. The overlay gets focus 7 ms after the press, so the release went to it (`event 4194314` instead of the root `927`). The filter looks only at the root window, and the shortcut stayed "pressed". The next press counted as autorepeat and was swallowed. Separately: with two shortcuts on one key (Print and Ctrl+Print) the filter returned on the first one whose modifiers did not match and never reached the second. Which one came first was decided by the `QHash` order.
- **Solution:** `owner_events = 0`: the press and release of a shortcut always come to the root window. In case of an interrupted grab, a release that arrives in our window also clears "pressed" (the event stays with the window). A shortcut whose modifiers do not match is skipped rather than ending the search. `pastetest --x11-hotkeys` now checks Print, Ctrl+Print and Shift+Print. The program in Xvfb with xfwm4: Ctrl+Print, Print, Ctrl+Print with the editor open, Ctrl+Print again, Print: all five fire.
- **Status:** solved. The bug existed before Ctrl+Print too. Nobody noticed it: X11 was checked only with `pastetest` in Xvfb, without the overlay.

## 2026-10-20 · Windows: "Set copied", but the clipboard has the previous content

- **Environment:** Windows 11 24H2 (VirtualBox 7.2, shared clipboard with the host on), the portable ZIP. Found by code review, reproduced.
- **Symptom:** while another program keeps the clipboard open, "Copy set" says "Set copied, press Ctrl+V in the chat", but the clipboard keeps the old text. The log has `OleSetClipboard: Failed to set mime data … COM error 0x800401d0` (`CLIPBRD_E_CANT_OPEN`). A sequential paste would silently paste the previous image or someone else's clipboard content the same way. Reproduction: a PowerShell script in the user's session calls `OpenClipboard` and waits 6 s; press "Copy set" meanwhile.
- **Cause:** with a busy clipboard `QClipboard::setMimeData` tries three times with 50 ms pauses and gives up, reporting only a warning to the log. `QtClipboard::write` always returned success. The clipboard is opened for a moment after every change: `rdpclip` in an RDP session, VBoxTray in a VM, clipboard managers. The clipboard snapshot before a paste could read empty at such a moment, and then the user's clipboard did not come back after the paste (from the code, not reproduced).
- **Solution:** after writing, `WinClipboard::write` checks `QClipboard::ownsClipboard()` and, if the clipboard is not ours, retries up to 20 times every 100 ms; if that fails, the error "The clipboard is busy". A new write cancels the retries of the previous one (otherwise a late clipboard restore would land over the next paste). `WinClipboard::snapshot` waits until the clipboard is free; busyness is checked by opening the clipboard from our own hidden window (in Wine re-opening with `NULL` from another thread succeeds). The tests `WinClipboard.BusyClipboardIsReportedNotIgnored`, `WriteWaitsUntilClipboardIsFree`, `SnapshotWaitsForBusyClipboard` fail without the fix.
- **Also:** X11 format names (`UTF8_STRING`, `STRING`, `TEXT`, `text/plain;charset=utf-8`, `x-special/gnome-copied-files`) no longer get into the Windows clipboard: they were registered as extra formats. The test `WinClipboard.NoX11FormatNamesOnWindows`.
- **Status:** solved.

## 2026-10-20 · Ctrl+Alt+V with a Markpaste window open pasted the set into that window

- **Environment:** Windows 11 24H2 (VirtualBox); common to all platforms (`AppController::paste`).
- **Symptom:** open the Markpaste settings and press Ctrl+Alt+V: the log says `paste target: "markpaste.exe"`, Ctrl+V goes to the settings window, and the strip says "Pasted".
- **Cause:** the paste went to the active window, whatever it was. The editor hides before a paste, the strip does not take focus, but the settings, the preview and the wizard stay active.
- **Solution:** if a program window other than the editor and the strip is active, the paste does not start: "Click into the chat window first: a Markpaste window is active now". "Copy set" and the "files + text" mode press no keys, so they have no such check. The wizard's paste check goes its own way and still pastes into its own field.
- **Status:** solved.

## 2026-10-20 · Windows: the strip at the bottom of the screen covered the taskbar

- **Environment:** Windows 11 24H2 (VirtualBox), the taskbar at the bottom.
- **Symptom:** drag the strip down: it snaps flush to the bottom of the screen over the taskbar and covers the clock, the layout indicator and Markpaste's own tray icon.
- **Cause:** the position of the strip the program places itself was computed from `QScreen::geometry()`, the whole screen including the taskbar.
- **Solution:** on Windows (`m_snapToEdge`) the edge and height are computed from `availableGeometry()`, the work area without the taskbar (`StripWindow::placementArea`). On KDE and Sway the layer-shell surface position is still set from the whole screen: the compositor moves it away from panels.
- **Status:** solved.

## 2026-10-20 · Windows version review: small fixes

- **Waiting for Shift/Alt/Win to be released before Ctrl+V lasts up to 1.5 s, and the window change was checked before it.** If the user switched to another window meanwhile, Ctrl+V would go there. Now the window is checked once more right before the press (`WinInputInjector::sendPaste`).
- **If `SendInput` failed on "Ctrl down", V was pressed anyway**, which would type "v" into the chat. Now after a failure only releases are sent.
- **`FallbackCapture` was removed on Windows.** `DxgiCapture` already falls back to GDI per monitor on every shot. The wrapper, after one completely failed shot (neither DXGI nor GDI), switched to `QScreen::grabWindow` for good, which is the same GDI but without DXGI and correct HDR until a restart. Found by reading the code: a complete failure could not be reproduced, with a locked screen DXGI captures the lock screen.
- **`tools/dev/winvm/vm.sh deploy` did not update the build if the program was running:** the directory was not deleted, and `Move-Item` silently put the new build inside the old one, so the previous version was being checked. Now `deploy` first stops `markpaste.exe` and fails on errors.
- **A long message widened the strip** (the message line had no width limit): on Windows the strip went past the right screen edge, and the text and the "hide" button were cut off. Visible with the message "Click into the chat window first…". Now the message is not wider than the list and wraps (the test `StripUi.LongMessageWrapsInsteadOfWideningStrip`, common to all platforms: without the fix the strip in the Linux build widened from 200 to 295 px; on KDE the compositor presses it to the edge, so it did not go off the screen but only got wider).
- **On Windows the strip did not snap to the edge again when it grew** because of a message or the "Stop" button, and its bottom went onto the taskbar. Now `StripWindow::resizeEvent` places it again (the test `StripUi.GrownStripStaysInsideWorkArea`, Windows build only).
- **The core and window tests "hung" on real Windows** without a single line of output. The cause is not the program: the tests enable the `offscreen` platform, and `qoffscreen.dll` is not in the portable ZIP; Qt fails at startup, and the process waits on an invisible error window. For checks, `vm.sh tests` (it puts the module in place itself, docs/build.md). With it on Windows 11: core 63, windows 36, platform 11, all pass.
- **Status:** solved.

## 2026-10-20 · Mattermost: pasting with Alt+V does not arrive, with Ctrl+Alt+V and the button it works

- **Environment:** Fedora 44 KDE Plasma 6.7.5, Wayland; the paste shortcut assigned in KDE is Alt+V; Mattermost Desktop (Electron). Found by the user.
- **Symptom:** Alt+V fires: the strip says "pasted", the set is cleared, the KDE notification "Remote control session started" appears, but nothing appears in Mattermost. Ctrl+Alt+V and the strip button work.
- **Cause (by the signs, not reproduced on Mattermost):** KDE takes the V of Alt+V, and the chat window sees only Alt↓ … Alt↑ with no keys in between, a lone Alt press. Electron (like Firefox and Windows programs) moves focus to the window menu on it, and our Ctrl+V goes into the menu. In Ctrl+Alt+V there is Ctrl between Alt↓ and Alt↑, so it does not count as a lone press; the strip button does not touch Alt.
- **Solution:** if the paste shortcut has Alt without Ctrl, the input session opens right on the press (without waiting for baking), and while Alt is still held the program presses and releases a "mask": Ctrl on Linux (RemoteDesktop portal, virtual-keyboard, XTEST), the unassigned code 0xE8 on Windows (as AutoHotkey does). The window no longer sees a lone Alt. If Alt was released before the session opened (usually 16–18 ms on KDE), there is no mask; the log says "Alt released before the session opened". `IInputInjector::sendModifierMask`, `PasteOptions::maskAltTap`; the tests `PasteOrchestrator.AltHotkeyIsMaskedWhileHeld`, `NoMaskWithoutAltOrAfterRelease`.
- **The first attempt did not work:** KDE reports all shortcuts of an action in one string, "Ctrl+Alt+V, Alt+V", and the "has Ctrl" check disabled the mask. Now each shortcut from the list is checked separately; the log says "Alt still held: modifier mask sent".
- **Status:** solved; the user checked it in Mattermost on 2026-10-20: Alt+V pastes the set.

## 2026-10-20 · "Delete shot" in the editor opened another shot

- **Environment:** KDE Wayland and Windows 11, shared behavior (`ShotEditor`). Found by the user.
- **Symptom:** take a shot, press "Delete shot" in the editor: the shot is deleted, and the editor opens the previous shot of the strip. Esc closes the window, as expected.
- **Cause:** it was designed this way: when the shown shot disappeared from the set, the editor moved to the last remaining one. The user had not chosen that shot.
- **Solution:** a shot gone from the set (the button, Ctrl+Z, deletion from the strip) closes the editor, as Esc does. The tests `ShotEditorUi.RemovedShotClosesEditor`, `ShotEditorUi.DeleteShotButtonRemovesAndCloses`; on Windows 11 (VirtualBox): shot B deleted, the window closed, A in place.
- **Status:** solved.

## 2026-10-20 · Windows: a fading editor gets into the shot

- **Environment:** Windows 11 24H2 (VirtualBox), the portable ZIP.
- **Symptom:** with the editor open, press Print: the new shot has a semi-transparent editor (the toolbar and the frame).
- **Cause:** before a capture the program hides its windows and waits 180 ms, while Windows hides a window with a DWM animation of about 200 ms; DXGI captured the middle of the animation.
- **Solution:** DWM animations are turned off for the program's windows (`DWMWA_TRANSITIONS_FORCEDISABLED` on every show of a top-level window, `disableWindowAnimations()` in `platform/WindowRoles.h`). Checked: shot B with the editor open is clean.
- **Status:** solved.

## 2026-10-20 · Windows: the first start of a new build takes about a minute

- **Environment:** Windows 11 24H2, Windows Defender with real-time protection, the portable 35 MB ZIP (unsigned).
- **Symptom:** after unpacking a new build, the window and the tray icon appear after 53–65 s; a repeated start of the same file takes less than a second.
- **Cause:** Defender checks new unsigned `.exe` and DLL files (in the Fedora build ICU alone is 32 MB) before they start.
- **What will help:** signing (SignPath, spec section 11) and a build with the official Qt without ICU and OpenSSL (ADR 0017): fewer files, and trusted ones. For now the user has to wait for the first start.
- **Status:** open, until the release build.

## 2026-10-20 · Windows over remote desktop: the shot is a black screen

- **Environment:** Windows Server 2022 Standard 21H2 (build 20348.4773), an RDP session; the portable ZIP of 2026-10-20. Found by the user.
- **Symptom:** Print → the overlay is black, and the shot in the set is black too.
- **Cause:** in an RDP session the desktop is not drawn by the GPU: DXGI Desktop Duplication is created but returns empty frames without rendering (`LastPresentTime = 0`). The code waited for a second frame, did not get it and took the first one, a black one. GDI works in RDP.
- **Solution:** in a remote session (`GetSystemMetrics(SM_REMOTESESSION)`) DXGI is not used, GDI right away; the wizard shows "GDI BitBlt (remote desktop)". Also, a black frame without rendering no longer counts as a shot in any session: the monitor is captured through GDI.
- **Check:** on the Windows 11 console (VirtualBox) DXGI still works (29–42 ms per 1920×967 frame). The fix has not been checked in RDP yet; waiting for the user (RDP is off in the test VM).
- **Status:** fixed, waiting for a check in RDP.

## 2026-10-20 · The Windows build under Wine: what got in the way and how it was worked around

- **Environment:** Fedora 44, MinGW-w64 16.2.1 cross-build + `mingw64-qt6` 6.11.2, Wine 11.0 Staging (DXVK 2.7.1), Xvfb 21.1.
- **CMake does not find the compiler in the googletest subproject**: "The CMAKE_C_COMPILER: x86_64-w64-mingw32-gcc is not a full path and was not found in the PATH", and on reconfiguring from ninja the same for the whole project. Cause: `Qt6Config.cmake` includes `qt.toolchain.cmake`, which includes the Fedora toolchain `/usr/share/mingw/toolchain-mingw64.cmake`, where the compilers are set without a path; the `mingw64-cmake` wrapper finds them through its environment, a plain `cmake` does not. Solution: our own `cmake/mingw64-fedora.cmake` with full paths and `QT_CHAINLOAD_TOOLCHAIN_FILE` pointing to it.
- **`gtest_discover_tests` fails after the build**: it tries to run the `.exe`. Solution: `DISCOVERY_MODE PRE_TEST` when cross-compiling.
- **Qt6Core.dll is not found when building the portable directory** (`file(GET_RUNTIME_DEPENDENCIES)`): for PE CMake lowercases DLL names, and case matters on a Linux file system. Solution: `cmake/CollectDlls.cmake` walks the import table itself.
- **An image put into the clipboard by another program only as `CF_DIB` is an empty `QImage` in Qt under Wine** (`hasImage()` = true, `imageData()` empty). The user's clipboard with an image would not be saved before a paste. Solution: on an empty result `WinClipboard::snapshot` reads `PNG` or `CF_DIB` itself (the test `WinClipboard.SnapshotTurnsDibIntoPng`). Qt's behavior on real Windows was not checked; the fallback is harmless.
- **The program crashes under Wine in Xvfb on a capture**: "Unhandled division by zero" inside `dxgi.dll` (Wine/DXVK): Xvfb has no monitor data for DXGI. On the Wine desktop (XWayland) this does not happen: `DuplicateOutput` honestly answers `0x887a0004` (DXGI_ERROR_UNSUPPORTED), and the capture goes through GDI. A workaround for checks: `MARKPASTE_CAPTURE=gdi`. Such a crash cannot be caught in code: it is an exception inside someone else's DLL.
- **The strip without a background on Windows**: after `WA_TranslucentBackground` (for the rounded corners) Qt stopped painting the window background and the border from the style sheet, only the buttons were visible. Solution: `StripWindow::paintEvent` paints `PE_Widget` from the style sheet if the window is transparent (on Linux the window is opaque and nothing changes there).
- **Hidden windows stay on the screen** under Wine in Xvfb without a window manager (the editor after "Done"). This is Wine without a WM, not the program: with xfwm4 the window hides. Checks are done with a window manager.
- **Status:** solved (the workarounds are described in docs/build.md).

## 2026-10-20 · Small fixes after a code review

- **The last comment did not get into a paste started with the hotkey.** Type a comment in the editor and press Ctrl+Alt+V right away: `paste()` hid the editor, while the comment is saved on focus loss, which on Wayland comes later than the set goes into the paste. Now `paste()` and `copySet()` save the comment first (`ShotEditor::commitPendingEdits`). Not reproduced in a live session, found by reading the code.
- **A click on any tray notification opened the release page** if there had been a new-version notification before: the `messageClicked` subscription waited for any next click. Now the address lives only until the next notification (`AppController::m_trayClickUrl`).
- **Dragging from the strip** with the id of a shot that is no longer in the set accessed `shots[-1]`. Such a shot is skipped.
- **Update check:** if the clock went back relative to the time of the last check, the timer interval overflowed `int`. Now it is limited to a day.
- **Status:** solved.

## 2026-10-20 · A hung paste cannot be cancelled with the "Stop" button

- **Environment:** any; found in a code review, reproduced by the test `PasteOrchestrator.CancelWorksWhileClipboardHangs`.
- **Symptom (expected):** if a platform operation never finishes (for example, a clipboard write through data-control after our connection to the compositor broke: KWin restarted, and Qt survived it on its own), the strip shows "Pasting 1 of N…" forever. "Stop" does nothing, new pastes do not start until the program restarts.
- **Cause:** `cancel()` only set a flag, and the flag was checked at the next step, which was never reached. The compositor's answer to `wl_display.sync` never comes after a disconnect, and the write promise stayed unfulfilled.
- **Solution:** `cancel()` ends the paste right away; it waits only for a Ctrl+V key sequence already in progress. On a disconnect (and on its own deletion) `DataControlDevice` fails pending writes with an error and rejects new ones right away. Along the way the tests `EmptySetFails` and `NoBackgroundClipboardRefusesSequential` were fixed: they passed by a 5-second timeout rather than by the signal.
- **Status:** solved. The sequential paste self-test in a nested Sway: PASS.

## 2026-10-20 · A crash when choosing an item in the thumbnail menu

- **Environment:** any; found in a code review, reproduced by a test (`offscreen`).
- **Symptom:** right-click a thumbnail in the strip → "Delete", "Move up", "Move down" or "Edit" → the program crashes: `double free or corruption (out)`.
- **Cause:** the menu item changes the set, the strip rebuilds (`StripWindow::rebuild`) and immediately deletes (`delete`) the old thumbnails, including the one whose menu is still in `exec()`. The menu is created on the stack as a child of the thumbnail, and Qt deletes it a second time.
- **Solution:** `rebuild` hides the old thumbnails and deletes them later (`deleteLater`). A click on a thumbnail is equally safe: it also rebuilds the strip right from its event handler. The tests `StripUi.ThumbMenuDeleteDoesNotCrash`, `StripUi.ThumbMenuMoveDoesNotCrash`.
- **Status:** solved.

## 2026-10-20 · Esc deletes an already accepted shot

- **Environment:** any; found in a code review, reproduced by a test.
- **Symptom:** take A, press "Done", take B. In B's editor press "Delete shot" or Ctrl+Z: the editor moves to A. Esc silently deletes A (without a question if it has no marks). If the set became empty, the deletion cannot be undone anywhere: Ctrl+Z exists only in the editor.
- **Cause:** the "just taken" flag (`ShotEditor::m_fresh`) stayed from B when the editor switched to another shot by itself after the deletion.
- **Solution:** the flag is reset on such a switch. The test `ShotEditorUi.OlderShotShownAfterFreshIsRemovedIsNotFresh`.
- **Status:** solved.

## 2026-10-20 · Portal clipboard: stale content comes back after a paste

- **Environment:** the GNOME path (the Clipboard portal in a RemoteDesktop session; on KDE with `MARKPASTE_CLIPBOARD=portal`). Found in a code review.
- **Symptom (expected):** paste the set, copy something in a browser, paste the set again: after the second paste the clipboard has not what was copied but the content from the previous paste.
- **Cause:** `PortalClipboard` resets "the clipboard is ours" on the `sessionClosed` signal, and `PortalRemoteDesktop::end()` (the program closing the session itself after every paste) did not send it, only a close by the portal did. In the next session `snapshot()` returned its own old entry without reading the real clipboard.
- **Solution:** `end()` sends `sessionClosed` too.
- **Status:** solved in code, not checked on GNOME.

## 2026-10-20 · A fragment of someone else's clipboard came back after a paste

- **Environment:** KDE (ext-data-control-v1) and the Clipboard portal. Found in a code review.
- **Symptom (expected):** the clipboard has a large image from a browser or an editor; after pasting the set a broken image comes back into the clipboard.
- **Cause:** `readAllAsync` (`platform/linux/FdIo.cpp`) on a timeout and beyond `maxBytes` returned whatever had arrived, and the read timeout was 1 s. The clipboard owner often encodes the image on request, and a 4K PNG might not make it.
- **Solution:** a fragment is not returned: the result is empty, and that type is simply not restored. The read timeout is 3 s for images and 1 s for text. The `FdIo.*` tests (`markpaste_ui_tests`).
- **Status:** solved.

## 2026-10-20 · Capture does not start after the overlay is closed from outside; a crash when switching tools during a drag

- **Environment:** found in a code review, both reproduced by tests.
- **Symptom 1:** on GNOME and X11 the overlay is an ordinary window. If it was closed with Alt+F4, the capture session did not end, and Print did nothing until the program restarted (`AppController::m_capturing` stayed `true`).
- **Solution 1:** `OverlayWidget::closeEvent` cancels the session. The test `CaptureOverlayUi.ClosingOverlayWindowCancelsSession`.
- **Symptom 2:** in the editor, start a crop with the mouse and, without releasing the button, press A (arrow): the program crashes.
- **Cause 2:** the drag continued with the new tool, and the arrow took the first point from the empty list of crop points.
- **Solution 2:** switching the tool drops an unfinished drag. The test `ShotEditorUi.SwitchingToolDuringDragIsSafe`.
- **Status:** solved.

## 2026-10-19 · `update_translations` corrupts `markpaste_ru.ts`

- **Environment:** Fedora 44, Qt 6.11, CMake + Ninja.
- **Symptom:** `cmake --build build --target update_translations` fails: `Unexpected characters … at i18n/markpaste_ru.ts:436`. Line numbers (`<location …>`) appear in the file although `-locations none` is set for the program.
- **Cause:** `markpaste_ru.ts` was attached via `qt_add_translations` twice, to the program and to `markpaste_ui_tests`. Each target has its own lupdate target, Ninja runs them in parallel, and two lupdate processes write the same file at once. The test target also lacks `-locations none`.
- **Solution:** the tests need only the `.qm`, so `tests/CMakeLists.txt` uses `qt_add_lrelease` + `qt_add_resources` instead of `qt_add_translations`. The same was done later in `tools/uipreview/CMakeLists.txt`, a third target with the same `.ts`. Now there is one lupdate.
- **Status:** solved.

## 2026-10-19 · After the rename the UI became English

- **Environment:** Fedora 44 KDE, `LANG=ru_RU.UTF-8`.
- **Symptom:** after the Pointshot → Markpaste rename the windows are shown in English (the user's screenshot).
- **Cause:** a Qt translation is looked up by context, the class name with its namespace (`ps::ShotEditor`). In the rename the `ps` → `mp` namespace changed in the code but not in the `.ts`, and no string was found. The `.qm` itself loaded fine.
- **Solution:** the contexts in `i18n/*.ts` were fixed to `mp::`, `lupdate` found all 121 strings, 0 untranslated. Added the test `Translations.RussianLoadsForEditor` (`markpaste_ui_tests`): it fails if the contexts drift apart again.
- **Rule:** when renaming classes or namespaces, run `cmake --build build --target update_translations` right away and check that the number of untranslated strings did not grow.
- **Status:** solved.

## 2026-10-19 · The strip moves only 3–5 s after the release

- **Environment:** Fedora 44 KDE, Plasma 6.7.5, Wayland, Qt 6.11.2, LayerShellQt 6.7.5.
- **Symptom:** after moving it by the header, the strip stayed in place for 3–5 s and then jumped where it should (reported by the user). Measured with `tools/dev/kwin-probe.py`: within 4.3 s after the move command the geometry in KWin did not change.
- **Cause:** `LayerShellQt::Window::setAnchors/setMargins` send the requests, but the compositor applies them only on `wl_surface.commit`, and Qt commits when it paints the window. After the mouse release there was nothing to repaint, and the changes waited until something updated the strip, e.g. the header tooltip that appears after a few seconds.
- **Solution:** after a position change, `widget->update()` and `QWindow::requestUpdate()` (`movePanel` in `platform/common/WindowRoles.cpp`). Measured after the fix: the new position is in KWin before the first poll (< 250 ms), both on the same monitor and when moving to another one.
- **Note:** when moving to another monitor the surface is recreated, and for about 0.5 s a fading copy is visible in the old place: that is KWin's window close animation.
- **Status:** solved.

## 2026-10-19 · A second instance does not find the first one, `--quit` does not arrive

- **Environment:** Qt 6.11.2, Fedora 44.
- **Symptom:** with the program running, `markpaste --capture` started a second full instance. `markpaste --quit` did not close the first one.
- **Cause:** `QLocalServer` with `UserAccessOption` creates the socket in a temporary directory (`/run/user/1000/.AxWCqw/s`) and renames it over the target path. `listen()` succeeds for every new process, and the last one takes over the name. Visible in `ss -xlp`: every process has its own LISTEN socket.
- **Solution:** a new process first connects to the socket and passes its command. Only if nobody answers does it remove the old socket and listen itself. `UserAccessOption` was removed: `$XDG_RUNTIME_DIR` is 0700 anyway.
- **Status:** solved, checked (`--capture` and `--quit` arrive, the process exits).

## 2026-10-19 · An empty log when the program is started in the background

- **Symptom:** `markpaste > app.log 2>&1 &` gives an empty file.
- **Cause:** Qt writes to journald when stderr is not a terminal.
- **Solution:** `QT_FORCE_STDERR_LOGGING=1` or `journalctl --user -f` (docs/build.md).
- **Status:** not a program problem, recorded for developers.

## 2026-10-19 · Pasting into our own window hangs and repeats dozens of times

- **Environment:** Fedora 44 KDE, Plasma 6.7.5, Wayland, Qt 6.11.2.
- **Symptom:** the `pastetest --self-test` self-check with the receiver in the same process. 3 images "arrived", and the text came out empty 37 times in a row.
- **Cause:** a deadlock. Qt reads the clipboard blockingly in the main thread, and the compositor's data request comes to our data-control connection, which was also served by the main thread. While the thread was stuck, Qt's client-side key repeat fired. The images "arrived" only because the receiver checked the formats and did not read the data.
- **Solution:** the Wayland connection was moved to a separate thread, see [ADR 0002](decisions/0002-wayland-connection-thread.md). The self-test receiver now really reads the data and by default runs as a separate process.
- **Status:** solved. `--self-test` and `--self-test --in-process`: PASS.

## 2026-10-19 · The portal refuses to register the app id: "App info not found"

- **Environment:** xdg-desktop-portal 1.22.1, Fedora 44 KDE.
- **Symptom:** `org.freedesktop.host.portal.Registry.Register("su._525.Markpaste")` → `Could not register app ID: App info not found`.
- **Cause:** the portal looks for the .desktop file through GLib (`GDesktopAppInfo`). GLib drops the file if the program from `Exec` is not on disk. Checked: `Gio.DesktopAppInfo.new()` returns NULL while the binary is missing, and an object as soon as it appears.
- **Solution:** the target `cmake --build build --target dev-install-desktop` installs a .desktop file into `~/.local/share/applications/` with `Exec` pointing to the binary in the build directory. While there was no main binary yet, `build/src/app/markpaste` was a link to `markpaste-pastetest`.
- **Consequence for packages:** the .desktop file and the binary must be installed together. Registering with the portals works only from a process for which the .desktop file is valid.
- **Status:** solved for development.

## 2026-10-19 · RemoteDesktop portal: closing the session right after Ctrl+V

- **Environment:** the same.
- **Symptom:** on the first run the last Ctrl+V repeated many times.
- **Cause:** mostly the deadlock from the entry above. But the `NotifyKeyboardKeycode` events themselves also reach the compositor asynchronously: a D-Bus reply means "the portal accepted it", not "KWin processed it".
- **Solution:** after the last Ctrl+V the session is closed only after the profile delay (≥ 400 ms). This rule is for `PasteOrchestrator`.
- **Status:** solved, the rule was carried over to the main paste code.

## 2026-10-20 · The first-start wizard opens on every start

- **Environment:** Fedora 44 KDE, Plasma 6.7.5, Wayland, Qt 6.11.2.
- **Symptom:** after closing and starting the program again, the wizard (APP-4) opens again. `~/.config/markpaste/markpaste.conf` has two `[%General]` sections with `setupShown=true`.
- **Cause:** the flags were kept under the keys `general/setupShown`, `general/welcomeShown`, `general/hideHintShown`. QSettings writes the `general` group to INI as `[%General]` and, when reading the file, turns it into the group `General`, with a capital letter. Keys are case-sensitive: `general/setupShown` is not found after a restart. Within one process everything worked (values from the cache), so the tests did not catch the bug. Checked with a separate program on a copy of the configuration: `general/setupShown=0`, `General/setupShown=1`.
- **Solution:** the flags moved to the `once/…` group; old `General/…` values are honored too, so those who have already seen the wizard will not see it again. The rule is in docs/build.md.
- **Status:** solved, checked by the user on 2026-10-20: the wizard does not open after a restart.

## 2026-10-20 · A new paste shortcut from settings has no effect

- **Environment:** the same, xdg-desktop-portal 1.22.1, GlobalShortcuts v2.
- **Symptom:** the paste shortcut is replaced with Alt+V in the program settings. After a restart the old Ctrl+Alt+V works, Alt+V does not. The log says `shortcut "paste-set" bound to "Ctrl+Alt+V"`.
- **Cause:** through the GlobalShortcuts portal the program only suggests a shortcut (`preferred_trigger`). KDE honors it at the first binding, after that it keeps its own (`~/.config/kglobalshortcutsrc`, group `su._525.Markpaste`) and ignores a new suggestion. This is by design of the specification: on Wayland shortcuts belong to the user and the desktop. The field in settings was editable and misleading.
- **Solution:** if the system assigns shortcuts (`IHotkeys::systemAssignsTriggers`, the portal), the fields in settings are read-only and show the actual shortcuts; the "Change shortcuts…" button opens the KDE dialog (`ConfigureShortcuts`, portal v2). A change in KDE comes as the `ShortcutsChanged` signal, and the program updates the display without a restart ([ADR 0012](decisions/0012-system-assigned-shortcuts.md)). The program's own value is kept only as the suggestion for the first binding.
- **Status:** solved, checked by the user on 2026-10-20: Alt+V was assigned via "Change shortcuts…" and works without a restart.

## 2026-10-20 · The "Remote control session started" notification on every paste

- **Environment:** the same, xdg-desktop-portal-kde 6.7.5.
- **Symptom:** every Ctrl+Alt+V shows the notification "KDE portal integration: Remote control session started. The application "Markpaste" uses the following special permissions: Input device control".
- **Cause:** on KDE Ctrl+V is emulated through the RemoteDesktop portal. The program opens a session before a paste and closes it after (the log: `RemoteDesktop session started` for every paste). The portal itself shows the notification on every `Start`: the `remotedesktopstarted` event in `/usr/share/knotifications6/xdg-desktop-portal-kde.notifyrc`, urgency High. The program cannot turn it off. The program's behavior has not changed since the first version.
- **Workaround for the user:** "System Settings" → "Notifications" → "Application settings" → "KDE portal integration" → "Started Remote Desktop" → uncheck the popup. The downside: the notification disappears for other programs too.
- **Proposed solution (not implemented):** keep the RemoteDesktop session open after a paste while there are no new pastes, e.g. for 5 minutes (a setting; 0 means as now). A series of pastes would give one notification, and every next paste would start faster, without `CreateSession`/`Start`.
  - The price: while the session is open, KDE keeps the "Remote Control" icon in the tray, and the program can formally control input.
  - Design: `PasteOrchestrator` calls `release()` instead of `end()`; the injector closes the session itself on an idle timer. The session may be closed from outside (the user clicked the tray icon, the `Session.Closed` signal), then the next `begin()` opens a new one.
  - Does not fit the Clipboard portal (GNOME): a clipboard written in the session lives only while it is open, and restoring the clipboard is tied to closing it (ADR 0010). There the session is closed right away, as now.
- **Status:** open, the decision is postponed.

## 2026-10-20 · After a paste the program holds hundreds of megabytes

- **Environment:** Fedora 44, glibc, Qt 6.11.2; measured with `markpaste-bench` (26 4K shots, pasting 5).
- **Symptom:** before the paste RSS 170 MB, after pasting 5 4K shots 650–750 MB, and it does not go down after seconds. The user's program in the background holds 189 MB.
- **Cause:** baking (`bakeSequence`) runs in the QtConcurrent thread pool. glibc gives every thread its own malloc arena; freed unpacked frames (33 MB per 4K frame) stay in the arenas and are not returned to the system. There is no leak: `malloc_trim(0)` returns everything, 748 → 166 MB.
- **Solution:** `mp::releaseFreedMemory()` (`src/platform/Memory.h`; `malloc_trim(0)` on glibc, nothing otherwise) 2 s after a paste, a drag, a save to folder, closing the preview and clearing the set. The orchestrator no longer keeps the baked images between pastes.
- **Pitfall:** the first version did nothing: no libc header was included in the file before `#ifdef __GLIBC__`, and the macro was not defined. Any libc header (`<cstdlib>`) is needed before the check.
- **Status:** solved, checked with `markpaste-bench`.

## 2026-10-20 · Capture stops going through KWin if the program file was replaced on the fly

- **Environment:** Fedora 44 KDE, Plasma 6.7.5, Wayland.
- **Symptom:** the log has `KWin ScreenShot2: The process is not authorized to take a screenshot`, and the program switches to the Screenshot portal: a shot takes 379–596 ms instead of ~220.
- **Cause:** KWin lets a process into ScreenShot2 if its executable matches `Exec` from the .desktop file. The program was rebuilt while it was running (a user gets the same on a package update through dnf): `/proc/<pid>/exe` points to `… (deleted)`, and the comparison fails.
- **Solution:** the shot is still taken through the portal (the fallback). If `/proc/self/exe` ends with ` (deleted)`, the program says: "Markpaste was updated while running. Restart it to capture without delays and dialogs".
- **Status:** solved; the notification has not been checked in a live session.

## 2026-10-21 · GNOME: the setup wizard paste test fails, "QWaylandDataOffer: timeout reading from pipe"

- **Environment:** Ubuntu 26.04 GNOME, Wayland, Markpaste 0.2.0 (the ubuntu26.04 .deb). No ext-data-control-v1: the clipboard goes through the Clipboard portal in a RemoteDesktop session. Reproduced on Fedora 44 KDE (Qt 6.11.2, xdg-desktop-portal-kde 6.7.5) through the same portal code: `markpaste-pastetest --portal-clipboard`, step 5b.
- **Symptom:** "Test paste" in the setup wizard: "Ctrl+V was sent, but the text did not appear". The log: `RemoteDesktop session started, devices = 1 clipboard = true`, then `QWaylandDataOffer: timeout reading from pipe` and `error reading data for mimeType text/plain;charset=utf-8`, the same for `text/plain`. Step 5b on 0.2.0: `own QClipboard reads (2002 ms): ""`.
- **Cause:** the paste target is our own window. Qt reads the Wayland offer synchronously in the main thread and polls the pipe. The data for the pipe comes only after the portal's `SelectionTransfer` signal → `SelectionWrite`, and that slot ran in the same main thread, so it could not run until Qt gave up. Pasting into other programs was not affected. The same deadlock was solved for data-control in ADR 0002; the portal path missed it.
- **Solution:** `PortalSelectionServer` (`src/platform/linux/portal/PortalClipboard.h`) lives in its own thread and serves `SelectionTransfer` there (QtDBus delivers the signal in the receiver's thread; the `SelectionWrite` call blocks only that thread). Step 5b after the fix: 1 ms, the text arrives.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21: "Ctrl+V reached the window", the text is in the field.

## 2026-10-21 · GNOME: "press Press Print to test" in the setup wizard

- **Environment:** Ubuntu 26.04 GNOME, GlobalShortcuts portal.
- **Symptom:** the wizard showed "Shot with editor: press Press Print to test", "Paste set: Press <Control><Alt>v — works".
- **Cause:** GNOME's `trigger_description` is "Press " plus a GTK accelerator (`Press <Control><Alt>v`); KDE gives `Ctrl+Alt+V`.
- **Solution:** `PortalGlobalShortcuts::displayTrigger` drops "Press " and turns the accelerator into `Ctrl+Alt+V`. Unit test `PortalGlobalShortcuts.DisplayTriggerFromGnomeAndKde`. A localized GNOME (not "Press ") would still show its own prefix.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21 ("press Print to test", "Ctrl+Alt+V").

## 2026-10-21 · GNOME: two strip headers, ours does not move the strip; the overlay was not full screen either

- **Environment:** Ubuntu 26.04 GNOME, Wayland, Markpaste 0.2.1 test build (the ubuntu26.04 .deb, built with LayerShellQt for Qt 6).
- **Symptom:** the strip has the GNOME title bar ("Markpaste — se…") above our header. The window moves only by the GNOME bar (anywhere on the screen, no cursor change). Dragging our header shows only the outline of the future position, and the strip stays in place.
- **Cause:** `layerShellAvailable()` checked only that Qt runs on Wayland, not that the compositor has `zwlr_layer_shell_v1` (the comment said PlatformFactory checks it, but nothing used that result). Mutter has no layer-shell, LayerShellQt silently made an ordinary decorated window, and the program treated it as a panel: the outline instead of `startSystemMove()`, `movePanel()` with no effect. The capture overlay went the same wrong way instead of its "ordinary full-screen window" branch.
- **Solution:** `layerShellAvailable()` also asks our Wayland connection for `zwlr_layer_shell_v1`. Without layer-shell the strip is frameless and transparent, our header calls `startSystemMove()`, and the cursor does not stay "grabbing" (the compositor takes the pointer, the release does not come to us). ADR 0004 is updated.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21: one header, dragging it moves the strip anywhere; the capture overlay covers the whole screen and opens 548 ms after Print.

## 2026-10-21 · GNOME: the strip returns to the middle of the screen after a capture

- **Environment:** Ubuntu 26.04 GNOME, Wayland, 0.2.1 test build.
- **Symptom:** the strip moved to the right corner comes back in the middle of the screen, over the shot editor, after each capture.
- **Cause:** before a capture the program hid its windows with `hide()`. On Wayland Qt destroys the `xdg_toplevel`, and `show()` creates a new window that Mutter places itself (Ubuntu centers new windows). A program cannot set the position.
- **Solution:** without layer-shell on Wayland the strip is not hidden for a capture but made fully transparent (`StripWindow::hideForCapture()`, ADR 0026). The first version left the strip transparent after a successful capture: that path shows the strip with `present()`, not `showAfterCapture()`; now `present()` removes the transparency.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21 (remotely, see below): the strip moved to the top-right corner stays there after a capture, with "Always on Top", and is not in the captured frame.

## 2026-10-21 · GNOME: the strip is covered by other windows

- **Environment:** the same.
- **Symptom:** any window covers the strip.
- **Cause:** Mutter has no layer-shell and no protocol for "keep above"; `Qt::WindowStaysOnTopHint` does nothing on Wayland. Expected by the spec (STR-1).
- **Solution:** a right click on the strip header opens the GNOME window menu (`xdg_toplevel.show_window_menu`), where the user ticks "Always on Top" (ADR 0026). It lasts while the strip window exists.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21: the right click opens the GNOME window menu (Take Screenshot, Hide, Maximize, Move, Resize, Always on Top, …); with "Always on Top" the strip stays above the active editor window.

## 2026-10-21 · GNOME: no program icon in the app grid

- **Environment:** Ubuntu 26.04 GNOME, the ubuntu26.04 .deb.
- **Symptom:** the app grid shows an empty place above "Markpaste", not even the generic icon.
- **Cause:** `su._525.Markpaste.svg` began with a long comment; `<svg` was at byte 416. shared-mime-info finds SVG by `<svg` within the first 256 bytes, so by content the file was `application/xml` (`gio info` on a copy without the extension). The icon cache had the icon (checked in an `ubuntu:26.04` container), and librsvg draws the file, so the loader is the likely point of failure: GNOME 49+ loads images with glycin, which picks the decoder by content. Not confirmed on GNOME itself.
- **Solution:** the comment moved after the `<svg>` tag; the test `AppIconFile.SvgIsRecognizedByContent`.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21: the app search shows the icon.

## 2026-10-21 · GNOME: Print opens the GNOME screenshot tool, not Markpaste

- **Environment:** Ubuntu 26.04 GNOME, GlobalShortcuts portal.
- **Symptom:** the portal bound Print to "Capture with editor", but Print opens the GNOME screenshot UI; in the log the capture hotkey never activates (Ctrl+Print and Ctrl+Alt+V do).
- **Cause:** GNOME Shell's own binding `org.gnome.shell.keybindings show-screenshot-ui = ['Print']` takes the key first. Expected by the spec (section 9).
- **Solution:** the setup wizard detects it and offers "Give Print to Markpaste": the key is removed from the GNOME setting with gsettings and the old value is saved; the settings can give it back (ADR 0026). Checked with `pastetest --gnome-keys Print --gnome-action release|restore` on a local copy of the schema.
- **Status:** solved, checked on Ubuntu 26.04 GNOME on 2026-10-21: the wizard showed the warning, the button set `show-screenshot-ui` to `@as []` (the first version also restarted the program; not needed, GNOME passes the key to the existing portal shortcut at once), and Print then activated our capture ("Print — works"). "Give the keys back" in the settings was not clicked on GNOME.

## 2026-10-21 · Remote testing of the Ubuntu VM from the host

- **Environment:** VirtualBox 7.2.18 on the Fedora host, the VM `Ubuntu26.04-test` (Ubuntu 26.04 live session, GNOME, Wayland, bridged network). The Guest Additions in the VM are the kernel's own (6.0, no VBoxService), so `VBoxManage guestcontrol` does not work.
- **How:** screenshots with `VBoxManage controlvm Ubuntu26.04-test screenshotpng`; commands over SSH as root (the user installed openssh-server); commands in the GNOME session as `ubuntu` with the environment of `gnome-shell` (`/usr/local/bin/insession` in the VM, plus `WAYLAND_DISPLAY=wayland-0`); mouse and keys with `ydotool` (`ydotoold` as root, uinput) after `gsettings set org.gnome.desktop.peripherals.mouse accel-profile flat`, then `ydotool mousemove --absolute` hits screen pixels exactly.
- **Pitfall:** Escape sent to close the app grid also closes a Markpaste dialog that has just got focus.
- **Status:** works; the live session loses all of it on reboot.

## 2026-10-21 · GNOME: "Copy set" fails with "Clipboard portal: no RemoteDesktop session"

- **Environment:** Ubuntu 26.04 GNOME, Wayland, the Clipboard portal as the program clipboard (no data-control).
- **Symptom:** the "Copy set" button (mode 2, files + text) shows "Paste failed: Clipboard portal: no RemoteDesktop session". The main fallback mode of GNOME (spec section 9) did not work at all.
- **Cause:** mode 2 writes the clipboard without an input session, and `PortalClipboard` can write only inside a RemoteDesktop session. Opening one would not help: what is written in the session disappears when it closes (ADR 0010).
- **Solution:** outside a session `PortalClipboard` writes through `QClipboard`. The click on "Copy set" gives the strip keyboard focus, which Mutter requires for `wl_data_device.set_selection`; the content lives as long as the program.
- **Status:** solved, checked in the VM on 2026-10-21: "Set copied — press Ctrl+V in the chat", Firefox receives `shot-A.png` on Ctrl+V. Firefox takes only the first file and no text from the files + text entry (a known property of mode 2, spec section 10).

## 2026-10-21 · GNOME: the strip does not grow when a shot is added

- **Environment:** the same, the strip without "Always on Top".
- **Symptom:** after the second shot the strip keeps the height for one shot; the second thumbnail is under "Copy set" and "New shot".
- **Cause:** the Qt log (`QT_LOGGING_RULES=qt.qpa.wayland.debug=true`): `Received xdg_toplevel.configure with QSize(254, 288) … WindowActive`, `Applied pending xdg_toplevel configure event: QSize(254, 288)`. When the active window changes, Mutter sends the strip a configure with its old size, and Qt resizes the window to it, undoing `adjustSize()`. The strip window was resizable.
- **Solution:** two parts. `QLayout::SetFixedSize` on the strip layout (min = max = content; GNOME's window menu then greys out Maximize and Resize). That alone did not help: the resize made while the strip was transparent and covered by the capture overlay never reached Mutter (`Didn't receive frame callback in time, window should now be inexposed`: Qt does not commit without frame callbacks), so Mutter kept sending the old size. `StripWindow::resizeEvent` now resizes the window back to its content size when the compositor imposes another one.
- **Status:** solved, checked in the VM on 2026-10-21: two and three shots in a row, the strip grows; deleting a shot shrinks it.

## 2026-10-21 · GNOME: the clipboard is empty after a sequential paste (PST-8)

- **Environment:** Ubuntu 26.04 GNOME, Clipboard portal; checked in the VM with a test page in Firefox.
- **Symptom:** "ORIGINAL-CLIPBOARD" copied in Firefox, then Ctrl+Alt+V pastes the set correctly (A 16462 bytes, B 7865 bytes, the text; every transfer served in ~1 ms), but a Ctrl+V afterwards pastes nothing: the clipboard is empty.
- **Cause:** the program saves the clipboard (`saved clipboard: text/plain…`) and writes it back through the portal before closing the session (`restoring clipboard … SetSelection`), but Mutter drops a selection set in a RemoteDesktop session when the session closes. On KDE the previous content comes back instead (ADR 0010).
- **Possible solutions:** keep the RemoteDesktop session open after a paste until someone else takes the clipboard or for a few minutes (GNOME shows the remote control indicator meanwhile); or accept it and say so in the help.
- **Decision (the user, 2026-10-21):** accepted as a GNOME limitation for now, described in README "On GNOME".
- **Status:** known limitation.

## 2026-10-21 · Firefox puts pasted pieces out of order (test page only)

- **Environment:** Firefox (snap) on Ubuntu 26.04, a plain `contenteditable` test page.
- **Symptom:** after a sequential paste the second image sometimes stands after the text, or an image appears where the caret was later.
- **Cause:** Firefox's caret after an inserted image in a `contenteditable` (and its later node normalization); the program's log shows the right order and sizes. Real chats take images as attachments, not inline.
- **Status:** not a program bug; check in real chats (spec section 12).

## 2026-10-21 · VM testing: ydotool absolute moves close GNOME menus

- **Symptom:** clicks on items of the tray menu (an AppIndicator menu drawn by GNOME Shell) did nothing.
- **Cause:** `ydotool mousemove --absolute` first moves the pointer to (0, 0), the Activities hot corner, which closes Shell menus. Qt's own menus are not affected.
- **Workaround:** inside a Shell menu move relatively (`ydotool mousemove -x DX -y DY`).

## 2026-10-21 · GNOME: a left click on the tray icon opens the menu, not the strip

- **Environment:** Ubuntu 26.04 GNOME, the `ubuntu-appindicators@ubuntu.com` extension (StatusNotifierItem host).
- **Symptom:** on KDE a left click on the tray icon shows or hides the strip; on Ubuntu it opens the menu.
- **Cause:** the extension's `indicatorStatusIcon.js`: a single primary click on an item with a menu toggles the menu (after the double-click time), a double click calls `Activate` (our `QSystemTrayIcon::Trigger`), a middle click calls `SecondaryActivate`. Our item reports `ItemIsMenu = false`; the extension has no setting for a single-click activation.
- **Solution:** none in the program; a double click shows or hides the strip (checked in the VM). Described in README "On GNOME".
- **Status:** known difference.
- **Update 2026-10-22:** the user reported that a double click opens the menu too. In the VM `ydotool` double clicks with 120–300 ms between presses showed and hid the strip every time, never the menu. The extension counts a double click only within the GNOME double-click time (`org.gnome.desktop.peripherals.mouse double-click`, 400 ms) and within the double-click distance, otherwise the first click opens the menu. Not reproduced; **Show set** is now the first menu item, and the log has `tray activated, reason …` for each activation. Status: open, waiting for the user's log.

## 2026-10-21 · KDE: Alt+V sometimes pastes nothing, and the set is cleared

- **Environment:** Fedora 44 KDE Plasma 6.7.5, Wayland; paste-set bound to "Ctrl+Alt+V, Alt+V"; "Clear the set after pasting" on.
- **Symptom:** now and then Alt+V pastes nothing into the chat, the "Remote control session started" notification appears as usual, and the strip is cleared, so the shots are lost. Once four times in a row, until the user switched to another window and back.
- **Cause:** in the log every failed paste has two `hotkey activated 1` about 160 ms apart; the successful ones have one. For Alt+V the program sends the Ctrl "mask" right away (so that a lone Alt does not open the chat's menu). If V is still held, Ctrl↓ turns Alt+V into Ctrl+Alt+V, which is bound to the same action: KDE reports `Deactivated` + `Activated`, and `Deactivated` again on Ctrl↑. The program took the first `Deactivated` for the release of the user's keys and sent Ctrl+V while Alt (and V) were still physically held, so the chat got Ctrl+Alt+V. The program cannot see that nothing was pasted, so "Clear the set after pasting" cleared the set. Switching windows did not matter: the user simply released the keys faster.
- **Solution:** `PasteOrchestrator` counts a release only after `releaseSettleMs` (150 ms) without a new press; an activation of the paste shortcut while a paste waits for the release (`hotkeyPressed()`) makes it wait again, up to the first Ctrl+V, with its own timeout. The log now has `hotkey released`. Tests `ChordChangeIsNotARelease`, `LatePressBeforeFirstKeysWaitsAgain`.
- **Status:** fixed, waiting for the user's check in daily use (it happens rarely). Not checked: only Alt+V bound (no Ctrl+Alt+V); KDE may then report `Deactivated` on Ctrl↓ with no new `Activated`. Since 2026-10-22 the settings warn about shortcuts with Alt but without Ctrl on Linux ([ADR 0027](decisions/0027-ctrl-v-on-windows.md)).

## 2026-10-22 · GNOME: the strip shown from the tray always comes back in the middle of the screen

- **Environment:** Ubuntu 26.04 GNOME 50, Wayland (Mutter, no layer-shell), Markpaste 0.2.1 test build.
- **Symptom:** the strip moved to a corner, hidden with "—", shown again from the tray: it is in the middle of the screen.
- **Cause:** `hide()` destroys the `xdg_toplevel`; the next `show()` is a new window, and Mutter places it in the middle (the program cannot set positions on Wayland). The same as the capture case of ADR 0026.
- **Solution:** where the compositor places windows, hiding minimizes the strip; showing restores it by xdg-activation. The tray passes a token (`ProvideXdgActivationToken`).
- **After a capture:** the strip stayed minimized, and GNOME showed "Markpaste — set — Markpaste is ready". Two tries failed:
  - activating after the overlay closed;
  - activating from the confirm handler through Qt's own `requestActivate`.
  What worked: the program asks for a token explicitly on behalf of the still-open, focused overlay (QtWayland private API) and closes the overlay once the token comes. [ADR 0026](decisions/0026-gnome-strip-and-desktop-keys.md), addendum.
- **Status:** checked in the VM on 2026-10-22:
  - moved to the bottom-right corner, "—", a double click on the tray: back in the corner;
  - moved to the top-left corner, "—", a capture to the strip: restored in the corner with the new shot (`activation token true` in the log);
  - after that, "—" and the tray again: in place.
