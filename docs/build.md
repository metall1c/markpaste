# Сборка и окружение разработчика

## Fedora 44 (главная платформа)

Проверено 2026-10-19: Fedora 44 KDE, GCC 16.2.1, CMake 4.3.0, Qt 6.11.2.

```bash
sudo dnf install -y --setopt=install_weak_deps=False \
  gcc-c++ cmake ninja-build \
  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtwayland-devel qt6-qttools-devel qt6-linguist \
  layer-shell-qt-devel wayland-devel wayland-protocols-devel libxkbcommon-devel \
  gtest-devel gmock-devel \
  xcb-util-keysyms-devel libXtst-devel        # для будущей X11-реализации

cmake -S . -B build -G Ninja
cmake --build build
./build/tests/markpaste_core_tests
cmake --build build --target dev-install-desktop   # обязательно, см. ниже
./build/src/app/markpaste
```

`libxkbcommon-devel` нужен для раскладки virtual-keyboard-v1 (Sway, Hyprland). `spdlog-devel` и `python3-dbusmock` поставлены, но пока не используются: логи идут через `QLoggingCategory`, тестов порталов в CI ещё нет.

## Пакет .rpm (и .tar.gz)

Проверено 2026-10-19 на Fedora 44: CPack 4.3.0, rpm-build 6.0.2.

```bash
sudo dnf install -y rpm-build
cmake -S . -B build-pkg -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
      -DMARKPASTE_BUILD_TESTS=OFF -DMARKPASTE_BUILD_TOOLS=OFF
cmake --build build-pkg --target package
# → build-pkg/markpaste-0.1.0-1.fc44.x86_64.rpm, build-pkg/markpaste-0.1.0-Linux.tar.gz
sudo dnf install ./build-pkg/markpaste-0.1.0-1.fc44.x86_64.rpm
```

- **`-DCMAKE_INSTALL_PREFIX=/usr` обязателен.** Путь в `Exec` .desktop-файла абсолютный и берётся из префикса установки: KWin пускает к ScreenShot2, только если он совпадает с исполняемым файлом процесса. С префиксом по умолчанию (`/usr/local`) пакет поставит программу в `/usr/local/bin`.
- В пакете: `/usr/bin/markpaste`, `.desktop`-файл, значок SVG (`hicolor/scalable`), AppStream metainfo, LICENSE. Переводы встроены в программу.
- Зависимости от библиотек (Qt 6, libwayland-client, LayerShellQt) rpm находит сам по связанным `.so`; вручную добавлен только `xdg-desktop-portal`. LayerShellQt становится обязательной зависимостью, если он был при сборке.
- Пакет привязан к версии Qt сборки (`libQt6Core.so.6(Qt_6.11)`): rpm для Fedora 44 собирать на Fedora 44.
- Проверка: `rpmlint build-pkg/*.rpm` — 0 ошибок, одно предупреждение (нет man-страницы); `desktop-file-validate`, `appstreamcli validate --no-net` — без замечаний.
- **.deb** включается автоматически, если есть `dpkg-shlibdeps` (собирать на Ubuntu/Debian). На Fedora собирается только rpm и tar.gz. На Ubuntu 24.04 пока не проверено.
- **Проверка новой версии** (ADR 0014) ходит по `MARKPASTE_UPDATE_URL` (по умолчанию — API последнего релиза на gitlab.525.su). Сборкам дистрибутивов, которые обновляются своим путём, — `-DMARKPASTE_UPDATE_URL=` (пусто): проверки нет. Страница в уведомлении — `MARKPASTE_RELEASES_URL`.
- **Разработчику:** `~/.local/share/applications/su._525.Markpaste.desktop` от `dev-install-desktop` важнее системного из пакета (каталог пользователя ищется первым) и указывает на `build/`. Чтобы проверить установленный пакет, этот файл надо удалить.

## Проверка под Sway без отдельной системы

```bash
sudo dnf install -y --setopt=install_weak_deps=False sway   # поставлен 2026-10-19, sway 1.11
printf 'output HEADLESS-1 resolution 1280x800\n' > /tmp/sway-headless.conf
WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WAYLAND_DISPLAY= sway -c /tmp/sway-headless.conf &
# новый сокет — следующий свободный, обычно wayland-1
WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ./build/tools/pastetest/markpaste-pastetest --check
WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ./build/tools/pastetest/markpaste-pastetest --self-test
```

Порталы при этом остаются от KDE (общая сессионная шина), поэтому так проверяются только протоколы Wayland: data-control, virtual-keyboard, layer-shell.

- `pastetest --portal-clipboard` — проверка портала Clipboard: пишет через портал и читает обратно через data-control, затем показывает, что осталось в буфере после закрытия сессии. Первый раз KDE спрашивает разрешение.
- Переменные для проверки запасных путей на KDE: `MARKPASTE_CLIPBOARD=portal` (буфер через портал), `MARKPASTE_INPUT=portal` (Ctrl+V только через RemoteDesktop).

## Проверка X11 без X11-сессии

