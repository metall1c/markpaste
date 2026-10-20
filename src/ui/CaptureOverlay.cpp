#include "CaptureOverlay.h"

#include "platform/WindowRoles.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>

#include <cmath>

namespace mp {

namespace {
const QColor kAccent(0xFF, 0x5A, 0x1F);
constexpr int kHandle = 8;          // логические px
constexpr int kMinSize = 4;         // физические px: меньше — считаем кликом
constexpr int kMagnifier = 128;     // логические px
constexpr int kZoom = 4;            // CAP-3

QScreen *screenByName(const QString &name)
{
    for (QScreen *s : QGuiApplication::screens())
        if (s->name() == name)
            return s;
    return nullptr;
}
} // namespace

// --- CaptureSession ---

CaptureSession::CaptureSession(const QList<ScreenFrame> &frames, bool quickMode, QObject *parent)
    : QObject(parent), m_frames(frames), m_quick(quickMode)
{
}

CaptureSession::~CaptureSession()
{
    closeAll();
}

void CaptureSession::start()
{
    for (const ScreenFrame &f : std::as_const(m_frames)) {
        if (f.image.isNull())
            continue;
        QScreen *screen = screenByName(f.screenName);
        if (!screen)
            continue;
        auto *w = new OverlayWidget(this, f, m_quick);
        m_widgets << w;
        if (!applyOverlayRole(w, screen)) {
            // Без layer-shell: обычное окно на весь экран (GNOME, X11).
            w->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
            w->setGeometry(screen->geometry());
            w->createWinId();
            w->windowHandle()->setScreen(screen);
            w->showFullScreen();
        } else {
            w->show();
        }
    }
    if (m_widgets.isEmpty()) {
        m_done = true;
        emit cancelled();
        return;
    }
    // Клавиатура — окну под курсором.
    const QPoint cursor = QCursor::pos();
    for (const auto &w : std::as_const(m_widgets))
        if (w && w->geometry().contains(cursor)) {
            w->activateWindow();
            w->setFocus();
        }
}

void CaptureSession::onSelectionStarted(OverlayWidget *source)
{
    for (const auto &w : std::as_const(m_widgets))
        if (w && w != source)
            w->clearSelection();
}

void CaptureSession::confirm(OverlayWidget *source, const QRect &physicalRect)
{
    if (m_done)
        return;
    m_done = true;
    CaptureSelection sel;
    sel.image = source->frame().image.copy(physicalRect);
    sel.image.setDevicePixelRatio(1.0);
    sel.devicePixelRatio = source->frame().scale;
    sel.screenName = source->frame().screenName;
    closeAll();
    emit selected(sel);
}

void CaptureSession::cancel()
{
    if (m_done)
        return;
    m_done = true;
    closeAll();
    emit cancelled();
}

void CaptureSession::closeAll()
{
    for (const auto &w : std::as_const(m_widgets))
        if (w) {
            w->hide();
            w->deleteLater();
        }
    m_widgets.clear();
}

// --- OverlayWidget ---

OverlayWidget::OverlayWidget(CaptureSession *session, const ScreenFrame &frame, bool quickMode)
    : m_session(session), m_frame(frame), m_quick(quickMode)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    m_display = frame.image;
    m_display.setDevicePixelRatio(frame.scale > 0 ? frame.scale : 1.0);
}

void OverlayWidget::showEvent(QShowEvent *)
{
    activateWindow();
    setFocus();
}

void OverlayWidget::closeEvent(QCloseEvent *e)
{
    // Окно закрыли извне (Alt+F4 на GNOME и X11, где оверлей — обычное окно). Без отмены сеанс
    // остался бы незавершённым, и следующий захват не запустился бы до перезапуска программы.
    m_session->cancel();
    QWidget::closeEvent(e);
}

void OverlayWidget::clearSelection()
{
    m_mode = Mode::Idle;
    m_sel = {};
    update();
}

QRect OverlayWidget::toPhysical(const QRectF &logical) const
{
    // Наружу расширяем до целых физических пикселей: ничего из выделенного не потеряется.
    const qreal s = m_frame.scale > 0 ? m_frame.scale : 1.0;
    const int x0 = int(std::floor(logical.left() * s));
    const int y0 = int(std::floor(logical.top() * s));
    const int x1 = int(std::ceil(logical.right() * s));
    const int y1 = int(std::ceil(logical.bottom() * s));
    return QRect(QPoint(x0, y0), QPoint(x1 - 1, y1 - 1)).intersected(m_frame.image.rect());
}

QRectF OverlayWidget::selectionRect() const
{
    return m_sel.normalized();
}

