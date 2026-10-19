add_executable(markpaste-uipreview main.cpp)
target_link_libraries(markpaste-uipreview PRIVATE markpaste_ui)
# Только .qm: цель lupdate у .ts должна быть одна (docs/problems.md)
qt_add_lrelease(markpaste-uipreview
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts
    QM_FILES_OUTPUT_VARIABLE uipreview_qm)
qt_add_resources(markpaste-uipreview uipreview_translations
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${uipreview_qm})
