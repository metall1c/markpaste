#include "Theme.h"

#include "render/ShotRenderer.h"

#include <QFile>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QStringList>
#include <QSvgRenderer>
#include <QWidget>

namespace mp {

namespace {

QColor g_accent = RenderStyle().accent;

const char *iconName(Icon icon)
{
    switch (icon) {
    case Icon::Mark: return "mark";
    case Icon::Arrow: return "arrow";
    case Icon::Pen: return "pen";
    case Icon::Point: return "point";
    case Icon::Blur: return "blur";
    case Icon::Crop: return "crop";
    case Icon::Undo: return "undo";
    case Icon::Redo: return "redo";
    case Icon::Trash: return "trash";
    case Icon::Camera: return "camera";
    case Icon::Check: return "check";
    case Icon::ChevronDown: return "chevron-down";
    case Icon::More: return "more";
    case Icon::Minimize: return "minimize";
    case Icon::Copy: return "copy";
    case Icon::Comment: return "comment";
    case Icon::Grip: return "grip";
    case Icon::StatusOk: return "status-ok";
    case Icon::StatusWarning: return "status-warn";
    case Icon::StatusFailed: return "status-fail";
    case Icon::StatusPending: return "status-pending";
    case Icon::MarkRounded: return "mark-rounded";
    case Icon::MarkEllipse: return "mark-ellipse";
    case Icon::FrameRect: return "frame-rect";
    case Icon::FrameRounded: return "frame-rounded";
    case Icon::FrameEllipse: return "frame-ellipse";
    }
    return "mark";
}

} // namespace

void setUiAccent(const QColor &accent)
{
    if (accent.isValid())
        g_accent = accent;
}

QColor uiAccent()
{
    return g_accent;
}

QColor mixColors(const QColor &a, const QColor &b, qreal t)
{
    const QColor x = a.toRgb(), y = b.toRgb();
    return QColor::fromRgbF(float(x.redF() + (y.redF() - x.redF()) * t), float(x.greenF() + (y.greenF() - x.greenF()) * t),
                            float(x.blueF() + (y.blueF() - x.blueF()) * t));
}

UiColors UiColors::from(const QPalette &palette)
{
    UiColors c;
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    c.dark = window.lightnessF() < 0.5;
    c.surface = window;
    c.text = text;
    // Оттенки — смесью фона с текстом: подходят к любой системной теме, а не к одной Breeze
    c.raised = c.dark ? mixColors(window, text, 0.07) : mixColors(window, Qt::white, 0.75);
    c.card = c.dark ? mixColors(window, text, 0.05) : palette.color(QPalette::Base);
    c.panel = c.dark ? mixColors(window, Qt::black, 0.18) : mixColors(window, Qt::white, 0.45);
    c.backdrop = c.dark ? mixColors(window, Qt::black, 0.38) : mixColors(window, text, 0.08);
    c.border = mixColors(window, text, c.dark ? 0.15 : 0.17);
    c.icon = mixColors(window, text, 0.82);
    c.hover = mixColors(window, text, 0.08);
    c.pressed = mixColors(window, text, 0.14);
    // Подсказки — бледнее текста, но читаемые (NFR-9)
    for (qreal t = 0.55; t <= 1.0; t += 0.03) {
        c.muted = mixColors(window, text, t);
        if (contrastRatio(c.muted, window) >= 4.5)
            break;
    }
    c.accent = g_accent;
    c.accentFill = badgeFillColor(g_accent);
    c.onAccent = Qt::white;
    // Ссылки — цветом акцента, осветлённым или затемнённым до читаемого на фоне окна
    c.link = c.accent;
    for (int i = 0; i < 20 && contrastRatio(c.link, window) < 4.5; ++i)
        c.link = c.dark ? mixColors(c.link, Qt::white, 0.15) : mixColors(c.link, Qt::black, 0.15);
    c.success = c.dark ? QColor(0x5c, 0xc9, 0x8a) : QColor(0x1e, 0x7d, 0x3c);
    c.warning = c.dark ? QColor(0xf0, 0xb4, 0x4c) : QColor(0x9a, 0x5b, 0x00);
    c.danger = c.dark ? QColor(0xff, 0x7a, 0x70) : QColor(0xc0, 0x36, 0x2c);
    return c;
}

void useAccentSelection(QWidget *window)
{
    const UiColors c = UiColors::from(window->palette());
    QPalette pal = window->palette();
    pal.setColor(QPalette::Highlight, c.accentFill);
    pal.setColor(QPalette::HighlightedText, c.onAccent);
    pal.setColor(QPalette::Link, c.link);
    pal.setColor(QPalette::LinkVisited, c.link);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    pal.setColor(QPalette::Accent, c.accentFill);   // флажки стиля Windows 11
#endif
    window->setPalette(pal);
}

void setMutedText(QWidget *widget)
{
    // От палитры окна, а не своей: при повторном вызове (смена темы) своя уже бледная
    const QWidget *source = widget->isWindow() || !widget->window() ? widget : widget->window();
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, UiColors::from(source->palette()).muted);
    widget->setPalette(pal);
}

QPixmap iconPixmap(Icon icon, const QColor &color, int size, qreal dpr)
{
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1/%2/%3/%4").arg(int(icon)).arg(color.rgba(), 0, 16).arg(size).arg(dpr);
    if (const auto it = cache.constFind(key); it != cache.constEnd())
        return *it;

    static QHash<int, QByteArray> sources;
    QByteArray &svg = sources[int(icon)];
    if (svg.isEmpty()) {
        QFile f(QStringLiteral(":/icons/ui/%1.svg").arg(QLatin1String(iconName(icon))));
        if (f.open(QIODevice::ReadOnly))
            svg = f.readAll();
    }
    QByteArray colored = svg;
    colored.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
    QSvgRenderer renderer(colored);

    QImage img(QSize(size, size) * dpr, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(color.alphaF());
        renderer.render(&p);
    }
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dpr);
    cache.insert(key, pm);
    return pm;
}

QSizeF keyCapSize(const QFontMetricsF &fm, const QString &text)
{
    return {fm.horizontalAdvance(text) + 12, fm.height() + 2};
}

void drawKeyCap(QPainter &p, const QRectF &rect, const QString &text, const QColor &fg, const QColor &bg,
                const QColor &border)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(border.isValid() ? QPen(border, 1) : QPen(Qt::NoPen));
    p.setBrush(bg);
    p.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    p.setPen(fg);
    p.drawText(rect, Qt::AlignCenter, text);
    p.restore();
}

QFont smallerFont(QFont f)
{
    if (f.pointSizeF() > 0)
        f.setPointSizeF(f.pointSizeF() * 0.88);
    else
        f.setPixelSize(std::max(8, int(f.pixelSize() * 0.88)));
    return f;
}

QString shortcutLabel(const QString &triggers)
{
    QString best;
    for (const QString &part : triggers.split(u',', Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (!t.isEmpty() && (best.isEmpty() || t.size() < best.size()))
            best = t;
    }
    best.replace(QStringLiteral("Primary"), QStringLiteral("Ctrl"));
    return best;
}

} // namespace mp
