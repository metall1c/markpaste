# 0008. Пакет .rpm через CPack, значок и автозапуск

- Статус: принято (2026-10-19)
- Требования ТЗ: разд. 11 («Сборка и распространение», автозапуск, идентификатор), APP-3, NFR-3.

## Решение

- **Пакеты — CPack** из того же CMake: RPM (если есть `rpmbuild`), DEB (если есть `dpkg-shlibdeps`), всегда TGZ. Отдельного .spec нет: для OBS (ТЗ разд. 11) он понадобится позже и может звать тот же `cmake --install`.
- **Зависимости** — автоматические по `.so` (rpm `AUTOREQ`, `dpkg-shlibdeps`), плюс `xdg-desktop-portal` вручную: без него нет ни захвата на GNOME, ни клавиш, ни Ctrl+V.
- **Абсолютный `Exec`** в .desktop-файле из `CMAKE_INSTALL_FULL_BINDIR`: KWin сверяет его с исполняемым файлом процесса (`X-KDE-DBUS-Restricted-Interfaces`). Поэтому пакет собирается с `-DCMAKE_INSTALL_PREFIX=/usr`, и `CPACK_PACKAGING_INSTALL_PREFIX` берётся из него же — иначе путь в пакете и в Exec разойдутся.
- **Значок** — SVG `data/icons/app.svg`, ставится как `hicolor/scalable/apps/su._525.Markpaste.svg`. Он повторяет значок, который `AppIcon.cpp` рисует кодом для трея и окон; программа файл значка не читает, он нужен меню приложений и центрам приложений.
- **AppStream metainfo** — `share/metainfo/su._525.Markpaste.metainfo.xml`, чтобы пакет был виден в Discover и GNOME Software.
- **Автозапуск** — файл `$XDG_CONFIG_HOME/autostart/su._525.Markpaste.desktop` (спецификация XDG Autostart), не значение в настройках программы: так его видит и может выключить и сама система (KDE «Автозапуск», GNOME Tweaks). Включённым считается файл без `Hidden=true` и без `X-GNOME-Autostart-enabled=false`. `Exec` — путь текущего процесса. Код — `src/platform/common/Autostart.cpp`; для Windows там заглушка (ключ Run — с Windows-версией).
- **Лицензия** — GPL-3.0-or-later (ТЗ разд. 11, CLAUDE.md), файл `LICENSE`.

## Рассмотренные варианты

- **Свой .spec для rpm.** Точнее управляет пакетом, но дублирует правила установки и не даёт .deb. Для ручной сборки CPack достаточно.
- **Относительный `Exec=markpaste`.** Проще для перемещаемых пакетов, но KWin тогда не узнаёт программу и не пускает к ScreenShot2 без диалога.
- **Автозапуск через портал Background** (`RequestBackground` с `autostart: true`). Нужен для Flatpak; для пакета из репозитория файл в `~/.config/autostart` проще и работает во всех окружениях.

## Последствия

- rpm привязан к версии Qt сборки; собирать для каждого выпуска Fedora отдельно.
- Если включить автозапуск из сборки разработчика, в `Exec` попадёт путь к `build/`.
- Подпись пакетов GPG и репозиторий (OBS) — ещё не сделаны.
