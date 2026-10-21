# 0021. Mark line halo: dark and semi-transparent, can be turned off in settings

- Status: accepted (2026-10-20)
- Spec items: section 10 "Baking": "the mark frame … with a 1 px white outline for contrast on any background"; section 11 "Settings → Appearance"

## Context

Under every colored line (frame, arrow, freehand, point ring) a white band of 1 px on each side was drawn, as in the spec. The user noticed white edges on all tools. Snapik has lines without an outline. The user's decision: make the halo less visible and move it into settings, on by default.

## Decision

- **The halo is dark and semi-transparent** (`RenderStyle::haloColor` = black with an alpha of 115/255), 1 logical px on each side of the line.
  - On a light shot and on a background of the mark's own color it gives a thin border.
  - On a dark shot (chat windows) it is barely visible.
  - A white halo on a dark background, on the contrary, caught the eye.
- **The checkbox "Thin dark halo around mark lines"** on the "Appearance" tab, on by default (`appearance/markHalo`). When off, lines are drawn without a halo, as in Snapik. It applies both on screen and to the images that go to the chat: one function, `paintMarks`, draws them.
- The halo's dashes match the line's dashes: the step is set from the colored line width, as for the white underlay before (ADR 0007).
- The arrowhead halo is drawn only as an outline. A semi-transparent fill under it would give double darkening along the edge.
- **The white ring of the number badge stays.** It is part of the numbered circle itself (ADR 0003) and does not depend on the checkbox.

## Options considered

- **Keep it white, as in the spec.** On dark chat shots it is more visible than the line itself.
- **Remove it entirely.** A red frame on a red button or a yellow one on a light background get lost. Now this is an option (the checkbox).
- **An offset shadow.** A shifted shadow on a thin line looks like a blurry duplicate. A uniform halo is simpler and does not change the geometry.

## Consequences

- The deviation from spec section 10 is recorded in `docs/deviations.md`.
- The checkbox applies at once to all marks of the set, including those already drawn: it is an appearance setting, not a mark property.
