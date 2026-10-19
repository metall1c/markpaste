#include "ShotCanvas.h"

#include "CommentPopup.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace ps {

namespace {
constexpr int kMinSourceSize = 6;   // физические px: меньше — случайный клик, а не метка
constexpr qreal kHandleRadius = 6;  // логические px
constexpr int kPad = 16;
} // namespace

ShotCanvas::ShotCanvas(CaptureSetModel *model, QWidget *parent)
    : QWidget(parent), m_model(model), m_popup(new CommentPopup(this))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 200);
    connect(m_model, &CaptureSetModel::changed, this, [this] {
        refreshImage();
        // Выделенный объект мог исчезнуть после отмены
        if (m_selected.valid() && rectOf(m_selected).isNull())
            m_selected = {};
        if (m_popup->isVisible() && shot() && shot()->markIndex(m_popup->markId()) < 0)
            m_popup->dismiss();
        update();
    });
    connect(m_popup, &CommentPopup::finishRequested, this, &ShotCanvas::finishRequested);
    connect(m_popup, &CommentPopup::committed, this, [this](const QUuid &markId, const QString &text) {
        m_model->setMarkComment(m_shotId, markId, text);
        setFocus();
    });
}

QSize ShotCanvas::sizeHint() const
{
    const Shot *s = shot();
    if (!s)
        return {640, 400};
    const qreal dpr = s->devicePixelRatio > 0 ? s->devicePixelRatio : 1.0;
    return (QSizeF(s->visibleRect().size()) / dpr).toSize() + QSize(2 * kPad, 2 * kPad);
}

const Shot *ShotCanvas::shot() const
{
    return m_model->shot(m_shotId);
}

int ShotCanvas::shotIndex() const
{
    return m_model->set().shotIndex(m_shotId);
}

void ShotCanvas::setShot(const QUuid &shotId)
{
    if (m_popup->isVisible())
        m_popup->commit();
    m_shotId = shotId;
    m_selected = {};
    m_crop = {};
    m_drag = Drag::None;
    m_imageSource.clear();
    refreshImage();
    emit cropPendingChanged(false);
    update();
}

void ShotCanvas::setTool(Tool tool)
{
    if (m_tool == tool)
        return;
    if (m_tool == Tool::Crop)
        cancelCrop();
    m_tool = tool;
    emit toolChanged(tool);
    update();
}

void ShotCanvas::setStyle(const RenderStyle &style)
{
    m_style = style;
    m_imageSource.clear();
    refreshImage();
    update();
}

void ShotCanvas::refreshImage()
{
    const Shot *s = shot();
    if (!s) {
        m_image = {};
        return;
    }
    // QByteArray разделяется неявно: сравнение с тем же буфером — по указателю, дёшево.
    if (m_imageSource.constData() == s->sourcePng.constData() && m_imageRedactions == s->redactions && !m_image.isNull())
        return;
    m_imageSource = s->sourcePng;
    m_imageRedactions = s->redactions;
    m_image = sourceWithRedactions(*s, m_style);
}

QTransform ShotCanvas::toWidget() const
{
    const Shot *s = shot();
    if (!s)
        return {};
    const QRect vis = s->visibleRect();
    const qreal dpr = s->devicePixelRatio > 0 ? s->devicePixelRatio : 1.0;
    // Не крупнее «как на экране», иначе мелкий снимок расплывается.
    const qreal fit = std::min((width() - 2.0 * kPad) / vis.width(), (height() - 2.0 * kPad) / vis.height());
    const qreal k = std::max(0.05, std::min(fit, 1.0 / dpr));
    const qreal ox = (width() - vis.width() * k) / 2;
    const qreal oy = (height() - vis.height() * k) / 2;
    QTransform t;
    t.translate(ox, oy);
    t.scale(k, k);
    t.translate(-vis.left(), -vis.top());
    return t;
}

QPointF ShotCanvas::toSource(const QPointF &widgetPos) const
{
    return toWidget().inverted().map(widgetPos);
}

QRect ShotCanvas::widgetRect(const QRect &sourceRect) const
{
    return toWidget().mapRect(QRectF(sourceRect)).toAlignedRect();
}

QRect ShotCanvas::rectOf(const Hit &hit) const
{
    const Shot *s = shot();
    if (!s || !hit.valid())
        return {};
    if (hit.isMark) {
        const int i = s->markIndex(hit.id);
        return i < 0 ? QRect() : s->marks[i].rect;
    }
    const int i = s->redactionIndex(hit.id);
    return i < 0 ? QRect() : s->redactions[i].rect;
}

int ShotCanvas::handleAt(const QRect &sourceRect, const QPointF &pos) const
{
    const QRectF r = toWidget().mapRect(QRectF(sourceRect));
    const qreal d = kHandleRadius;
    if (!r.adjusted(-d, -d, d, d).contains(pos))
        return HNone;
    int h = HNone;
    if (std::abs(pos.x() - r.left()) <= d)
        h |= HLeft;
    else if (std::abs(pos.x() - r.right()) <= d)
        h |= HRight;
    if (std::abs(pos.y() - r.top()) <= d)
        h |= HTop;
    else if (std::abs(pos.y() - r.bottom()) <= d)
        h |= HBottom;
    return h;
}

