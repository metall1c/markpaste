#pragma once


namespace mp {

// OS-specific process setup, first thing in main(), before QApplication.
// Unix: writing to a pipe whose reader closed its end must not kill the process (SIGPIPE).
// Linux, static Qt: file dialogs through the xdg-desktop-portal theme (see Process.cpp).
// Windows: nothing special, Qt itself enables per-monitor v2 DPI awareness.
void prepareProcess();

// Right after QApplication is created: undo what prepareProcess() set only for our own Qt,
// so that programs we start (a file manager for "Open folder") do not inherit it.
void finishProcessSetup();

} // namespace mp
