#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace mp {

// Временные картинки для режимов «файлы + текст» и перетаскивания (ТЗ разд. 10, 11).
// Каталог: $XDG_RUNTIME_DIR/markpaste (Linux, в tmpfs, доступ только владельцу),
// иначе <temp>/markpaste-<pid>. Права 0700. Очищается при очистке набора и при выходе.
class TempFiles
{
public:
    static QString directory();
    // Пишет файл и возвращает абсолютный путь; пустая строка — ошибка.
    static QString write(const QString &fileName, const QByteArray &data);
    static void clear();
};

} // namespace mp