ShotCanvas::Hit ShotCanvas::hitTest(const QPointF &pos) const
{
    const Shot *s = shot();
    if (!s)
        return {};
    // Сначала ручки выделенного объекта
    if (m_selected.valid()) {
        const int h = handleAt(rectOf(m_selected), pos);
        if (h != HNone)
            return Hit{m_selected.isMark, m_selected.id, h};
    }
    const QPointF src = toSource(pos);
    // Верхние — последние в списке; метки поверх размытия
    for (int i = int(s->marks.size()) - 1; i >= 0; --i)
        if (QRectF(s->marks[i].rect).contains(src))
            return Hit{true, s->marks[i].id, HNone};
    for (int i = int(s->redactions.size()) - 1; i >= 0; --i)
        if (QRectF(s->redactions[i].rect).contains(src))
            return Hit{false, s->redactions[i].id, HNone};
    return {};
}

void ShotCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    const Shot *s = shot();
    if (!s || m_image.isNull()) {
        p.drawText(rect(), Qt::AlignCenter, tr("No shot selected"));
        return;
    }
    const QRect vis = s->visibleRect();
    const QTransform t = toWidget();

    // Снимок и метки — в координатах исходника
    Shot preview = *s;
    if (m_drag != Drag::None && !m_live.isEmpty()) {
        if (m_drag == Drag::Create && m_tool == Tool::Mark)
            preview.marks.append(Mark{QUuid(), m_live, {}});
        else if (m_dragTarget.isMark) {
            const int i = preview.markIndex(m_dragTarget.id);
            if (i >= 0)
                preview.marks[i].rect = m_live;
        }
    }

    p.save();
    p.setTransform(t);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(QRectF(vis), m_image, QRectF(vis));
    paintMarks(p, preview, std::max(0, shotIndex()), m_style, QRectF(vis));
    p.restore();

    // Рамки размытия видны только в редакторе
    p.setRenderHint(QPainter::Antialiasing);
    QPen redPen(palette().highlight().color(), 1, Qt::DashLine);
    p.setBrush(Qt::NoBrush);
    for (const Redaction &r : s->redactions) {
        QRect rr = r.rect;
        if (m_drag == Drag::Move || m_drag == Drag::Resize)
            if (!m_dragTarget.isMark && m_dragTarget.id == r.id)
                rr = m_live;
        p.setPen(redPen);
        p.drawRect(widgetRect(rr));
    }
    if (m_drag == Drag::Create && m_tool == Tool::Blur && !m_live.isEmpty()) {
        p.setPen(redPen);
        p.drawRect(widgetRect(m_live));
    }

    // Ручки выделенного объекта
    if (m_selected.valid() && m_drag == Drag::None) {
        const QRectF r = t.mapRect(QRectF(rectOf(m_selected)));
        p.setPen(QPen(palette().highlight().color(), 1));
        p.setBrush(Qt::white);
        for (QPointF c : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()})
            p.drawEllipse(c, 4, 4);
    }

    // Обрезка: всё снаружи затемнено
    const QRect crop = (m_drag == Drag::Create && m_tool == Tool::Crop) ? m_live : m_crop;
    if (m_tool == Tool::Crop && !crop.isEmpty()) {
        QPainterPath outside;
        outside.addRect(t.mapRect(QRectF(vis)));
        QPainterPath inside;
        inside.addRect(t.mapRect(QRectF(crop)));
        p.fillPath(outside.subtracted(inside), QColor(0, 0, 0, 140));
        p.setPen(QPen(Qt::white, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(t.mapRect(QRectF(crop)));
    }
}

void ShotCanvas::updateCursor(const QPointF &pos)
{
    if (m_tool == Tool::Crop) {
        setCursor(Qt::CrossCursor);
        return;
    }
    const Hit h = hitTest(pos);
    if (h.handle == (HLeft | HTop) || h.handle == (HRight | HBottom))
        setCursor(Qt::SizeFDiagCursor);
    else if (h.handle == (HRight | HTop) || h.handle == (HLeft | HBottom))
        setCursor(Qt::SizeBDiagCursor);
    else if (h.handle & (HLeft | HRight))
        setCursor(Qt::SizeHorCursor);
    else if (h.handle & (HTop | HBottom))
        setCursor(Qt::SizeVerCursor);
    else if (h.valid())
        setCursor(Qt::SizeAllCursor);
    else
        setCursor(Qt::CrossCursor);
}

void ShotCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !shot())
        return;
    if (m_popup->isVisible())
        m_popup->commit();
    setFocus();
    const QPointF pos = e->position();
    m_pressSource = toSource(pos);

    if (m_tool == Tool::Crop) {
        m_drag = Drag::Create;
        m_live = {};
        return;
    }

    const Hit h = hitTest(pos);
    if (h.valid()) {
        m_selected = Hit{h.isMark, h.id, HNone};
        m_dragTarget = h;
        m_dragOrigin = rectOf(h);
        m_live = m_dragOrigin;
        m_drag = h.handle != HNone ? Drag::Resize : Drag::Move;
    } else {
        m_selected = {};
        m_dragTarget = {};
        m_drag = Drag::Create;
        m_live = {};
    }
    update();
}

