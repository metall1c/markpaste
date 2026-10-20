#pragma once

#include "Theme.h"

#include <QPushButton>
#include <QToolButton>

#include <optional>

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
    void setKeyHint(const QString &keys);
    void setButtonHeight(int height);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QFont textFont() const;
    QFont keyFont() const;

    Kind m_kind;
    std::optional<Icon> m_glyph;
    QString m_keys;
    int m_height = 36;
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
