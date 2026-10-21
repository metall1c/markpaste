#include "PenPicker.h"

#include "Controls.h"
#include "render/ShotRenderer.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <cmath>

namespace mp {

namespace {

// The first swatch is the empty color: "accent from settings"
const QList<QColor> kSwatches{QColor(), QColor(0xE5, 0x39, 0x35), QColor(0xFD, 0xD8, 0x35), QColor(0x43, 0xA0, 0x47),
                              QColor(0x1E, 0x88, 0xE5), QColor(0x8E, 0x24, 0xAA), QColor(0x21, 0x21, 0x21),
                              QColor(Qt::white)};

QPen linePen(const QColor &color, qreal width, LineStyle line)
{
    QPen pen(color, width, Qt::SolidLine, Qt::RoundCap);
    if (line == LineStyle::Dash) {
        pen.setCapStyle(Qt::FlatCap);
        pen.setDashPattern({3, 2});
    } else if (line == LineStyle::Dot) {
        pen.setDashPattern({0.001, 2});
    }
    return pen;
}

QIcon circleIcon(const QColor &color, const QColor &border, int size = 18)
{
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(border, 1));
    p.setBrush(color);
    p.drawEllipse(QRectF(1.5, 1.5, size - 3, size - 3));
    return QIcon(pm);
}

QIcon lineIcon(const QColor &color, qreal width, LineStyle line, QSize size = {40, 16})
{
    QPixmap pm(size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(linePen(color, width, line));
    const qreal y = size.height() / 2.0;
    p.drawLine(QPointF(width / 2 + 2, y), QPointF(size.width() - width / 2 - 2, y));
    return QIcon(pm);
}

QString widthText(qreal w)
{
    return PenPicker::tr("%1 px").arg(QString::number(w, 'g', 3));
}

// Line sample across the whole panel width
class LinePreview : public QWidget
{
public:
    explicit LinePreview(QWidget *parent = nullptr) : QWidget(parent) { setFixedHeight(36); }
    void set(const QColor &color, qreal width, LineStyle line)
    {
        m_color = color;
        m_width = width;
        m_line = line;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(palette().alternateBase());
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        p.setPen(linePen(m_color, m_width, m_line));
        const qreal y = height() / 2.0;
        p.drawLine(QPointF(14, y), QPointF(width() - 14, y));
    }

private:
    QColor m_color;
    qreal m_width = 3;
    LineStyle m_line = LineStyle::Solid;
};

QToolButton *panelButton(QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setIconSize(QSize(40, 16));
    b->setMinimumSize(52, 30);
    return b;
}

} // namespace

PenPicker::PenPicker(QWidget *parent)
    : QWidget(parent), m_accent(RenderStyle().accent), m_defaultWidth(RenderStyle().frameWidth)
{
    m_colorButton = new FlatButton;
    m_colorButton->setText(tr("Mark color"));
    m_colorButton->setToolTip(tr("Mark color"));
    m_colorButton->setPopupMode(QToolButton::InstantPopup);
    m_colorButton->setIconSize(QSize(18, 18));
    m_colorButton->setChevron(true);
    m_colorMenu = new QMenu(m_colorButton);
    auto *colorAction = new QWidgetAction(m_colorMenu);
    colorAction->setDefaultWidget(buildColorPanel());
    m_colorMenu->addAction(colorAction);
    m_colorButton->setMenu(m_colorMenu);

    m_widthButton = new FlatButton;
    m_widthButton->setToolTip(tr("Line width and style"));
    m_widthButton->setPopupMode(QToolButton::InstantPopup);
    m_widthButton->setIconSize(QSize(28, 12));
    m_widthButton->setTextVisible(true);
    m_widthButton->setChevron(true);
    m_widthMenu = new QMenu(m_widthButton);
    auto *widthAction = new QWidgetAction(m_widthMenu);
    widthAction->setDefaultWidget(buildWidthPanel());
    m_widthMenu->addAction(widthAction);
    m_widthButton->setMenu(m_widthMenu);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_colorButton);
    layout->addWidget(m_widthButton);
    refresh();
}

QWidget *PenPicker::buildColorPanel()
{
    auto *panel = new QWidget;
    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(12, 10, 12, 10);
    auto *title = new QLabel(tr("Color"));
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    v->addWidget(title);

    auto *grid = new QGridLayout;
    grid->setSpacing(4);
    for (int i = 0; i < kSwatches.size(); ++i) {
        auto *b = new QToolButton;
        b->setAutoRaise(true);
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setIconSize(QSize(22, 22));
        b->setToolTip(i == 0 ? tr("Accent color from settings") : kSwatches[i].name());
        connect(b, &QToolButton::clicked, this, [this, i] {
            MarkPen p = m_pen;
            p.color = kSwatches[i];
            change(p);
            m_colorMenu->close();
        });
        m_swatches.append(b);
        grid->addWidget(b, i / 4, i % 4);
    }
    v->addLayout(grid);

    auto *custom = new QPushButton(tr("Other color…"));
    custom->setFocusPolicy(Qt::NoFocus);
    connect(custom, &QPushButton::clicked, this, [this] {
        m_colorMenu->close();
        const QColor c = QColorDialog::getColor(shownColor(), window(), tr("Mark color"));
        if (c.isValid()) {
            MarkPen p = m_pen;
            p.color = c;
            change(p);
        }
    });
    v->addWidget(custom);
    return panel;
}

