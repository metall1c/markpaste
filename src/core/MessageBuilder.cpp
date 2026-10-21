#include "MessageBuilder.h"

#include <QStringList>

namespace mp {

MessageTemplate MessageTemplate::defaultFor(const QLocale &locale)
{
    MessageTemplate t;
    t.markLine = QStringLiteral("{id} {text}");
    if (locale.language() == QLocale::Russian)
        t.shotHeader = QStringLiteral("Снимок {letter}");
    else
        t.shotHeader = QStringLiteral("Shot {letter}");
    return t;
}

QString buildMessage(const CaptureSet &set, const MessageTemplate &tpl)
{
    QStringList blocks;

    if (!tpl.intro.trimmed().isEmpty())
        blocks << tpl.intro.trimmed();

    // No shot has comments, so there are no "Shot A" headers either: there is nothing to refer to
    // in the text, however many shots there are (the user's decision, ADR 0018)
    const bool anyComments = set.totalComments() > 0;
    for (int si = 0; anyComments && si < set.shots.size(); ++si) {
        const Shot &shot = set.shots[si];
        QStringList lines;
        // Marks without a comment do not get into the text: "A1" without words says nothing.
        // The number stays as on the image: A2 is still called A2 even if A1 is skipped.
        for (int mi = 0; mi < shot.marks.size(); ++mi) {
            const QString comment = shot.marks[mi].comment.trimmed();
            if (comment.isEmpty())
                continue;
            QString line = tpl.markLine;
            line.replace(QStringLiteral("{id}"), markLabel(si, mi));
            line.replace(QStringLiteral("{text}"), comment);
            lines << line;
        }
        // If there are comments anywhere, every shot keeps its header:
        // it shows which image is which.
        QString header = tpl.shotHeader;
        header.replace(QStringLiteral("{letter}"), QString(shotLetter(si)));
        lines.prepend(header);
        blocks << lines.join(u'\n');
    }

    if (!tpl.outro.trimmed().isEmpty())
        blocks << tpl.outro.trimmed();

    return blocks.join(QStringLiteral("\n\n"));
}

} // namespace mp
