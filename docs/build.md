# Build and developer environment

## Fedora 44 (the main platform)

Checked on 2026-10-19: Fedora 44 KDE, GCC 16.2.1, CMake 4.3.0, Qt 6.11.2.

```bash
sudo dnf install -y --setopt=install_weak_deps=False \
  gcc-c++ cmake ninja-build \
  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtwayland-devel qt6-qttools-devel qt6-linguist \
  qt6-qtsvg-devel \
  layer-shell-qt-devel wayland-devel wayland-protocols-devel libxkbcommon-devel \
  gtest-devel gmock-devel \
  xcb-util-keysyms-devel libXtst-devel        # for the X11 implementation

cmake -S . -B build -G Ninja
cmake --build build
./build/tests/markpaste_core_tests
cmake --build build --target dev-install-desktop   # required, see below
./build/src/app/markpaste
```

`libxkbcommon-devel` is needed for the virtual-keyboard-v1 keymap (Sway, Hyprland). `qt6-qtsvg-devel` is for the UI icons (since 2026-10-20, [ADR 0019](decisions/0019-ui-style.md)); on Debian/Ubuntu it is `libqt6svg6-dev`. `spdlog-devel` and `python3-dbusmock` are installed but not used yet: logs go through `QLoggingCategory`, and there are no portal tests in CI yet.

## A package for one distribution with its system Qt

The release packages are the static build since 0.2.3 (see "The release packages: the static build" below). A package linked against the distribution's own Qt is still built from the same sources, for example for a distribution repository or to get the system style (Breeze) and fcitx input. Such a package runs only with the Qt it was built with (since 0.2.1 the private Qt API ties it to the exact minor version, ADR 0028), so build it on the release it is for, and rebuild it when the distribution updates Qt.

The quickest way is a clean container of that release (Docker or Podman), see "Distribution packages in containers" below:

```bash
tools/package/build-in-container.sh fedora:44      # or ubuntu:24.04, ubuntu:26.04, debian:13
```

On the system itself:

**Fedora** (checked on 2026-10-19 on Fedora 44: CPack 4.3.0, rpm-build 6.0.2); the build dependencies are in "Fedora 44" above:

```bash
sudo dnf install -y rpm-build
cmake -S . -B build-pkg -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
      -DMARKPASTE_BUILD_TESTS=OFF -DMARKPASTE_BUILD_TOOLS=OFF
cmake --build build-pkg --target package
# → build-pkg/markpaste-<version>-1.fc44.x86_64.rpm (and a .tar.gz of the same files)
sudo dnf install ./build-pkg/markpaste-<version>-1.fc44.x86_64.rpm
```

**Debian and Ubuntu** (Debian 13, Ubuntu 26.04; on Ubuntu 24.04 leave out `liblayershellqtinterface-dev`, it is the Qt 5 one there):

```bash
sudo apt install g++ cmake ninja-build pkg-config dpkg-dev file \
  qt6-base-dev qt6-base-private-dev qt6-wayland-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
  qt6-svg-dev libgl-dev libwayland-dev wayland-protocols libxkbcommon-dev liblayershellqtinterface-dev \
  libgtest-dev libgmock-dev libxcb1-dev libxcb-xtest0-dev libxcb-keysyms1-dev libxtst-dev
cmake -S . -B build-pkg -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
      -DMARKPASTE_BUILD_TESTS=OFF -DMARKPASTE_BUILD_TOOLS=OFF -DCPACK_DEBIAN_PACKAGE_RELEASE=1~debian13
cmake --build build-pkg --target package
# → build-pkg/markpaste_<version>-1~debian13_amd64.deb
sudo apt install ./build-pkg/markpaste_<version>-1~debian13_amd64.deb
```

Debian 12 is too old for the system-Qt build: its wayland-protocols 1.31 has no `ext-data-control-v1` (the static image installs 1.47 for that reason). There use the static build.

