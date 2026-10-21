# 0011. X11: global hotkeys via XGrabKey, Ctrl+V via XTEST

- Status: accepted (2026-10-19)
- Spec items: section 9 (X11: XGrabKey, XTest, MIT-SHM), CAP-1, PST-1; item 6 of the Linux version plan. X11 is lowest priority (ADR 0001).

## Decision

- **Hotkeys: `X11Hotkeys`** (`src/platform/linux/x11/`): `xcb_grab_key` on the root window through Qt's connection (`QNativeInterface::QX11Application`), events through a Qt native event filter. A shortcut is grabbed in 4 variants: without lock modifiers, with CapsLock, with NumLock and with both; otherwise the key is silent with NumLock on.
- **A taken shortcut.** `xcb_grab_key_checked` + `xcb_request_check`: BadAccess means someone else has grabbed the shortcut, giving a `BindResult` with the error "… is already used by another program" (spec section 14: report it and suggest another one). The wizard and the log show it.
- **Autorepeat.** While a key is held, X11 sends "released/pressed" pairs. A release during which the key is still down according to `QueryKeymap` counts as autorepeat; a repeated "pressed" while already pressed too. Result: one `activated` and one `released` per hold.
- **Ctrl+V: `X11InputInjector`**: `xcb_test_fake_input`, Ctrl↓ V↓ V↑ Ctrl↑ with 20 ms pauses, as on Wayland. The V code is the key that has `v` in the layout (`xcb_key_symbols_get_keycode`), otherwise 55 (evdev KEY_V + 8). There is no session: `begin/end` are empty.
- **Waiting for modifier release** is real on X11: `waitModifiersReleased` polls `QueryKeymap` every 20 ms until all modifiers except CapsLock and NumLock (Lock and Mod2) are released.
- **Clipboard**: `QtClipboard(true)`: on X11 the selection owner may be in the background. The image is also set via `setImageData`, so that Qt offers it in other formats.
- **Capture**: `QScreen::grabWindow` (Qt). MIT-SHM from the spec was not done: on Xvfb 1280×800 a shot takes 5 ms; 4K measurements when there is a real X11 session.
- **Shortcut display**: "Ctrl+Alt+V", not the portable "Primary+Alt+V".
- Build: `xcb`, `xcb-xtest`, `xcb-keysyms` via pkg-config, optional (`MARKPASTE_HAVE_X11`). Under XWayland the X11 implementations are not enabled: global hotkeys do not work there (spec section 14).

## Checks (Xvfb, no window manager, 2026-10-19)

- `pastetest --x11-hotkeys` (presses via xdotool/XTEST): Print and Ctrl+Alt+V give `activated`/`released`, V and Ctrl+V without Alt do not fire, holding Print for 0.9 s gives one activation. PASS.
- A second process grabbing the same shortcuts gets "already used by another program".
- `pastetest --self-test` (XTEST + QClipboard): 3 images + text, PASS 3/3, in the same process and with the receiver in a separate process.
- The program itself on Xvfb: the factory chose XGrabKey and XTEST; Print starts a capture; the wizard: "Check capture" ✓ (1 screen 1280×800, 5 ms) and "Check paste" ✓.

## Consequences

- A real X11 session (KDE/Xfce on X11) with a window manager was not checked: Xvfb has no WM, no hotkey grabs by the desktop (KDE on X11 holds Print itself), no Russian layout.
- `canDetectForegroundChange` = false: a change of the active window during a paste (`_NET_ACTIVE_WINDOW`) is not tracked yet.
