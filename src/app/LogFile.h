#pragma once

#include <QString>

namespace mp {

// Log file (spec section 7, "Logs"): every Qt logging message is duplicated to
// <AppLocalDataLocation>/logs/markpaste.log, on Linux ~/.local/share/markpaste/logs.
// The previous handler (journald or stderr) keeps working.
//
// Size-based rotation: at 1 MB the file becomes markpaste.1.log, 3 old files are kept.
// The log never contains shot contents or comments, only events (NFR-4).
class LogFile
{
public:
    // Called once in main(), after the application name is set and before the first messages
    static void install();
    static QString directory();
    static QString currentPath();

    // For tests: rotate if the file is larger than maxBytes
    static void rotate(const QString &path, qint64 maxBytes, int keep);
};

} // namespace mp