- **`-DCMAKE_INSTALL_PREFIX=/usr` is required.** The `Exec` path of the .desktop file is absolute and comes from the install prefix: KWin lets a program into ScreenShot2 only if it matches the process executable. With the default prefix (`/usr/local`) the package would install the program into `/usr/local/bin`.
- The package contains `/usr/bin/markpaste`, the `.desktop` file, the SVG icon (`hicolor/scalable`), the AppStream metainfo and LICENSE. Translations are built into the program.
- rpm finds the library dependencies (Qt 6, libwayland-client, LayerShellQt) by itself from the linked `.so` files; only `xdg-desktop-portal` is added by hand. LayerShellQt becomes a required dependency if it was present at build time.
- The package is tied to the Qt version of the build (`libQt6Core.so.6(Qt_6.11)`): build the rpm for Fedora 44 on Fedora 44.
- Check: `rpmlint build-pkg/*.rpm`: 0 errors, one warning (no man page); `desktop-file-validate`, `appstreamcli validate --no-net`: no remarks.
- **.deb** is enabled automatically if `dpkg-shlibdeps` is present (build on Ubuntu/Debian). On Fedora only rpm and tar.gz are built.
- **The new-version check** (ADR 0014, ADR 0024) uses `MARKPASTE_UPDATE_URL` (by default the GitHub API for the latest release of `MARKPASTE_GITHUB_REPO`, `metall1c/markpaste`). Distro builds that update their own way use `-DMARKPASTE_UPDATE_URL=` (empty): no check. The page in the notification is `MARKPASTE_RELEASES_URL`.
- **Old build directories keep the old URLs.** The URL variables are CMake cache entries; after they changed (GitLab → GitHub, 2026-10-21) an existing build directory still has the old ones. Reset them: `cmake -U 'MARKPASTE_*URL' -U MARKPASTE_GITHUB_REPO <build dir>`.
- **For the developer:** `~/.local/share/applications/su._525.Markpaste.desktop` from `dev-install-desktop` wins over the system one from the package (the user directory is searched first) and points to `build/`. To check an installed package, delete this file (and the icon in `~/.local/share/icons/hicolor/scalable/apps/`).

## Distribution packages in containers

`tools/package/build-in-container.sh` builds a system-Qt package for one release in a clean container of it. Up to 0.2.2 these were the release packages; now the CI builds them as a check (the system Qt build and the Qt 6.4 minimum on Ubuntu 24.04) and does not release them (docs/release.md). Docker or Podman is needed.

```bash
tools/package/build-in-container.sh fedora:44      # → dist/: .rpm
tools/package/build-in-container.sh ubuntu:24.04   # → dist/: markpaste_<version>-1.ubuntu24.04_amd64.deb (Qt 6.4.2)
tools/package/build-in-container.sh ubuntu:26.04   # → dist/: …-1.ubuntu26.04_amd64.deb
tools/package/build-in-container.sh debian:13      # → dist/: …-1.debian13_amd64.deb
tools/package/build-in-container.sh windows        # → dist/: the portable .zip and -setup.exe (MinGW in fedora:44)
tools/package/build-in-container.sh static         # → dist/: markpaste-<version>-linux-x86_64.tar.gz (static Qt, see below)
```

- The script mounts the sources read-only, builds in the container's own directory with the same CMake options as above, runs the core and UI tests (Linux targets), and copies the packages to `dist/`. `CONTAINER_ENGINE=docker|podman` picks the engine.
- Building on Ubuntu 24.04 (Qt 6.4.2) also checks that the code uses no Qt API newer than 6.4 (spec section 7). It passed on the first try for 0.2.0.
- **A .deb fits only the release it was built on.** Library package names differ between releases (the 64-bit time_t transition renamed `libqt6core6` to `libqt6core6t64` in Ubuntu 24.04, and later releases changed them again), so the Ubuntu 24.04 package does not install on Ubuntu 25.10/26.04 or Debian 12/13 (checked 2026-10-21). Hence one .deb per release, with the release in the Debian revision (`-1~ubuntu24.04`). The file is named with a dot instead of the tilde (`markpaste_0.2.0-1.ubuntu24.04_amd64.deb`): GitHub Releases replaces `~` in asset names, and the names in `SHA256SUMS` must match the downloads.
- Ubuntu 24.04 has only the Qt 5 LayerShellQt (Plasma 5.27), so the script does not install it there: linking a Qt 5 library into a Qt 6 program would break it. Ubuntu 26.04 and Debian 13 get the Qt 6 one.
- **LayerShellQt older than 6.4** (Debian 13 has 6.3) has no `Window::setScreen`: the build failed with "'class LayerShellQt::Window' has no member named 'setScreen'". CMake now defines `MARKPASTE_LAYERSHELL_SET_SCREEN` only for 6.4+, and older versions set the output through `QWindow::setScreen` (docs/problems.md, 2026-10-21).
- The Fedora image is used with `--disablerepo="*openh264*"`: during the 0.2.0 build a mirror of the Cisco openh264 repository failed ("All mirrors were tried"), and nothing from it is needed.
- Debian packaging details required by lintian: a man page (`data/markpaste.1` → `markpaste.1.gz`), a DEP-5 `copyright` file (`data/copyright.in`), `changelog.Debian.gz` for a package with a Debian revision (`changelog.gz` otherwise), no `CPACK_DEBIAN_PACKAGE_DESCRIPTION` (CPack builds the synopsis from the summary itself). Checks: docs/release.md.

