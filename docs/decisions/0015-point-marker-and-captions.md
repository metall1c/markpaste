# 0015. Point marker and comment captions on the image

- Status: accepted (2026-10-20)

## Context

EDT-9: "arrow and point marker as alternative mark shapes" (the arrow was done in ADR 0006). EDT-10: "comment captions right on the image (the "text on image" option)".

## Decision

**Point marker**: `MarkShape::Point`, the fourth mark shape in the shared A1, A2… numbering.
- The geometry is one point in `Mark::points` (physical pixels of the source), `rect` is 1×1 around it. A point is not placed outside the visible area; a crop that leaves it outside removes it, like other marks.
- Look: the number badge centered on the point and a ring of the mark color around it with a white underlay (since 2026-10-20 a dark semi-transparent halo, ADR 0021). The ring carries the pen's color, width and line style: without it a point would not differ from the badges of other shapes. At the shot edge the badge moves inwards (EDT-3), and the ring moves with it.
- Editor: the "● Point" button, key **D** (P is taken by the pen). Placed with a click; without releasing the mouse, the point can be moved to the right place right away. Mouse hit-testing is by the circle of the badge with the ring; a selected point gets a dashed circle and is dragged only as a whole.
- In `set.json`: `"shape": "point"` (ADR 0013).

**Captions on the image (EDT-10)**: the option "Also write comments on the image, next to the numbers" on the "Message" tab, `message/captionsOnImage`, off by default.
- A white plate with a border of the mark color and dark text to the right of the badge; if it does not fit, to the left; if there is no room there either, below the badge. The plate is always entirely inside the shot, long text wraps by words (width up to 280 logical px).
- The font is half the badge diameter, scaled by the shot DPI, like the badges.
- Drawn by the same `paintMarks`: captions are visible in the editor, thumbnails, the preview, the paste and "Save to folder".
- The message text does not change: the comments stay in it too.

## Options considered

- **A point as a small filled circle with the number next to it** (like a pin). Rejected: a number next to the point competes with neighboring marks for space, while a badge in the center shows the spot by itself.
- **Captions instead of the message text** (images without text). Rejected: text in a chat can be searched and quoted; this can be done with the template (an empty mark line) if needed.
- **A separate "Text" tool**, a label anywhere without a number. Rejected: the spec talks about comment captions; free text without a number does not get into the message and breaks the "number ↔ comment" link.

## Consequences

- Captions can overlap each other and neighboring marks if the marks are dense: there is no overlap-free layout.
- Not checked in a live session; checked by the tests `Renderer.PointIsBadgeWithRingCenteredOnSpot`, `Renderer.CaptionsOnImageOnlyWhenEnabled`, `ShotEditorUi.PointToolPlacesPointByClick` and the uipreview image.
