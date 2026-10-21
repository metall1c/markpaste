# 0025. Release packages are built in containers by one script; the release is published by hand

- Status: accepted (2026-10-21)
- Spec items: section 11 (packages, releases from CI with SHA-256 checksums), section 12 (building in a Fedora 44 container, Setup.exe on a clean Windows image), section 7 (Qt 6.4 minimum checked by a CI build on Ubuntu 24.04), NFR-3, NFR-12

## Context

The first public release 0.2.0 needs .rpm, .deb, a Linux archive and the Windows installer and ZIP. Until now the .rpm was built on the developer machine, the .deb was not built at all (it needs Debian/Ubuntu), and the Windows files came from the developer's `build-win`. A package built on the developer machine depends on whatever is installed there, and the Qt 6.4 minimum had never been checked.

## Decision

- **One script, `tools/package/build-in-container.sh <fedora:44|ubuntu:24.04|ubuntu:26.04|debian:13|windows>`**, builds in a clean container from the read-only sources, runs the core and UI tests (Linux), and puts the packages into `dist/`. `windows` is the MinGW cross-build (ADR 0017) in `fedora:44`, with the portable ZIP and the NSIS installer (ADR 0023).
- **One .deb per release: Ubuntu 24.04, Ubuntu 26.04, Debian 13.** Library package names differ between releases, and the Ubuntu 24.04 package does not install on Ubuntu 25.10/26.04 or Debian 12/13 (checked 2026-10-21). The release goes into the Debian revision: `markpaste_0.2.0-1.ubuntu24.04_amd64.deb`. The Ubuntu 24.04 build with its Qt 6.4.2 also checks that no newer Qt API is used; for 0.2.0 it passed on the first try. The Debian 13 build found that LayerShellQt 6.3 has no `Window::setScreen` (fixed, docs/problems.md).
- **CI** (`.github/workflows/ci.yml`, GitHub Actions) runs the same script for all five targets on every push, tag and pull request and keeps the packages as artifacts.
- **The release is published by hand** as a draft (`gh release create --draft`) from packages built and checked locally (lintian, rpmlint, installation in clean containers, Windows tests under Wine), then published after a look at the page. Steps: `docs/release.md`.
- Debian packaging requirements found by lintian are part of the build: a man page (`data/markpaste.1`), a DEP-5 `copyright` file (`data/copyright.in`), a Debian changelog generated from the version (`changelog.Debian.gz` for a package with a Debian revision), no duplicated description synopsis.

## Options considered

- **Publishing the release from CI on a tag** (the spec's "releases are built only in CI"). Postponed: the first release needs a manual check of every file, and a tag-triggered job would create a second release next to the manual one. Can be switched on later by adding a job that uploads the CI artifacts to a draft release.
- **One .deb for all Debian-based systems.** Impossible with system Qt: the dependency names differ (see above). A self-contained package with a bundled Qt would contradict NFR-3 (up to 5 MB) and the spec's "system Qt on Linux". Debian 12 (Qt 6.4, no `t64` names) can be added as one more script target if needed.
- **Building on the developer machine as before.** Packages would depend on the local system state, and the Qt 6.4 check would stay manual.
- **The Windows release on a Windows CI image with MSVC and the official Qt** (spec section 7). Still the plan for size (NFR-3) and signing, but not done yet; the MinGW cross-build is what was checked on Windows 11 (ADR 0016, 0017).

## Consequences

- 0.2.0 sizes: .rpm 0.6 MB, each .deb 0.7 MB (NFR-3: up to 5 MB), the Windows installer 23.8 MB (up to 40 MB), the portable ZIP 35 MB.
- Ubuntu 24.04 has no Qt 6 LayerShellQt: there the strip and the overlay are ordinary windows (as on GNOME), and on KDE the strip's paste button copies the set instead of pasting it.
- The Fedora image needs `--disablerepo="*openh264*"`: a mirror of the Cisco repository failed during the 0.2.0 build, and nothing from it is needed.
- The `.tar.gz` comes from the Fedora build and needs a system Qt 6.11.
