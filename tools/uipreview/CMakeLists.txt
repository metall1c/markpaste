# The settings window comes from the program: its sources are built in here too
add_executable(markpaste-uipreview main.cpp
    ${PROJECT_SOURCE_DIR}/src/app/Settings.cpp
    ${PROJECT_SOURCE_DIR}/src/app/SettingsDialog.cpp
    ${PROJECT_SOURCE_DIR}/src/app/LogFile.cpp
    ${PROJECT_SOURCE_DIR}/src/app/AppIcon.cpp)
target_include_directories(markpaste-uipreview PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(markpaste-uipreview PRIVATE MARKPASTE_APP_ID="su._525.MarkpastePreview"
    MARKPASTE_VERSION="${PROJECT_VERSION}" MARKPASTE_HOMEPAGE_URL="${MARKPASTE_HOMEPAGE_URL}")
find_package(Qt6 REQUIRED COMPONENTS Test)
target_link_libraries(markpaste-uipreview PRIVATE markpaste_ui Qt6::Test)
# Only .qm: there must be one lupdate target per .ts (docs/problems.md)
qt_add_lrelease(markpaste-uipreview
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts
    QM_FILES_OUTPUT_VARIABLE uipreview_qm)
qt_add_resources(markpaste-uipreview uipreview_translations
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${uipreview_qm})