## The release packages: the static build

ADR 0028. The image `tools/static/Dockerfile` (Debian 12) builds a static Qt 6.12 and LayerShellQt into `/opt/qt`; `tools/static/build.sh` builds Markpaste with it, runs the tests and packs one .deb (Debian 12+, Ubuntu 22.04+), one .rpm (Fedora, openSUSE) and the .tar.gz with `install.sh`.

```bash
tools/static/build.sh                         # → dist/: markpaste_<version>-1_amd64.deb, markpaste-<version>-1.x86_64.rpm,
                                              #   markpaste-<version>-linux-x86_64.tar.gz; build/static/markpaste
tools/package/build-in-container.sh static    # the same, as the CI target
docker build -t markpaste-static tools/static # rebuild the image (Qt version: ARG QT_VERSION)
```

- The image is built on first use: ~6 minutes on 16 cores (qtbase is ~2000 steps). The sources come from download.qt.io and download.kde.org; tarballs put into `tools/static/sources/` (ignored by git) are used instead, with the same SHA-256 checks. From here download.qt.io did not answer on 2026-10-22, a mirror worked: `https://mirrors.ocf.berkeley.edu/qt/official_releases/qt/` (`--build-arg QT_MIRROR=…`).
- CMake finds a static Qt by the type of `Qt6::Core` (`MARKPASTE_STATIC_QT`). Then `markpaste_import_qt_plugins()` adds the plugins a static Qt does not link by default (wayland, offscreen, ibus, compose, the portal theme), the static LayerShellQt gets `Wayland::Client` and `Qt6::WaylandClientPrivate`, and `qtbase_ru.qm` goes into the resources.
- Pitfalls on Debian 12 (docs/problems.md, 2026-10-22): `INPUT_opengl=no`, not `FEATURE_opengl=OFF`; CMake 3.30+ for ECM; wayland-protocols 1.47 for `ext-data-control-v1`; the `wl_fixes` header clash; libdbus must be linked.
- The xcb extensions and xcb-util are linked from their `.a` files: the image deletes the `libxcb-*.so` development links (not `libxcb.so`). Check the result: `objdump -p build/static/markpaste | grep NEEDED`.
- A debug build for gdb: the same `cmake` in the image with `-DCMAKE_BUILD_TYPE=RelWithDebInfo` (`build.sh` strips the binary).

## Checking under Sway without a separate system

```bash
sudo dnf install -y --setopt=install_weak_deps=False sway   # installed 2026-10-19, sway 1.11
printf 'output HEADLESS-1 resolution 1280x800\n' > /tmp/sway-headless.conf
WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WAYLAND_DISPLAY= sway -c /tmp/sway-headless.conf &
# the new socket is the next free one, usually wayland-1
WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ./build/tools/pastetest/markpaste-pastetest --check
WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ./build/tools/pastetest/markpaste-pastetest --self-test
```

The portals here stay KDE's (the shared session bus), so only Wayland protocols are checked this way: data-control, virtual-keyboard, layer-shell.

- `pastetest --portal-clipboard` checks the Clipboard portal: it writes through the portal and reads back through data-control and through our own `QClipboard` (step 5b, a paste into our own window), then shows what is left in the clipboard after the session closes. The first time KDE asks for permission.
- `pastetest --window-menu` opens a frameless window; a right click sends `xdg_toplevel.show_window_menu` (the GNOME "Always on Top" path of the strip, ADR 0026).
- `pastetest --gnome-keys Print [--gnome-action release|restore]` shows which GNOME setting holds the key, frees it or gives it back. Without GNOME: a local schema copy with `GSETTINGS_SCHEMA_DIR`, `GSETTINGS_BACKEND=keyfile`, `XDG_CONFIG_HOME` and `XDG_CURRENT_DESKTOP=GNOME`.
- Variables for checking the fallback paths on KDE: `MARKPASTE_CLIPBOARD=portal` (clipboard through the portal), `MARKPASTE_INPUT=portal` (Ctrl+V only through RemoteDesktop).

## Checking X11 without an X11 session

