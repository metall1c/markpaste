cmake_minimum_required(VERSION 3.28)

project(Markpaste VERSION 0.1.0 LANGUAGES C CXX)

# Название и идентификатор — в одном месте: имя проекта рабочее (ТЗ, разд. 14).
# Идентификатор после релиза не меняется: от него зависят разрешения порталов.
set(MARKPASTE_APP_NAME "Markpaste")
set(MARKPASTE_APP_ID "su._525.Markpaste")
# Проверка новой версии (APP-7): API последнего релиза GitLab. Пустая строка — проверки нет
# (например, для сборок дистрибутивов, которые обновляются своим путём).
set(MARKPASTE_UPDATE_URL "https://gitlab.525.su/api/v4/projects/che%2Fmain%2Fmarkpaste/releases/permalink/latest"
    CACHE STRING "URL of the latest release JSON (GitLab API); empty disables update checks")
set(MARKPASTE_RELEASES_URL "https://gitlab.525.su/che/main/markpaste/-/releases"
    CACHE STRING "Releases page shown in the update notification")
set(MARKPASTE_EXECUTABLE "markpaste")

include(GNUInstallDirs)
set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATADIR}/doc/${MARKPASTE_EXECUTABLE})

# Позиционно-независимый исполняемый файл (PIE): требование пакетов Fedora и Debian
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(CheckPIESupported)
check_pie_supported()

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
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)   # раскладка виртуальной клавиатуры
    # X11: глобальные клавиши (XGrabKey) и Ctrl+V (XTEST). Без них X11 — только захват и буфер.
    pkg_check_modules(XCB QUIET IMPORTED_TARGET xcb xcb-xtest xcb-keysyms)
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
    # Путь в Exec — абсолютный: KWin сверяет его с исполняемым файлом процесса.
    # Поэтому пакет собирается с -DCMAKE_INSTALL_PREFIX=/usr (docs/build.md).
    set(MARKPASTE_DESKTOP_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/${MARKPASTE_EXECUTABLE}")
    configure_file(data/app.desktop.in ${CMAKE_BINARY_DIR}/data/${MARKPASTE_APP_ID}.desktop @ONLY)
    configure_file(data/app.metainfo.xml.in ${CMAKE_BINARY_DIR}/data/${MARKPASTE_APP_ID}.metainfo.xml @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/data/${MARKPASTE_APP_ID}.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${CMAKE_BINARY_DIR}/data/${MARKPASTE_APP_ID}.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES data/icons/app.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME ${MARKPASTE_APP_ID}.svg)

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

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# --- Пакеты (ТЗ разд. 11): cmake --build <каталог> --target package ---
# Зависимости от библиотек (Qt 6, libwayland-client, LayerShellQt) пакетные системы
# находят сами по связанным .so; вручную — только то, что нужно во время работы.
set(CPACK_PACKAGE_NAME ${MARKPASTE_EXECUTABLE})
set(CPACK_PACKAGE_VENDOR "Markpaste")
set(CPACK_PACKAGE_CONTACT "Andrey <a@525.su>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Capture, mark and paste screenshots with comments into any chat")
set(CPACK_PACKAGE_DESCRIPTION "Press a hotkey, select an area, put numbered marks A1, A2... on it and
comment each one. One more hotkey pastes the whole set - images and text -
into any chat or AI assistant.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://gitlab.525.su/che/main/markpaste")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_STRIP_FILES ON)
set(CPACK_GENERATOR "TGZ")

find_program(MARKPASTE_RPMBUILD rpmbuild)
if(MARKPASTE_RPMBUILD)
    list(APPEND CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    # Порталы — захват, клавиши, Ctrl+V; без них программа работает в урезанном режиме
    set(CPACK_RPM_PACKAGE_REQUIRES "xdg-desktop-portal")
    # Общие каталоги принадлежат другим пакетам (filesystem, hicolor-icon-theme)
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        ${CMAKE_INSTALL_FULL_DATADIR}/applications
        ${CMAKE_INSTALL_FULL_DATADIR}/metainfo
        ${CMAKE_INSTALL_FULL_DATADIR}/icons
        ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor
        ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable
        ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps
        ${CMAKE_INSTALL_FULL_DATADIR}/doc)
endif()

find_program(MARKPASTE_DPKG_SHLIBDEPS dpkg-shlibdeps)
if(MARKPASTE_DPKG_SHLIBDEPS)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}\n${CPACK_PACKAGE_DESCRIPTION}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "xdg-desktop-portal")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "xdg-desktop-portal-kde | xdg-desktop-portal-gnome")
endif()

include(CPack)
