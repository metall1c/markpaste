# Ядро: модель, текст сообщения, запекание. Без кода под конкретную ОС (NFR-11).
add_library(pointshot_core STATIC
    core/CaptureSet.h core/CaptureSet.cpp
    core/CaptureSetModel.h core/CaptureSetModel.cpp
    core/MessageBuilder.h core/MessageBuilder.cpp
    render/ShotRenderer.h render/ShotRenderer.cpp
    paste/PasteOrchestrator.h paste/PasteOrchestrator.cpp
    paste/TempFiles.h paste/TempFiles.cpp
)
target_include_directories(pointshot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pointshot_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent pointshot_platform_api)

add_subdirectory(platform)

# Интерфейс: оверлей, редактор, лента. Без кода под конкретную ОС.
add_library(pointshot_ui STATIC
    ui/CaptureOverlay.h ui/CaptureOverlay.cpp
    ui/CommentPopup.h ui/CommentPopup.cpp
    ui/ShotCanvas.h ui/ShotCanvas.cpp
    ui/ShotEditor.h ui/ShotEditor.cpp
    ui/StripWindow.h ui/StripWindow.cpp
    ui/MoveGhost.h ui/MoveGhost.cpp
)
target_link_libraries(pointshot_ui PUBLIC pointshot_core pointshot_platform Qt6::Widgets)

add_executable(pointshot
    app/main.cpp
    app/AppController.h app/AppController.cpp
    app/AppIcon.h app/AppIcon.cpp
    app/Settings.h app/Settings.cpp
    app/SettingsDialog.h app/SettingsDialog.cpp
    app/SingleInstance.h app/SingleInstance.cpp
)
set_target_properties(pointshot PROPERTIES
    OUTPUT_NAME ${POINTSHOT_EXECUTABLE}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src/app)
target_link_libraries(pointshot PRIVATE pointshot_ui Qt6::Network)
target_compile_definitions(pointshot PRIVATE
    POINTSHOT_APP_ID="${POINTSHOT_APP_ID}"
    POINTSHOT_VERSION="${PROJECT_VERSION}")
install(TARGETS pointshot RUNTIME DESTINATION bin)

# Переводы (APP-5, NFR-10): .ts в i18n/, .qm встраиваются в программу под :/i18n.
# Обновить строки: cmake --build build --target update_translations
qt_add_translations(pointshot
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/pointshot_ru.ts ${PROJECT_SOURCE_DIR}/i18n/pointshot_en.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -locations none -no-obsolete)