```bash
Xvfb :97 -screen 0 1280x800x24 -nolisten tcp &
export DISPLAY=:97 QT_QPA_PLATFORM=xcb WAYLAND_DISPLAY= XDG_SESSION_TYPE=x11
./build/tools/pastetest/markpaste-pastetest --x11-hotkeys   # presses via xdotool
./build/tools/pastetest/markpaste-pastetest --self-test
```

- Needs `xorg-x11-server-Xvfb`, `xdotool`, and for the build `libxcb-devel xcb-util-keysyms-devel` (xcb-xtest is part of libxcb).
- To run the program itself next to a running copy, it needs its own `XDG_RUNTIME_DIR` (the single-instance socket) and `XDG_CONFIG_HOME`. The socket path must be no longer than 108 bytes (`sun_path`), otherwise "the socket cannot be created": a directory in `/run/user/$UID/` works, a long path in a temporary directory does not.
- Do not use a group named `general`/`General` in `QSettings`: in INI it is written as `[%General]` (`[General]` is the root), and on the next read of the file the key gets the name `General/…`, with a capital letter. The key `general/x` is not found after a restart (docs/problems.md, 2026-10-20). The "already shown" flags are now in the `once` group.
- Stop test servers by PID (`pgrep -x Xvfb`), not `pkill -f`: the pattern matches the shell's own command line.

## Windows: cross-compiling on Fedora and checking under Wine

Checked on 2026-10-20: Fedora 44, MinGW-w64 GCC 16.2.1, Qt 6.11.2 (`mingw64-qt6-*`), Wine 11.0 Staging. Why this way: [ADR 0017](decisions/0017-windows-cross-build.md).

```bash
sudo dnf install -y mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-qt6-qttools mingw64-qt6-qtsvg \
  mingw64-qt6-qtimageformats mingw64-qt6-qttranslations wine \
  mingw32-nsis                               # makensis, for the installer
cmake -G Ninja -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-fedora.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-win
cmake --build build-win --target package   # → build-win/package/Markpaste-<version>-win64-portable.zip
cmake --build build-win --target installer # → build-win/package/Markpaste-<version>-win64-setup.exe (ADR 0023)
```

Tests under Wine (Qt DLLs are taken from the MinGW sysroot):

```bash
export WINEPATH='Z:\usr\x86_64-w64-mingw32\sys-root\mingw\bin' WINEDEBUG=-all DXVK_LOG_LEVEL=none
QT_QPA_PLATFORM=offscreen wine build-win/tests/markpaste_core_tests.exe
QT_QPA_PLATFORM=offscreen wine build-win/tests/markpaste_ui_tests.exe
QT_PLUGIN_PATH='Z:\usr\x86_64-w64-mingw32\sys-root\mingw\lib\qt6\plugins' wine build-win/tests/markpaste_windows_tests.exe
```

The program itself runs from the install directory, which is self-contained (like the ZIP): `cmake --install build-win --prefix build-win/stage --strip`, then `wine build-win/stage/markpaste.exe`. Settings are in the Wine registry (`HKCU\Software\markpaste\markpaste`), the log is in `~/.wine/drive_c/users/$USER/AppData/Local/markpaste/markpaste/logs/`.

An end-to-end check without touching the desktop: Wine in Xvfb with its own prefix and a window manager:

```bash
Xvfb :99 -screen 0 1600x900x24 &
export DISPLAY=:99 WAYLAND_DISPLAY= WINEPREFIX=/path/to/prefix MARKPASTE_CAPTURE=gdi
wineboot -i; xfwm4 --compositor=off &
wine notepad & wine build-win/stage/markpaste.exe &
xdotool mousemove 400 300 click 1; xdotool key Print        # then xdotool and import -window root
```

