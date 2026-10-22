# Making a release

Releases are published on GitHub (`github.com/metall1c/markpaste`, [ADR 0024](decisions/0024-github-home-and-releases.md)). Since 0.2.0 GitHub Actions builds them: a pushed `vX.Y.Z` tag gives a draft release with all packages, which is published by hand ([ADR 0025](decisions/0025-release-packages-in-containers.md)).

## Checklist

1. **Version and notes.**
   - `project(Markpaste VERSION X.Y.Z)` in `CMakeLists.txt`.
   - A `<release version="X.Y.Z" date="YYYY-MM-DD">` entry on top of `<releases>` in `data/app.metainfo.xml.in`.
   - A `## [X.Y.Z] — YYYY-MM-DD` section in `CHANGELOG.md` and the link at the bottom: it becomes the release text (`tools/package/release-notes.sh X.Y.Z` prints it).
   - Commit, push to `origin` and `github`, wait for the CI to pass.
2. **Tag:** `git tag -a vX.Y.Z -m "Markpaste X.Y.Z"`, then `git push github vX.Y.Z` and `git push origin vX.Y.Z`.
3. **GitHub builds the release.** The tag starts the CI: the five package targets build and test in containers, then the `release` job checks that the tag matches `CMakeLists.txt` and has a `CHANGELOG.md` section, computes `SHA256SUMS` and creates a **draft** release with all files and the notes. About 10 minutes; watch it with `gh run watch --repo metall1c/markpaste`.
4. **Check the draft** on the Releases page: the files, the text. Optionally install a package from it (step 6).
5. **Publish:** the **Publish release** button or `gh release edit vX.Y.Z --repo metall1c/markpaste --draft=false`. Only a published release is visible to the update check (`/releases/latest` skips drafts and pre-releases).
6. After publishing: update `docs/status.md` and `docs/platform-matrix.md` if anything was checked during the release.

If the tag has to be moved before publishing (a fix after tagging), push the tag again with `-f`: the `release` job replaces the files of the existing draft and keeps its text. It refuses to touch a published release.

## Local build and checks

The same packages can be built and checked on the developer machine (Docker or Podman), as was done for the first release:

```bash
rm -rf dist
tools/package/build-in-container.sh fedora:44      # .rpm, .tar.gz; core and UI tests inside
tools/package/build-in-container.sh ubuntu:24.04   # .deb with Qt 6.4; the same tests
tools/package/build-in-container.sh ubuntu:26.04   # .deb for Ubuntu 26.04
tools/package/build-in-container.sh debian:13      # .deb for Debian 13
tools/package/build-in-container.sh windows        # portable .zip and -setup.exe (MinGW)

# each .deb in a clean container of its own release
docker run --rm -v "$PWD/dist:/d:ro,z" ubuntu:24.04 sh -c 'apt-get update -q && apt-get install -y -q lintian /d/*.ubuntu24.04_amd64.deb && lintian /d/*.ubuntu24.04_amd64.deb'
docker run --rm -v "$PWD/dist:/d:ro,z" fedora:44 sh -c 'dnf install -y -q --disablerepo="*openh264*" rpmlint /d/*.rpm && rpmlint /d/*.rpm'
```

The Windows tests run under Wine from `build-win` (docs/build.md): core, UI and `markpaste_windows_tests`. For 0.2.0: lintian clean for all three .deb, rpmlint 0 errors and 0 warnings; every package installs in a clean container of its release and `markpaste --version` prints the version; the Windows installer installs silently into a clean Wine prefix.

## CI

`.github/workflows/ci.yml`:

- `packages`: runs `build-in-container.sh` for `fedora:44`, `ubuntu:24.04`, `ubuntu:26.04`, `debian:13`, `windows` and `static` (it builds Qt first, ~30 minutes) on every push to `main`, on tags and on pull requests, and keeps the packages as workflow artifacts (90 days).
- `release`: only on a `v*` tag, after `packages`; the draft release as described above. It uses the workflow's own `GITHUB_TOKEN` with `contents: write`, no secrets are needed.

GitHub-hosted runners are free without a minutes limit for public repositories.

## Package names

| File | What |
| --- | --- |
| `markpaste-X.Y.Z-1.fc44.x86_64.rpm` | Fedora 44 |
| `markpaste_X.Y.Z-1.ubuntu24.04_amd64.deb` | Ubuntu 24.04 (Qt 6.4) |
| `markpaste_X.Y.Z-1.ubuntu26.04_amd64.deb` | Ubuntu 26.04 |
| `markpaste_X.Y.Z-1.debian13_amd64.deb` | Debian 13 |
| `markpaste-X.Y.Z-linux-x86_64.tar.gz` | Any distribution: the static build with its own Qt (ADR 0028), since 0.2.2. Until 0.2.1: `markpaste-X.Y.Z-Linux.tar.gz`, the Fedora build that needed Qt 6.11 |
| `Markpaste-X.Y.Z-win64-setup.exe` | Windows 10/11 installer, per-user |
| `Markpaste-X.Y.Z-win64-portable.zip` | Windows 10/11 portable |
| `SHA256SUMS` | Checksums of all files |