QWidget *PenPicker::buildWidthPanel()
{
    auto *panel = new QWidget;
    panel->setMinimumWidth(240);
    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(8);

    auto bold = [](QLabel *l) {
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        return l;
    };

    auto *head = new QHBoxLayout;
    head->addWidget(bold(new QLabel(tr("Width"))));
    head->addStretch();
    m_widthValue = new QLabel;
    head->addWidget(m_widthValue);
    v->addLayout(head);

    auto *presets = new QHBoxLayout;
    m_presets = new QButtonGroup(this);
    m_presets->setExclusive(false);   // with a width not from the presets, none is checked
    for (qreal w : presetWidths()) {
        auto *b = panelButton(panel);
        b->setIcon(lineIcon(palette().text().color(), w, LineStyle::Solid));
        b->setToolTip(widthText(w));
        connect(b, &QToolButton::clicked, this, [this, w] {
            MarkPen p = m_pen;
            p.width = w;
            change(p);
        });
        m_presets->addButton(b);
        presets->addWidget(b);
    }
    v->addLayout(presets);

    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(1, kMaxWidth);
    m_slider->setFocusPolicy(Qt::NoFocus);
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        if (qFuzzyCompare(shownWidth(), qreal(value)))
            return;   // moved programmatically in refresh()
        MarkPen p = m_pen;
        p.width = value;
        change(p);
    });
    v->addWidget(m_slider);

    auto *preview = new LinePreview;
    m_preview = preview;
    v->addWidget(preview);

    v->addSpacing(4);
    v->addWidget(bold(new QLabel(tr("Line"))));
    auto *lines = new QHBoxLayout;
    m_lines = new QButtonGroup(this);
    const QList<std::pair<LineStyle, QString>> styles{{LineStyle::Solid, tr("Solid")},
                                                      {LineStyle::Dash, tr("Dashed")},
                                                      {LineStyle::Dot, tr("Dotted")}};
    for (const auto &[line, name] : styles) {
        auto *b = panelButton(panel);
        b->setIcon(lineIcon(palette().text().color(), 2, line, {56, 16}));
        b->setIconSize(QSize(56, 16));
        b->setToolTip(name);
        connect(b, &QToolButton::clicked, this, [this, line] {
            MarkPen p = m_pen;
            p.line = line;
            change(p);
        });
        m_lines->addButton(b, int(line));
        lines->addWidget(b);
    }
    v->addLayout(lines);
    return panel;
}

void PenPicker::setPen(const MarkPen &pen)
{
    m_pen = pen;
    refresh();
}

void PenPicker::setDefaults(const QColor &accent, qreal width)
{
    m_accent = accent;
    m_defaultWidth = width;
    refresh();
}

QColor PenPicker::shownColor() const
{
    return m_pen.color.isValid() ? m_pen.color : m_accent;
}

qreal PenPicker::shownWidth() const
{
    return m_pen.width > 0 ? m_pen.width : m_defaultWidth;
}

void PenPicker::change(const MarkPen &pen)
{
    if (pen == m_pen)
        return;
    m_pen = pen;
    refresh();
    emit penChanged(m_pen);
}

void PenPicker::refresh()
{
    const QColor color = shownColor();
    const qreal width = shownWidth();
    const UiColors ui = UiColors::from(palette());
    const QColor border = ui.border;

    m_colorButton->setIcon(circleIcon(color, border));
    for (int i = 0; i < m_swatches.size(); ++i) {
        m_swatches[i]->setIcon(circleIcon(i == 0 ? m_accent : kSwatches[i], border, 22));
        m_swatches[i]->setChecked(m_pen.color == kSwatches[i]);
    }

    m_widthButton->setText(widthText(width));
    m_widthButton->setIcon(lineIcon(ui.icon, std::min<qreal>(width, 6), m_pen.line, {28, 12}));
    m_widthValue->setText(widthText(width));
    const QList<qreal> presets = presetWidths();
    const QList<QAbstractButton *> buttons = m_presets->buttons();
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setChecked(qFuzzyCompare(presets[i], width));
    m_slider->setValue(int(std::lround(width)));
    if (auto *b = m_lines->button(int(m_pen.line)))
        b->setChecked(true);
    static_cast<LinePreview *>(m_preview)->set(color, width, m_pen.line);
}

} // namespace mp
