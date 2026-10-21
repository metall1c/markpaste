# Cross-compiling for Windows x64 on Fedora: MinGW-w64 and Qt from the mingw64-* packages (docs/build.md).
#   cmake -G Ninja -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-fedora.cmake
# Full compiler paths, without the mingw64-cmake wrapper: it sets them through the environment,
# and a cmake re-run from ninja lost them.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_TRIPLET x86_64-w64-mingw32)
set(CMAKE_C_COMPILER /usr/bin/${MINGW_TRIPLET}-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/${MINGW_TRIPLET}-g++)
set(CMAKE_RC_COMPILER /usr/bin/${MINGW_TRIPLET}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TRIPLET}/sys-root/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Qt tools (moc, rcc, lrelease) are for Linux, from the mingw64-qt6-* packages
set(QT_HOST_PATH /usr CACHE PATH "")
# Qt6Config includes its own qt.toolchain.cmake, which includes the Fedora toolchain where the
# compilers are set without a path. After that project() in subprojects (googletest) did not find them.
set(QT_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_FILE} CACHE FILEPATH "")
