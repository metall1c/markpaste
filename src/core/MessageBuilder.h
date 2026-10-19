#pragma once

#include "CaptureSet.h"

#include <QLocale>
#include <QString>

namespace ps {

// Шаблон текста сообщения (PST-3, ТЗ разд. 10). Локализуется отдельно от интерфейса (NFR-10):
// язык шаблона может не совпадать с языком UI.
struct MessageTemplate {
    QString shotHeader;      // {letter} → A
    QString markLine;        // {id} → A1, {text} → комментарий
    QString intro;           // вводная фраза, может быть пустой
    QString outro;           // заключительная фраза, может быть пустой
    bool textBeforeImages = false;

    static MessageTemplate defaultFor(const QLocale &locale);
};

// Правила:
// - снимок без меток даёт только заголовок;
// - пустой комментарий даёт строку только с номером (хвостовые пробелы и разделители «:», «-», «—» убираются);
// - многострочный комментарий сохраняет переносы;
// - между снимками — пустая строка.
QString buildMessage(const CaptureSet &set, const MessageTemplate &tpl);

} // namespace ps
