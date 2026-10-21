cmake_minimum_required(VERSION 3.28)

project(Markpaste VERSION 0.2.0 LANGUAGES C CXX)

# Name and ID live in one place: the project name is a working one (spec, section 14).
# The ID never changes after a release: portal permissions depend on it.
set(MARKPASTE_APP_NAME "Markpaste")
set(MARKPASTE_APP_ID "su._525.Markpaste")
# The project's home is GitHub (docs/decisions/0024-github-home-and-releases.md): the page,
# releases and the new-version check (APP-7) come from one "owner/name".
set(MARKPASTE_GITHUB_REPO "metall1c/markpaste" CACHE STRING "GitHub repository owner/name")
# GitHub API for the latest release. An empty string disables the check (e.g. for distro
# builds that are updated their own way).
set(MARKPASTE_UPDATE_URL "https://api.github.com/repos/${MARKPASTE_GITHUB_REPO}/releases/latest"
    CACHE STRING "URL of the latest release JSON (GitHub API); empty disables update checks")
set(MARKPASTE_RELEASES_URL "https://github.com/${MARKPASTE_GITHUB_REPO}/releases"
    CACHE STRING "Releases page shown in the update notification")
# Project page: in the About window, in packages and in AppStream
set(MARKPASTE_HOMEPAGE_URL "https://github.com/${MARKPASTE_GITHUB_REPO}"
    CACHE STRING "Project page shown in the About window")
set(MARKPASTE_EXECUTABLE "markpaste")

include(GNUInstallDirs)
set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATADIR}/doc/${MARKPASTE_EXECUTABLE})
# Windows: portable layout, everything in one directory next to the .exe
if(WIN32)
    set(MARKPASTE_INSTALL_BINDIR .)
    set(CMAKE_INSTALL_DOCDIR .)
