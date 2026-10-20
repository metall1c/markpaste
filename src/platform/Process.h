#pragma once

namespace mp {

// Настройка процесса под ОС — первым делом в main(), до QApplication.
// Unix: запись в pipe получателя, который закрыл свой конец, не должна убивать процесс (SIGPIPE).
// Windows: ничего особенного — Qt сам включает DPI awareness per-monitor v2.
void prepareProcess();

} // namespace mp
