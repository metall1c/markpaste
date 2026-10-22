add_executable(markpaste-pastetest main.cpp SelfTest.h SelfTest.cpp)
target_link_libraries(markpaste-pastetest PRIVATE markpaste_core markpaste_platform Qt6::Widgets)
markpaste_import_qt_plugins(markpaste-pastetest)
target_compile_definitions(markpaste-pastetest PRIVATE
    MARKPASTE_APP_ID="${MARKPASTE_APP_ID}")
