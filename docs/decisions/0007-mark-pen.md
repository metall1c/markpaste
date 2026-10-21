# 0007. Color, width and line style for each mark

- Status: accepted (2026-10-19)
- Spec items: section 10 (one accent color for marks, set in settings), EDT-3, NFR-9 (badge contrast). The user's request on 2026-10-19: "make a tool to choose the frame color and width", modeled on a panel with width presets, a slider, a line sample and a line style.

## Decision

- **Model.** `Mark` has a `pen` field (`MarkPen`): color, width in logical px, line style (`Solid`, `Dash`, `Dot`). Empty fields mean "from the common style": an invalid color is the accent from settings, width 0 is the default width, 2 px for all shapes (it was 3 for frames and freehand and 4 for arrows; reduced on 2026-10-19 at the user's request). So marks without a chosen color are recolored together with the accent in settings. Changing the pen is `CaptureSetModel::setMarkPen`, one entry in the undo history.
- **Rendering.** `markColor` and `markWidth` resolve empty fields; the width is multiplied by the shot's `devicePixelRatio`, as before. The number badge uses the mark color darkened to a 4.5:1 contrast with the white digit (the same `badgeFillColor`, ADR 0003). Dashes and dots are set in absolute pixels from the colored line width, not in fractions of the pen width: the white underlay pen is wider, and the dashes would drift apart. The arrowhead is always solid.
- **Editor.** To the right of the tools there are two buttons: a color circle and "2 px ▾". The color panel: 8 swatches (the first is the accent from settings) and "Other color…" (QColorDialog). The width panel: presets 2/3/5/8 px, a slider 1–20 px, a line sample, the line style. The panels are a `QMenu` with a `QWidgetAction`: on Wayland this is a popup surface that the compositor places next to the button; the program cannot place a window next to a button itself.
- **What the choice changes.** The pen from the panel applies to new marks and, if a mark is selected, to it. Selecting a mark shows its pen in the panel, and the next mark is drawn with it too. The last chosen pen is kept in settings (`editor/markColor`, `editor/markWidth`, `editor/markLine`, since 2026-10-20 also `editor/markFrame`, the frame shape, ADR 0020) and restored at startup. Since 2026-10-20 Enter in the comment field drops the selection: a pen chosen after that applies to the next mark (ADR 0020).
- **Default tool.** Every shot opened in the editor starts with the "Mark" tool (frame), even if an arrow or freehand was chosen for the previous shot.

## Options considered

- **Only a common color in settings (as in the spec).** Does not allow "blue arrow, red frame" on one shot, which the user asked for.
- **Store resolved values in the mark (always a color and a width).** Then changing the accent in settings would not recolor what is already drawn, and old marks and tests would have to be rewritten.
- **Separate windows for the panels.** On Wayland a window does not appear next to a button; a menu does.

## Consequences

- For an empty field the panel shows the width of the current shape (`defaultMarkWidth`); currently all shapes have 2 px.
- A width once chosen in the panel is kept in settings and wins over the default.
- White and yellow are less visible on a light shot: their white underlay merges with the background; left to the user's choice. *Since 2026-10-20 the underlay is a dark semi-transparent halo that can be turned off in settings (ADR 0021): it helps white and yellow lines on a light shot.*
