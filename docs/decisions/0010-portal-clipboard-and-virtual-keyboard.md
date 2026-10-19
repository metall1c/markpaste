# 0010. Буфер через портал Clipboard (GNOME) и Ctrl+V через virtual-keyboard-v1 (Sway, Hyprland)

- Статус: принято (2026-10-19)
- Требования ТЗ: разд. 9 (матрица механизмов), разд. 10 (режим 1, восстановление буфера PST-8), п. 5 плана Linux-версии (docs/status.md).

## Контекст

Режиму 1 нужны две вещи из фона: записать буфер и нажать Ctrl+V. На KDE это ext-data-control-v1 и портал RemoteDesktop. В GNOME (Mutter) нет data-control; в Sway и Hyprland нет портала RemoteDesktop (xdg-desktop-portal-wlr и -hyprland его не реализуют).

## Решение

### Буфер: портал Clipboard

- `PortalClipboard` (`src/platform/linux/portal/`). Работает только внутри сессии RemoteDesktop: до `Start` вызывается `RequestClipboard`, после `Start` портал сообщает `clipboard_enabled`.
- Запись: `SetSelection(mime_types)` → на каждый запрос получателя портал шлёт `SelectionTransfer(mime, serial)` → `SelectionWrite(serial)` даёт fd → данные пишутся в пуле потоков (`writeAllAndClose`, 3 с) → `SelectionWriteDone(serial, ok)`. Подсказка Klipper `x-kde-passwordManagerHint` добавляется, как и в data-control.
- Чтение (сохранение перед вставкой): типы чужого selection — из сигнала `SelectionOwnerChanged`, данные — `SelectionRead(mime)`. Только текст и картинки, как у data-control. Если сигнал ещё не приходил — пробуем `text/plain;charset=utf-8`.
- **Новое свойство `IClipboard::needsInputSession()`.** Для портала оно `true`, и `PasteOrchestrator` и мастер меняют порядок: сначала сессия ввода, потом сохранение буфера; буфер возвращается **до** закрытия сессии, а не после. При ошибке запись буфера тоже уходит раньше `Session.Close` (сообщения D-Bus на одном подключении обрабатываются по порядку).
- Выбор в `PlatformFactory`: data-control → портал Clipboard (если есть и RemoteDesktop, и Clipboard) → QClipboard без фоновой записи. `MARKPASTE_CLIPBOARD=portal` включает портал принудительно (проверка на KDE, где он тоже есть: xdg-desktop-portal-kde 6.7.5).

### Ctrl+V: virtual-keyboard-v1

- `VirtualKeyboardInjector` (`src/platform/linux/wayland/`): `zwp_virtual_keyboard_manager_v1` из нашего подключения Wayland (поток `markpaste-wayland`, ADR 0002). Без диалога и без сессии; `end()` ничего не делает.
- Раскладка — US из libxkbcommon (`evdev/pc105/us`), передаётся через memfd. Ctrl+V — по коду KEY_V (47), плюс явный `modifiers()` с маской Ctrl: так делает wtype, и так модификатор доходит даже до клиентов, которые не следят за состоянием клавиш.
- После создания клавиатуры — roundtrip: композитор, который не доверяет клиенту, отвечает ошибкой протокола `unauthorized` (и рвёт подключение) — узнаём об этом сразу, а не на первой вставке.
- Выбор в `PlatformFactory`: если композитор даёт протокол — он (wlroots), иначе портал RemoteDesktop. Если буфер идёт через портал Clipboard — только RemoteDesktop (буфер живёт в его сессии). `MARKPASTE_INPUT=portal` — принудительно портал.
- XML протокола нет в wayland-protocols: копия `virtual-keyboard-unstable-v1.xml` из wlroots 0.19 (MIT) лежит в `src/platform/linux/wayland/protocols/`.

## Проверки

- virtual-keyboard-v1: вложенный headless Sway 1.11 (wlroots 0.19) на этой машине, `pastetest --self-test` — 3 картинки + текст, PASS в 4 прогонах из 4 (в том же процессе и с приёмником в отдельном процессе). См. docs/build.md, как поднять.
- Портал Clipboard: **не проверен**. `pastetest --portal-clipboard` на KDE остановился на диалоге разрешения RemoteDesktop+Clipboard: экран был заблокирован. Нужно прогнать при пользователе, затем — на GNOME 50.
- Оркестратор: тесты `SessionBoundClipboard*` на заглушках (порядок begin → snapshot → … → write → end).

## Рассмотренные варианты

- **Держать сессию RemoteDesktop открытой всё время, пока программа работает** — тогда буфер через портал жил бы и после вставки. Отказались: окружение всё это время показывает индикатор удалённого управления (ТЗ разд. 9).
- **wtype/ydotool как внешние программы.** Лишняя зависимость; ydotool требует uinput и прав root.
- **virtual-keyboard-v1 с раскладкой пользователя.** Её не узнать без фокуса; US + код клавиши даёт Ctrl+V при любой раскладке.

## Последствия и ограничения

- Через портал Clipboard буфер после вставки, скорее всего, пустеет: selection принадлежит сессии, которая закрывается. Что остаётся в буфере после `Session.Close` — проверит шаг 7 `pastetest --portal-clipboard`.
- Пока наши нажатия идут, окно в Sway получает раскладку US; следующее нажатие настоящей клавиатуры возвращает раскладку пользователя.
- libxkbcommon стал зависимостью сборки (у Qt он уже есть во время работы).
