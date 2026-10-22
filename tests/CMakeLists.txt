# Linux: the system package (gtest-devel, libgtest-dev). For cross-compiling for Windows
# (MinGW on Fedora) there is no package, so sources of the same version are fetched (docs/build.md).
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)   # the Fedora MinGW toolchain enables shared libraries
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(markpaste_core_tests
    test_main.cpp
    test_capture_set_model.cpp
    test_message_builder.cpp
    test_renderer.cpp
    test_paste_orchestrator.cpp
    test_set_storage.cpp
)
target_link_libraries(markpaste_core_tests PRIVATE markpaste_core GTest::gtest)

include(GoogleTest)
# When cross-compiling, tests cannot run right after the build (they are .exe), so the list is taken
# before running ctest. The tests themselves are run on Windows through Wine (docs/build.md).
if(CMAKE_CROSSCOMPILING)
    set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
endif()
# Font rendering needs QGuiApplication; in CI without a display, offscreen.
gtest_discover_tests(markpaste_core_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(markpaste_ui_tests
    test_ui_main.cpp
    test_editor_ui.cpp
    test_platform.cpp
    test_setup_wizard.cpp
    test_app_parts.cpp
    test_strip_ui.cpp
    # Program parts with no dependency on the controller
    ${PROJECT_SOURCE_DIR}/src/app/LogFile.cpp
    ${PROJECT_SOURCE_DIR}/src/app/UpdateChecker.cpp
    ${PROJECT_SOURCE_DIR}/src/app/Settings.cpp
    ${PROJECT_SOURCE_DIR}/src/app/SettingsDialog.cpp
)
target_include_directories(markpaste_ui_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(markpaste_ui_tests PRIVATE MARKPASTE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    MARKPASTE_APP_ID="su._525.MarkpasteTest")
target_link_libraries(markpaste_ui_tests PRIVATE markpaste_ui Qt6::Network Qt6::Test GTest::gtest)
gtest_discover_tests(markpaste_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# Only .qm, without an lupdate target: otherwise update_translations runs two lupdate
# processes on the same markpaste_ru.ts at once and corrupts the file (docs/problems.md).
qt_add_lrelease(markpaste_ui_tests
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/markpaste_ru.ts
    QM_FILES_OUTPUT_VARIABLE markpaste_test_qm)
qt_add_resources(markpaste_ui_tests test_translations
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${markpaste_test_qm})

# Windows platform implementations on the real Qt windows module (under Wine or on Windows)
if(WIN32)
    add_executable(markpaste_windows_tests test_windows.cpp)
    target_include_directories(markpaste_windows_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(markpaste_windows_tests PRIVATE markpaste_platform Qt6::Widgets Qt6::Test GTest::gtest)
    target_compile_definitions(markpaste_windows_tests PRIVATE NOMINMAX UNICODE _UNICODE)
endif()
