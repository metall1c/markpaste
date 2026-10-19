#pragma once

#include "core/CaptureSet.h"

#include <QColor>
#include <QImage>
#include <QList>

class QPainter;

namespace ps {

// Размеры — в логических пикселях, при запекании умножаются на devicePixelRatio снимка.
struct RenderStyle {
    QColor accent = QColor(0xFF, 0x5A, 0x1F);   // ТЗ разд. 10
    qreal frameWidth = 3.0;
    qreal outlineWidth = 1.0;
    qreal badgeDiameter = 28.0;
    int pixelBlock = 12;                       // EDT-6
};

// Фон значка: акцентный цвет, затемнённый до контраста ≥ 4.5:1 с белым текстом (NFR-9).
// Рамка рисуется исходным акцентным цветом. См. docs/decisions/0003-badge-contrast.md.
QColor badgeFillColor(const QColor &accent);
double contrastRatio(const QColor &a, const QColor &b);

// Рисует рамки и значки меток. Painter уже переведён в координаты исходника (физические
// пиксели). Общая функция для запекания и редактора: на экране видно ровно то, что уйдёт в чат.
void paintMarks(QPainter &p, const Shot &shot, int shotIndex, const RenderStyle &style, const QRectF &bounds);

// Исходник с применённым размытием, без обрезки и меток (для редактора и запекания).
QImage sourceWithRedactions(const Shot &shot, const RenderStyle &style = {});

// Декодирует исходник, применяет размытие, обрезку и метки. Исходный Shot не меняется.
QImage renderShot(const Shot &shot, int shotIndex, const RenderStyle &style = {});

// Пикселизация на месте: блоки выровнены по левому верхнему углу rect, каждый
// заливается средним цветом. Исходные пиксели в результат не попадают (EDT-6, NFR-7).
void pixelate(QImage &image, const QRect &rect, int block);

// Склейка для режима 4: снимки по вертикали, отступ 16 px, буква над каждым.
QImage composeVertical(const QList<QImage> &images, const QList<QString> &labels, qreal dpr = 1.0);

struct EncodeOptions {
    int maxLongSide = 4096;                    // PST-7
    qint64 jpegThresholdBytes = 5 * 1024 * 1024;
    int jpegQuality = 85;
};

struct EncodedImage {
    QByteArray data;
    QByteArray mimeType;                       // image/png или image/jpeg
    QString extension;                         // png или jpg
    QSize size;
};

EncodedImage encodeForPaste(QImage image, const EncodeOptions &options = {});

} // namespace ps
