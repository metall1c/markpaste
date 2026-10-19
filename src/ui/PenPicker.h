#pragma once

#include "core/CaptureSet.h"

#include <QWidget>

class QButtonGroup;
class QLabel;
class QMenu;
class QSlider;
class QToolButton;

namespace mp {

// Цвет, толщина и тип линии меток: две кнопки панели редактора с выпадающими панелями.
//
// Панели — QMenu с QWidgetAction: на Wayland программа не может сама поставить окно у кнопки,
// а меню — всплывающая поверхность, которую композитор ставит относительно родителя.
// Клики внутри панели меню не закрывают, поэтому ползунок и кнопки толщины работают как в окне.
class PenPicker : public QWidget
{
    Q_OBJECT
public:
    explicit PenPicker(QWidget *parent = nullptr);

    // Без сигнала penChanged: показать перо выделенной метки или загруженное из настроек
    void setPen(const MarkPen &pen);
    MarkPen pen() const { return m_pen; }
    // Чем показывать пустые поля пера: акцент из настроек и толщина формы по умолчанию
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
    QColor m_accent;          // из RenderStyle по умолчанию, см. конструктор
    qreal m_defaultWidth;

    QToolButton *m_colorButton;
    QToolButton *m_widthButton;
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
