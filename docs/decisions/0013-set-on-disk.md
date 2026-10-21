# 0013. The set on disk: "survive a restart", save to a folder, preview

- Status: accepted (2026-10-20)

## Context

NFR-5 (the set survives a restart, the user data directory with mode 0700), APP-6 (saving the set to a folder: PNG and `message.md`), PST-6 (preview of what goes to the chat).

## Decision

**Session (NFR-5)**: `SetStorage::saveSession/loadSession`, in the core, no OS-specific code.
- The checkbox "Keep the set between restarts" on the "System" tab, `session/keep`, **off by default** (the spec says "when the option is on").
- Directory: `QStandardPaths::AppLocalDataLocation/session` → `~/.local/share/markpaste/session`, mode 0700, files 0600.
- Format: `set.json` (version 1: shots in order, size, dpr, crop, marks with all pen fields, blurs) and `<uuid>.png`, the shot source without marks and without blur, the same bytes as in memory (`Shot::sourcePng`).
- A shot's source does not change while the shot is in the set, so the PNG is written once; on edits only `set.json` is rewritten (QSaveFile, atomically), and stale PNGs are deleted. `set.json` is written last: if something fails, the old file refers to PNGs that still exist.
- Writing happens 1 s after the last edit (a timer) and on exit. Loading happens at startup, without undo history.
- When the checkbox is turned off, the directory is deleted right away and on every start.
- A broken `set.json` or a newer version: the set is not loaded, the user sees a message, the file is left alone. A shot without a PNG is skipped, the others load.

**Save to a folder (APP-6)**: the "Save to folder…" item in the strip menu (the gear button became a menu), in the tray menu and in the preview window.
- The user picks a folder, and `Markpaste YYYY-MM-DD HH-mm-ss` is created inside it: new saves do not overwrite old ones.
- `A.png`, `B.png`…: baked shots (marks, blur, crop), always lossless PNG, without downscaling for the chat.
- `message.md`: `![A](A.png)`, one line per shot, and the message text from the template; the "text before/after images" order is as in the paste.
- Baking runs in the thread pool; when it is done, the folder opens in the file manager.

**Preview (PST-6)**: `PreviewDialog`, the "Preview message…" item in the same places.
- The images and text come from `PasteOrchestrator::bakeSequence`, the same code as the paste: JPEG instead of PNG for large shots, downscaling to 4096 px, stitching in mode 4 and the text position are all visible. For each image there is the format, the size in pixels and bytes.
- The "files + text" mode is shown as a sequence: the images and text are the same, only the delivery differs.
- Buttons: "Copy text", "Save to folder…", "Close". There is no paste from the window: it goes to the active window, and the active one would be the preview window itself.

## Options considered

- **Keep baked images in the session**: then the marks could not be edited after a restart. Rejected.
- **Keep the source already blurred** (irreversibly, on disk too). Rejected: after a restart the blur could not be moved or removed; protection comes from the 0700 directory and the option being off by default. If needed, a separate checkbox.
- **QDataStream instead of JSON**: more compact, but not human-readable and fragile when fields change. JSON with a version number is easier to debug.
- **Undo history after a restart**: not saved: the state snapshots take memory, and their value after a restart is small.

## Consequences

- `~/.local/share/markpaste/session` holds the sources **without blur**: what the user covered with blur is visible on disk. The hint next to the checkbox says where the set is kept.
- A set of 26 4K shots takes about 50–100 MB on disk while the option is on and the set is not cleared.
- Restoring after a restart in a live session has not been checked yet; the format and permissions are checked by the `SetStorage.*` tests.
