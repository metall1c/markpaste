#pragma once

#include <functional>

class QWidget;
class QScreen;

namespace mp {

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

// Лента без layer-shell, которую программа сама ставит к краю экрана (Windows): окно без рамки,
// пользователь тянет его за заголовок ленты, а по окончании перетаскивания вызывается
// onMoveFinished — один раз, а не на каждое движение. false — так нельзя: окно двигает оконный
// менеджер, программа его положение не трогает.
bool watchPanelMoves(QWidget *widget, std::function<void()> onMoveFinished);

// Отдать окну клавиатуру сразу после показа. На Windows система не даёт программе стать
// активной, если последнее событие ввода получила не она — а Print Screen проглатывает наш хук,
// и событие до программы не доходит. Тогда окно только мигает на панели задач, а Esc и Enter
// уходят в прежнее окно.
void forceActivate(QWidget *widget);

} // namespace mp
