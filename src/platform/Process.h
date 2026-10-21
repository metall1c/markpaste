#pragma once

#include <QStringList>

namespace mp {

// OS-specific process setup, first thing in main(), before QApplication.
// Unix: writing to a pipe whose reader closed its end must not kill the process (SIGPIPE).
// Windows: nothing special, Qt itself enables per-monitor v2 DPI awareness.
void prepareProcess();

// Start the program again with these arguments as soon as this process has exited; the caller then quits.
// false: not supported here (Windows).
bool restartWhenExited(const QStringList &args);

} // namespace mp
