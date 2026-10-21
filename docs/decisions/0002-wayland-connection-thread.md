# 0002. Our own Wayland connection for data-control lives in a separate thread

- Status: accepted (2026-10-19)
- Spec items: PST-1, PST-8, section 7 ("Protocols … through a separate libwayland-client connection"), section 8 "Threads"

## Context

The spec suggests serving the libwayland-client connection for data-control with a `QSocketNotifier` in the main thread. On the prototype this led to a deadlock.

When our own program pastes from a clipboard owned by our data-control source (the user pressed Ctrl+V in the comment field while the set is in the clipboard), the following happens:

1. Qt reads the clipboard **blockingly** in the main thread: QtWaylandClient waits for data from a pipe.
2. The compositor sends a `send` request to our data-control source, that is, to our second connection.
3. The events of that connection are dispatched by the same main thread, which is blocked. There is no answer, Qt waits until its timeout, and the paste comes out empty.
4. Along the way Qt's client-side key repeat breaks (on Wayland the client does it): while the main thread is stuck, the repeat timer fires, and one Ctrl+V turns into dozens of pastes.

The symptom in the self-test: the images "arrived" (Qt did not read the data, it only checked the formats), and the text came out empty 37 times.

## Decision

`WaylandConnection` and all protocol listeners (`DataControlDevice`) live in a separate thread `markpaste-wayland` with its own event loop. `DataControlClipboard` is a thread-safe facade for the main thread: methods are passed to the Wayland thread via `QMetaObject::invokeMethod`, results come back via `QFuture`. Data is written to the pipe by the `QtConcurrent` pool, so that the Wayland thread stays free.

## Options considered

- **The main thread, as in the spec.** Ruled out by the deadlock above.
- **Forbid pasting into our own windows while the clipboard is ours.** Fragile: all paths that read the clipboard inside Qt (context menu, middle button, drag and drop) cannot be intercepted.
- **Set the clipboard through QClipboard (Qt's connection).** On Wayland Qt changes the clipboard only with keyboard focus, and we need to write from the background.

## Consequences

- Code in `platform/linux/wayland/` must follow the rule: protocol objects are accessed only from the connection thread.
- `hasGlobal()` and `global()` read a snapshot taken at connection time, so they are safe from any thread.
- Check: `markpaste-pastetest --self-test --in-process`: a receiver in our process gets all images and the text.
