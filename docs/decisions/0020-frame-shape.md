# 0020. Mark frame shape: rectangle, rounded rectangle, ellipse

- Status: accepted (2026-10-20)
- Spec items: EDT-1 (a rectangular mark dragged with the mouse), EDT-3 (the number badge at the frame's top-left corner, not beyond the shot edge), EDT-4, EDT-8

## Context

The user asked to choose the shape on the main "Mark" tool, as in Snapik: a menu on the tool button with "Rectangle", "Rounded rectangle", "Ellipse" (a Snapik screenshot, 2026-10-20). The spec knows only a rectangular mark.

## Decision

- **The shape is a pen field**: `MarkPen::frame` (`FrameShape::Rect | Rounded | Ellipse`), not a separate mark field. This way the shape goes the same paths as color and width:
  - new marks take the current pen;
  - for the selected frame it changes with one edit in the undo history;
  - selecting a mark shows its shape, and the next frame will be the same;
  - the last choice is kept in settings (`editor/markFrame`).
- **The geometry is the same for all three shapes**: the bounding rectangle `rect`. Moving, resize handles, crop and the "inside the shot" check did not change. Only painting changes: `frameOutline()` in `ShotRenderer` builds the path of a rectangle, a rounded rectangle or an ellipse. The line is entirely inside `rect`. The corner radius is 10 logical px, for a small frame no more than a third of the side. The joins of the rounded frame and the ellipse are round.
- **The ellipse's number badge** sits on the ellipse itself at 45° top-left. In the corner of the bounding rectangle it would hang in empty space. This refines EDT-3 for the ellipse. For the rounded frame the badge is in the top-left corner, as for the rectangle: it covers the rounding.
- **Mouse hit-testing**: an ellipse is hit by the ellipse itself (with a 5 px margin), not by the corners of the bounding rectangle. A drag from an empty corner draws a new frame. A rounded frame is hit like a rectangle.
- **Arrows, freehand lines and points need no shape.** Choosing a shape with an arrow selected changes the shape only for new frames: nothing changes on the arrow, and there is no empty undo step.
- **UI:**
  - next to the "Mark" button there is a narrow button with a down arrow and a menu of the three shapes with icons and a mark on the current one;
  - the "Mark" button icon itself shows the current shape;
  - choosing a shape enables the "Mark" tool.
- **Storing the set on disk**: the field `"frame": "rounded" | "ellipse"` is written only for a non-rectangular frame. Old sets are read unchanged, their frames are rectangular.

## Options considered

- **New mark shapes** (`MarkShape::RoundedRect`, `MarkShape::Ellipse`). All places that distinguish frames and line shapes would have to change (`isLinear`, handles, moving, crop, saving). No gain: the geometry is the same.
- **A separate mark field `frame`**. It would need its own model edit, its own selection signal and its own settings entry: the same paths as the pen, once more.
- **The shape in the pen panel** (next to color and width). In Snapik the shape is chosen on the tool. The pen panel is shared by all tools, while only frames need a shape.

## Consequences and limitations

- `MarkPen` is called a "pen" but also carries the frame shape; this is noted in the struct's comment.
- **Enter in the comment field drops the selection** (the user's decision, 2026-10-20). At first the selection stayed, and a shape chosen right after a frame "for the next one" changed it too. This was found on Windows: the A1 ellipse became rounded. Now a choice of shape and color after Enter applies to the next mark. If the field closed by a click outside (for example, on the color button), the mark stays selected and is recolored: this way the person edits exactly it. Esc closes the field, the selection stays.
- There is no key for changing the shape. R and M enable "Mark" with the current shape.
- The message text does not depend on the shape.

## Checks

- Tests:
  - core: painting the three shapes (only the rectangle's corner is painted), the badge on the ellipse, saving to disk;
  - UI: the menu for a new and a selected frame, undo, an arrow without an empty step, ellipse hit-testing, after Enter the shape and color apply to the next mark.
- Windows 11 24H2 (VirtualBox, 2026-10-20): the menu with a check mark on the current shape, the ellipse and the rounded frame are drawn, undo restores the previous shape.
- KDE: the menu rendered in the Breeze style without a display (`QT_STYLE_OVERRIDE=breeze markpaste-uipreview --dark`): radio buttons on the items.
