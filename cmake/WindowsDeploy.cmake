# Портативная сборка Windows (ТЗ разд. 11): markpaste.exe, DLL Qt и MinGW, плагины Qt и
# переводы — в одном каталоге. windeployqt в пакетах Fedora нет, поэтому зависимости
# собираем сами по таблице импорта (CollectDlls.cmake).
#
# Подключается из корневого CMakeLists.txt при WIN32. MARKPASTE_QT_PREFIX — корень Qt для
# Windows (…/sys-root/mingw), DLL берутся из его bin.

set(MARKPASTE_QT_PLUGINS
    platforms/qwindows.dll
    styles/qmodernwindowsstyle.dll
    imageformats/qjpeg.dll
    imageformats/qico.dll
    imageformats/qsvg.dll
    iconengines/qsvgicon.dll
    tls/qschannelbackend.dll   # HTTPS для проверки обновлений — системный Schannel, без OpenSSL
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

# Qt ищет плагины и переводы относительно qt.conf рядом с .exe
file(WRITE ${CMAKE_BINARY_DIR}/qt.conf "[Paths]\nPrefix=.\nPlugins=plugins\nTranslations=translations\n")
install(FILES ${CMAKE_BINARY_DIR}/qt.conf DESTINATION .)

# DLL: всё, что тянут .exe и плагины, кроме системных библиотек Windows
list(TRANSFORM MARKPASTE_QT_PLUGINS PREPEND ${MARKPASTE_QT_PREFIX}/lib/qt6/plugins/ OUTPUT_VARIABLE plugin_paths)
install(CODE "
    set(BINARIES \"$<TARGET_FILE:markpaste>;${plugin_paths}\")
    set(DLL_DIR \"${MARKPASTE_QT_PREFIX}/bin\")
    set(OBJDUMP \"${CMAKE_OBJDUMP}\")
    set(STRIP \"${CMAKE_STRIP}\")
    set(DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
    include(\"${CMAKE_CURRENT_LIST_DIR}/CollectDlls.cmake\")
")
