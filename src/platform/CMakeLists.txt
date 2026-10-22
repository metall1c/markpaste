# Platform layer. OS-specific code is allowed only here (NFR-11).
# Interfaces and types are visible to the core.
add_library(markpaste_platform_api STATIC
    PlatformTypes.h PlatformTypes.cpp
    Interfaces.h
)
target_include_directories(markpaste_platform_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(markpaste_platform_api PUBLIC Qt6::Core Qt6::Gui)

# Implementations. The core does not see them, PlatformFactory assembles them.
add_library(markpaste_platform STATIC
    PlatformFactory.h PlatformFactory.cpp
    common/QtClipboard.h common/QtClipboard.cpp
    common/QtScreenCapture.h common/QtScreenCapture.cpp
    common/FallbackCapture.h common/FallbackCapture.cpp
    WindowRoles.h common/WindowRoles.cpp
    Paths.h common/Paths.cpp
    Autostart.h common/Autostart.cpp
    Memory.h common/Memory.cpp
    Process.h common/Process.cpp
)
target_link_libraries(markpaste_platform PUBLIC markpaste_platform_api Qt6::Gui Qt6::Widgets)
if(MARKPASTE_STATIC_QT)
    target_compile_definitions(markpaste_platform PRIVATE MARKPASTE_STATIC_QT)   # Process.cpp
endif()
if(LayerShellQt_FOUND)
    target_link_libraries(markpaste_platform PRIVATE LayerShellQt::Interface)
    target_compile_definitions(markpaste_platform PRIVATE MARKPASTE_HAVE_LAYERSHELLQT)
    # Window::setScreen appeared in LayerShellQt 6.4; older ones (Debian 13 has 6.3) take the
    # output from QWindow::screen()
    if(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.4)
        target_compile_definitions(markpaste_platform PRIVATE MARKPASTE_LAYERSHELL_SET_SCREEN)
    endif()
endif()

if(MARKPASTE_LINUX)
    # Client code of the Wayland protocols for our own connection.
    function(markpaste_wayland_protocol target xml)
        get_filename_component(base ${xml} NAME_WE)
        set(header ${CMAKE_CURRENT_BINARY_DIR}/${base}-client-protocol.h)
        set(code ${CMAKE_CURRENT_BINARY_DIR}/${base}-protocol.c)
        add_custom_command(OUTPUT ${header}
            COMMAND ${WAYLAND_SCANNER} client-header ${xml} ${header}
            DEPENDS ${xml} VERBATIM)
        add_custom_command(OUTPUT ${code}
            COMMAND ${WAYLAND_SCANNER} private-code ${xml} ${code}
            DEPENDS ${xml} VERBATIM)
        target_sources(${target} PRIVATE ${header} ${code})
    endfunction()

    markpaste_wayland_protocol(markpaste_platform
        ${WAYLAND_PROTOCOLS_DIR}/staging/ext-data-control/ext-data-control-v1.xml)
    # Not in wayland-protocols: a copy from wlroots (MIT), see the file
    markpaste_wayland_protocol(markpaste_platform
        ${CMAKE_CURRENT_SOURCE_DIR}/linux/wayland/protocols/virtual-keyboard-unstable-v1.xml)

    # The compositor's window menu for our own Qt windows (GNOME "Always on Top"): Qt's xdg_toplevel comes
    # from QPlatformNativeInterface, so private Qt Gui headers; only the request header of xdg-shell is needed,
    # the interface symbols are Qt's
    set(QT_NO_PRIVATE_MODULE_WARNING ON)   # ADR 0026: one call, rebuilt with each distribution Qt
    find_package(Qt6 QUIET OPTIONAL_COMPONENTS GuiPrivate)
    if(TARGET Qt6::GuiPrivate)
        set(xdg_shell_header ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h)
        add_custom_command(OUTPUT ${xdg_shell_header}
            COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml ${xdg_shell_header}
            DEPENDS ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml VERBATIM)
        target_sources(markpaste_platform PRIVATE ${xdg_shell_header})
        target_link_libraries(markpaste_platform PRIVATE Qt6::GuiPrivate)
        target_compile_definitions(markpaste_platform PRIVATE MARKPASTE_HAVE_XDG_WINDOW_MENU)
    else()
        message(STATUS "Qt6::GuiPrivate not found: no window menu for the strip on GNOME")
    endif()

    target_sources(markpaste_platform PRIVATE
        linux/wayland/XdgWindowMenu.h linux/wayland/XdgWindowMenu.cpp
        linux/GnomeKeybindings.h linux/GnomeKeybindings.cpp
        linux/FdIo.h linux/FdIo.cpp
        linux/portal/PortalBus.h linux/portal/PortalBus.cpp
        linux/portal/PortalRemoteDesktop.h linux/portal/PortalRemoteDesktop.cpp
        linux/portal/PortalClipboard.h linux/portal/PortalClipboard.cpp
        linux/portal/PortalScreenshot.h linux/portal/PortalScreenshot.cpp
        linux/portal/PortalGlobalShortcuts.h linux/portal/PortalGlobalShortcuts.cpp
        linux/kde/KWinScreenshot.h linux/kde/KWinScreenshot.cpp
        linux/wayland/WaylandConnection.h linux/wayland/WaylandConnection.cpp
        linux/wayland/DataControlClipboard.h linux/wayland/DataControlClipboard.cpp
        linux/wayland/VirtualKeyboardInjector.h linux/wayland/VirtualKeyboardInjector.cpp
    )
    target_include_directories(markpaste_platform PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(markpaste_platform PUBLIC Qt6::DBus Qt6::Concurrent PkgConfig::WAYLAND_CLIENT)
    target_link_libraries(markpaste_platform PRIVATE PkgConfig::XKBCOMMON)

    if(XCB_FOUND)
        target_sources(markpaste_platform PRIVATE
            linux/x11/X11Keys.h linux/x11/X11Keys.cpp
            linux/x11/X11Hotkeys.h linux/x11/X11Hotkeys.cpp
            linux/x11/X11InputInjector.h linux/x11/X11InputInjector.cpp
        )
        target_link_libraries(markpaste_platform PRIVATE PkgConfig::XCB)
        target_compile_definitions(markpaste_platform PUBLIC MARKPASTE_HAVE_X11)
    else()
        message(STATUS "xcb, xcb-xtest or xcb-keysyms not found: no global hotkeys and Ctrl+V on X11")
    endif()
endif()

if(WIN32)
    target_sources(markpaste_platform PRIVATE
        windows/WinKeys.h windows/WinKeys.cpp
        windows/KeyboardHook.h windows/KeyboardHook.cpp
        windows/WinHotkeys.h windows/WinHotkeys.cpp
        windows/WinInputInjector.h windows/WinInputInjector.cpp
        windows/WinClipboard.h windows/WinClipboard.cpp
        windows/DxgiCapture.h windows/DxgiCapture.cpp
    )
    target_link_libraries(markpaste_platform PUBLIC Qt6::Concurrent)
    target_link_libraries(markpaste_platform PRIVATE d3d11 dxgi dwmapi gdi32 user32 advapi32)
    # No min/max macros and no extra headers from windows.h
    target_compile_definitions(markpaste_platform PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()
