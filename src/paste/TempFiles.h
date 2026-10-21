#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace mp {

// Temporary images for the "files + text" modes and drag and drop (spec sections 10, 11).
// Directory: $XDG_RUNTIME_DIR/markpaste (Linux, on tmpfs, owner-only access),
// otherwise <temp>/markpaste-<pid>. Mode 0700. Cleared when the set is cleared and on exit.
class TempFiles
{
public:
    static QString directory();
    // Writes the file and returns its absolute path; an empty string means an error.
    static QString write(const QString &fileName, const QByteArray &data);
    static void clear();
};

} // namespace mp
