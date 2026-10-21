# CLAUDE.md

## About the project

An open alternative to Snapik for Windows 10/11 and Linux: **Markpaste** (ID `su._525.Markpaste`, executable `markpaste`; the spec uses the working name Pointshot, see `docs/decisions/0005-*.md`). Repository: https://github.com/metall1c/markpaste (public home, releases); the working mirror is https://gitlab.525.su/che/main/markpaste. Scenario: hotkey → area shot → marks A1, A2… with comments → pasting the whole set (images + text) into any chat in one action.

- The full spec: [TZ.md](TZ.md). It is the source of requirements (IDs like CAP-1, EDT-3, PST-1, NFR-2), referenced in code, commits and documentation.
- Stack: C++20, Qt 6 (Widgets + QPainter), CMake + Ninja. On Linux the system Qt, no API newer than Qt 6.4.
- The main Linux platform is **Fedora 44 KDE Plasma on Wayland** (the user's decision, refining the spec: see `docs/decisions/0001-*.md`). Every feature is first built and checked here. Then GNOME 50 and Sway, X11 last. The developer machine is the reference system itself (Fedora 44 KDE, Wayland), so everything is checked in the live session.
- The user works daily with the Linux version (Fedora 44 KDE) and the Windows 11 version; both are release platforms. The core and UI code is platform-independent.
- Architecture: UI → core → 4 platform interfaces (`ICapture`, `IHotkeys`, `IClipboard`, `IInputInjector`) → implementations in `platform/`. Platform code (`#ifdef _WIN32` and the like) only in `platform/`. The core picks its behavior by `capabilities()`, not by the OS name.
- Default license: GPL-3.0-or-later.

## Documenting along the way is mandatory

The spec describes intentions, not what came out. As the implementation goes, record the **actual** state in `docs/`. Documentation is updated in the same task as the code, not "later".

### What to record

1. **Technical decisions**: `docs/decisions/NNNN-short-name.md` (ADR). Every decision someone could have made differently: the choice of an API, library, format, mode, delay value, dropping a spec item. Format:
   - Context: what task and which spec requirements (IDs).
   - Decision: what was actually done.
   - Options considered and why they were rejected.
   - Consequences and limitations.
   - Status: accepted / replaced by decision NNNN.
2. **Deviations from the spec**: if the implementation differs from TZ.md (a hypothesis did not hold, a different API, different numbers), write an ADR and add a row to `docs/deviations.md`: spec item → what was done instead → link to the ADR. TZ.md is not edited without an explicit request.
3. **Tricky parts**: `docs/architecture/` (or a code comment if it concerns one function): non-obvious algorithms, asynchronous chains (portals, the paste state machine), coordinate and DPI conversion, initialization order, threads. Write *why* it is built this way, not a retelling of the code.
4. **Problems and their solutions**: `docs/problems.md`, a log in the format:
   - Date, environment (OS, DE, Wayland/X11, Qt/compositor/portal versions).
   - Symptom and how to reproduce.
   - Cause (if found) and how it was solved or worked around.
   - Links to bug trackers, documentation, commits.
   Unsolved problems are recorded too, with the status "open".
5. **Platform check results**: `docs/platform-matrix.md`: the actual capability matrix (spec section 9) and chat compatibility (section 12): what was checked, where, when and with what result. Especially the spec's hypotheses: the Clipboard portal in GNOME 50, `ext-data-control-v1` in Plasma 6.6, overlay focus on GNOME, paste profile delays, portal capture speed.
6. **Build and developer environment**: `docs/build.md`: the actual dependencies and build commands for each platform, pitfalls.

### Rules

- After each finished task, check: is there a new decision, deviation, tricky part or problem? If so, record it before considering the task done.
- **Documentation and code comments are written in English** (since 2026-10-21, before publishing on GitHub), short and specific: versions, commands, error messages, measured numbers. `README.ru.md` is the Russian copy of `README.md` and is kept in sync with it. UI strings stay English in code with Russian translations in `i18n/`.
- Talk to the user in Russian.
- Do not duplicate: if an entry exists, update it rather than create a new one. Mark outdated content instead of silently deleting it.
- In the report to the user about a finished task, mention which documents were updated.

## Commits and push

- After each significant change (a feature, a bug fix, a notable documentation change), commit and push right away, without waiting for a separate request: to `origin` (`git@gitlab.525.su:che/main/markpaste.git`) and to `github` (`git@github.com:metall1c/markpaste.git`), branch `main`.
- Before committing: the build has no errors, the `markpaste_core_tests` and `markpaste_ui_tests` tests pass, the documentation is updated per the rules above.
- Commit messages are in English (since 2026-10-21, in both repositories): the first line says what was done, below it why, with links to spec requirement IDs and docs/.
- Program version: after each code change (not documentation only), bump the version in `project(Markpaste VERSION …)` in the root `CMakeLists.txt` in the same commit: the last digit by default (0.1.1 → 0.1.2); the middle one for a notable new feature if the user says so. Mention the new version in the commit message.
- Releases: tag `vX.Y.Z` on `main`, push the tag to both remotes, publish a GitHub release with the packages (`docs/release.md`).
