# 0006. Arrows and freehand lines are numbered marks

- Status: accepted (2026-10-19)
- Spec items: EDT-9 (arrow as a mark shape, priority C), EDT-1…EDT-4, EDT-7. The spec has no freehand tool; it was added at the user's request (docs/status.md, plan, item 2).

## Context

The user asked to move the arrow and freehand drawing up in the plan: "arrow A1, frame A2, freehand loop A3", as in other programs. Each shape must give a line in the message text, like a rectangular mark.

## Decision

- **Model.** `Mark` got a `shape` (`Rect`, `Arrow`, `Freehand`) and `points` in physical pixels of the source. An arrow is `{tail, tip}`, freehand is a polyline. For line shapes `rect` is the bounding rectangle of the points, recomputed by the model (`CaptureSetModel::fitMark`); the canvas and the comment popup still rely on `rect`. The number is the position in the shared `marks` list, so numbering, message text, counters and undo work unchanged.
- **Model API:** `addLinearMark`, `setMarkPoints`. `setMarkRect` refuses line shapes: they have no frame to drag by a corner.
- **Number badge:** frame, the top-left corner; arrow, the tail; freehand, the first point (`Mark::badgeAnchor`). The "inside the shot" constraint (EDT-3) is the same as for frames.
- **Drawing** is in `paintMarks`, shared by the editor and baking. The arrow shaft is ~~4~~ 2 logical px (frame and freehand ~~3~~ 2; the default width was reduced on 2026-10-19 at the user's request), the head is a filled triangle 4 shaft widths long, but no shorter than 12 logical px, and 1.1 of its length wide; on a short arrow the head is no longer than 2/3 of the arrow. A 1 px white outline, as for frames. All sizes are multiplied by the shot's `devicePixelRatio`.
- **Freehand:** raw mouse points are simplified with Ramer–Douglas–Peucker when added, with a tolerance of 0.5·dpr physical px (half a screen pixel). A straight stroke of 140 points shrinks to 2. It is drawn smoothed: quadratic curves through the segment midpoints, with the polyline vertices as control points.
- **Crop (EDT-7)** works by the line, not by the bounding rectangle: a diagonal arrow whose rectangle touches the area while the line does not is considered "outside" and removed after the same confirmation. A segment is clipped with Liang–Barsky. A freehand polyline that goes over the edge and comes back falls apart into pieces; the **longest** one is kept: one mark, one badge.
- **Editor.** Buttons "▭ Mark", "➚ Arrow", "✎ Pen", keys R / A / P (M, the old frame key, is kept). The comment opens after the mouse is released. Line shapes are hit by the distance to the line: half the width + 5 logical px. A frame is hit by its whole area, as before. A selected arrow has two handles, its ends; freehand is dragged only as a whole (a dashed outline instead of handles).
- **Minimum sizes:** an arrow shorter than the badge diameter (28 logical px) is not created, it would hide under the badge entirely. Freehand needs at least 6 physical px along the longer side, like a frame.

## Options considered

- **A separate list of arrows in `Shot`, like `redactions`.** Numbering would have to be merged from two lists, and the order written everywhere it is needed (text, badges, comment list). One shared `marks` is simpler.
- **Keep all mouse points for freehand.** Hundreds of points per stroke in every undo state snapshot, with no visible gain.
- **Split a clipped polyline into several strokes.** Would need separators in `points` and a rule for where the badge goes. The longest piece is almost always what the user circled.
- **Hit freehand by the bounding rectangle.** A loop would cover everything inside it for the mouse: a new mark could no longer be drawn inside.

## Consequences and limitations

- ~~Color and width are shared by all marks.~~ Outdated: every mark has its own color, width and line style, [ADR 0007](0007-mark-pen.md).
- If an arrow or freehand line is started inside an existing frame, the frame is dragged (as for a new frame inside an old one).
- A freehand line taken over the shot edge with the mouse follows the edge (the points are pressed to the border).
- The point marker from EDT-9 is not there yet.

Addition 2026-10-20: the fourth shape, the point marker, see [ADR 0015](0015-point-marker-and-captions.md).
