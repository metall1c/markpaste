# Portable Windows build (spec section 11): markpaste.exe, Qt and MinGW DLLs, Qt plugins and
# translations in one directory. Fedora packages have no windeployqt, so dependencies are
# collected by hand from the import table (CollectDlls.cmake).
#
# Included from the root CMakeLists.txt when WIN32. MARKPASTE_QT_PREFIX is the Qt root for
# Windows (…/sys-root/mingw); DLLs are taken from its bin.

set(MARKPASTE_QT_PLUGINS
    platforms/qwindows.dll
    styles/qmodernwindowsstyle.dll
    imageformats/qjpeg.dll
    imageformats/qico.dll
    imageformats/qsvg.dll
    iconengines/qsvgicon.dll
    tls/qschannelbackend.dll   # HTTPS for the update check: system Schannel, no OpenSSL
)

foreach(plugin IN LISTS MARKPASTE_QT_PLUGINS)
    get_filename_component(dir ${plugin} DIRECTORY)
    get_filename_component(file ${plugin} NAME)
    install(FILES ${MARKPASTE_QT_PREFIX}/lib/qt6/plugins/${plugin} DESTINATION plugins/${dir})
    install(CODE "execute_process(COMMAND \"${CMAKE_STRIP}\" --strip-unneeded \"\${CMAKE_INSTALL_PREFIX}/plugins/${dir}/${file}\")")
endforeach()

install(FILES
    ${MARKPASTE_QT_PREFIX}/share/qt6/translations/qtbase_ru.qm
    ${MARKPASTE_QT_PREFIX}/share/qt6/translations/qtbase_en.qm
    DESTINATION translations)

# Qt looks for plugins and translations relative to qt.conf next to the .exe
file(WRITE ${CMAKE_BINARY_DIR}/qt.conf "[Paths]\nPrefix=.\nPlugins=plugins\nTranslations=translations\n")
install(FILES ${CMAKE_BINARY_DIR}/qt.conf DESTINATION .)

# DLLs: everything the .exe and plugins pull in, except Windows system libraries
list(TRANSFORM MARKPASTE_QT_PLUGINS PREPEND ${MARKPASTE_QT_PREFIX}/lib/qt6/plugins/ OUTPUT_VARIABLE plugin_paths)
install(CODE "
    set(BINARIES \"$<TARGET_FILE:markpaste>;${plugin_paths}\")
    set(DLL_DIR \"${MARKPASTE_QT_PREFIX}/bin\")
    set(OBJDUMP \"${CMAKE_OBJDUMP}\")
    set(STRIP \"${CMAKE_STRIP}\")
    set(DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
    include(\"${CMAKE_CURRENT_LIST_DIR}/CollectDlls.cmake\")
")