else()
    set(MARKPASTE_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
endif()

# Position-independent executable (PIE): required by Fedora and Debian packaging
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

option(MARKPASTE_BUILD_TESTS "Build tests" ON)
option(MARKPASTE_BUILD_TOOLS "Build debugging utilities (paste prototype)" ON)

# Minimum is Qt 6.4 (system Qt in Ubuntu 24.04 and Debian 12, spec section 7).
# Svg: UI icons (docs/decisions/0019-ui-style.md)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent Network LinguistTools)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MARKPASTE_LINUX ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)   # virtual keyboard keymap
    # X11: global hotkeys (XGrabKey) and Ctrl+V (XTEST). Without them X11 has only capture and clipboard.
    pkg_check_modules(XCB QUIET IMPORTED_TARGET xcb xcb-xtest xcb-keysyms)
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

    # LayerShellQt is optional: without it the overlay and the strip are ordinary windows (spec, section 7).
    find_package(LayerShellQt QUIET)
    if(LayerShellQt_FOUND)
        message(STATUS "LayerShellQt found: overlay and strip use layer-shell")
    else()
        message(STATUS "LayerShellQt not found: overlay and strip are ordinary windows")
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

# .desktop file. Its name = app id: portals identify the program by it (spec sections 9, 11),
# and KWin recognizes a trusted program for ScreenShot2.
if(MARKPASTE_LINUX)
    # The Exec path is absolute: KWin compares it with the process executable.
    # That is why packages are built with -DCMAKE_INSTALL_PREFIX=/usr (docs/build.md).
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

    # For development: the same file, but Exec points to the binary in the build directory.
    # Without it the host Registry answers "App info not found" (docs/problems.md).
    set(MARKPASTE_DESKTOP_EXEC "${CMAKE_BINARY_DIR}/src/app/${MARKPASTE_EXECUTABLE}")
    configure_file(data/app.desktop.in ${CMAKE_BINARY_DIR}/dev/${MARKPASTE_APP_ID}.desktop @ONLY)
    add_custom_target(dev-install-desktop
        COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/share/applications
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/dev/${MARKPASTE_APP_ID}.desktop
                $ENV{HOME}/.local/share/applications/${MARKPASTE_APP_ID}.desktop
        # Icon named after the app id, as the package installs it, only into the user directory
        COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/share/icons/hicolor/scalable/apps
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/data/icons/app.svg
                $ENV{HOME}/.local/share/icons/hicolor/scalable/apps/${MARKPASTE_APP_ID}.svg
        COMMAND sh -c "command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t $ENV{HOME}/.local/share/icons/hicolor || true"
        COMMAND sh -c "command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 --noincremental >/dev/null 2>&1 || true"
        COMMENT "Installing dev .desktop file and icon into ~/.local/share"
        VERBATIM)
endif()

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

if(MARKPASTE_LINUX)
    # Man page, Debian copyright and changelog: lintian requires them, rpmlint warns without a man page.
    # Compressed at configure time (file(ARCHIVE_CREATE ... FORMAT raw)), as packages expect.
    configure_file(data/markpaste.1 ${CMAKE_BINARY_DIR}/data/${MARKPASTE_EXECUTABLE}.1 @ONLY)
    configure_file(data/copyright.in ${CMAKE_BINARY_DIR}/data/copyright @ONLY)
    string(TIMESTAMP MARKPASTE_CHANGELOG_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)
    # A package with a Debian revision (0.2.0-1~ubuntu24.04, set by tools/package/build-in-container.sh)
    # is not "native" for lintian and needs changelog.Debian.gz with the same version
    if(CPACK_DEBIAN_PACKAGE_RELEASE)
        set(MARKPASTE_CHANGELOG_VERSION "${PROJECT_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}")
        set(MARKPASTE_CHANGELOG_NAME changelog.Debian)
    else()
        set(MARKPASTE_CHANGELOG_VERSION "${PROJECT_VERSION}")
        set(MARKPASTE_CHANGELOG_NAME changelog)
    endif()
    file(WRITE ${CMAKE_BINARY_DIR}/data/${MARKPASTE_CHANGELOG_NAME}
        "${MARKPASTE_EXECUTABLE} (${MARKPASTE_CHANGELOG_VERSION}) unstable; urgency=medium\n\n"
        "  * Release ${PROJECT_VERSION}: ${MARKPASTE_RELEASES_URL}/tag/v${PROJECT_VERSION}\n\n"
        " -- Andrey Che <metallicvrn@gmail.com>  ${MARKPASTE_CHANGELOG_DATE}\n")
    foreach(f ${MARKPASTE_EXECUTABLE}.1 ${MARKPASTE_CHANGELOG_NAME})
        file(ARCHIVE_CREATE OUTPUT ${CMAKE_BINARY_DIR}/data/${f}.gz PATHS ${CMAKE_BINARY_DIR}/data/${f}
             FORMAT raw COMPRESSION GZip COMPRESSION_LEVEL 9)
    endforeach()
    install(FILES ${CMAKE_BINARY_DIR}/data/${MARKPASTE_EXECUTABLE}.1.gz DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    install(FILES ${CMAKE_BINARY_DIR}/data/copyright ${CMAKE_BINARY_DIR}/data/${MARKPASTE_CHANGELOG_NAME}.gz
            DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

if(WIN32)
    # Qt root for Windows: the directory above lib/cmake/Qt6
    get_filename_component(MARKPASTE_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    include(cmake/WindowsDeploy.cmake)
    include(cmake/WindowsInstaller.cmake)   # installer target → Setup.exe (ADR 0023)
endif()

# --- Packages (spec section 11): cmake --build <dir> --target package ---
# Library dependencies (Qt 6, libwayland-client, LayerShellQt) are found by the package
# tools from the linked .so files; only runtime needs are listed by hand.
set(CPACK_PACKAGE_NAME ${MARKPASTE_EXECUTABLE})
set(CPACK_PACKAGE_VENDOR "Markpaste")
set(CPACK_PACKAGE_CONTACT "Andrey Che <metallicvrn@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Capture, mark and paste screenshots with comments into any chat")
set(CPACK_PACKAGE_DESCRIPTION "Press a hotkey, select an area, put numbered marks A1, A2... on it and
comment each one. One more hotkey pastes the whole set - images and text -
into any chat or AI assistant.")
set(CPACK_PACKAGE_HOMEPAGE_URL ${MARKPASTE_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_STRIP_FILES ON)
if(WIN32)
    # Portable ZIP. The installer is the installer target (NSIS, ADR 0023); auto-update (Velopack) later
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_SYSTEM_NAME "win64")
    set(CPACK_PACKAGE_FILE_NAME "Markpaste-${PROJECT_VERSION}-win64-portable")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX "")   # everything right in Markpaste-…-portable/, no usr/local
    set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/package)
else()
    set(CPACK_GENERATOR "TGZ")
endif()

find_program(MARKPASTE_RPMBUILD rpmbuild)
find_program(MARKPASTE_DPKG_SHLIBDEPS dpkg-shlibdeps)
if(MARKPASTE_RPMBUILD AND NOT WIN32)
    list(APPEND CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    # Portals: capture, hotkeys, Ctrl+V; without them the program runs in a reduced mode
    set(CPACK_RPM_PACKAGE_REQUIRES "xdg-desktop-portal")
    # Shared directories belong to other packages (filesystem, hicolor-icon-theme)
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        ${CMAKE_INSTALL_FULL_DATADIR}/applications
        ${CMAKE_INSTALL_FULL_DATADIR}/metainfo
        ${CMAKE_INSTALL_FULL_DATADIR}/icons
        ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor
        ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable
        ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps
        ${CMAKE_INSTALL_FULL_DATADIR}/doc
        ${CMAKE_INSTALL_FULL_MANDIR}
        ${CMAKE_INSTALL_FULL_MANDIR}/man1)
endif()

if(MARKPASTE_DPKG_SHLIBDEPS AND NOT WIN32)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "xdg-desktop-portal")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "xdg-desktop-portal-kde | xdg-desktop-portal-gnome")
endif()

include(CPack)
