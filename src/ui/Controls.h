#pragma once

#include "Theme.h"

#include <QPushButton>
#include <QToolButton>

#include <optional>

class QLabel;

namespace mp {

// Buttons and small elements of the program's windows. They paint themselves rather than use the system style: on Breeze,
// Windows 11 and Fusion they look the same, colors come from UiColors (docs/decisions/0019-ui-style.md).

// Icon button: editor tools, undo, trash, strip header buttons, pen.
// The button text is its name for screen readers and tests; it is visible on the button only with
// setTextVisible(true). The icon comes from the set (setGlyph) or is a custom QIcon (setIcon), e.g. a color swatch.
class FlatButton : public QToolButton
{
    Q_OBJECT
public:
    explicit FlatButton(QWidget *parent = nullptr);
    FlatButton(Icon glyph, const QString &text, QWidget *parent = nullptr);

    void setGlyph(Icon glyph);
    void setTextVisible(bool visible);
    void setChevron(bool chevron);    // a "panel will open" arrow
    void setSide(int side, int iconSide);   // height (and minimum width) and icon size

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

// Action button: primary (filled with the accent) or secondary (with a border).
// May show an icon on the left and a shortcut on the right ("Paste set  Alt+V").
// Without text it is a square icon button.
class ActionButton : public QPushButton
{
    Q_OBJECT
public:
    enum class Kind { Primary, Secondary };

    explicit ActionButton(const QString &text, Kind kind = Kind::Secondary, QWidget *parent = nullptr);

    void setKind(Kind kind);
    Kind kind() const { return m_kind; }
    void setGlyph(std::optional<Icon> glyph);
    void setSwatch(const QColor &color);   // a color circle instead of an icon (mark color in settings)
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
    bool m_keyboardFocus = false;   // focus frame only when reached by Tab, not by a click
};

// A "pill" tab (settings window): the selected one is tinted with the accent
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

// Card: a rounded block with a border (wizard steps, text in the preview, system details)
class Card : public QWidget
{
    Q_OBJECT
public:
    explicit Card(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

// Check result: an icon (✓, ⚠, ✗, clock) and text in the result's color
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

// Shortcut hint: "[Ctrl+Z] undo"
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

// A thin separator line in the border color
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

// Background of a window area (side panel) with a border line along the given edges
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
