#include "Controls.h"

#include <QFontMetricsF>
#include <QPainter>

namespace mp {

namespace {
constexpr int kChevron = 14;
} // namespace

// --- FlatButton ---

FlatButton::FlatButton(QWidget *parent)
    : QToolButton(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    setAutoRaise(true);
    setCursor(Qt::ArrowCursor);
}

FlatButton::FlatButton(Icon glyph, const QString &text, QWidget *parent)
    : FlatButton(parent)
{
    m_glyph = glyph;
    setText(text);
    setToolTip(text);
}

void FlatButton::setGlyph(Icon glyph)
{
    m_glyph = glyph;
    update();
}

void FlatButton::setTextVisible(bool visible)
{
    m_textVisible = visible;
    updateGeometry();
    update();
}

void FlatButton::setChevron(bool chevron)
{
    m_chevron = chevron;
    updateGeometry();
    update();
}

void FlatButton::setSide(int side, int iconSide)
{
    m_side = side;
    m_iconSide = iconSide;
    updateGeometry();
    update();
}

QSize FlatButton::sizeHint() const
{
    const bool hasIcon = m_glyph || !icon().isNull();
    const QSize iconSz = m_glyph ? QSize(m_iconSide, m_iconSide) : iconSize();
    if (!m_textVisible && !m_chevron)
        return {std::max(m_side, hasIcon ? iconSz.width() + 12 : 0), m_side};
    qreal w = hasIcon ? iconSz.width() : 0;
    if (m_textVisible)
        w += (w > 0 ? 6 : 0) + QFontMetricsF(font()).horizontalAdvance(text());
    if (m_chevron)
        w += 2 + kChevron;
    return {std::max(m_side, int(std::ceil(w)) + 8 + 6), m_side};
}

void FlatButton::paintEvent(QPaintEvent *)
{
    const UiColors c = UiColors::from(palette());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool checked = isChecked();
    QColor bg = Qt::transparent;
    if (checked)
        bg = c.accentFill;
    else if (isDown())
        bg = c.pressed;
    else if (underMouse() && isEnabled())
        bg = c.hover;
    if (bg.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        const qreal r = m_side >= 32 ? 8 : 6;
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }

    QColor fg = checked ? c.onAccent : c.icon;
    QColor textColor = checked ? c.onAccent : c.text;
    if (!isEnabled()) {
        fg.setAlphaF(0.35f);
        textColor.setAlphaF(0.35f);
    }
    const qreal dpr = devicePixelRatioF();
    QPixmap pm;
    if (m_glyph)
        pm = iconPixmap(*m_glyph, fg, m_iconSide, dpr);
    else if (!icon().isNull())
        pm = icon().pixmap(iconSize(), dpr, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    const QSizeF pmSize = pm.isNull() ? QSizeF() : pm.deviceIndependentSize();

    const QFontMetricsF fm(font());
    const qreal textW = m_textVisible ? fm.horizontalAdvance(text()) : 0;
    qreal contentW = pmSize.width();
    if (m_textVisible)
        contentW += (contentW > 0 ? 6 : 0) + textW;
    if (m_chevron)
        contentW += 2 + kChevron;
    qreal x = (m_textVisible || m_chevron) ? 8 : (width() - contentW) / 2;
    const qreal cy = height() / 2.0;

    if (!pm.isNull()) {
        p.drawPixmap(QPointF(x, cy - pmSize.height() / 2), pm);
        x += pmSize.width();
    }
    if (m_textVisible) {
        if (!pm.isNull())
            x += 6;
        p.setPen(textColor);
        p.drawText(QRectF(x, 0, textW + 1, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        x += textW;
    }
    if (m_chevron) {
        x += 2;
        p.drawPixmap(QPointF(x, cy - kChevron / 2.0), iconPixmap(Icon::ChevronDown, fg, kChevron, dpr));
    }
}

// --- ActionButton ---

ActionButton::ActionButton(const QString &text, Kind kind, QWidget *parent)
    : QPushButton(text, parent), m_kind(kind)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
}

void ActionButton::setKind(Kind kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    updateGeometry();
    update();
}

void ActionButton::setGlyph(std::optional<Icon> glyph)
{
    m_glyph = glyph;
    updateGeometry();
    update();
}

void ActionButton::setKeyHint(const QString &keys)
{
    m_keys = keys;
    updateGeometry();
    update();
}

void ActionButton::setButtonHeight(int height)
{
    m_height = height;
    updateGeometry();
}

QFont ActionButton::textFont() const
{
    QFont f = font();
    f.setWeight(m_kind == Kind::Primary ? QFont::DemiBold : QFont::Medium);
    return f;
}

QFont ActionButton::keyFont() const
{
    return smallerFont(font());
}

QSize ActionButton::sizeHint() const
{
    if (text().isEmpty())
        return {m_height, m_height};
    qreal w = QFontMetricsF(textFont()).horizontalAdvance(text());
    if (m_glyph)
        w += 18 + 8;
    if (!m_keys.isEmpty())
        w += 10 + keyCapSize(QFontMetricsF(keyFont()), m_keys).width();
    return {int(std::ceil(w)) + 2 * 14, m_height};
}

QSize ActionButton::minimumSizeHint() const
{
    return {text().isEmpty() ? m_height : 48, m_height};
}

void ActionButton::paintEvent(QPaintEvent *)
{
    const UiColors c = UiColors::from(palette());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool hover = underMouse() && isEnabled();

    QColor bg, fg, border;
    if (m_kind == Kind::Primary) {
        bg = c.accentFill;
        if (isDown())
            bg = bg.darker(112);
        else if (hover)
            bg = bg.lighter(112);
        fg = c.onAccent;
        if (!isEnabled()) {
            bg = mixColors(c.surface, c.accentFill, 0.4);
            fg.setAlphaF(0.6f);
        }
    } else {
        bg = isDown() ? mixColors(c.raised, c.text, 0.1) : hover ? mixColors(c.raised, c.text, 0.05) : c.raised;
        border = c.border;
        fg = c.text;
        if (!isEnabled()) {
            bg = c.surface;
            fg = c.muted;
            fg.setAlphaF(0.6f);
        }
    }
    p.setPen(border.isValid() ? QPen(border, 1) : QPen(Qt::NoPen));
    p.setBrush(bg);
    p.drawRoundedRect(r, 8, 8);

    const qreal dpr = devicePixelRatioF();
    const QColor iconColor = m_kind == Kind::Primary ? fg : (isEnabled() ? c.icon : fg);
    if (text().isEmpty()) {
        if (m_glyph) {
            const QPixmap pm = iconPixmap(*m_glyph, iconColor, 18, dpr);
            p.drawPixmap(QPointF((width() - 18) / 2.0, (height() - 18) / 2.0), pm);
        }
        return;
    }

    const QFont tf = textFont();
    const QFontMetricsF fm(tf);
    const QFontMetricsF km(keyFont());
    const qreal textW = fm.horizontalAdvance(text());
    const QSizeF keySize = m_keys.isEmpty() ? QSizeF() : keyCapSize(km, m_keys);
    qreal contentW = textW + (m_glyph ? 18 + 8 : 0) + (m_keys.isEmpty() ? 0 : 10 + keySize.width());
    // Не влезает — сначала прячем сочетание, текст важнее
    const bool showKeys = !m_keys.isEmpty() && contentW + 2 * 10 <= width();
    if (!showKeys && !m_keys.isEmpty())
        contentW -= 10 + keySize.width();
    qreal x = std::max(10.0, (width() - contentW) / 2);
    const qreal cy = height() / 2.0;
    if (m_glyph) {
        p.drawPixmap(QPointF(x, cy - 9), iconPixmap(*m_glyph, iconColor, 18, dpr));
        x += 18 + 8;
    }
    p.setFont(tf);
    p.setPen(fg);
    const qreal avail = width() - x - 10 - (showKeys ? 10 + keySize.width() : 0);
    p.drawText(QRectF(x, 0, std::max(0.0, avail), height()), Qt::AlignVCenter | Qt::AlignLeft,
               fm.elidedText(text(), Qt::ElideRight, avail));
    x += std::min(textW, avail);
    if (showKeys) {
        p.setFont(keyFont());
        const QRectF key(x + 10, cy - keySize.height() / 2, keySize.width(), keySize.height());
        if (m_kind == Kind::Primary)
            drawKeyCap(p, key, m_keys, fg, QColor(255, 255, 255, 52), QColor());
        else
            drawKeyCap(p, key, m_keys, c.text, c.surface, c.border);
    }
}

// --- KeyHint ---

KeyHint::KeyHint(const QString &keys, const QString &label, QWidget *parent)
    : QWidget(parent), m_keys(keys), m_label(label)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void KeyHint::setLabel(const QString &label)
{
    m_label = label;
    updateGeometry();
    update();
}

QSize KeyHint::sizeHint() const
{
    const QSizeF key = keyCapSize(QFontMetricsF(smallerFont(font())), m_keys);
    const qreal w = key.width() + 5 + QFontMetricsF(font()).horizontalAdvance(m_label);
    return {int(std::ceil(w)) + 1, int(std::ceil(std::max(key.height(), QFontMetricsF(font()).height())))};
}

void KeyHint::paintEvent(QPaintEvent *)
{
    const UiColors c = UiColors::from(palette());
    QPainter p(this);
    const QFont kf = smallerFont(font());
    const QSizeF key = keyCapSize(QFontMetricsF(kf), m_keys);
    p.setFont(kf);
    drawKeyCap(p, QRectF(0, (height() - key.height()) / 2, key.width(), key.height()), m_keys, c.text, c.raised, c.border);
    p.setFont(font());
    p.setPen(c.muted);
    p.drawText(QRectF(key.width() + 5, 0, width() - key.width() - 5, height()), Qt::AlignVCenter | Qt::AlignLeft, m_label);
}

// --- Rule ---

Rule::Rule(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), m_orientation(orientation)
{
    if (orientation == Qt::Vertical)
        setFixedSize(13, 24);
    else
        setFixedHeight(1);
}

void Rule::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QColor color = UiColors::from(palette()).border;
    if (m_orientation == Qt::Vertical)
        p.fillRect(QRect(width() / 2, 0, 1, height()), color);
    else
        p.fillRect(rect(), color);
}

// --- Panel ---

Panel::Panel(Qt::Edges edges, QWidget *parent)
    : QWidget(parent), m_edges(edges)
{
}

void Panel::paintEvent(QPaintEvent *)
{
    const UiColors c = UiColors::from(palette());
    QPainter p(this);
    p.fillRect(rect(), c.panel);
    if (m_edges & Qt::LeftEdge)
        p.fillRect(QRect(0, 0, 1, height()), c.border);
    if (m_edges & Qt::RightEdge)
        p.fillRect(QRect(width() - 1, 0, 1, height()), c.border);
    if (m_edges & Qt::TopEdge)
        p.fillRect(QRect(0, 0, width(), 1), c.border);
    if (m_edges & Qt::BottomEdge)
        p.fillRect(QRect(0, height() - 1, width(), 1), c.border);
}

} // namespace mp
