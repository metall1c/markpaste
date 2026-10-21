# Core: model, message text, baking. No OS-specific code (NFR-11).
add_library(markpaste_core STATIC
    core/CaptureSet.h core/CaptureSet.cpp
    core/Geometry.h core/Geometry.cpp
    core/CaptureSetModel.h core/CaptureSetModel.cpp
    core/MessageBuilder.h core/MessageBuilder.cpp
    core/SetStorage.h core/SetStorage.cpp
    render/ShotRenderer.h render/ShotRenderer.cpp
    paste/PasteOrchestrator.h paste/PasteOrchestrator.cpp
    paste/TempFiles.h paste/TempFiles.cpp
)
target_include_directories(markpaste_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(markpaste_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent markpaste_platform_api)

add_subdirectory(platform)

# UI: overlay, editor, strip. No OS-specific code.
add_library(markpaste_ui STATIC
    ui/CaptureOverlay.h ui/CaptureOverlay.cpp
    ui/CommentPopup.h ui/CommentPopup.cpp
    ui/ShotCanvas.h ui/ShotCanvas.cpp
    ui/PenPicker.h ui/PenPicker.cpp
    ui/SetupWizard.h ui/SetupWizard.cpp
    ui/ShotEditor.h ui/ShotEditor.cpp
    ui/StripWindow.h ui/StripWindow.cpp
    ui/MoveGhost.h ui/MoveGhost.cpp
    ui/PreviewDialog.h ui/PreviewDialog.cpp
    ui/Theme.h ui/Theme.cpp
    ui/Controls.h ui/Controls.cpp
    ui/AboutDialog.h ui/AboutDialog.cpp
)
target_link_libraries(markpaste_ui PUBLIC markpaste_core markpaste_platform Qt6::Widgets Qt6::Svg)
# UI icons: :/icons/ui/<name>.svg
file(GLOB MARKPASTE_UI_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/data/icons/ui/*.svg)
qt_add_resources(markpaste_ui ui_icons
    PREFIX /icons/ui
    BASE ${PROJECT_SOURCE_DIR}/data/icons/ui
    FILES ${MARKPASTE_UI_ICONS})

add_executable(markpaste
    app/main.cpp
    app/AppController.h app/AppController.cpp
    app/AppIcon.h app/AppIcon.cpp
    app/LogFile.h app/LogFile.cpp
    app/Settings.h app/Settings.cpp
    app/SettingsDialog.h app/SettingsDialog.cpp
    app/SingleInstance.h app/SingleInstance.cpp
    app/UpdateChecker.h app/UpdateChecker.cpp
)
set_target_properties(markpaste PROPERTIES
    OUTPUT_NAME ${MARKPASTE_EXECUTABLE}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src/app)
target_link_libraries(markpaste PRIVATE markpaste_ui Qt6::Network)
target_compile_definitions(markpaste PRIVATE
    MARKPASTE_APP_ID="${MARKPASTE_APP_ID}"
    MARKPASTE_VERSION="${PROJECT_VERSION}"
    MARKPASTE_UPDATE_URL="${MARKPASTE_UPDATE_URL}"
    MARKPASTE_RELEASES_URL="${MARKPASTE_RELEASES_URL}"
    MARKPASTE_HOMEPAGE_URL="${MARKPASTE_HOMEPAGE_URL}")
install(TARGETS markpaste RUNTIME DESTINATION ${MARKPASTE_INSTALL_BINDIR})

if(WIN32)
    # No console window; icon, version and manifest are in the .exe resources
    set_target_properties(markpaste PROPERTIES WIN32_EXECUTABLE ON)
    set(MARKPASTE_ICO ${PROJECT_SOURCE_DIR}/data/icons/app.ico)
    set(MARKPASTE_MANIFEST ${PROJECT_SOURCE_DIR}/data/windows/markpaste.exe.manifest)
    configure_file(${PROJECT_SOURCE_DIR}/data/windows/markpaste.rc.in ${CMAKE_CURRENT_BINARY_DIR}/markpaste.rc @ONLY)
    target_sources(markpaste PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/markpaste.rc)
endif()

# Translations (APP-5, NFR-10): .ts in i18n/, .qm are embedded into the program under :/i18n.
# Update the strings: cmake --build build --target update_translations
qt_add_translations(markpaste
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts ${PROJECT_SOURCE_DIR}/i18n/markpaste_en.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -locations none -no-obsolete)
