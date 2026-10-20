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

    // Ни у одного снимка нет комментариев — нет и заголовков «Снимок A»: сослаться в тексте
    // не на что, сколько бы снимков ни было (решение пользователя, ADR 0018)
    const bool anyComments = set.totalComments() > 0;
    for (int si = 0; anyComments && si < set.shots.size(); ++si) {
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
        // Если комментарии есть хоть где-то, заголовок остаётся у каждого снимка:
        // по нему видно, какая картинка какая.
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