- **The toolchain is `cmake/mingw64-fedora.cmake`, not the `mingw64-cmake` wrapper.** The wrapper sets the compilers without a path through the environment, and on a cmake re-run from ninja or in the googletest subproject they were "not found" (docs/problems.md, 2026-10-20).
- **googletest** is not packaged for MinGW: it is downloaded with `FetchContent` (network is needed on the first configure).
- **`gtest_discover_tests`** cannot run the `.exe` after the build when cross-compiling: `PRE_TEST` mode; we run the tests ourselves through Wine.
- **Wine ≠ Windows.** DXGI Desktop Duplication is not implemented in Wine (`DuplicateOutput: 0x887a0004`), so capture goes through GDI. In Xvfb Wine's dxgi.dll crashes with a division by zero; use `MARKPASTE_CAPTURE=gdi` there. Without a window manager hidden windows stay on the screen (the picture does not update), so run xfwm4. Wine sees hotkeys (`RegisterHotKey`, the hook) only while a Wine window has focus.
- **The ZIP is 35 MB** (the directory is 81 MB): Fedora's Qt pulls in ICU (32 MB), OpenSSL, GLib. For a size-optimized release, the official Qt (ADR 0017).
- **The installer (NSIS 3.11, `mingw32-nsis`).** The `installer` target installs the program into a temporary `build-win/installer/stage`, builds the uninstall file list from it and calls `makensis`. Without `makensis` there is no target (a message at cmake configure time). A check under Wine in a clean prefix: `WINEPREFIX=… wine Markpaste-…-setup.exe /S`, uninstall with `uninstall.exe /S` from `%LOCALAPPDATA%\Programs\Markpaste`.
- **`markpaste.exe --version` and `--help` on Windows show a message box** (the program has no console) and wait for a click: do not call them in check scripts.

### Testing in VirtualBox

Checked on 2026-10-20: VirtualBox 7.2, a Windows 11 24H2 guest with Guest Additions 7.2.18, the user logged in on the console.

```bash
export MP_VM=Win11-test MP_VM_USER=admin MP_VM_PASS=…
tools/dev/winvm/vm.sh setup                                   # helpers to C:\mp
tools/dev/winvm/vm.sh deploy build-win/package/Markpaste-<version>-win64-portable.zip
tools/dev/winvm/vm.sh start                                   # in the user's session
tools/dev/winvm/vm.sh key e0 2a e0 37 e0 b7 e0 aa             # Print Screen
tools/dev/winvm/vm.sh drag 300 250 800 500; tools/dev/winvm/vm.sh shot /tmp/vm.png; tools/dev/winvm/vm.sh log
tools/dev/winvm/vm.sh tests                                   # three test suites on real Windows, ~10 s
```

- A process from `guestcontrol run` lives outside the user's session: its windows are not visible. Programs are started by a Task Scheduler task with `LogonType Interactive` (`run.ps1`).
- The arguments after `--` of `guestcontrol run` go to the program as a whole: the program name is not repeated (`-- /c dir`, not `-- cmd /c dir`).
- `guestcontrol` has a filtered administrator token (UAC): elevated actions (firewall, HKLM) fail.
- The mouse is `SendInput` from `input.ps1` via `conhost --headless`: an ordinary console window takes focus, and the paste honestly stops with "the window changed".
- `keyboardputscancode` keys are a real keyboard for Windows: this is how the Print Screen hook is checked. They go to the active window: before pressing, check with a screenshot that nobody is using the guest.
- `deploy` first stops `markpaste.exe`: the directory of a running program is not deleted, and the new build used to land silently inside the old one (problems.md, 2026-10-20).
- `tests` puts the test `.exe` files, `Qt6Test.dll` and `qoffscreen.dll` into the program directory. The core and window tests enable the `offscreen` platform themselves; without `qoffscreen.dll` (not in the ZIP) Qt fails at startup, and the process hangs forever on an invisible error window without a single line of output.
- The VirtualBox shared clipboard (`clipboard=bidirectional`): VBoxTray opens the guest's clipboard after every change, like `rdpclip` in RDP. The clipboard tests wait while processing events (`QTest::qWaitFor`) rather than with a blocking `QFuture::result()`: our thread serves the data to VBoxTray.
- `shot` does not work while the guest's screen is off: press any key first (`key 2a aa` is Shift).
- On the login screen the default layout may be Russian: press Win+Space before the password (`key e0 5b 39 b9 e0 db`).
- The first start of a new build and the first paste after it are slow (Defender checks the DLLs and Qt modules on first load): the first paste took 15–40 s, the next one about 3 s.

## What you get

