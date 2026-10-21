#pragma once

#include "core/CaptureSet.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPainterPath>

class QPainter;

namespace mp {

// Sizes are in logical pixels; when baking they are multiplied by the shot's devicePixelRatio.
struct RenderStyle {
    QColor accent = QColor(0xFF, 0x5A, 0x1F);   // spec section 10
    qreal frameWidth = 2.0;                    // frame and freehand line
    qreal arrowWidth = 2.0;                    // arrow shaft; the head is derived from it
    qreal outlineWidth = 1.0;                  // white ring of the number badge
    // Mark line halo: a thin dark semi-transparent band under the colored line so that the mark
    // reads on a background of its own color and on a light shot; on a dark one it is barely visible.
    // Turned off in settings (docs/decisions/0021-mark-halo.md)
    bool halo = true;
    qreal haloWidth = 1.0;                     // logical px on each side of the line
    QColor haloColor = QColor(0, 0, 0, 115);
    qreal badgeDiameter = 28.0;
    int pixelBlock = 12;                       // EDT-6
    bool captions = false;                     // EDT-10: the comment as a caption next to the badge
    qreal captionMaxWidth = 280.0;
};

// Badge background: the accent color darkened to a contrast ≥ 4.5:1 with white text (NFR-9).
// The frame is drawn in the original accent color. See docs/decisions/0003-badge-contrast.md.
QColor badgeFillColor(const QColor &accent);

// The mark's actual color and width (logical px): empty MarkPen fields come from the style.
QColor markColor(const Mark &mark, const RenderStyle &style);
// Outline of a frame of shape frame inside rect (a line of lineWidth entirely inside); for
// painting and for mouse hit-testing of an ellipse
QPainterPath frameOutline(const QRectF &rect, FrameShape frame, qreal lineWidth, qreal dpr);
qreal markWidth(const Mark &mark, const RenderStyle &style);
qreal defaultMarkWidth(MarkShape shape, const RenderStyle &style);
double contrastRatio(const QColor &a, const QColor &b);

// Draws frames, arrows, freehand lines, points, mark badges and (if enabled) captions. The painter is already in source coordinates (physical
// pixels). Shared by baking and the editor: the screen shows exactly what goes to the chat.
void paintMarks(QPainter &p, const Shot &shot, int shotIndex, const RenderStyle &style, const QRectF &bounds);

// The source with blur applied, without crop and marks (for the editor and baking).
QImage sourceWithRedactions(const Shot &shot, const RenderStyle &style = {});

// Decodes the source and applies blur, crop and marks. The source Shot is not changed.
QImage renderShot(const Shot &shot, int shotIndex, const RenderStyle &style = {});

// In-place pixelation: blocks are aligned to the top-left corner of rect, each one
// is filled with its average color. Source pixels do not get into the result (EDT-6, NFR-7).
void pixelate(QImage &image, const QRect &rect, int block);

// Stitching for mode 4: shots stacked vertically, 16 px apart, a letter above each.
QImage composeVertical(const QList<QImage> &images, const QList<QString> &labels, qreal dpr = 1.0);

struct EncodeOptions {
    int maxLongSide = 4096;                    // PST-7
    qint64 jpegThresholdBytes = 5 * 1024 * 1024;
    int jpegQuality = 85;
};

struct EncodedImage {
    QByteArray data;
    QByteArray mimeType;                       // image/png or image/jpeg
    QString extension;                         // png or jpg
    QSize size;
};

EncodedImage encodeForPaste(QImage image, const EncodeOptions &options = {});

} // namespace mp
