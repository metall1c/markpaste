#include "platform/Memory.h"

#include <cstdlib>   // определяет __GLIBC__: без заголовка libc проверка ниже всегда ложна

#if defined(__GLIBC__) || defined(_WIN32)
#include <malloc.h>
#endif

namespace mp {

void releaseFreedMemory()
{
#ifdef __GLIBC__
    malloc_trim(0);
#elif defined(_WIN32)
    _heapmin();   // CRT отдаёт системе свободные страницы своей кучи
#endif
}

} // namespace mp
