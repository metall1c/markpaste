#pragma once

#include "CaptureSet.h"
#include "MessageBuilder.h"

#include <QString>

#include <optional>

namespace mp {

struct RenderStyle;

// Набор на диске: «пережить перезапуск» (NFR-5) и сохранение в папку (APP-6).
//
// Сессия (NFR-5) — то, из чего набор восстанавливается для правки:
//   <dir>/set.json       — метки, размытия, обрезка, порядок снимков (формат версии 1)
//   <dir>/<uuid>.png     — исходники без меток и без размытия, как в памяти (Shot::sourcePng)
// Исходник снимка не меняется, пока снимок в наборе, поэтому PNG пишется один раз, а при
// каждом сохранении переписывается только set.json. Каталог и файлы — 0700/0600: исходники
// содержат то, что пользователь потом размыл (ADR 0013).
class SetStorage
{
public:
    static bool saveSession(const CaptureSet &set, const QString &dir, QString *error = nullptr);
    // Нет каталога или set.json — пустой набор, без ошибки. Битый файл — nullopt и error.
    static std::optional<CaptureSet> loadSession(const QString &dir, QString *error = nullptr);
    static void removeSession(const QString &dir);

    // JSON без картинок — отдельно для тестов
    static QByteArray toJson(const CaptureSet &set);
    static std::optional<CaptureSet> fromJson(const QByteArray &json, QString *error = nullptr);

    // Сохранение в папку (APP-6): запечённые A.png, B.png… (всегда PNG, без потерь) и
    // message.md — текст сообщения со ссылками на картинки. Возвращает записанные пути.
    static QStringList exportSet(const CaptureSet &set, const RenderStyle &style, const MessageTemplate &tpl,
                                 const QString &dir, QString *error = nullptr);
    // Текст message.md: картинки ![A](A.png) — до или после текста, как при вставке
    static QString exportMarkdown(const CaptureSet &set, const MessageTemplate &tpl);
};

} // namespace mp
