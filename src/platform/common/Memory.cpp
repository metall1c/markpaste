#include "platform/Memory.h"

#include <cstdlib>   // определяет __GLIBC__: без заголовка libc проверка ниже всегда ложна

#ifdef __GLIBC__
#include <malloc.h>
#endif

namespace mp {

void releaseFreedMemory()
{
#ifdef __GLIBC__
    malloc_trim(0);
#endif
}

} // namespace mp
