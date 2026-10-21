#include "platform/Memory.h"

#include <cstdlib>   // defines __GLIBC__: without a libc header the check below is always false

#if defined(__GLIBC__) || defined(_WIN32)
#include <malloc.h>
#endif

namespace mp {

void releaseFreedMemory()
{
#ifdef __GLIBC__
    malloc_trim(0);
#elif defined(_WIN32)
    _heapmin();   // the CRT returns free pages of its heap to the system
#endif
}

} // namespace mp
