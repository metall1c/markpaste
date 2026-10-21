# 0003. The mark badge background is darkened to a 4.5:1 contrast

- Status: accepted (2026-10-19)
- Spec items: EDT-3, NFR-9, section 10 "Baking"

## Context

Two requirements of the spec contradict each other:

- the badge is a circle of the accent color (#FF5A1F by default) with white text (EDT-3, section 10);
- the contrast of badges is at least 4.5:1 (NFR-9).

The WCAG contrast of white on #FF5A1F is about 3.1:1, so the requirement is not met. The user may also pick an even lighter accent.

## Decision

- The mark frame is drawn in the original accent color.
- The badge background is the same color, darkened in 2 % steps towards black until its contrast with white is ≥ 4.5:1 (`badgeFillColor()` in `render/ShotRenderer.cpp`). The hue is kept, dark colors are not changed.
- The badge has a 1 px white outline so that it reads on any background.

## Options considered

- Black text on the accent color: contrast 6.7:1, but it contradicts EDT-3 and reads worse on dark accents.
- Change the default accent to a darker one: the frame would be less visible on dark interfaces.

## Consequences

For #FF5A1F the badge comes out noticeably darker than the frame. The `Renderer.BadgeContrastAtLeast45` test checks the threshold for a set of accents.
