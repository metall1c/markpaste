find_package(GTest REQUIRED)

add_executable(markpaste_core_tests
    test_main.cpp
    test_capture_set_model.cpp
    test_message_builder.cpp
    test_renderer.cpp
    test_paste_orchestrator.cpp
)
target_link_libraries(markpaste_core_tests PRIVATE markpaste_core GTest::gtest)

include(GoogleTest)
# Рендер шрифтов требует QGuiApplication; в CI без дисплея — offscreen.
gtest_discover_tests(markpaste_core_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(markpaste_ui_tests
    test_ui_main.cpp
    test_editor_ui.cpp
    test_platform.cpp
)
target_link_libraries(markpaste_ui_tests PRIVATE markpaste_ui Qt6::Test GTest::gtest)
gtest_discover_tests(markpaste_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# Только .qm, без цели lupdate: иначе update_translations запускает два lupdate
# над одним markpaste_ru.ts одновременно и портит файл (docs/problems.md).
qt_add_lrelease(markpaste_ui_tests
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts
    QM_FILES_OUTPUT_VARIABLE markpaste_test_qm)
qt_add_resources(markpaste_ui_tests test_translations
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${markpaste_test_qm})
