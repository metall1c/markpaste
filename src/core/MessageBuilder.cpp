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

static QString chopTrailingSeparators(QString s)
{
    static const QString separators = QStringLiteral(" \t:-—–");
    while (!s.isEmpty() && separators.contains(s.back()))
        s.chop(1);
    return s;
}

QString buildMessage(const CaptureSet &set, const MessageTemplate &tpl)
{
    QStringList blocks;

    if (!tpl.intro.trimmed().isEmpty())
        blocks << tpl.intro.trimmed();

    for (int si = 0; si < set.shots.size(); ++si) {
        const Shot &shot = set.shots[si];
        QStringList lines;
        QString header = tpl.shotHeader;
        header.replace(QStringLiteral("{letter}"), QString(shotLetter(si)));
        lines << header;

        for (int mi = 0; mi < shot.marks.size(); ++mi) {
            const QString comment = shot.marks[mi].comment.trimmed();
            QString line = tpl.markLine;
            line.replace(QStringLiteral("{id}"), markLabel(si, mi));
            if (comment.isEmpty()) {
                line.replace(QStringLiteral("{text}"), QString());
                line = chopTrailingSeparators(line);
            } else {
                line.replace(QStringLiteral("{text}"), comment);
            }
            lines << line;
        }
        blocks << lines.join(u'\n');
    }

    if (!tpl.outro.trimmed().isEmpty())
        blocks << tpl.outro.trimmed();

    return blocks.join(QStringLiteral("\n\n"));
}

} // namespace mp
