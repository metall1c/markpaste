# Установщик Windows на NSIS (ADR 0023): cmake --build build-win --target installer
# → build-win/package/Markpaste-<версия>-win64-setup.exe
#
# Своя схема .nsi, а не генератор NSIS из CPack: шаблон CPack ставит программу для всех
# пользователей с правами администратора, а ТЗ (разд. 11) требует установку в профиль.

find_program(MARKPASTE_MAKENSIS makensis)
if(NOT MARKPASTE_MAKENSIS)
    message(STATUS "makensis not found: the Windows installer target is disabled (dnf install mingw32-nsis)")
    return()
endif()

set(MARKPASTE_INSTALLER_DIR ${CMAKE_BINARY_DIR}/installer)
set(MARKPASTE_INSTALLER_STAGE ${MARKPASTE_INSTALLER_DIR}/stage)
set(MARKPASTE_UNINSTALL_LIST ${MARKPASTE_INSTALLER_DIR}/uninstall-files.nsh)
set(MARKPASTE_INSTALLER_OUT ${CMAKE_BINARY_DIR}/package/Markpaste-${PROJECT_VERSION}-win64-setup.exe)
set(MARKPASTE_ICO ${PROJECT_SOURCE_DIR}/data/icons/app.ico)

# Окно лицензии NSIS — RichEdit: строки только с LF он показывает одной простынёй
file(READ ${PROJECT_SOURCE_DIR}/LICENSE license_text)
string(REPLACE "\n" "\r\n" license_text "${license_text}")
set(MARKPASTE_LICENSE_CRLF ${MARKPASTE_INSTALLER_DIR}/LICENSE.txt)
file(WRITE ${MARKPASTE_LICENSE_CRLF} "${license_text}")

configure_file(${PROJECT_SOURCE_DIR}/data/windows/installer.nsi.in ${MARKPASTE_INSTALLER_DIR}/markpaste.nsi @ONLY)

add_custom_target(installer
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${MARKPASTE_INSTALLER_STAGE}
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${MARKPASTE_INSTALLER_STAGE} --strip
    COMMAND ${CMAKE_COMMAND} -DSTAGE=${MARKPASTE_INSTALLER_STAGE} -DOUT=${MARKPASTE_UNINSTALL_LIST}
            -P ${PROJECT_SOURCE_DIR}/cmake/NsisFileList.cmake
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/package
    COMMAND ${MARKPASTE_MAKENSIS} -V2 ${MARKPASTE_INSTALLER_DIR}/markpaste.nsi
    DEPENDS markpaste
    COMMENT "Building ${MARKPASTE_INSTALLER_OUT}"
    VERBATIM)
