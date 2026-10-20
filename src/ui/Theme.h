#pragma once

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QString>

class QPainter;
class QPalette;
class QRectF;
class QFontMetricsF;

namespace mp {

// Оформление окон программы (docs/decisions/0019-ui-style.md). Фон и текст — из системной
// палитры, поэтому светлая и тёмная тема берутся из системы. Акцент — из настроек, тот же,
// что у меток: им залиты выбранный инструмент, главная кнопка и значки снимков.
struct UiColors {
    bool dark = false;
    QColor surface;    // фон окна, панель инструментов
    QColor raised;     // второстепенная кнопка, поле комментария, клавиша
    QColor card;       // карточка комментария
    QColor panel;      // боковая панель комментариев
    QColor backdrop;   // подложка под снимком
    QColor border;
    QColor text;
    QColor muted;      // подсказки (контраст с фоном не ниже 4.5:1)
    QColor icon;
    QColor hover;
    QColor pressed;
    QColor accent;     // как задан в настройках: рамки, обводка выбранного
    QColor accentFill; // затемнённый до контраста 4.5:1 с белым: заливки под белым текстом
    QColor onAccent;

    static UiColors from(const QPalette &palette);
};

// Акцент интерфейса — один на программу, меняется вместе с настройками
void setUiAccent(const QColor &accent);
QColor uiAccent();

QColor mixColors(const QColor &a, const QColor &b, qreal t);

enum class Icon {
    Mark, Arrow, Pen, Point, Blur, Crop, Undo, Redo, Trash, Camera, Check,
    ChevronDown, More, Minimize, Copy, Comment, Grip
};

// Значок из набора data/icons/ui нужного цвета; size — в логических пикселях
QPixmap iconPixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio);

// «Клавиша»: текст в скруглённой рамке (подсказки сочетаний, кнопка «Вставить набор Alt+V»)
QSizeF keyCapSize(const QFontMetricsF &fm, const QString &text);
void drawKeyCap(QPainter &p, const QRectF &rect, const QString &text, const QColor &fg, const QColor &bg,
                const QColor &border);

// Мелкий шрифт подсказок и клавиш: 88 % от основного
QFont smallerFont(QFont font);

// Сочетание для подписи: из «Ctrl+Alt+V, Alt+V» (так их отдаёт KDE) — самое короткое,
// переносимое Primary — как Ctrl
QString shortcutLabel(const QString &triggers);

} // namespace mp
