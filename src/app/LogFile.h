#pragma once

#include <QString>

namespace mp {

// Журнал в файл (ТЗ разд. 7, «Логи»): все сообщения Qt-логирования дублируются в
// <AppLocalDataLocation>/logs/markpaste.log — на Linux ~/.local/share/markpaste/logs.
// Прежний обработчик (journald или stderr) продолжает работать.
//
// Ротация по размеру: при 1 МБ файл становится markpaste.1.log, хранится 3 старых файла.
// В журнал не пишется содержимое снимков и комментариев — только события (NFR-4).
class LogFile
{
public:
    // Вызывается один раз в main(), после имени программы и до первых сообщений
    static void install();
    static QString directory();
    static QString currentPath();

    // Для тестов: ротация, если файл больше maxBytes
    static void rotate(const QString &path, qint64 maxBytes, int keep);
};

} // namespace mp
