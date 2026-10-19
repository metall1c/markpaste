#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace ps {

// Временные картинки для режимов «файлы + текст» и перетаскивания (ТЗ разд. 10, 11).
// Каталог: $XDG_RUNTIME_DIR/pointshot (Linux, в tmpfs, доступ только владельцу),
// иначе <temp>/pointshot-<pid>. Права 0700. Очищается при очистке набора и при выходе.
class TempFiles
{
public:
    static QString directory();
    // Пишет файл и возвращает абсолютный путь; пустая строка — ошибка.
    static QString write(const QString &fileName, const QByteArray &data);
    static void clear();
};

} // namespace ps
