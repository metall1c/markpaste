# 0001. Главная платформа Linux — Fedora 44 KDE Plasma на Wayland

- Статус: принято (2026-10-19)
- Требования ТЗ: раздел 2 («Linux — упор на Wayland»), раздел 9 («Приоритет: Wayland на Fedora 44»), раздел 13 (этапы)

## Контекст

ТЗ называет эталоном три редакции Fedora 44 на Wayland (GNOME 50, KDE Plasma 6.6, Sway) и считает самой важной и сложной GNOME. Пользователь уточнил приоритет: главная платформа — Fedora 44 KDE Plasma на Wayland. Машина разработки — та же система.

## Решение

- Linux-реализация сначала делается и проверяется на KDE Plasma (Wayland). Потом GNOME и Sway, X11 — последним.
- Работу начинаем с Linux, а не с Windows (в ТЗ этап «Ядро и Windows» шёл первым). Ядро и UI всё равно пишем без привязки к ОС (NFR-11).
- Фактическая версия Plasma на машине — 6.7.5, а не 6.6 из ТЗ: Fedora 44 получила обновление.

## Что проверено сразу (2026-10-19, Plasma 6.7.5, KWin 6.7.5, Qt 6.11.2, xdg-desktop-portal 1.22.1)

- KWin отдаёт обычным клиентам `ext_data_control_manager_v1` (v1), так что программа может менять буфер обмена из фона. Гипотеза ТЗ для KDE подтверждена на уровне протокола.
- `zwp_virtual_keyboard_manager_v1` KWin обычным клиентам **не** отдаёт. Ctrl+V эмулируем через портал RemoteDesktop.
- Порталы: Screenshot v2, ScreenCast v5, GlobalShortcuts v2, RemoteDesktop v2, Clipboard v1, InputCapture v2. Есть `org.freedesktop.host.portal.Registry`. FileTransfer **нет**.
- `org.kde.KWin.ScreenShot2` v5 есть, доступен только «доверенным» программам: в .desktop-файле должно быть `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2` (так сделано у Spectacle).
- `zwlr_layer_shell_v1` v5 есть, LayerShellQt 6.7.5 есть в репозитории.

## Последствия

- На KDE режим 1 (последовательная вставка) основной: data-control для буфера плюс RemoteDesktop для Ctrl+V.
- Быстрый захват на KDE — через KWin ScreenShot2, запасной — портал Screenshot.
- Решения, которые подходят только для KDE, держим за платформенными интерфейсами и `capabilities()`, чтобы GNOME и Sway добавлялись без правок ядра.
