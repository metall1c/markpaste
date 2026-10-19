#pragma once

#include "core/CaptureSetModel.h"
#include "render/ShotRenderer.h"

#include <QImage>
#include <QUuid>
#include <QWidget>

#include <optional>

namespace mp {

class CommentPopup;

// Холст редактора снимка: метки (рамка, стрелка, карандаш), размытие, обрезка (ТЗ 5.2).
//
// Координаты меток — физические пиксели исходника. Холст хранит одно преобразование
// «исходник → окно» (toWidget) и рисует метки той же функцией, что и запекание
// (paintMarks), поэтому на экране ровно то, что уйдёт в чат.
// Живое перетаскивание рисуется локально, в модель уходит одна правка на отпускание мыши:
// так в истории отмен нет сотни промежуточных шагов.
class ShotCanvas : public QWidget
{
    Q_OBJECT
public:
    enum class Tool { Mark, Arrow, Pen, Blur, Crop };

    explicit ShotCanvas(CaptureSetModel *model, QWidget *parent = nullptr);

    void setShot(const QUuid &shotId);
    QUuid shotId() const { return m_shotId; }
    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setStyle(const RenderStyle &style);

    bool hasPendingCrop() const { return !m_crop.isEmpty(); }
    void applyCrop();
    void cancelCrop();
    void deleteSelection();
    void commitComment();
    // Выделить метку и открыть её комментарий (из списка комментариев)
    void editMark(const QUuid &markId);

    QSize sizeHint() const override;

signals:
    void toolChanged(mp::ShotCanvas::Tool tool);
    void cropPendingChanged(bool pending);
    void finishRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    enum class Drag { None, Create, Move, Resize };
    enum Handle { HNone = 0, HLeft = 1, HRight = 2, HTop = 4, HBottom = 8 };
    struct Hit {
        bool isMark = false;
        QUuid id;
        int handle = HNone;
        int point = -1;   // конец стрелки: 0 — хвост, 1 — острие
        bool valid() const { return !id.isNull(); }
        bool resizes() const { return handle != HNone || point >= 0; }
    };

    const Shot *shot() const;
    int shotIndex() const;
    void refreshImage();
    QTransform toWidget() const;
    QPointF toSource(const QPointF &widgetPos) const;
    QRect widgetRect(const QRect &sourceRect) const;
    Hit hitTest(const QPointF &widgetPos) const;
    int handleAt(const QRect &sourceRect, const QPointF &widgetPos) const;
    QRect rectOf(const Hit &hit) const;
    const Mark *markOf(const Hit &hit) const;
    bool hitsMark(const Mark &mark, const QPointF &widgetPos) const;
    QPoint clampedSource(const QPointF &widgetPos) const;
    std::optional<MarkShape> creatingShape() const;   // форма, которую рисует текущий инструмент
    void openComment(const QUuid &markId);
    void updateCursor(const QPointF &widgetPos);

    CaptureSetModel *m_model;
    QUuid m_shotId;
    Tool m_tool = Tool::Mark;
    RenderStyle m_style;

    // Кэш исходника с размытием: пересчитывается только при смене кадра или размытий
    QImage m_image;
    QByteArray m_imageSource;
    QList<Redaction> m_imageRedactions;

    // Выделение и перетаскивание
    Hit m_selected;
    Drag m_drag = Drag::None;
    Hit m_dragTarget;
    QPointF m_pressSource;
    QRect m_dragOrigin;
    QRect m_live;      // текущий прямоугольник перетаскивания, в координатах исходника
    QList<QPoint> m_dragOriginPoints;   // то же для стрелки и карандаша
    QList<QPoint> m_livePoints;
    QRect m_crop;      // ожидающая подтверждения обрезка

    CommentPopup *m_popup;
};

} // namespace mp
