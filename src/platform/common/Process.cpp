#include "platform/Process.h"

#include <csignal>

namespace mp {

void prepareProcess()
{
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif
}

} // namespace mp
