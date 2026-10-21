#pragma once

#include <QString>

namespace mp {

// Autostart at login (APP-3, spec section 11).
// Linux: the <appId>.desktop file in $XDG_CONFIG_HOME/autostart (XDG Autostart specification).
// Windows: the <appId> value in HKCU\...\CurrentVersion\Run; autostartFilePath() returns its path.
bool isAutostartSupported();
bool isAutostartEnabled(const QString &appId);
// executable is the full path to the program; empty means the current process.
bool setAutostartEnabled(const QString &appId, bool enabled, const QString &executable = {});
QString autostartFilePath(const QString &appId);

} // namespace mp
