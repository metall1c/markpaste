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

    for (int si = 0; si < set.shots.size(); ++si) {
        const Shot &shot = set.shots[si];
        QStringList lines;
        // Метки без комментария в текст не попадают: «A1» без слов ничего не сообщает.
        // Номер остаётся как на картинке — A2 так и называется A2, даже если A1 пропущена.
        for (int mi = 0; mi < shot.marks.size(); ++mi) {
            const QString comment = shot.marks[mi].comment.trimmed();
            if (comment.isEmpty())
                continue;
            QString line = tpl.markLine;
            line.replace(QStringLiteral("{id}"), markLabel(si, mi));
            line.replace(QStringLiteral("{text}"), comment);
            lines << line;
        }
        // Единственный снимок без комментариев — без «Снимок A»: сослаться в тексте не на что.
        // Из нескольких снимков заголовок остаётся у каждого: по нему видно, какая картинка какая.
        if (lines.isEmpty() && set.shots.size() == 1)
            continue;
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
