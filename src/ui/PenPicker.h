#pragma once

#include "core/CaptureSet.h"

#include <QWidget>

class QButtonGroup;
class QLabel;
class QMenu;
class QSlider;
class QToolButton;

namespace mp {

class FlatButton;

// Mark color, width and line style: two editor toolbar buttons with drop-down panels.
//
// The panels are a QMenu with a QWidgetAction: on Wayland a program cannot place a window next to a button itself,
// while a menu is a popup surface that the compositor places relative to its parent.
// Clicks inside the panel do not close the menu, so the slider and the width buttons work as in a window.
class PenPicker : public QWidget
{
    Q_OBJECT
public:
    explicit PenPicker(QWidget *parent = nullptr);

    // Without the penChanged signal: show the pen of the selected mark or the one loaded from settings
    void setPen(const MarkPen &pen);
    MarkPen pen() const { return m_pen; }
    // How to show empty pen fields: the accent from settings and the shape's default width
    void setDefaults(const QColor &accent, qreal width);

    static QList<qreal> presetWidths() { return {2, 3, 5, 8}; }
    static constexpr int kMaxWidth = 20;

signals:
    void penChanged(const mp::MarkPen &pen);

private:
    QColor shownColor() const;
    qreal shownWidth() const;
    void change(const MarkPen &pen);
    void refresh();
    QWidget *buildColorPanel();
    QWidget *buildWidthPanel();

    MarkPen m_pen;
    QColor m_accent;          // from the default RenderStyle, see the constructor
    qreal m_defaultWidth;

    FlatButton *m_colorButton;
    FlatButton *m_widthButton;
    QMenu *m_colorMenu;
    QMenu *m_widthMenu;
    QList<QToolButton *> m_swatches;
    QLabel *m_widthValue = nullptr;
    QSlider *m_slider = nullptr;
    QWidget *m_preview = nullptr;
    QButtonGroup *m_presets = nullptr;
    QButtonGroup *m_lines = nullptr;
};

} // namespace mp
