# 0023. Windows installer with NSIS, per-user install

- Status: accepted (2026-10-20); temporary, until Velopack with auto-update
- Spec items: section 11 (Setup.exe with auto-update and a portable ZIP; per-user install without administrator rights), NFR-3 (installer up to 40 MB), section 12 (building Setup.exe)

## Context

The user asked for a Windows installer. The spec names Velopack: Setup.exe plus auto-update from releases. Velopack is the .NET tool `vpk` and an update library that has to be built into the program and tied to the releases. That is separate work. The installer is needed now, and it must be built in the same place as the Windows version, by cross-compiling on Fedora (ADR 0017).

## Decision

- **NSIS 3.11** from Fedora packages (`mingw32-nsis`, provides `/usr/bin/makensis`). The `installer` target in `build-win`: `cmake --install` into a temporary directory → the uninstall file list → `makensis` → `build-win/package/Markpaste-<version>-win64-setup.exe`. The script is `data/windows/installer.nsi.in`, the build is `cmake/WindowsInstaller.cmake`, `cmake/NsisFileList.cmake`. Without `makensis` there is no target, and the rest of the build does not break.
- **Our own `.nsi`, not CPack's NSIS generator.** The CPack template installs the program for all users and asks for administrator rights, while the spec requires a per-user install.
- **Installation:**
  - into `%LOCALAPPDATA%\Programs\Markpaste`, the folder can be changed; `RequestExecutionLevel user`, no UAC;
  - a Start menu shortcut;
  - the "Apps" entry in `HKCU\…\Uninstall\su._525.Markpaste`: version, size, silent uninstall `/S`;
  - "Run Markpaste" on the last page;
  - the wizard language follows the Windows language (Russian or English);
  - the GPL license is shown without a mandatory "I accept".
- **A running program.** Before copying files and before uninstalling, the installer calls `markpaste.exe --quit` from the install folder: the same single-instance socket as the CLI. Otherwise the files are locked.
- **Uninstall:**
  - only the installed files are deleted, from a list built at build time. `RMDir /r` on a folder the user may have chosen is dangerous;
  - autostart is removed: the `su._525.Markpaste` value in `HKCU\…\Run` and the mark in `StartupApproved\Run`;
  - settings (`HKCU\Software\markpaste`) and the log stay.
- **Silent install:** `Markpaste-…-setup.exe /S`, uninstall `uninstall.exe /S`.

## Options considered

- **Velopack now.** Needs the .NET SDK, building a Windows package from Linux and building updates into the program. That is the next step together with APP-7; the version check already exists (ADR 0014).
- **CPack's NSIS generator.** An all-users install with administrator rights contradicts the spec. Changing that would mean rewriting its template.
- **Inno Setup.** Windows only: on Fedora through Wine, with Inno Setup itself downloaded. NSIS is in the Fedora packages.
- **MSI (wixl from msitools).** A per-user install is awkward in MSI, and there is no own wizard with languages.

## Consequences

- **The size** of Setup.exe 0.1.4 is 23.8 MB (LZMA solid), below the 40 MB of NFR-3. The program directory is 80.6 MB, of which ICU is 32 MB (Fedora's Qt, ADR 0017).
- **No auto-update**: a new version is installed over the old one, and a running copy closes by itself. If a file no longer exists in the new version, the old one stays in the folder until the program is uninstalled.
- **The installer is not signed**: SmartScreen shows "Unknown publisher". SignPath signing comes together with the release build in CI (spec section 11).
- **Checked** only under Wine 11.0 (2026-10-20): silent and normal install, install over a running program, uninstall (platform-matrix.md). Not checked on real Windows 11.
