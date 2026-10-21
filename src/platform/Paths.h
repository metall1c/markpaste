#pragma once

#include <QString>

namespace mp {

// Local socket name for the single instance. On Unix, a path in the user's
// directory ($XDG_RUNTIME_DIR), not in the shared /tmp; on Windows, a pipe name.
QString singleInstanceSocketName(const QString &appId);

} // namespace mp
