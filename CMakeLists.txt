cmake_minimum_required(VERSION 3.28)

project(Markpaste VERSION 0.1.0 LANGUAGES C CXX)

# Название и идентификатор — в одном месте: имя проекта рабочее (ТЗ, разд. 14).
# Идентификатор после релиза не меняется: от него зависят разрешения порталов.
set(MARKPASTE_APP_NAME "Markpaste")
set(MARKPASTE_APP_ID "su._525.Markpaste")
set(MARKPASTE_EXECUTABLE "markpaste")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(MARKPASTE_BUILD_TESTS "Собирать тесты" ON)
option(MARKPASTE_BUILD_TOOLS "Собирать отладочные утилиты (прототип вставки)" ON)

# Минимум — Qt 6.4 (системный Qt в Ubuntu 24.04 и Debian 12, ТЗ разд. 7).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network LinguistTools)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MARKPASTE_LINUX ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

    # LayerShellQt необязателен: без него оверлей и лента — обычные окна (ТЗ, разд. 7).
    find_package(LayerShellQt QUIET)
    if(LayerShellQt_FOUND)
        message(STATUS "LayerShellQt найден: оверлей и лента через layer-shell")
    else()
        message(STATUS "LayerShellQt не найден: оверлей и лента — обычные окна")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

add_compile_definitions(
    QT_NO_CAST_FROM_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
    QT_DISABLE_DEPRECATED_UP_TO=0x060400
)

add_subdirectory(src)

if(MARKPASTE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(MARKPASTE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# .desktop-файл. Его имя = app id: по нему порталы узнают программу (ТЗ разд. 9, 11),
# а KWin — доверенную программу для ScreenShot2.
if(MARKPASTE_LINUX)
    set(MARKPASTE_DESKTOP_EXEC "${CMAKE_INSTALL_PREFIX}/bin/${MARKPASTE_EXECUTABLE}")
    configure_file(data/app.desktop.in ${CMAKE_BINARY_DIR}/data/${MARKPASTE_APP_ID}.desktop @ONLY)

    # Для разработки: тот же файл, но Exec указывает на бинарник в каталоге сборки.
    # Без него host Registry отвечает «App info not found» (docs/problems.md).
    set(MARKPASTE_DESKTOP_EXEC "${CMAKE_BINARY_DIR}/src/app/${MARKPASTE_EXECUTABLE}")
    configure_file(data/app.desktop.in ${CMAKE_BINARY_DIR}/dev/${MARKPASTE_APP_ID}.desktop @ONLY)
    add_custom_target(dev-install-desktop
        COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/share/applications
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/dev/${MARKPASTE_APP_ID}.desktop
                $ENV{HOME}/.local/share/applications/${MARKPASTE_APP_ID}.desktop
        COMMAND sh -c "command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 --noincremental >/dev/null 2>&1 || true"
        COMMENT "Installing dev .desktop file into ~/.local/share/applications"
        VERBATIM)
endif()
