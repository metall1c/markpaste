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
)
target_link_libraries(markpaste_ui_tests PRIVATE markpaste_ui Qt6::Test GTest::gtest)
gtest_discover_tests(markpaste_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
