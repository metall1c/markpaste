#pragma once

#include "CaptureSet.h"
#include "MessageBuilder.h"

#include <QString>

#include <optional>

namespace mp {

struct RenderStyle;

// The set on disk: "survive a restart" (NFR-5) and save to a folder (APP-6).
//
// The session (NFR-5) is what the set is restored from for editing:
//   <dir>/set.json       — marks, blurs, crop, shot order (format version 1)
//   <dir>/<uuid>.png     — sources without marks and blur, as in memory (Shot::sourcePng)
// A shot's source never changes while the shot is in the set, so the PNG is written once and
// each save rewrites only set.json. The directory and files are 0700/0600: the sources
// contain what the user later blurred (ADR 0013).
class SetStorage
{
public:
    static bool saveSession(const CaptureSet &set, const QString &dir, QString *error = nullptr);
    // No directory or set.json: an empty set, no error. A broken file: nullopt and error.
    static std::optional<CaptureSet> loadSession(const QString &dir, QString *error = nullptr);
    static void removeSession(const QString &dir);

    // JSON without images, separately for tests
    static QByteArray toJson(const CaptureSet &set);
    static std::optional<CaptureSet> fromJson(const QByteArray &json, QString *error = nullptr);

    // Save to a folder (APP-6): baked A.png, B.png… (always PNG, lossless) and
    // message.md, the message text with links to the images. Returns the written paths.
    static QStringList exportSet(const CaptureSet &set, const RenderStyle &style, const MessageTemplate &tpl,
                                 const QString &dir, QString *error = nullptr);
    // message.md text: images ![A](A.png) before or after the text, as when pasting
    static QString exportMarkdown(const CaptureSet &set, const MessageTemplate &tpl);
};

} // namespace mp
