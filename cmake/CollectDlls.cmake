# Скрипт установки: копирует в каталог программы DLL, которые нужны указанным .exe и .dll.
# file(GET_RUNTIME_DEPENDENCIES) при кросс-сборке приводит имена DLL к нижнему регистру и
# не находит Qt6Core.dll на регистрозависимой ФС Linux — поэтому обходим импорт сами.
#
# Переменные: BINARIES — с чего начать, DLL_DIR — где лежат DLL, OBJDUMP, STRIP, DESTINATION.
# Копии DLL очищаются от отладочных данных: в пакетах MinGW они есть, а пользователю не нужны
# (libstdc++-6.dll — 29 МБ с ними и 2 МБ без).

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
        # Системные DLL Windows (kernel32, user32, d3d11…) в каталоге MinGW не лежат — их не трогаем
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
