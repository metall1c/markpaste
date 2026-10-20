# Кросс-сборка под Windows x64 на Fedora: MinGW-w64 и Qt из пакетов mingw64-* (docs/build.md).
#   cmake -G Ninja -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-fedora.cmake
# Пути к компиляторам — полные, без обёртки mingw64-cmake: она задаёт их через окружение,
# и перезапуск cmake из ninja их терял.
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

# Инструменты Qt (moc, rcc, lrelease) — для Linux, из пакетов mingw64-qt6-*
set(QT_HOST_PATH /usr CACHE PATH "")
# Qt6Config подключает свой qt.toolchain.cmake, а тот — тулчейн Fedora, где компиляторы
# заданы без пути. После этого project() в подпроектах (googletest) их не находил.
set(QT_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_FILE} CACHE FILEPATH "")
