# Замеры NFR-1, NFR-2 без настоящего экрана: docs/platform-matrix.md, раздел «Замеры»
add_executable(markpaste-bench main.cpp)
find_package(Qt6 REQUIRED COMPONENTS Test)
target_link_libraries(markpaste-bench PRIVATE markpaste_ui Qt6::Test)
