# How the program is built (actual state)

Updated: 2026-10-20.

## Layers and CMake targets

```
markpaste (app/)             controller, settings, single instance, tray, log, version check
  └ markpaste_ui (ui/)       overlay, editor, strip; no OS-specific code
      ├ markpaste_core       core/ model, set on disk · render/ baking · paste/ paste orchestrator
      │   └ markpaste_platform_api   PlatformTypes + 4 interfaces
      └ markpaste_platform   PlatformFactory, WindowRoles, Paths, implementations:
            common/          QtClipboard, QtScreenCapture, FallbackCapture
            linux/portal/    PortalBus, RemoteDesktop, Screenshot, GlobalShortcuts
            linux/wayland/   WaylandConnection (own thread), DataControlClipboard
            linux/kde/       KWinScreenshot
            linux/x11/       X11Hotkeys, X11InputInjector
            windows/         DxgiCapture, WinHotkeys + KeyboardHook (own thread), WinClipboard, WinInputInjector
```

The core sees only `markpaste_platform_api`. OS `#ifdef`s are only in `src/platform/` (check: `grep -rn 'ifdef\|Q_OS_' src | grep -v ^src/platform/`).

## What PlatformFactory picks on KDE Wayland

| Interface | Implementation | Fallback |
| --- | --- | --- |
| ICapture | KWin ScreenShot2, each monitor in native pixels | Screenshot portal (`FallbackCapture` switches for good after the first error) |
| IHotkeys | GlobalShortcuts portal (the system assigns the shortcut, ADR 0012) | none, then the CLI `markpaste --capture`, `--capture-to-strip` |
| IClipboard | ext-data-control-v1 | QClipboard (no background writes: mode 1 is disabled) |
| IInputInjector | RemoteDesktop portal | none, mode 2 |

## What PlatformFactory picks on Windows (ADR 0016)

| Interface | Implementation | Fallback |
| --- | --- | --- |
| ICapture | DXGI Desktop Duplication per monitor, on failure GDI BitBlt of that monitor (decided on every shot) | `MARKPASTE_CAPTURE=gdi`, without DXGI. `FallbackCapture` is not used: it would switch for good |
| IHotkeys | `RegisterHotKey`; Print Screen via the `WH_KEYBOARD_LL` hook in its own thread | — |
| IClipboard | QClipboard (OLE) + `PNG` and "not into history" marks; a clipboard busy in another program is retried for up to ~2 s, then an error | — |
| IInputInjector | `SendInput` with UIPI and active window change checks | — |

