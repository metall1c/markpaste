# 0017. Windows-сборка: MinGW на Fedora, тесты под Wine, портативный ZIP

- Статус: принято (2026-10-20)
- Требования ТЗ: разд. 7 (Qt на Windows — свежий, через aqtinstall; vcpkg), разд. 11 (Setup.exe с автообновлением и portable ZIP), разд. 12 (сборка Setup.exe на чистом образе Windows), NFR-3 (установщик до 40 МБ), NFR-12

## Контекст

Машина разработчика — Fedora 44, Windows рядом нет. Windows-версию надо собирать и проверять, не выходя из Linux, а релизную сборку — потом в CI на Windows.

## Решение

- **Кросс-сборка MinGW-w64 на Fedora.** Пакеты `mingw64-gcc-c++` 16.2 и `mingw64-qt6-*` 6.11.2 (Qt — той же версии, что системный на Fedora). Тулчейн — свой `cmake/mingw64-fedora.cmake`, без обёртки `mingw64-cmake`: полные пути к компиляторам, и `QT_CHAINLOAD_TOOLCHAIN_FILE` указывает на него же (docs/problems.md, 2026-10-20). Команды — docs/build.md.
- **googletest** для MinGW в Fedora нет: при отсутствии системного `GTest` берём исходники v1.17.0 через `FetchContent` (с проверкой SHA-256). На Linux по-прежнему системный `gtest-devel`.
- **Тесты под Wine 11.** `markpaste_core_tests` и `markpaste_ui_tests` — на модуле `offscreen`, как на Linux. Отдельный `markpaste_windows_tests` — на настоящем модуле `windows`: буфер OLE и его форматы, хук, `RegisterHotKey`, GDI-захват. Сквозная проверка программы — Wine в Xvfb с xfwm4 и xdotool (docs/build.md).
- **Портативный ZIP** — `cmake --build build-win --target package` → `Markpaste-<версия>-win64-portable.zip`: `markpaste.exe`, DLL Qt и MinGW, плагины (`qwindows`, `qmodernwindowsstyle`, `qjpeg`, `qico`, `qsvg`, `qsvgicon`, `qschannelbackend`), переводы Qt, `qt.conf`. `windeployqt` в пакетах Fedora нет; `file(GET_RUNTIME_DEPENDENCIES)` при кросс-сборке приводит имена DLL к нижнему регистру и не находит `Qt6Core.dll` на регистрозависимой ФС — поэтому зависимости обходит свой скрипт `cmake/CollectDlls.cmake` по таблице импорта (`objdump -p`) и очищает копии от отладочных данных.
- **Ресурсы .exe**: значок `data/icons/app.ico` (из `app.svg`, 16–256 px), сведения о версии, манифест (`asInvoker`, Windows 10/11, длинные пути, UTF-8). Программа без окна консоли (`WIN32_EXECUTABLE`).

## Рассмотренные варианты

- **MSVC + Qt из aqtinstall, как в ТЗ.** Нужна Windows (или виртуальная машина с лицензией) — для CI это правильный путь, для ежедневной разработки на Fedora нет.
- **Qt из MSYS2 под Wine.** Тот же MinGW, но установка и обновление сложнее, чем `dnf install`.
- **Сборка установщика Velopack сейчас.** Сначала — проверить программу на настоящей Windows; установщик и автообновление (APP-7) — следующий шаг.

## Последствия и ограничения

- **Размер.** Qt из Fedora собран с ICU (`icudata77.dll` — 32 МБ) и тянет OpenSSL, GLib, fontconfig: каталог программы — 81 МБ, ZIP — 35 МБ. Для NFR-3 (до 40 МБ установщик) релиз собирать с официальным Qt без ICU (Windows 10+ даёт ICU сам) — это и есть путь ТЗ через aqtinstall в CI.
- Сборка MinGW — для разработки и проверки. Релизная (MSVC, подпись SignPath, Velopack) — в CI на Windows, ещё не сделана.
- Wine — не Windows: DXGI Desktop Duplication в нём не реализован, UIPI и «Ножниц» нет, без оконного менеджера скрытые окна остаются на экране. Результаты под Wine отмечены в platform-matrix.md отдельно от настоящей Windows.
