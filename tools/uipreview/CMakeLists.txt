add_executable(markpaste-uipreview main.cpp)
target_link_libraries(markpaste-uipreview PRIVATE markpaste_ui)
qt_add_translations(markpaste-uipreview
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts
    RESOURCE_PREFIX /i18n)
