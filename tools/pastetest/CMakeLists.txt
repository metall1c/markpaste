add_executable(pointshot-pastetest main.cpp SelfTest.h SelfTest.cpp)
target_link_libraries(pointshot-pastetest PRIVATE pointshot_core pointshot_platform Qt6::Widgets)
target_compile_definitions(pointshot-pastetest PRIVATE
    POINTSHOT_APP_ID="${POINTSHOT_APP_ID}")
