# 0017. Windows build: MinGW on Fedora, tests under Wine, a portable ZIP

- Status: accepted (2026-10-20)
- Spec items: section 7 (a fresh Qt on Windows via aqtinstall; vcpkg), section 11 (Setup.exe with auto-update and a portable ZIP), section 12 (building Setup.exe on a clean Windows image), NFR-3 (installer up to 40 MB), NFR-12

## Context

The development machine runs Fedora 44 and there is no Windows nearby. The Windows version must be built and checked without leaving Linux, and the release build later in CI on Windows.

## Decision

- **MinGW-w64 cross-compilation on Fedora.** The packages `mingw64-gcc-c++` 16.2 and `mingw64-qt6-*` 6.11.2 (Qt of the same version as the system one on Fedora). The toolchain is our own `cmake/mingw64-fedora.cmake`, without the `mingw64-cmake` wrapper: full compiler paths, and `QT_CHAINLOAD_TOOLCHAIN_FILE` points to the same file (docs/problems.md, 2026-10-20). Commands are in docs/build.md.
- **googletest** is not packaged for MinGW in Fedora: without a system `GTest` the v1.17.0 sources are fetched with `FetchContent` (with a SHA-256 check). On Linux the system `gtest-devel` is still used.
- **Tests under Wine 11.** `markpaste_core_tests` and `markpaste_ui_tests` run on the `offscreen` module, as on Linux. A separate `markpaste_windows_tests` runs on the real `windows` module: the OLE clipboard and its formats, the hook, `RegisterHotKey`, GDI capture. The end-to-end check of the program is Wine in Xvfb with xfwm4 and xdotool (docs/build.md).
- **Portable ZIP**: `cmake --build build-win --target package` → `Markpaste-<version>-win64-portable.zip`: `markpaste.exe`, Qt and MinGW DLLs, plugins (`qwindows`, `qmodernwindowsstyle`, `qjpeg`, `qico`, `qsvg`, `qsvgicon`, `qschannelbackend`), Qt translations, `qt.conf`. Fedora packages have no `windeployqt`; `file(GET_RUNTIME_DEPENDENCIES)` lowercases DLL names when cross-compiling and does not find `Qt6Core.dll` on a case-sensitive file system, so our own script `cmake/CollectDlls.cmake` walks the dependencies by the import table (`objdump -p`) and strips debug data from the copies.
- **.exe resources**: the icon `data/icons/app.ico` (from `app.svg`, 16–256 px), version info, a manifest (`asInvoker`, Windows 10/11, long paths, UTF-8). The program has no console window (`WIN32_EXECUTABLE`).

## Options considered

- **MSVC + Qt from aqtinstall, as in the spec.** Needs Windows (or a VM with a license): the right path for CI, but not for daily development on Fedora.
- **Qt from MSYS2 under Wine.** The same MinGW, but installing and updating is harder than `dnf install`.
- **Building the Velopack installer now.** First check the program on real Windows; the installer and auto-update (APP-7) are the next step.

## Consequences and limitations

- **Size.** Fedora's Qt is built with ICU (`icudata77.dll` is 32 MB) and pulls in OpenSSL, GLib, fontconfig: the program directory is 81 MB, the ZIP is 35 MB. For NFR-3 (installer up to 40 MB) the release should be built with the official Qt without ICU (Windows 10+ provides ICU itself), which is the spec's path via aqtinstall in CI.
- The MinGW build is for development and checks. The release one (MSVC, SignPath signing, Velopack) is for CI on Windows and is not done yet.
- Wine is not Windows: DXGI Desktop Duplication is not implemented there, there is no UIPI and no Snipping Tool, and without a window manager hidden windows stay on the screen. Results under Wine are listed in platform-matrix.md separately from real Windows.
