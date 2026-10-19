#pragma once

#include <QString>

namespace mp {

// Автозапуск при входе в систему (APP-3, ТЗ разд. 11).
// Linux — файл <appId>.desktop в $XDG_CONFIG_HOME/autostart (спецификация XDG Autostart).
// Windows — ключ Run в реестре (пока не реализовано: isAutostartSupported() == false).
bool isAutostartSupported();
bool isAutostartEnabled(const QString &appId);
// executable — полный путь к программе; пустой — текущий процесс.
bool setAutostartEnabled(const QString &appId, bool enabled, const QString &executable = {});
QString autostartFilePath(const QString &appId);

} // namespace mp
