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

`spdlog-devel` и `python3-dbusmock` поставлены, но пока не используются: логи идут через `QLoggingCategory`, тестов порталов в CI ещё нет.

## Что получается

| Файл | Что это |
| --- | --- |
| `build/src/app/markpaste` | Программа |
| `build/tests/markpaste_core_tests` | Тесты ядра (GoogleTest): модель, текст, запекание, оркестратор вставки |
| `build/tests/markpaste_ui_tests` | Тесты окон (GoogleTest + QtTest, offscreen): клавиши редактора (Ctrl+Enter, Esc) и загрузка русского перевода. `QTest::keyClicks` не умеет кириллицу — вводить текст через `insertPlainText` |
| `build/tools/pastetest/markpaste-pastetest` | Прототип вставки и диагностика платформы (`--check`, `--self-test`, `--capture kwin\|portal`, `--mode 1\|2`) |
| `tools/dev/kwin-probe.py "команда" [фильтр]` | Отправляет команду работающей программе и каждые 250 мс читает у KWin геометрию её окон. Для замеров на живой сессии KDE. Отладочные команды сокета: `debug-move-strip <экран> <left\|right> <отступ>`, `debug-ghost <x> <y> <w> <h>` |
| `build/tools/uipreview/markpaste-uipreview DIR` | Рисует ленту и редактор на тестовом наборе без дисплея и сохраняет PNG в DIR |

## Подводные камни

- **.desktop-файл нужен уже при разработке.** Без него портал отвечает «App info not found» на регистрацию app id, а KWin не пускает к ScreenShot2. Цель `dev-install-desktop` ставит `~/.local/share/applications/su._525.Markpaste.desktop` с `Exec` на бинарник из `build/` и обновляет кэш KDE (`kbuildsycoca6`). Бинарник из `Exec` должен существовать, иначе GLib отбрасывает файл (docs/problems.md).
- **Логи.** Когда stderr — не терминал, Qt пишет в journald. Для разработки:
  `QT_FORCE_STDERR_LOGGING=1 QT_LOGGING_RULES="markpaste.*=true" ./build/src/app/markpaste`.
  Без перенаправления лог — в `journalctl --user -f`.
- **Управление запущенным экземпляром:** `markpaste --capture | --paste | --copy | --show | --settings | --clear | --quit`.
- **`pkill -f build/src/app/markpaste` убивает и саму оболочку**, если путь есть в её командной строке. Используйте `pkill -x markpaste`.
- **Переводы:** строки обновляются `cmake --build build --target update_translations` (lupdate), файлы — `i18n/*.ts`. `.qm` встраиваются в программу под `:/i18n`.
- **API Qt не новее 6.4.** На Fedora стоит 6.11, поэтому компилятор не поймает новые вызовы. Защита — будущая сборка в CI на Ubuntu 24.04 (Qt 6.4.2). Уже используется только то, что есть в 6.4: `QFuture::then/unwrap`, `QPromise`, `qt_add_translations`.
