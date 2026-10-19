add_executable(pointshot-uipreview main.cpp)
target_link_libraries(pointshot-uipreview PRIVATE pointshot_ui)
qt_add_translations(pointshot-uipreview
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/pointshot_ru.ts
    RESOURCE_PREFIX /i18n)
