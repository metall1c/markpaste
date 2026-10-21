# 0018. Marks without a comment do not get into the message text

- Status: accepted (2026-10-20, the user's request; extended the same day to a set of several shots)
- Spec items: section 3 ("A comment… may be empty, then only the number goes into the message"), section 10 (template: "empty comments give a line with just the number, shots without marks give just a header"), PST-3

## Context

The user put an arrow without a comment on the only shot and got the text "Shot A / A1" in the chat: it says nothing and only clutters the message.

## Decision

`buildMessage` (`src/core/MessageBuilder.cpp`):
- a mark of any shape without a comment gives no line; the other numbers stay as on the image (if A1 has no words and A2 does, the text has "A2 …");
- if no shot of the set has comments, there is no text: no "Shot A" and no mark numbers, however many shots there are. At first this was only for a single shot; the user asked for the same for the whole strip. The opening and closing phrases from settings stay: the user writes them, and they are empty by default;
- if at least one shot has comments, every shot keeps its header: it shows which image is which.

Empty text is not pasted at all: in mode 1 there is no "text" step, in the "files + text" mode there is no text part, in `message.md` (APP-6) only images. When the set is dragged from the strip (mode 3), empty text is not put into the clipboard, and the strip does not say "Text copied".

## Options considered

- Drop the header of every shot without comments, even when others have comments. Then the text has "Shot B / B1 …" without "Shot A", and it is not clear that the first image is A. The headers stay as long as there is a comment anywhere.

## Consequences

- Differs from the spec (sections 3 and 10): a line in docs/deviations.md.
- Tests `MessageBuilder.MarksWithoutCommentAreSkipped`, `MessageBuilder.SingleShotWithoutCommentsGivesNoText`, `MessageBuilder.SeveralShotsWithoutCommentsGiveNoText`, `MessageBuilder.ShotWithoutCommentsAmongSeveralGivesHeaderOnly`.
