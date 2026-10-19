# Ядро: модель, текст сообщения, запекание. Без кода под конкретную ОС (NFR-11).
add_library(markpaste_core STATIC
    core/CaptureSet.h core/CaptureSet.cpp
    core/Geometry.h core/Geometry.cpp
    core/CaptureSetModel.h core/CaptureSetModel.cpp
    core/MessageBuilder.h core/MessageBuilder.cpp
    render/ShotRenderer.h render/ShotRenderer.cpp
    paste/PasteOrchestrator.h paste/PasteOrchestrator.cpp
    paste/TempFiles.h paste/TempFiles.cpp
)
target_include_directories(markpaste_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(markpaste_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent markpaste_platform_api)

add_subdirectory(platform)

# Интерфейс: оверлей, редактор, лента. Без кода под конкретную ОС.
add_library(markpaste_ui STATIC
    ui/CaptureOverlay.h ui/CaptureOverlay.cpp
    ui/CommentPopup.h ui/CommentPopup.cpp
    ui/ShotCanvas.h ui/ShotCanvas.cpp
    ui/PenPicker.h ui/PenPicker.cpp
    ui/ShotEditor.h ui/ShotEditor.cpp
    ui/StripWindow.h ui/StripWindow.cpp
    ui/MoveGhost.h ui/MoveGhost.cpp
)
target_link_libraries(markpaste_ui PUBLIC markpaste_core markpaste_platform Qt6::Widgets)

add_executable(markpaste
    app/main.cpp
    app/AppController.h app/AppController.cpp
    app/AppIcon.h app/AppIcon.cpp
    app/Settings.h app/Settings.cpp
    app/SettingsDialog.h app/SettingsDialog.cpp
    app/SingleInstance.h app/SingleInstance.cpp
)
set_target_properties(markpaste PROPERTIES
    OUTPUT_NAME ${MARKPASTE_EXECUTABLE}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src/app)
target_link_libraries(markpaste PRIVATE markpaste_ui Qt6::Network)
target_compile_definitions(markpaste PRIVATE
    MARKPASTE_APP_ID="${MARKPASTE_APP_ID}"
    MARKPASTE_VERSION="${PROJECT_VERSION}")
install(TARGETS markpaste RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Переводы (APP-5, NFR-10): .ts в i18n/, .qm встраиваются в программу под :/i18n.
# Обновить строки: cmake --build build --target update_translations
qt_add_translations(markpaste
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts ${PROJECT_SOURCE_DIR}/i18n/markpaste_en.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -locations none -no-obsolete)
