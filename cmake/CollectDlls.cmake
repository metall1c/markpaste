# Install script: copies into the program directory the DLLs that the given .exe and .dll need.
# file(GET_RUNTIME_DEPENDENCIES) lowercases DLL names when cross-compiling and does not find
# Qt6Core.dll on a case-sensitive Linux file system, so we walk the imports ourselves.
#
# Variables: BINARIES (where to start), DLL_DIR (where the DLLs are), OBJDUMP, STRIP, DESTINATION.
# The DLL copies are stripped of debug data: MinGW packages ship it, users do not need it
# (libstdc++-6.dll is 29 MB with it and 2 MB without).

file(GLOB available RELATIVE ${DLL_DIR} ${DLL_DIR}/*.dll)
foreach(name IN LISTS available)
    string(TOLOWER ${name} key)
    set(dll_${key} ${name})
endforeach()

set(queue ${BINARIES})
set(copied)
while(queue)
    list(POP_FRONT queue binary)
    execute_process(COMMAND ${OBJDUMP} -p ${binary} OUTPUT_VARIABLE dump RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "objdump failed for ${binary}")
    endif()
    string(REGEX MATCHALL "DLL Name: [^\n]+" imports "${dump}")
    foreach(line IN LISTS imports)
        string(REPLACE "DLL Name: " "" name "${line}")
        string(STRIP "${name}" name)
        string(TOLOWER "${name}" key)
        # Windows system DLLs (kernel32, user32, d3d11…) are not in the MinGW directory; leave them alone
        if(NOT DEFINED dll_${key} OR key IN_LIST copied)
            continue()
        endif()
        list(APPEND copied ${key})
        file(INSTALL ${DLL_DIR}/${dll_${key}} DESTINATION ${DESTINATION})
        if(STRIP)
            execute_process(COMMAND ${STRIP} --strip-unneeded ${DESTINATION}/${dll_${key}})
        endif()
        list(APPEND queue ${DLL_DIR}/${dll_${key}})
    endforeach()
endwhile()