| File | What it is |
| --- | --- |
| `build/src/app/markpaste` | The program |
| `build/tests/markpaste_core_tests` | Core tests (GoogleTest): model, text, baking, paste orchestrator, the set on disk and saving to a folder |
| `build/tests/markpaste_ui_tests` | Window tests (GoogleTest + QtTest, offscreen): editor keys (Ctrl+Enter, Esc), drawing arrows and freehand with the mouse, the pen panel, autostart, the first-start wizard on platform stubs (`tests/FakePlatform.h`), the message preview, loading the Russian translation. `app/LogFile.cpp` and `app/UpdateChecker.cpp` are built in here too (log rotation, version comparison). `QTest::keyClicks` cannot type Cyrillic, so text is entered with `insertPlainText` |
| `build/tools/pastetest/markpaste-pastetest` | Paste prototype and platform diagnostics (`--check`, `--self-test`, `--capture kwin\|portal`, `--mode 1\|2`) |
| `tools/dev/kwin-probe.py "command" [filter]` | Sends a command to the running program and reads the geometry of its windows from KWin every 250 ms. For measurements in a live KDE session. Debug socket commands: `debug-move-strip <screen> <left\|right> <offset>`, `debug-ghost <x> <y> <w> <h>` |
| `build/tools/bench/markpaste-bench [--shots 26] [--paste 5]` | NFR-1 and NFR-2 measurements without a screen: memory of a set of 4K shots, the paste peak and time, memory return (docs/platform-matrix.md, "Measurements") |
| `build-win/tests/markpaste_windows_tests.exe` | The Windows platform implementations on the real Qt `windows` module (under Wine or on Windows): shortcut parsing, clipboard formats (PNG, CF_DIB, CF_HDROP, "not into history" marks), another program's CF_DIB → PNG, `RegisterHotKey` and a taken shortcut, the Print Screen hook, GDI capture |
| `build/tools/uipreview/markpaste-uipreview [--dark] [--lang en\|ru] DIR` | Renders the strip (with a set and empty), the editor on a test set (frames, arrow, freehand with different pens, blur; separately with an open comment field), the color and width panels, the first-start wizard, the About window, the preview window, the strip menu, the settings tabs and a shot with captions on the image without a display, and saves PNGs to DIR. `--dark` is the Breeze Dark palette, `--lang` the UI and demo content language (Russian by default). The README screenshots in `docs/images/` are made with it |
| `build/tools/uipreview/markpaste-uipreview --live [SEC [FILE.png]]` | Shows the strip (layer-shell) and the shot window in a real session for SEC seconds; with FILE.png it captures the whole screen via spectacle (KDE) after 2.5 s, other windows included. Does not touch a running program |
| `build/tools/uipreview/markpaste-uipreview --live-dialogs 0 PREFIX` | The settings, wizard and preview windows one after another in a real session; each is captured separately (`spectacle -a`, active window only) into PREFIX-1…3.png |
| `MARKPASTE_RENDER_SAMPLE=file.png build/tests/markpaste_core_tests --gtest_filter='*Sample*'` | Bakes a sample of all mark shapes into a PNG, for a visual check of the renderer |

## Pitfalls

- **The .desktop file is needed already during development.** Without it the portal answers "App info not found" to the app id registration, and KWin does not let the program into ScreenShot2. The `dev-install-desktop` target installs `~/.local/share/applications/su._525.Markpaste.desktop` with `Exec` pointing to the binary in `build/` and the icon `~/.local/share/icons/hicolor/scalable/apps/su._525.Markpaste.svg` (before 2026-10-20 only the package installed the icon, and the developer shortcut had none) and refreshes the KDE cache (`kbuildsycoca6`). The binary from `Exec` must exist, otherwise GLib drops the file (docs/problems.md).
- **Logs.** When stderr is not a terminal, Qt writes to journald. For development:
  `QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="markpaste.*=true" ./build/src/app/markpaste`.
  Without redirection the log is in `journalctl --user -f`.
- **Controlling a running instance:** `markpaste --capture | --capture-to-strip | --paste | --copy | --show | --settings | --setup | --about | --clear | --quit`. `--capture-to-strip` is a shot straight to the strip, without the editor (ADR 0022).
- **`pkill -f build/src/app/markpaste` kills the shell itself** if the path is in its command line. Use `pkill -x markpaste`.
- **Translations:** strings are updated with `cmake --build build --target update_translations` (lupdate), the files are `i18n/*.ts`. The `.qm` files are embedded into the program under `:/i18n`. Attach each `.ts` via `qt_add_translations` to one target only, otherwise parallel lupdate runs corrupt the file (docs/problems.md); tests use `qt_add_lrelease`.
- **Private Qt Gui headers** (`Qt6::GuiPrivate`, packages `qt6-qtbase-private-devel` / `qt6-base-private-dev`) are used for one call: Qt's `xdg_toplevel` for the GNOME window menu (ADR 0026). Without them the build goes on without the window menu.
- **No Qt API newer than 6.4.** Fedora has 6.11, so the compiler will not catch newer calls. The protection is the Ubuntu 24.04 (Qt 6.4.2) build in CI and in the release containers. Only what exists in 6.4 is used: `QFuture::then/unwrap`, `QPromise`, `qt_add_translations`.
