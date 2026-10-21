# Problem log

Each entry: date, environment, symptom, cause, solution, links. New entries go on top. Unsolved ones are recorded too, with the status "open".

---

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
