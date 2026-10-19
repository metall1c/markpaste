# Markpaste

Снимки экрана с пометками — в любой чат одним действием.

Горячая клавиша → выделить область → поставить метки A1, A2… (рамка, стрелка, карандаш) и подписать каждую → ещё одна клавиша вставляет весь набор, картинки и текст, в чат или ИИ-ассистент:

```
Снимок A
A1 Кнопку «Купить» сделать ярче
A2 Цену крупнее
```

Открытый аналог Snapik для Linux (главная платформа — KDE Plasma на Wayland) и, позже, Windows. Без сети, аккаунтов и сбора статистики.

## Установка

Пакет .rpm собирается из исходников, см. [docs/build.md](docs/build.md):

```bash
cmake -S . -B build-pkg -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
      -DMARKPASTE_BUILD_TESTS=OFF -DMARKPASTE_BUILD_TOOLS=OFF
cmake --build build-pkg --target package
sudo dnf install ./build-pkg/markpaste-*.rpm
```

## Документация

- [TZ.md](TZ.md) — техническое задание.
- [docs/status.md](docs/status.md) — что сделано и что осталось.
- [docs/platform-matrix.md](docs/platform-matrix.md) — что проверено на каких окружениях.
- [docs/decisions/](docs/decisions/) — принятые решения.

## Лицензия

GPL-3.0-or-later, см. [LICENSE](LICENSE).