Files on Windows: settings in the registry `HKCU\Software\markpaste\markpaste`; the set and the log in `%LOCALAPPDATA%\markpaste\markpaste\`; autostart in `HKCU\…\CurrentVersion\Run`.

## Program files on Linux

| What | Where | Mode |
| --- | --- | --- |
| Settings | `~/.config/markpaste/markpaste.conf` (QSettings, INI) | ordinary |
| The set between restarts (NFR-5, ADR 0013) | `~/.local/share/markpaste/session/`: `set.json` + sources `<uuid>.png` | 0700 / 0600 |
| Log (ADR 0014) | `~/.local/share/markpaste/logs/markpaste.log`, 1 MB × 3 | 0700 / 0600 |
| Temporary images for "files + text" and drag and drop | `$XDG_RUNTIME_DIR/markpaste/` | 0700 / 0600 |

## Coordinates and scale

- A frame from `ICapture` is in the monitor's **physical** pixels, `ScreenFrame::scale` = physical / logical.
- The overlay is a window the size of the monitor in logical coordinates. The frame is drawn with `devicePixelRatio = scale`, i.e. 1:1 in physical pixels. The selection is converted to physical pixels rounding **outwards** (floor on the left/top, ceil on the right/bottom): nothing selected is lost.
- In the model everything is in physical pixels of the source, and the crop is kept as a separate rectangle. Marks do not move on crop: they stay in source coordinates.
- The frame width, badge size and pixelation block are multiplied by `Shot::devicePixelRatio`. So a shot from a 200 % monitor looks the same in the chat as one from 100 %.
- The editor draws the shot through a single "source → window" transform, and the marks with the same `paintMarks` function as baking: the screen shows exactly what goes to the chat.
- Arrow and freehand points are integer pixel coordinates of the source; they are drawn through pixel centers (+0.5), and mouse hit-testing is computed in window coordinates after mapping the points through the same transform.

## Mark shapes (ADR 0006)

Frames, arrows and freehand lines are in one list `Shot::marks`, hence the shared A1, A2… numbering. Any shape edit goes through `CaptureSetModel::fitMark`: it fits the mark into the visible area (a frame by intersection, an arrow and freehand by clipping the line), simplifies the freehand line and recomputes `rect`. The same method decides on crop whether a mark is removed, so the count in the "delete N marks?" question and the actual crop do not diverge. The geometry (clipping a segment and a polyline, simplification, distance to a line) is in `core/Geometry`.

A mark's pen (`MarkPen`, ADR 0007) keeps only what the user chose; empty fields are resolved at paint time (`markColor`, `markWidth`) from `RenderStyle`. So changing the accent in settings recolors marks without their own color. The canvas holds the current pen: the panel changes it and the selected mark's pen, and selecting a mark does the opposite, putting its pen into the panel and the canvas.

During a drag the canvas keeps a live copy: `m_live` for frames and blur, `m_livePoints` for line shapes. The model gets one edit when the mouse is released.

## Sequential paste (PasteOrchestrator)

A chain of futures and timers in the main thread. Every delayed step checks the generation counter `m_generation`, so a cancel or an error extinguishes all the tails of the previous run.

0. A shortcut with Alt without Ctrl (Alt+V): right away, in parallel with baking, the input session and, while Alt is held, `sendModifierMask()`; otherwise the chat window takes the Alt release for a single press and opens its menu (docs/problems.md). The session is opened once: `openSession` queues those who asked for it while it is opening.
1. Bake all shots in parallel (`QtConcurrent::blockingMapped` inside `QtConcurrent::run`).
2. Save the clipboard (`snapshot`): text and images only.
3. `injector->begin()`: a RemoteDesktop session by the restore token; the first time, a KDE dialog.
4. If the paste was started by a hotkey, wait for `released` (the portal's Deactivated) or a 1.5 s timeout, plus 50 ms. Otherwise a held Alt turns our Ctrl+V into Ctrl+Alt+V.
5. For each item: write to the clipboard → 80 ms → Ctrl+V → 400 ms.
6. After the last one: one more pause (≥ 400 ms), only then `end()`. Key events reach KWin asynchronously, and closing the session early lost "V released" (docs/problems.md).
7. After 1 s, restore the clipboard. On an error or cancel the clipboard is restored right away.

Cancel ("Stop") ends the paste right away without waiting for the current operation: a clipboard write or opening the session may never finish (the connection to the compositor broke), and while a paste is "running", a new one cannot start. The exception is the key sequence Ctrl↓ V↓ V↑ Ctrl↑ (`m_keysInFlight`): it is waited for, otherwise Ctrl may stay held, and the next step sees the cancel. A data-control write does not hang on a broken connection either: on the `disconnected` signal `DataControlDevice` fails the pending writes.

A paste with keys does not start if a window of the program itself is active (settings, preview, wizard): Ctrl+V would go to it. The editor does not count: `AppController::paste` hides it before pasting; the strip does not take focus.

Mode 2 (files + text) is one entry: `text/uri-list`, `x-special/gnome-copied-files`, text with MIME synonyms, the first image as `image/png`. The files are in `$XDG_RUNTIME_DIR/markpaste/` (0700) and are deleted when the set is cleared and on exit.

## The "Paste set" button in the strip

On KDE the strip is a layer-shell surface with `KeyboardInteractivityNone`: a click on it does not take keyboard focus from the chat window. So the button starts mode 1 rather than only copying, as the spec suggests for Wayland. Without layer-shell (GNOME) the button copies the set in mode 2. On Windows a window with `WS_EX_NOACTIVATE` gives the same (ADR 0016); checked under Wine and on Windows 11 in VirtualBox (pasting into Paint), see platform-matrix.md.

## Strip: where it stands and when it is visible

See [ADR 0004](../decisions/0004-strip-placement-and-visibility.md). In short: Qt does not know the position of a layer-shell surface, so the strip computes it itself from the edge, margins and size (`StripWindow::assumedTopLeft`). The move happens once, when the header is released. Visible only while the set is not empty (if there is a tray).

## Threads

- Main: UI, model, D-Bus calls (replies come from the QtDBus thread through the queue).
- `markpaste-wayland`: our own connection to the compositor and data-control (ADR 0002).
- The `QtConcurrent` pool: baking, PNG, writing clipboard data into a pipe, decoding the portal frame, DXGI/GDI capture on Windows.
- Windows: the `KeyboardHook` thread with its own message loop; it only compares the key code and signals the main thread (ADR 0016).

## First-start wizard (ADR 0009)

`SetupWizard` (ui/) gets the controller's `Platform` and calls the interfaces itself: a shot through `ICapture`, the chain `snapshot → begin → write → sendPaste → end → write(saved)`, the same order as in `PasteOrchestrator`. While the wizard window is visible, the controller does not perform hotkey actions but passes them to `hotkeyActivated`. The actual shortcuts arrive asynchronously (`bindAll`), so the controller passes them to the wizard both when it opens and when the binding is ready.

## Single instance

`QLocalServer` on `$XDG_RUNTIME_DIR/su._525.Markpaste.sock`. A new process **first connects** to the socket and passes its command. Only if nobody answers does it remove the old socket and listen itself. The result of `listen()` cannot be relied on (docs/problems.md).
