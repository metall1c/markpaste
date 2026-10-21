#pragma once

#include "CaptureSet.h"

#include <QLocale>
#include <QString>

namespace mp {

// Message text template (PST-3, spec section 10). Localized separately from the UI (NFR-10):
// the template language may differ from the UI language.
struct MessageTemplate {
    QString shotHeader;      // {letter} → A
    QString markLine;        // {id} → A1, {text} → the comment
    QString intro;           // opening phrase, may be empty
    QString outro;           // closing phrase, may be empty
    bool textBeforeImages = false;

    static MessageTemplate defaultFor(const QLocale &locale);
};

// Rules:
// - a mark without a comment gives no line (the other numbers stay as on the image);
// - if no shot has comments, there are no headers and no mark lines
//   (only the opening and closing phrases from settings remain, if set);
// - if there are comments, a shot without comments gives only its header;
// - a multi-line comment keeps its line breaks;
// - shots are separated by an empty line.
QString buildMessage(const CaptureSet &set, const MessageTemplate &tpl);

} // namespace mp
