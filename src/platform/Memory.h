#pragma once

namespace mp {

// Return freed memory to the system (NFR-2).
//
// Baking runs in the thread pool, and every glibc thread gets its own malloc arena. A 4K frame
// unpacked is 33 MB; after pasting 5 shots the freed memory stays in the arenas, and
// the program holds 500+ MB in the background although it needs ~170 (markpaste-bench, docs/platform-matrix.md).
// glibc does not return memory from the middle of arenas by itself: malloc_trim(0) does. On other libcs and OSes
// it does nothing. Call it after heavy operations, not in a loop: walking the arenas costs milliseconds.
void releaseFreedMemory();

} // namespace mp
