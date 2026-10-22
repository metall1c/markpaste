#include "PlatformTypes.h"

namespace mp {

namespace {
struct ParsedTrigger {
    bool ctrl = false, alt = false, shift = false, meta = false;
    QString key;
};

// "Primary+Alt+V", "Ctrl+Alt+V", "Control+v" → modifiers and the key, case-insensitive
ParsedTrigger parseTriggerParts(const QString &trigger)
{
    ParsedTrigger t;
    for (const QString &raw : trigger.split(u'+', Qt::SkipEmptyParts)) {
        const QString part = raw.trimmed().toLower();
        if (part == u"ctrl" || part == u"control" || part == u"primary")
            t.ctrl = true;
        else if (part == u"alt")
            t.alt = true;
        else if (part == u"shift")
            t.shift = true;
        else if (part == u"meta" || part == u"super" || part == u"win" || part == u"logo")
            t.meta = true;
        else
            t.key = part;
    }
    return t;
}

template <typename Pred>
bool anyTrigger(const QString &triggers, Pred pred)
{
    for (const QString &trigger : triggers.split(u',', Qt::SkipEmptyParts))
        if (pred(parseTriggerParts(trigger)))
            return true;
    return false;
}
} // namespace

QString defaultPasteHotkey()
{
#ifdef _WIN32
    return QStringLiteral("Primary+V");
#else
    return QStringLiteral("Primary+Alt+V");
#endif
}

bool triggersHaveAltWithoutCtrl(const QString &triggers)
{
    return anyTrigger(triggers, [](const ParsedTrigger &t) { return t.alt && !t.ctrl; });
}

bool triggersHavePlainPaste(const QString &triggers)
{
    return anyTrigger(triggers, [](const ParsedTrigger &t) {
        return t.ctrl && !t.alt && !t.shift && !t.meta && t.key == u"v";
    });
}

QStringList ClipboardPayload::mimeTypes() const
{
    QStringList list;
    for (const ClipboardEntry &e : entries)
        list << e.mimeType;
    return list;
}

const ClipboardEntry *ClipboardPayload::find(const QString &mimeType) const
{
    for (const ClipboardEntry &e : entries)
        if (e.mimeType == mimeType)
            return &e;
    return nullptr;
}

void ClipboardPayload::addText(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    for (const char *mime : {"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT"})
        entries.append({QString::fromLatin1(mime), utf8});
}

void ClipboardPayload::addImage(const QByteArray &encoded, const QByteArray &mimeType)
{
    entries.append({QString::fromLatin1(mimeType), encoded});
}

ClipboardPayload ClipboardPayload::text(const QString &text)
{
    ClipboardPayload p;
    p.addText(text);
    return p;
}

ClipboardPayload ClipboardPayload::image(const QByteArray &encoded, const QByteArray &mimeType)
{
    ClipboardPayload p;
    p.addImage(encoded, mimeType);
    return p;
}

} // namespace mp
