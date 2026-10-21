# 0024. GitHub as the public home: project page, releases, update check

- Status: accepted (2026-10-21). Replaces the GitLab part of [0014](0014-logs-and-update-check.md).
- Spec items: NFR-4, APP-7 (update check against GitHub Releases), section 11 (distribution).

## Context

The project was developed in a private GitLab (gitlab.525.su). Its API answers `404 Project Not Found` without a login, so the update check never worked, and the About window, packages and AppStream data pointed to a page nobody outside could open. The user decided to publish the project on GitHub as `github.com/metall1c/markpaste` and to make GitHub the public home. The first public release is 0.2.0.

## Decision

- **One CMake variable for the repository:** `MARKPASTE_GITHUB_REPO` (default `metall1c/markpaste`). The project page (`MARKPASTE_HOMEPAGE_URL`), the releases page (`MARKPASTE_RELEASES_URL`) and the update URL (`MARKPASTE_UPDATE_URL`) are derived from it. The page is used in the About window, the CPack packages, the NSIS installer and the AppStream metainfo (`homepage` and `bugtracker` = `/issues`).
- **Update check — GitHub API:** `GET https://api.github.com/repos/<owner>/<name>/releases/latest` with `Accept: application/vnd.github+json` and the `Markpaste/<version>` User-Agent (GitHub rejects requests without one). `/releases/latest` never returns drafts or pre-releases; `draft` and `prerelease` flags are checked anyway. The release page comes from `html_url`; the GitLab answer (`_links.self`) is still understood, so a build may point `MARKPASTE_UPDATE_URL` elsewhere. Only `https` pages are opened.
- **Contact:** author e-mail `metallicvrn@gmail.com` in the About window and in the package contact (`CPACK_PACKAGE_CONTACT`), at the user's request.
- **GitLab stays the working remote** (`origin`); GitHub is added as a second remote, and `main` and release tags are pushed to both.

## Options considered

- **Keep GitLab, open the project there.** Rejected by the user: GitHub is where people look for such tools, and the spec already names GitHub Releases.
- **Hard-code the URLs.** Rejected: three addresses to keep in sync, and a fork could not point the update check to itself.

## Consequences

- Existing build directories keep the old GitLab URLs in `CMakeCache.txt`: reconfigure with `cmake -U 'MARKPASTE_*URL' -U MARKPASTE_GITHUB_REPO <build dir>` (or delete the cache).
- Unauthenticated GitHub API requests are limited to 60 per hour per IP; one request a day per user is far below that.
- The application ID `su._525.Markpaste` does not change: portal permissions depend on it (ADR 0005).
