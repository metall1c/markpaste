# Платформенный слой. Только здесь допустим код под конкретную ОС (NFR-11).
# Интерфейсы и типы — их видит ядро.
add_library(pointshot_platform_api STATIC
    PlatformTypes.h PlatformTypes.cpp
    Interfaces.h
)
target_include_directories(pointshot_platform_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(pointshot_platform_api PUBLIC Qt6::Core Qt6::Gui)

# Реализации. Ядро их не видит, их собирает PlatformFactory.
add_library(pointshot_platform STATIC
    PlatformFactory.h PlatformFactory.cpp
    common/QtClipboard.h common/QtClipboard.cpp
    common/QtScreenCapture.h common/QtScreenCapture.cpp
    common/FallbackCapture.h common/FallbackCapture.cpp
    WindowRoles.h common/WindowRoles.cpp
    Paths.h common/Paths.cpp
)
target_link_libraries(pointshot_platform PUBLIC pointshot_platform_api Qt6::Gui Qt6::Widgets)
if(LayerShellQt_FOUND)
    target_link_libraries(pointshot_platform PRIVATE LayerShellQt::Interface)
    target_compile_definitions(pointshot_platform PRIVATE POINTSHOT_HAVE_LAYERSHELLQT)
endif()

if(POINTSHOT_LINUX)
    # Клиентский код протоколов Wayland для нашего отдельного подключения.
    function(pointshot_wayland_protocol target xml)
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

    pointshot_wayland_protocol(pointshot_platform
        ${WAYLAND_PROTOCOLS_DIR}/staging/ext-data-control/ext-data-control-v1.xml)

    target_sources(pointshot_platform PRIVATE
        linux/FdIo.h linux/FdIo.cpp
        linux/portal/PortalBus.h linux/portal/PortalBus.cpp
        linux/portal/PortalRemoteDesktop.h linux/portal/PortalRemoteDesktop.cpp
        linux/portal/PortalScreenshot.h linux/portal/PortalScreenshot.cpp
        linux/portal/PortalGlobalShortcuts.h linux/portal/PortalGlobalShortcuts.cpp
        linux/kde/KWinScreenshot.h linux/kde/KWinScreenshot.cpp
        linux/wayland/WaylandConnection.h linux/wayland/WaylandConnection.cpp
        linux/wayland/DataControlClipboard.h linux/wayland/DataControlClipboard.cpp
    )
    target_include_directories(pointshot_platform PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(pointshot_platform PUBLIC Qt6::DBus Qt6::Concurrent PkgConfig::WAYLAND_CLIENT)
endif()
