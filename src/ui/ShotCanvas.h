#pragma once

#include "core/CaptureSetModel.h"
#include "render/ShotRenderer.h"

#include <QImage>
#include <QUuid>
#include <QWidget>

#include <optional>

namespace mp {

class CommentPopup;

// Shot editor canvas: marks (frame, arrow, freehand, point), blur, crop (spec 5.2).
//
// Mark coordinates are physical pixels of the source. The canvas keeps one transform
// "source → window" (toWidget) and draws the marks with the same function as baking
// (paintMarks), so the screen shows exactly what goes to the chat.
// A live drag is drawn locally, and the model gets one edit when the mouse is released:
// this way the undo history has no hundreds of intermediate steps.
class ShotCanvas : public QWidget
{
    Q_OBJECT
public:
    enum class Tool { Mark, Arrow, Pen, Point, Blur, Crop };

    explicit ShotCanvas(CaptureSetModel *model, QWidget *parent = nullptr);

    void setShot(const QUuid &shotId);
    QUuid shotId() const { return m_shotId; }
    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setStyle(const RenderStyle &style);
    const RenderStyle &style() const { return m_style; }
    // Pen for new marks. If a mark is selected, the pen applies to it too (one edit in the history).
    void setPen(const MarkPen &pen);
    MarkPen pen() const { return m_pen; }

    bool hasPendingCrop() const { return !m_crop.isEmpty(); }
    void applyCrop();
    void cancelCrop();
    void deleteSelection();
    void commitComment();
    // Select a mark and open its comment (from the comment list)
    void editMark(const QUuid &markId);
    // The selected mark; empty if a blur or nothing is selected
    QUuid selectedMark() const { return m_selected.isMark ? m_selected.id : QUuid(); }

    QSize sizeHint() const override;

signals:
    void toolChanged(mp::ShotCanvas::Tool tool);
    // A mark is selected: the pen panel shows its color and width
    void markSelected(const mp::MarkPen &pen, mp::MarkShape shape);
    void cropPendingChanged(bool pending);
    void finishRequested();
    void selectedMarkChanged(const QUuid &markId);   // the comment list highlights its card

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
        int point = -1;   // arrow end: 0 is the tail, 1 is the tip
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
    std::optional<MarkShape> creatingShape() const;   // the shape the current tool draws
    void openComment(const QUuid &markId);
    void selectMark(const QUuid &markId);
    void setSelection(const Hit &hit);
    void updateCursor(const QPointF &widgetPos);

    CaptureSetModel *m_model;
    QUuid m_shotId;
    Tool m_tool = Tool::Mark;
    RenderStyle m_style;
    MarkPen m_pen;

    // Cache of the source with blur: recomputed only when the frame or the blurs change
    QImage m_image;
    QByteArray m_imageSource;
    QList<Redaction> m_imageRedactions;

    // Selection and dragging
    Hit m_selected;
    Drag m_drag = Drag::None;
    Hit m_dragTarget;
    QPointF m_pressSource;
    QRect m_dragOrigin;
    QRect m_live;      // the current drag rectangle, in source coordinates
    QList<QPoint> m_dragOriginPoints;   // the same for arrows and freehand lines
    QList<QPoint> m_livePoints;
    QRect m_crop;      // a crop waiting for confirmation

    CommentPopup *m_popup;
};

} // namespace mp