```bash
Xvfb :97 -screen 0 1280x800x24 -nolisten tcp &
export DISPLAY=:97 QT_QPA_PLATFORM=xcb WAYLAND_DISPLAY= XDG_SESSION_TYPE=x11
./build/tools/pastetest/markpaste-pastetest --x11-hotkeys   # нажимает через xdotool
./build/tools/pastetest/markpaste-pastetest --self-test
```

- Нужны `xorg-x11-server-Xvfb`, `xdotool`, для сборки — `libxcb-devel xcb-util-keysyms-devel` (xcb-xtest входит в libxcb).
- Чтобы запустить саму программу рядом с работающей копией, нужны свои `XDG_RUNTIME_DIR` (сокет единственного экземпляра) и `XDG_CONFIG_HOME`. Путь сокета не длиннее 108 байт (`sun_path`), иначе «the socket cannot be created» — каталог в `/run/user/$UID/` подходит, длинный путь во временном каталоге — нет.
- Группу с именем `general`/`General` в `QSettings` не использовать: в INI она пишется как `[%General]` (`[General]` — корень), а при следующем чтении файла ключ получает имя `General/…` — с большой буквы. Ключ `general/x` после перезапуска не находится (docs/problems.md, 2026-10-20). Флаги «уже показано» теперь в группе `once`.
- Останавливать тестовые серверы — по PID (`pgrep -x Xvfb`), не `pkill -f`: шаблон совпадает с командной строкой самой оболочки.

## Что получается

| Файл | Что это |
| --- | --- |
| `build/src/app/markpaste` | Программа |
| `build/tests/markpaste_core_tests` | Тесты ядра (GoogleTest): модель, текст, запекание, оркестратор вставки, набор на диске и сохранение в папку |
| `build/tests/markpaste_ui_tests` | Тесты окон (GoogleTest + QtTest, offscreen): клавиши редактора (Ctrl+Enter, Esc), рисование стрелки и карандаша мышью, панель пера, автозапуск, мастер первого запуска на заглушках платформы (`tests/FakePlatform.h`), предпросмотр сообщения, загрузка русского перевода. Сюда же собраны `app/LogFile.cpp` и `app/UpdateChecker.cpp` (ротация журнала, сравнение версий). `QTest::keyClicks` не умеет кириллицу — вводить текст через `insertPlainText` |
| `build/tools/pastetest/markpaste-pastetest` | Прототип вставки и диагностика платформы (`--check`, `--self-test`, `--capture kwin\|portal`, `--mode 1\|2`) |
| `tools/dev/kwin-probe.py "команда" [фильтр]` | Отправляет команду работающей программе и каждые 250 мс читает у KWin геометрию её окон. Для замеров на живой сессии KDE. Отладочные команды сокета: `debug-move-strip <экран> <left\|right> <отступ>`, `debug-ghost <x> <y> <w> <h>` |
| `build/tools/uipreview/markpaste-uipreview DIR` | Рисует ленту, редактор на тестовом наборе (рамки, стрелка, карандаш разными перьями, размытие), панели цвета и толщины, мастер первого запуска, окно предпросмотра и меню ленты без дисплея, сохраняет PNG в DIR |
| `MARKPASTE_RENDER_SAMPLE=файл.png build/tests/markpaste_core_tests --gtest_filter='*Sample*'` | Запекает пример всех форм меток в PNG — для глазной проверки рендера |

## Подводные камни

- **.desktop-файл нужен уже при разработке.** Без него портал отвечает «App info not found» на регистрацию app id, а KWin не пускает к ScreenShot2. Цель `dev-install-desktop` ставит `~/.local/share/applications/su._525.Markpaste.desktop` с `Exec` на бинарник из `build/` и обновляет кэш KDE (`kbuildsycoca6`). Бинарник из `Exec` должен существовать, иначе GLib отбрасывает файл (docs/problems.md).
- **Логи.** Когда stderr — не терминал, Qt пишет в journald. Для разработки:
  `QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="markpaste.*=true" ./build/src/app/markpaste`.
  Без перенаправления лог — в `journalctl --user -f`.
- **Управление запущенным экземпляром:** `markpaste --capture | --paste | --copy | --show | --settings | --setup | --clear | --quit`.
- **`pkill -f build/src/app/markpaste` убивает и саму оболочку**, если путь есть в её командной строке. Используйте `pkill -x markpaste`.
- **Переводы:** строки обновляются `cmake --build build --target update_translations` (lupdate), файлы — `i18n/*.ts`. `.qm` встраиваются в программу под `:/i18n`. Каждый `.ts` подключать через `qt_add_translations` только к одной цели, иначе параллельные lupdate портят файл (docs/problems.md); тестам — `qt_add_lrelease`.
- **API Qt не новее 6.4.** На Fedora стоит 6.11, поэтому компилятор не поймает новые вызовы. Защита — будущая сборка в CI на Ubuntu 24.04 (Qt 6.4.2). Уже используется только то, что есть в 6.4: `QFuture::then/unwrap`, `QPromise`, `qt_add_translations`.