void ShotCanvas::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    const Shot *s = shot();
    if (!s)
        return;
    if (m_drag == Drag::None) {
        updateCursor(pos);
        return;
    }
    const QRect bounds = s->visibleRect();
    const QPointF src = toSource(pos);
    switch (m_drag) {
    case Drag::Create:
        m_live = rectFromPoints(m_pressSource.toPoint(), src.toPoint()).intersected(bounds);
        break;
    case Drag::Move: {
        QRect r = m_dragOrigin.translated((src - m_pressSource).toPoint());
        r.moveLeft(std::clamp(r.left(), bounds.left(), bounds.right() - r.width() + 1));
        r.moveTop(std::clamp(r.top(), bounds.top(), bounds.bottom() - r.height() + 1));
        m_live = r;
        break;
    }
    case Drag::Resize: {
        QPoint tl = m_dragOrigin.topLeft();
        QPoint br = m_dragOrigin.bottomRight();
        const QPoint d = (src - m_pressSource).toPoint();
        if (m_dragTarget.handle & HLeft) tl.rx() += d.x();
        if (m_dragTarget.handle & HRight) br.rx() += d.x();
        if (m_dragTarget.handle & HTop) tl.ry() += d.y();
        if (m_dragTarget.handle & HBottom) br.ry() += d.y();
        m_live = rectFromPoints(tl, br).intersected(bounds);
        break;
    }
    case Drag::None:
        break;
    }
    update();
}

void ShotCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || m_drag == Drag::None)
        return;
    const Drag drag = std::exchange(m_drag, Drag::None);
    const QRect live = m_live;
    m_live = {};

    if (drag == Drag::Create) {
        if (live.width() < kMinSourceSize || live.height() < kMinSourceSize) {
            update();
            return;
        }
        if (m_tool == Tool::Crop) {
            m_crop = live;
            emit cropPendingChanged(true);
        } else if (m_tool == Tool::Mark) {
            const QUuid id = m_model->addMark(m_shotId, live);
            if (!id.isNull()) {
                m_selected = Hit{true, id, HNone};
                openComment(id);
            }
        } else {
            const QUuid id = m_model->addRedaction(m_shotId, live);
            if (!id.isNull())
                m_selected = Hit{false, id, HNone};
        }
    } else if (live != m_dragOrigin && !live.isEmpty()) {
        if (m_dragTarget.isMark)
            m_model->setMarkRect(m_shotId, m_dragTarget.id, live);
        else
            m_model->setRedactionRect(m_shotId, m_dragTarget.id, live);
    }
    update();
}

void ShotCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    const Hit h = hitTest(e->position());
    if (h.valid() && h.isMark)
        openComment(h.id);   // EDT-4
}

void ShotCanvas::openComment(const QUuid &markId)
{
    const Shot *s = shot();
    if (!s)
        return;
    const int i = s->markIndex(markId);
    if (i < 0)
        return;
    m_popup->open(markId, markLabel(shotIndex(), i), s->marks[i].comment, widgetRect(s->marks[i].rect));
}

void ShotCanvas::editMark(const QUuid &markId)
{
    m_selected = Hit{true, markId, HNone};
    update();
    openComment(markId);
}

void ShotCanvas::commitComment()
{
    if (m_popup->isVisible())
        m_popup->commit();
}

void ShotCanvas::deleteSelection()
{
    if (!m_selected.valid())
        return;
    if (m_selected.isMark)
        m_model->removeMark(m_shotId, m_selected.id);
    else
        m_model->removeRedaction(m_shotId, m_selected.id);
    m_selected = {};
    update();
}

void ShotCanvas::applyCrop()
{
    if (m_crop.isEmpty())
        return;
    const int outside = m_model->marksOutsideCrop(m_shotId, m_crop);
    if (outside > 0) {
        const auto answer = QMessageBox::question(
            this, tr("Crop"), tr("%n mark(s) outside the new area will be deleted. Crop anyway?", nullptr, outside));
        if (answer != QMessageBox::Yes)
            return;
    }
    m_model->setCrop(m_shotId, m_crop);
    cancelCrop();
    setTool(Tool::Mark);
}

void ShotCanvas::cancelCrop()
{
    if (m_crop.isEmpty())
        return;
    m_crop = {};
    emit cropPendingChanged(false);
    update();
}

void ShotCanvas::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        deleteSelection();
        return;
    case Qt::Key_Escape:
        if (hasPendingCrop())
            cancelCrop();
        else if (m_selected.valid()) {
            m_selected = {};
            update();
        } else
            QWidget::keyPressEvent(e);   // окно закроется
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (hasPendingCrop())
            applyCrop();
        else if (m_selected.valid() && m_selected.isMark)
            openComment(m_selected.id);
        return;
    default:
        QWidget::keyPressEvent(e);
    }
}

void ShotCanvas::resizeEvent(QResizeEvent *)
{
    if (m_popup->isVisible())
        m_popup->commit();
}

} // namespace ps
