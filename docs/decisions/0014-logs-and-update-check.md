# 0014. Log file and new version notification

- Status: accepted (2026-10-20); the GitLab part is replaced by decision [0024](0024-github-home-and-releases.md) (2026-10-21)

## Context

Spec section 7: "Logs: spdlog with rotation, without shot contents". APP-7: on Linux, a notification about a new version, updating through the package manager. NFR-4: the only network request is the version check against GitHub Releases, which can be turned off in settings.

## Decision

**Log**: `LogFile`, a `qInstallMessageHandler` handler.
- Writes every Qt logging message (categories `markpaste.*`) to `~/.local/share/markpaste/logs/markpaste.log` (`AppLocalDataLocation/logs`), directory 0700, file 0600. The previous handler (journald or stderr) keeps working.
- Size-based rotation: 1 MB, 3 old files (`markpaste.1.log` … `markpaste.3.log`); checked at startup and before every write. Flushed to disk after every line: the log is needed exactly when the program crashes.
- Installed only in the main instance: a second instance lives for a moment and only passes a command.
- An "Open log folder" button on the "System" tab.

**Version check**: `UpdateChecker`.
- Once a day (the time of the last check is in the `updates/lastCheck` setting), the first one 30 s after startup. One GET, a 15 s timeout.
- ~~The source is **GitLab**, not GitHub: the project lives on gitlab.525.su. `GET /api/v4/projects/che%2Fmain%2Fmarkpaste/releases/permalink/latest` → `tag_name` (`v0.2.0` or `0.2.0`), compared with `QVersionNumber`. An `upcoming_release` does not count.~~ Since 2026-10-21 the source is GitHub Releases (`/repos/<owner>/<name>/releases/latest`), see [ADR 0024](0024-github-home-and-releases.md). The GitLab answer is still understood.
- The URL is set at build time: `-DMARKPASTE_UPDATE_URL=…`; empty means no check (for distro builds). The releases page is `MARKPASTE_RELEASES_URL`.
- A new version gives the notification "Markpaste X is out. Update it through your package manager"; a click on the tray notification opens the release page. Each version is reported once (`updates/notifiedVersion`).
- Network errors and 404 go only to the log and are not shown to the user.
- The checkbox "Check once a day whether a new version is out" on the "System" tab, on by default (the spec says "can be turned off in settings").

## Options considered

- **spdlog** (as in the spec). Rejected: a new dependency just for writing a file, while all the program's messages already go through Qt logging with categories. The Qt handler is 100 lines and also catches Qt's own messages.
- **`StateLocation` for the log directory** (`~/.local/state`, more correct per XDG). Rejected: it appeared in Qt 6.7, and the minimum is 6.4.
- ~~**GitHub Releases.** There is no GitHub repository.~~ Chosen on 2026-10-21 (ADR 0024).

## Consequences

- Deviations from the spec: the Qt handler instead of spdlog (`docs/deviations.md`).
- ~~**The check does not work at the moment:** the project on gitlab.525.su is private, the API answers `404 Project Not Found` without a login (checked 2026-10-20).~~ Outdated: since 0.2.0 the check goes to the public GitHub repository and starts working with the first published release.
- The log contains events (capture, paste, portals), but not the contents of shots and comments: the code never writes comments to the log.
