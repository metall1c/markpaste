# Making a release

Releases are published on GitHub (`github.com/metall1c/markpaste`, [ADR 0024](decisions/0024-github-home-and-releases.md)); the packages are built in containers by one script ([ADR 0025](decisions/0025-release-packages-in-containers.md)). Checked for 0.2.0 on 2026-10-21.

## Checklist

1. **Version and notes.**
   - `project(Markpaste VERSION X.Y.Z)` in `CMakeLists.txt`.
   - A `<release version="X.Y.Z" date="YYYY-MM-DD">` entry on top of `<releases>` in `data/app.metainfo.xml.in`.
   - A `## [X.Y.Z]` section in `CHANGELOG.md` and the link at the bottom.
   - Commit, push to `origin` and `github`, wait for the CI to pass.
2. **Packages** (Docker or Podman; the sources are mounted read-only):

   ```bash
   rm -rf dist
   tools/package/build-in-container.sh fedora:44      # .rpm, .tar.gz; core and UI tests inside
   tools/package/build-in-container.sh ubuntu:24.04   # .deb with Qt 6.4; the same tests
   tools/package/build-in-container.sh ubuntu:26.04   # .deb for Ubuntu 26.04
   tools/package/build-in-container.sh debian:13      # .deb for Debian 13
   tools/package/build-in-container.sh windows        # portable .zip and -setup.exe (MinGW)
   ```

   The Windows tests run under Wine from `build-win` (docs/build.md): core, UI and `markpaste_windows_tests`.
3. **Package checks.**

   ```bash
   # each .deb in a clean container of its own release
   docker run --rm -v "$PWD/dist:/d:ro,z" ubuntu:24.04 sh -c 'apt-get update -q && apt-get install -y -q lintian /d/*~ubuntu24.04_amd64.deb && lintian /d/*~ubuntu24.04_amd64.deb'
   docker run --rm -v "$PWD/dist:/d:ro,z" fedora:44 sh -c 'dnf install -y -q --disablerepo="*openh264*" rpmlint /d/*.rpm && rpmlint /d/*.rpm'
   ```

   For 0.2.0: lintian clean for all three .deb, rpmlint 0 errors and 0 warnings; every package installs in a clean container of its release and `markpaste --version` prints the version; the Windows installer installs silently into a clean Wine prefix.
4. **Checksums:** `(cd dist && sha256sum * > SHA256SUMS)`.
5. **Tag:** `git tag -a vX.Y.Z -m "Markpaste X.Y.Z"`, `git push origin vX.Y.Z`, `git push github vX.Y.Z`.
6. **A draft release** with the notes from `CHANGELOG.md`:

   ```bash
   gh release create vX.Y.Z --repo metall1c/markpaste --draft --title "Markpaste X.Y.Z" \
       --notes-file <notes.md> dist/*
   ```

   Check the draft on GitHub and press **Publish release**. Only a published release is visible to the update check (`/releases/latest` skips drafts and pre-releases).
7. After publishing: update `docs/status.md` and `docs/platform-matrix.md` if anything was checked during the release.

## CI

`.github/workflows/ci.yml` runs the same script for `fedora:44`, `ubuntu:24.04`, `ubuntu:26.04`, `debian:13` and `windows` on every push to `main`, on tags and on pull requests, and uploads the packages as workflow artifacts. The release itself is published by hand from locally built and checked packages (step 6).

## Package names

| File | What |
| --- | --- |
| `markpaste-X.Y.Z-1.fc44.x86_64.rpm` | Fedora 44 |
| `markpaste_X.Y.Z-1~ubuntu24.04_amd64.deb` | Ubuntu 24.04 (Qt 6.4) |
| `markpaste_X.Y.Z-1~ubuntu26.04_amd64.deb` | Ubuntu 26.04 |
| `markpaste_X.Y.Z-1~debian13_amd64.deb` | Debian 13 |
| `markpaste-X.Y.Z-Linux.tar.gz` | The Fedora 44 build as an archive (needs Qt 6.11) |
| `Markpaste-X.Y.Z-win64-setup.exe` | Windows 10/11 installer, per-user |
| `Markpaste-X.Y.Z-win64-portable.zip` | Windows 10/11 portable |
| `SHA256SUMS` | Checksums of all files |
