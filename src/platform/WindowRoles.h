#pragma once


class QWidget;
class QScreen;

namespace ps {

// Особые роли окон, которые на разных системах делаются по-разному (ТЗ разд. 9).
// Вызывать ДО первого show(). Возвращают true, если роль назначена средствами
// композитора (layer-shell); false — окно остаётся обычным, и вызывающий сам
// разворачивает его на весь экран или ставит «поверх».

enum class PanelEdge { Left, Right };

// Положение панели: у левого или правого края, на высоте offset от верха экрана
// (логические px; -1 — по центру), с отступом margin от края.
struct PanelPlacement {
    PanelEdge edge = PanelEdge::Right;
    int offset = -1;
    int margin = 12;
};

bool layerShellAvailable();

// Оверлей захвата: поверх всего, на весь указанный экран, забирает клавиатуру.
bool applyOverlayRole(QWidget *widget, QScreen *screen);

// Прозрачный слой на весь экран поверх всего, не принимает ни мышь, ни клавиатуру.
// Для подсказок вроде контура переносимой ленты. false — layer-shell нет.
bool applyGhostRole(QWidget *widget, QScreen *screen);

// Лента: панель у края экрана поверх окон. Клавиатуру НЕ забирает, поэтому клик по ней
// не уводит фокус из окна чата — кнопка «Вставить» может работать в режиме 1.
bool applyPanelRole(QWidget *widget, QScreen *screen, const PanelPlacement &placement);

// Переставить уже показанную панель (STR-5). Пользователь не может двигать поверхность
// layer-shell мышью — положение задаёт программа. Смена монитора пересоздаёт поверхность.
// false — панель не layer-shell, её двигает оконный менеджер.
bool movePanel(QWidget *widget, QScreen *screen, const PanelPlacement &placement);

} // namespace ps
