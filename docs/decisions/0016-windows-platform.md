# 0016. Windows: DXGI capture, RegisterHotKey and a hook for hotkeys, OLE clipboard, SendInput

- Status: accepted (2026-10-20)
- Spec items: section 9 ("Windows"), section 10 (clipboard format, "own events", UIPI), CAP-1, PST-1, PST-2, STR-1, STR-5, NFR-4, NFR-5, NFR-11, APP-3 (autostart)

## Context

The Linux version is ready, the next stage is Windows 10/11 (ADR 0001). The core and the UI already do not depend on the OS: the Windows version is four implementations of the platform interfaces in `src/platform/windows/` (namespace `mp::win`) and `#ifdef Q_OS_WIN` branches in the shared files of `src/platform/common/`. There is no Windows code outside `src/platform/`: the check `grep -rn 'ifdef\|Q_OS_' src | grep -v ^src/platform/` is empty.

## Decision

### Capture: `DxgiCapture`

- **DXGI Desktop Duplication** per monitor: `D3D11CreateDevice` on the monitor's adapter → `IDXGIOutput5::DuplicateOutput1` with the single format `B8G8R8A8_UNORM` (the system itself converts an HDR desktop to 8 bits) → `AcquireNextFrame` → copy into a staging texture → `QImage::Format_RGB32`. Without `IDXGIOutput5` (Windows 10 before 1703), `IDXGIOutput1::DuplicateOutput`.
- The first duplication frame is black on some drivers: if it is empty and `LastPresentTime == 0`, we wait for one more frame for 100 ms, otherwise we take what there is.
- A rotated monitor: the duplication surface is in the panel's own orientation, rotated by `DXGI_OUTDUPL_DESC.Rotation`.
- **GDI BitBlt** (`CAPTUREBLT`, in physical pixels) for a monitor DXGI did not provide: the lock screen and a UAC window (`E_ACCESSDENIED`), a driver without duplication, Wine. The decision is made per monitor and per shot, not "failed once, forever" as in `FallbackCapture`: a temporary DXGI failure must not deprive HDR monitors of the correct image until a restart. That is why `DxgiCapture` is not wrapped into `FallbackCapture` → `QtScreenCapture`: after one completely failed shot it would switch to `QScreen::grabWindow` for good, which is the same GDI but without DXGI (review, 2026-10-20).
- Matching with `QScreen`: by device name (`\\.\DISPLAY1`; Qt 6.11 names screens the same way), and if that does not match, by the top-left corner: Qt keeps it in physical coordinates also with different monitor scales.
- The capture runs in the thread pool (`QtConcurrent`); the `QScreen` geometry is taken beforehand in the main thread. The D3D11 device is created for every shot, without a cache: simpler and without objects shared between threads. If this turns out to be slow (the NFR-1 target is 300 ms on 2×4K), cache the device by the adapter LUID.
- **Remote desktop** (`SM_REMOTESESSION`): GDI only, DXGI returns empty black frames there. And in general a black frame without rendering (`LastPresentTime = 0`) is not accepted: the monitor is captured via GDI (docs/problems.md, 2026-10-20).
- `MARKPASTE_CAPTURE=gdi`: GDI only, for drivers where DXGI misbehaves and for Wine without a screen (docs/problems.md, 2026-10-20).

### Hotkeys: `WinHotkeys`

- Ordinary shortcuts use `RegisterHotKey` on a message-only window (`HWND_MESSAGE`), with `MOD_NOREPEAT`. The system itself handles modifiers, sends no autorepeat and answers `ERROR_HOTKEY_ALREADY_REGISTERED` if the shortcut is taken; the user gets "… is already used by another program", as on X11.
- **Print Screen goes through the `WH_KEYBOARD_LL` hook** (`KeyboardHook`) rather than `RegisterHotKey`: on Windows 11 the key is taken by the Snipping Tool (spec section 9). The hook swallows the press: neither the Snipping Tool nor copying the screen to the clipboard fires.
- The hook lives in **its own thread with its own message loop**. Windows waits for the hook's answer no longer than `LowLevelHooksTimeout` and silently removes a slow hook; in the main thread it would wait for image baking. The thread only compares the key code and sends a signal to the main thread through the queue; it does not write to the log. Presses with `LLKHF_INJECTED` (our own) are skipped. The hook is installed only while at least one shortcut is bound to it.
- **Release.** `RegisterHotKey` does not report it, but the paste waits for it so that a held Alt does not turn our Ctrl+V into Ctrl+Alt+V. After the hotkey fires we poll `GetAsyncKeyState` every 20 ms until the key and the shortcut's modifiers are released, then `released`. The hook sees the release itself.
- The shortcut from settings is portable (`Primary+Alt+V`) → `QKeySequence` → virtual key code. Shown as "Ctrl+Alt+V".

### Clipboard: `WinClipboard`

