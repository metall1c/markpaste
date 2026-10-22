# NFR-1, NFR-2 measurements without a real screen: docs/platform-matrix.md, "Measurements" section
add_executable(markpaste-bench main.cpp)
find_package(Qt6 REQUIRED COMPONENTS Test)
target_link_libraries(markpaste-bench PRIVATE markpaste_ui Qt6::Test)
markpaste_import_qt_plugins(markpaste-bench)
