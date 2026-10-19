#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QUuid>

namespace mp {

inline constexpr int kMaxShots = 26;            // A–Z
inline constexpr int kMaxCommentLength = 500;   // EDT-5

// Все прямоугольники — в физических пикселях исходного снимка (NFR-7, CAP-6),
// а не относительно обрезки: обрезка хранится отдельно и отменяется без копии кадра.

// Форма метки (EDT-9). Все формы нумеруются вместе: A1 может быть стрелкой, A2 — рамкой.
enum class MarkShape { Rect, Arrow, Freehand };

enum class LineStyle { Solid, Dash, Dot };

// Цвет, толщина и тип линии метки. Пустые значения — из общего стиля (RenderStyle):
// так метки без выбранного цвета перекрашиваются вместе с акцентом в настройках.
struct MarkPen {
    QColor color;                  // недействительный — акцентный цвет из настроек
    qreal width = 0;               // логические px; 0 — толщина формы по умолчанию
    LineStyle line = LineStyle::Solid;

    bool operator==(const MarkPen &) const = default;
};

struct Mark {
    QUuid id;
    QRect rect;                    // рамка; у стрелки и карандаша — охватывающий прямоугольник points
    QString comment;
    MarkShape shape = MarkShape::Rect;
    QList<QPoint> points;          // стрелка: {хвост, острие}; карандаш: ломаная после упрощения
    MarkPen pen;

    bool isLinear() const { return shape != MarkShape::Rect; }
    QPoint badgeAnchor() const { return isLinear() && !points.isEmpty() ? points.first() : rect.topLeft(); }

    bool operator==(const Mark &) const = default;
};

struct Redaction {
    QUuid id;
    QRect rect;

    bool operator==(const Redaction &) const = default;
};

struct Shot {
    QUuid id;
    QByteArray sourcePng;          // исходник без меток, сжатый (NFR-2)
    QSize sourceSize;              // физические пиксели
    qreal devicePixelRatio = 1.0;  // масштаб монитора, с которого снят кадр
    QRect crop;                    // пустой — без обрезки
    QList<Mark> marks;             // номер метки = позиция + 1
    QList<Redaction> redactions;

    QRect sourceRect() const { return QRect(QPoint(0, 0), sourceSize); }
    QRect visibleRect() const { return crop.isNull() ? sourceRect() : crop; }
    int commentCount() const;      // метки с непустым комментарием
    int markIndex(const QUuid &markId) const;
    int redactionIndex(const QUuid &redactionId) const;

    bool operator==(const Shot &) const = default;
};

struct CaptureSet {
    QList<Shot> shots;             // буква снимка = позиция в списке

    int shotIndex(const QUuid &shotId) const;
    int totalMarks() const;
    int totalComments() const;
    bool isFull() const { return shots.size() >= kMaxShots; }
    bool isEmpty() const { return shots.isEmpty(); }

    bool operator==(const CaptureSet &) const = default;
};

// Буква снимка по позиции: 0 → 'A'. Номера не хранятся, а вычисляются,
// поэтому перестановка, удаление и отмена не оставят рассинхронизированных номеров.
QChar shotLetter(int shotIndex);
QString markLabel(int shotIndex, int markIndex);   // (0, 0) → "A1"

// Прямоугольник по двум углам, включая оба. QRect(p1, p2).normalized() для этого не годится:
// в Qt 6 перевёрнутый прямоугольник нормализуется со сдвигом на пиксель.
QRect rectFromPoints(const QPoint &a, const QPoint &b);

} // namespace mp