- `QClipboard` (OLE) writes from the background, without focus. Qt spreads the entry over Windows formats: the image into `CF_DIB`/`CF_DIBV5`, the file list into `CF_HDROP`, the text into `CF_UNICODETEXT`.
- Qt does not put the **`PNG`** format itself, while Chrome, Telegram and Office read it first (transparency, lossless): we add the PNG bytes as they are via `application/x-qt-windows-mime;value="PNG"`.
- **"Not into history"** (NFR-4, spec section 10): `ExcludeClipboardContentFromMonitorProcessing`, `CanIncludeInClipboardHistory` = 0, `CanUploadToCloudClipboard` = 0; the Win+V history, cloud sync and clipboard monitors skip such entries. When the clipboard is restored after a paste there are no marks: it is the user's clipboard again.
- The clipboard snapshot before a paste: if Qt did not parse another program's image (under Wine `CF_DIB` gave an empty `QImage`), we read `PNG` or `CF_DIB` ourselves; `CF_DIB` is a BMP file without the 14-byte header, so we add it and read as BMP.
- **A busy clipboard.** After every change other programs briefly open the clipboard (`rdpclip` in RDP, VBoxTray, clipboard managers), and `OleSetClipboard` does not write meanwhile; Qt only leaves a warning. So after a write we check `QClipboard::ownsClipboard()` and retry up to 20 times every 100 ms, otherwise it is an error; a new write cancels the retries of the previous one. The clipboard snapshot waits until it is released (the check opens the clipboard from our own hidden window). Retries run on a timer, without blocking the main thread: while the clipboard is being read, our thread serves the data (OLE delayed rendering).
- Only `text/plain`, `text/uri-list` and images go into the clipboard; X11 synonyms (`UTF8_STRING`…) from the shared set of formats are dropped.

### Ctrl+V: `WinInputInjector`

- `SendInput` one event at a time: Ctrl↓ V↓ V↑ Ctrl↑ with 20 ms pauses, as on X11 and Wayland. V is the virtual code `VK_V`: Windows programs match the shortcut by it, so Ctrl+V pastes with a Russian layout too.
- Before pressing we wait for Shift/Alt/Win to be released (up to 1.5 s).
- **Window change** (spec section 10, step 6): the window active in `begin()` is remembered; if another one is active before the next Ctrl+V, the paste stops with an error. The check is done after waiting for the modifiers, right before the press. `canDetectForegroundChange()` = true.
- If `SendInput` failed, only releases are sent after that: V without Ctrl would type "v".
- If a window of the program itself is active (settings, preview, wizard), the paste does not start (`AppController::paste`, on all platforms).
- **UIPI**: `SendInput` silently does not reach a window started as administrator and returns no error. So `begin()` compares the processes' integrity levels (`TokenIntegrityLevel`) and reports right away: the window runs as administrator, copy the set and paste it by hand. There is no automatic switch to the "files + text" mode as in the spec yet.
- The process name of the target window is written to the log, groundwork for per-program delay profiles.

### Windows

- **Strip** (`applyPanelRole`): a frameless window above all others, `Qt::WindowDoesNotAcceptFocus` and `WS_EX_NOACTIVATE`. A click on the strip does not take focus from the chat, so the "Paste" button works in mode 1, as on KDE with layer-shell. Rounded corners: a transparent window, the strip paints its own background from the style sheet.
- **Moving the strip** (STR-5): dragged by the header (`startSystemMove`). The end of the drag is `WM_EXITSIZEMOVE` (`watchPanelMoves`): the strip snaps flush to the nearest edge at the height where it was released, and the position is saved. The edge and height stay within the work area (`availableGeometry`) so as not to cover the taskbar. Nothing is done during the drag, otherwise the window would "jump" under the mouse.
- **DWM window animations are turned off** for all program windows (`disableWindowAnimations`): before a capture the program hides its windows, and a fading editor got into the frame.
- **Overlay**: an ordinary full-screen window above all others (as on GNOME and X11). `forceActivate` gives it the keyboard: Windows does not let a program become active if the last input event was not received by it, and our hook swallows Print Screen. We attach to the thread of the active window (`AttachThreadInput`) and call `SetForegroundWindow`.

### Other

- **Autostart**: the value `su._525.Markpaste` in `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, the path in quotes. Turning it off in "Task Manager → Startup" (`StartupApproved\Run`) is seen by the program as "off"; turning it on from the program removes that mark.
- **Settings**: `QSettings` in the registry (`HKCU\Software\markpaste\markpaste`); data and log in `%LOCALAPPDATA%\markpaste\markpaste` (NFR-5). Mode 0700 is not needed on Windows: the profile directory is closed by the owner's ACL.
- **Memory**: `releaseFreedMemory()` calls `_heapmin()`.
- **SIGPIPE** moved from `main.cpp` to `platform/Process.h` (`prepareProcess()`): Windows does not have it.

## Options considered

- **Windows.Graphics.Capture** (spec section 9, the main path). On Windows 10 it draws a yellow border around the capture that cannot be turned off; it needs C++/WinRT, which MinGW does not have. DXGI handles HDR via `DuplicateOutput1`. Go back to WGC if the DXGI image on HDR monitors turns out to be wrong.
- **Only the `WH_KEYBOARD_LL` hook for all shortcuts.** The hook sees releases, but for ordinary shortcuts `RegisterHotKey` is more reliable: Windows does not remove it for slowness, and antivirus software does not care about it (spec section 14, the "keyboard hook" risk).
- **The hook in the main thread.** Simpler, but Windows removes the hook if the main thread is busy longer than `LowLevelHooksTimeout`.
- **Our own WinAPI clipboard code instead of QClipboard.** Qt already provides CF_DIB, CF_HDROP and delayed rendering; what is missing (`PNG` and the marks) is added through `QMimeData`.

## Consequences and limitations

- Checked under Wine 11 and on Windows 11 24H2 in VirtualBox (docs/platform-matrix.md): DXGI, Print Screen via the hook, `forceActivate`, pasting with the hotkey and the strip button, stopping on a window change. Not checked: RDP after the fix, HDR, monitors with different scales, UIPI, the Win+V history, autostart at login, Windows 10.
- Not done: ~~intercepting the ordinary Ctrl+V (PST-2)~~ (done on 2026-10-22, [ADR 0027](0027-ctrl-v-on-windows.md)), per-program delay profiles, capturing the window under the cursor (CAP-5), a wizard hint about the Snipping Tool, an automatic switch to "files + text" on UIPI.
