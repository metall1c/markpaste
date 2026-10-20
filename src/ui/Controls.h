#pragma once

#include "Theme.h"

#include <QPushButton>
#include <QToolButton>

#include <optional>

class QLabel;

namespace mp {

// Кнопки и мелкие элементы окон программы. Рисуются сами, а не стилем системы: на Breeze,
// Windows 11 и Fusion выглядят одинаково, цвета — из UiColors (docs/decisions/0019-ui-style.md).

// Кнопка-значок: инструменты редактора, отмена, корзина, кнопки заголовка ленты, перо.
// Текст кнопки — имя для экранного диктора и тестов; на самой кнопке он виден, только если
// setTextVisible(true). Значок — из набора (setGlyph) или свой QIcon (setIcon), например образец цвета.
class FlatButton : public QToolButton
{
    Q_OBJECT
public:
    explicit FlatButton(QWidget *parent = nullptr);
    FlatButton(Icon glyph, const QString &text, QWidget *parent = nullptr);

    void setGlyph(Icon glyph);
    void setTextVisible(bool visible);
    void setChevron(bool chevron);    // стрелка «откроется панель»
    void setSide(int side, int iconSide);   // высота (и наименьшая ширина) и размер значка

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    std::optional<Icon> m_glyph;
    bool m_textVisible = false;
    bool m_chevron = false;
    int m_side = 36;
    int m_iconSide = 20;
};

// Кнопка действия: главная (залита акцентом) или второстепенная (с рамкой).
// Может показать значок слева и сочетание клавиш справа («Вставить набор  Alt+V»).
// Без текста — квадратная кнопка-значок.
class ActionButton : public QPushButton
{
    Q_OBJECT
public:
    enum class Kind { Primary, Secondary };

    explicit ActionButton(const QString &text, Kind kind = Kind::Secondary, QWidget *parent = nullptr);

    void setKind(Kind kind);
    Kind kind() const { return m_kind; }
    void setGlyph(std::optional<Icon> glyph);
    void setSwatch(const QColor &color);   // кружок цвета вместо значка (цвет меток в настройках)
    void setKeyHint(const QString &keys);
    void setButtonHeight(int height);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;

private:
    QFont textFont() const;
    QFont keyFont() const;

    Kind m_kind;
    std::optional<Icon> m_glyph;
    QColor m_swatch;
    QString m_keys;
    int m_height = 36;
    bool m_keyboardFocus = false;   // рамку фокуса — только при переходе Tab, не от щелчка
};

// Вкладка-«пилюля» (окно настроек): выбранная подкрашена акцентом
class PillButton : public QPushButton
{
    Q_OBJECT
public:
    explicit PillButton(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;

private:
    bool m_keyboardFocus = false;
};

// Карточка: скруглённый блок с рамкой (шаги мастера, текст в предпросмотре, сведения о системе)
class Card : public QWidget
{
    Q_OBJECT
public:
    explicit Card(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

// Результат проверки: значок (✓, ⚠, ✗, часы) и текст цветом результата
class StatusLine : public QWidget
{
    Q_OBJECT
public:
    enum class State { None, Pending, Ok, Warning, Failed };
    explicit StatusLine(QWidget *parent = nullptr);
    void setState(State state, const QString &text);
    State state() const { return m_state; }
    QString text() const;

protected:
    void paintEvent(QPaintEvent *) override;
    void changeEvent(QEvent *) override;

private:
    void applyColor();

    State m_state = State::None;
    QLabel *m_label;
};

// Подсказка сочетания: «[Ctrl+Z] отменить»
class KeyHint : public QWidget
{
    Q_OBJECT
public:
    KeyHint(const QString &keys, const QString &label, QWidget *parent = nullptr);
    void setLabel(const QString &label);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_keys;
    QString m_label;
};

// Тонкая разделительная линия цвета рамок
class Rule : public QWidget
{
    Q_OBJECT
public:
    explicit Rule(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Qt::Orientation m_orientation;
};

// Фон области окна (боковая панель) с линией рамки по краям edges
class Panel : public QWidget
{
    Q_OBJECT
public:
    explicit Panel(Qt::Edges edges = {}, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Qt::Edges m_edges;
};

} // namespace mp
