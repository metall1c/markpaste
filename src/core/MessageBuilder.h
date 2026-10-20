#pragma once

#include "CaptureSet.h"

#include <QLocale>
#include <QString>

namespace mp {

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
// - метка без комментария строки не даёт (номера остальных — как на картинке);
// - если ни у одного снимка нет комментариев — ни заголовков, ни строк меток
//   (остаются только вводная и заключительная фразы из настроек, если заданы);
// - если комментарии есть, снимок без комментариев даёт только заголовок;
// - многострочный комментарий сохраняет переносы;
// - между снимками — пустая строка.
QString buildMessage(const CaptureSet &set, const MessageTemplate &tpl);

} // namespace mp
