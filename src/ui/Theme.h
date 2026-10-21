#pragma once

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QString>

class QPainter;
class QPalette;
class QRectF;
class QFontMetricsF;
class QWidget;

namespace mp {

// Styling of the program's windows (docs/decisions/0019-ui-style.md). Background and text come from the system
// palette, so the light and dark themes come from the system. The accent comes from settings, the same one
// as for marks: it fills the selected tool, the primary button and the shot badges.
struct UiColors {
    bool dark = false;
    QColor surface;    // window background, toolbar
    QColor raised;     // secondary button, comment field, key cap
    QColor card;       // comment card
    QColor panel;      // side panel with comments
    QColor backdrop;   // backdrop under the shot
    QColor border;
    QColor text;
    QColor muted;      // hints (contrast with the background at least 4.5:1)
    QColor icon;
    QColor hover;
    QColor pressed;
    QColor accent;     // as set in settings: frames, outline of the selected item
    QColor accentFill; // darkened to a 4.5:1 contrast with white: fills under white text
    QColor onAccent;
    QColor link;       // the accent with a 4.5:1 contrast against the window background
    // Check results (wizard): readable on a card in both dark and light themes
    QColor success;
    QColor warning;
    QColor danger;

    static UiColors from(const QPalette &palette);
};

// The UI accent: one per program, changes together with the settings
void setUiAccent(const QColor &accent);
QColor uiAccent();

QColor mixColors(const QColor &a, const QColor &b, qreal t);

// Selection, checkboxes, focus frame and links of the window's system widgets in the accent color rather than the system one
// (settings, wizard, preview windows)
void useAccentSelection(QWidget *window);
// Hints and explanations in the theme's pale color
void setMutedText(QWidget *widget);

enum class Icon {
    Mark, Arrow, Pen, Point, Blur, Crop, Undo, Redo, Trash, Camera, Check,
    ChevronDown, More, Minimize, Copy, Comment, Grip,
    StatusOk, StatusWarning, StatusFailed, StatusPending,
    MarkRounded, MarkEllipse, FrameRect, FrameRounded, FrameEllipse
};

// An icon from the data/icons/ui set in the needed color; size is in logical pixels
QPixmap iconPixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio);

// "Key cap": text in a rounded frame (shortcut hints, the "Paste set Alt+V" button)
QSizeF keyCapSize(const QFontMetricsF &fm, const QString &text);
void drawKeyCap(QPainter &p, const QRectF &rect, const QString &text, const QColor &fg, const QColor &bg,
                const QColor &border);

// Small font for hints and keys: 88 % of the main one
QFont smallerFont(QFont font);

// The shortcut for a label: the shortest of "Ctrl+Alt+V, Alt+V" (that is how KDE reports them),
// the portable Primary shown as Ctrl
QString shortcutLabel(const QString &triggers);

} // namespace mp