int OverlayWidget::handleAt(const QPointF &pos) const
{
    const QRectF r = selectionRect();
    if (r.isEmpty())
        return None;
    const qreal d = kHandle;
    int h = None;
    if (std::abs(pos.x() - r.left()) <= d)
        h |= Left;
    else if (std::abs(pos.x() - r.right()) <= d)
        h |= Right;
    if (std::abs(pos.y() - r.top()) <= d)
        h |= Top;
    else if (std::abs(pos.y() - r.bottom()) <= d)
        h |= Bottom;
    if (h != None && r.adjusted(-d, -d, d, d).contains(pos))
        return h;
    return None;
}

void OverlayWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawImage(QPointF(0, 0), m_display);

    const QRectF sel = selectionRect();
    QPainterPath dim;
    dim.addRect(rect());
    if (!sel.isEmpty()) {
        QPainterPath hole;
        hole.addRect(sel);
        dim = dim.subtracted(hole);
    }
    p.fillPath(dim, QColor(0, 0, 0, 110));

    if (!sel.isEmpty()) {
        p.setPen(QPen(Qt::white, 1));
        p.drawRect(sel.adjusted(-1, -1, 0, 0));
        p.setPen(QPen(kAccent, 1, Qt::DashLine));
        p.drawRect(sel.adjusted(-1, -1, 0, 0));

        // Размер в физических пикселях
        const QRect phys = toPhysical(sel);
        const QString size = QStringLiteral("%1 × %2").arg(phys.width()).arg(phys.height());
        QFont f = font();
        f.setPixelSize(13);
        f.setBold(true);
        p.setFont(f);
        const QRectF label = QFontMetricsF(f).boundingRect(size).adjusted(-6, -3, 6, 3);
        QPointF at(sel.left(), sel.top() - label.height() - 4);
        if (at.y() < 0)
            at.setY(sel.top() + 4);
        const QRectF box(at, label.size());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 180));
        p.drawRoundedRect(box, 4, 4);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, size);

        if (m_mode == Mode::Adjusting || m_mode == Mode::Moving || m_mode == Mode::Resizing) {
            p.setPen(QPen(kAccent, 1));
            p.setBrush(Qt::white);
            for (QPointF c : {sel.topLeft(), sel.topRight(), sel.bottomLeft(), sel.bottomRight(),
                              QPointF(sel.center().x(), sel.top()), QPointF(sel.center().x(), sel.bottom()),
                              QPointF(sel.left(), sel.center().y()), QPointF(sel.right(), sel.center().y())})
                p.drawRect(QRectF(c.x() - 3.5, c.y() - 3.5, 7, 7));
        }
    } else {
        // Подсказка, пока ничего не выделено
        const QString hint = tr("Drag to select an area · Click — whole screen · Esc — cancel");
        QFont f = font();
        f.setPixelSize(15);
        p.setFont(f);
        const QRectF box = QFontMetricsF(f).boundingRect(hint).adjusted(-12, -8, 12, 8);
        const QRectF placed(QPointF((width() - box.width()) / 2, 32), box.size());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 170));
        p.drawRoundedRect(placed, 8, 8);
        p.setPen(Qt::white);
        p.drawText(placed, Qt::AlignCenter, hint);
    }

    if (m_hasCursor && (m_mode == Mode::Idle || m_mode == Mode::Selecting))
        paintMagnifier(p, m_cursor);
}

void OverlayWidget::paintMagnifier(QPainter &p, const QPointF &pos)
{
    const qreal s = m_frame.scale > 0 ? m_frame.scale : 1.0;
    // Квадрат kMagnifier логических px показывает (kMagnifier / kZoom) логических px вокруг курсора.
    const qreal srcLogical = qreal(kMagnifier) / kZoom;
    const QRectF srcPhys((pos.x() - srcLogical / 2) * s, (pos.y() - srcLogical / 2) * s, srcLogical * s, srcLogical * s);

    QRectF box(pos.x() + 24, pos.y() + 24, kMagnifier, kMagnifier);
    if (box.right() > width())
        box.moveRight(pos.x() - 24);
    if (box.bottom() > height() - 24)
        box.moveBottom(pos.y() - 24);

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);   // видно отдельные пиксели
    QPainterPath clip;
    clip.addRoundedRect(box, 6, 6);
    p.setClipPath(clip);
    p.fillRect(box, Qt::black);
    p.drawImage(box, m_frame.image, srcPhys);
    p.setPen(QPen(QColor(255, 255, 255, 200), 1));
    p.drawLine(QPointF(box.center().x(), box.top()), QPointF(box.center().x(), box.bottom()));
    p.drawLine(QPointF(box.left(), box.center().y()), QPointF(box.right(), box.center().y()));
    p.setClipping(false);
    p.setPen(QPen(kAccent, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(box, 6, 6);

    const QString coords = QStringLiteral("%1, %2").arg(int(pos.x() * s)).arg(int(pos.y() * s));
    QFont f = font();
    f.setPixelSize(11);
    p.setFont(f);
    const QRectF label(box.left(), box.bottom() + 2, box.width(), 16);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(label, 3, 3);
    p.setPen(Qt::white);
    p.drawText(label, Qt::AlignCenter, coords);
    p.restore();
}

void OverlayWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton) {
        m_session->cancel();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    const QPointF pos = e->position();
    m_session->onSelectionStarted(this);

    if (m_mode == Mode::Adjusting) {
        m_handle = handleAt(pos);
        m_anchor = pos;
        m_selAtPress = selectionRect();
        if (m_handle != None) {
            m_mode = Mode::Resizing;
            return;
        }
        if (m_selAtPress.contains(pos)) {
            m_mode = Mode::Moving;
            return;
        }
    }
    m_mode = Mode::Selecting;
    m_anchor = pos;
    m_sel = QRectF(pos, pos);
    update();
}

void OverlayWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    m_cursor = pos;
    m_hasCursor = true;
    const QRectF bounds = rect();
    switch (m_mode) {
    case Mode::Selecting:
        m_sel = QRectF(m_anchor, pos).normalized().intersected(bounds);
        break;
    case Mode::Moving: {
        QRectF r = m_selAtPress.translated(pos - m_anchor);
        r.moveLeft(std::clamp(r.left(), 0.0, bounds.width() - r.width()));
        r.moveTop(std::clamp(r.top(), 0.0, bounds.height() - r.height()));
        m_sel = r;
        break;
    }
    case Mode::Resizing: {
        QRectF r = m_selAtPress;
        const QPointF d = pos - m_anchor;
        if (m_handle & Left) r.setLeft(r.left() + d.x());
        if (m_handle & Right) r.setRight(r.right() + d.x());
        if (m_handle & Top) r.setTop(r.top() + d.y());
        if (m_handle & Bottom) r.setBottom(r.bottom() + d.y());
        m_sel = r.normalized().intersected(bounds);
        break;
    }
    case Mode::Adjusting: {
        const int h = handleAt(pos);
        if (h == (Left | Top) || h == (Right | Bottom))
            setCursor(Qt::SizeFDiagCursor);
        else if (h == (Right | Top) || h == (Left | Bottom))
            setCursor(Qt::SizeBDiagCursor);
        else if (h & (Left | Right))
            setCursor(Qt::SizeHorCursor);
        else if (h & (Top | Bottom))
            setCursor(Qt::SizeVerCursor);
        else
            setCursor(selectionRect().contains(pos) ? Qt::SizeAllCursor : Qt::CrossCursor);
        break;
    }
    case Mode::Idle:
        break;
    }
    update();
}

void OverlayWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (m_mode == Mode::Selecting) {
        const QRect phys = toPhysical(selectionRect());
        if (phys.width() < kMinSize || phys.height() < kMinSize)
            m_sel = rect();   // клик — весь экран
        m_mode = Mode::Adjusting;
        if (m_quick)
            return confirmSelection();
    } else if (m_mode == Mode::Moving || m_mode == Mode::Resizing) {
        m_mode = Mode::Adjusting;
    }
    update();
}

void OverlayWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !selectionRect().isEmpty())
        confirmSelection();
}

void OverlayWidget::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        m_session->cancel();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!selectionRect().isEmpty())
            confirmSelection();
        return;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        if (selectionRect().isEmpty())
            return;
        // Шаг — один физический пиксель; с Shift меняется размер, а не положение.
        const qreal step = 1.0 / (m_frame.scale > 0 ? m_frame.scale : 1.0);
        const qreal dx = e->key() == Qt::Key_Left ? -step : e->key() == Qt::Key_Right ? step : 0;
        const qreal dy = e->key() == Qt::Key_Up ? -step : e->key() == Qt::Key_Down ? step : 0;
        QRectF r = selectionRect();
        if (e->modifiers() & Qt::ShiftModifier)
            r.setBottomRight(r.bottomRight() + QPointF(dx, dy));
        else
            r.translate(dx, dy);
        m_sel = r.intersected(rect());
        m_mode = Mode::Adjusting;
        update();
        return;
    }
    default:
        QWidget::keyPressEvent(e);
    }
}

void OverlayWidget::confirmSelection()
{
    const QRect phys = toPhysical(selectionRect());
    if (phys.isEmpty())
        return;
    m_session->confirm(this, phys);
}

} // namespace mp
