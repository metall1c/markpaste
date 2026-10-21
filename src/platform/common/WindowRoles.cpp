#include "platform/WindowRoles.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#ifdef MARKPASTE_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

#include <dwmapi.h>
#include <windows.h>
#endif

namespace mp {

bool layerShellAvailable()
{
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
    // LayerShellQt works only on top of Qt's native Wayland client.
    // PlatformFactory checks that the compositor has zwlr_layer_shell_v1.
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
#else
    return false;
#endif
}

#ifdef MARKPASTE_HAVE_LAYERSHELLQT
static LayerShellQt::Window *layerWindow(QWidget *widget)
{
    // The surface role is assigned when the platform window is created,
    // so the QWindow is needed before show(), but after this call no create() again.
    widget->setAttribute(Qt::WA_NativeWindow);
    widget->createWinId();
    return LayerShellQt::Window::get(widget->windowHandle());
}

// The output of the layer surface. LayerShellQt before 6.4 has no Window::setScreen and reads
// QWindow::screen() (ScreenFromQWindow, its default).
static void setLayerScreen(LayerShellQt::Window *lw, QWidget *widget, QScreen *screen)
{
#ifdef MARKPASTE_LAYERSHELL_SET_SCREEN
    Q_UNUSED(widget);
    lw->setScreen(screen);
#else
    Q_UNUSED(lw);
    widget->windowHandle()->setScreen(screen);
#endif
}

static QScreen *layerScreen(LayerShellQt::Window *lw, QWidget *widget)
{
#ifdef MARKPASTE_LAYERSHELL_SET_SCREEN
    Q_UNUSED(widget);
    return lw->screen();
#else
    Q_UNUSED(lw);
    return widget->windowHandle()->screen();
#endif
}
#endif

bool applyOverlayRole(QWidget *widget, QScreen *screen)
{
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
    if (!layerShellAvailable())
        return false;
    auto *lw = layerWindow(widget);
    if (!lw)
        return false;
    lw->setLayer(LayerShellQt::Window::LayerOverlay);
    lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                 | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    lw->setExclusiveZone(-1);   // do not move because of panels
    lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    lw->setScope(QStringLiteral("markpaste-overlay"));
    setLayerScreen(lw, widget, screen);
    widget->setGeometry(screen->geometry());
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    return false;
#endif
}

bool applyGhostRole(QWidget *widget, QScreen *screen)
{
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
    if (!layerShellAvailable())
        return false;
    // Input passes through: on Wayland Qt gives the surface an empty input region
    widget->setWindowFlag(Qt::WindowTransparentForInput);
    widget->setAttribute(Qt::WA_TranslucentBackground);
    auto *lw = layerWindow(widget);
    if (!lw)
        return false;
    using W = LayerShellQt::Window;
    lw->setLayer(W::LayerOverlay);
    lw->setAnchors(W::Anchors(W::AnchorTop | W::AnchorBottom | W::AnchorLeft | W::AnchorRight));
    lw->setExclusiveZone(-1);
    lw->setKeyboardInteractivity(W::KeyboardInteractivityNone);
    lw->setScope(QStringLiteral("markpaste-ghost"));
    setLayerScreen(lw, widget, screen);
    widget->setGeometry(screen->geometry());
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    return false;
#endif
}

#ifdef MARKPASTE_HAVE_LAYERSHELLQT
static void setPlacement(LayerShellQt::Window *lw, const PanelPlacement &pl)
{
    using W = LayerShellQt::Window;
    const W::Anchor side = pl.edge == PanelEdge::Left ? W::AnchorLeft : W::AnchorRight;
    if (pl.offset < 0) {
        lw->setAnchors(W::Anchors(side));   // one side: centered along it
        lw->setMargins(pl.edge == PanelEdge::Left ? QMargins(pl.margin, 0, 0, 0) : QMargins(0, 0, pl.margin, 0));
    } else {
        lw->setAnchors(W::Anchors(side | W::AnchorTop));
        lw->setMargins(pl.edge == PanelEdge::Left ? QMargins(pl.margin, pl.offset, 0, 0)
                                                  : QMargins(0, pl.offset, pl.margin, 0));
    }
}
#endif

bool applyPanelRole(QWidget *widget, QScreen *screen, const PanelPlacement &placement)
{
#ifdef Q_OS_WIN
    Q_UNUSED(screen);
    Q_UNUSED(placement);
    // A frameless panel, above windows and without activation: clicking the strip does not take focus from the chat,
    // and "Paste" works in mode 1 (spec section 10). StripWindow sets the position.
    // Rounded strip corners: the window is transparent, StripWindow paints the background with its frame
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setAttribute(Qt::WA_StyledBackground);
    widget->setWindowFlags(widget->windowFlags() | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus);
    widget->createWinId();
    const auto hwnd = reinterpret_cast<HWND>(widget->winId());
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_NOACTIVATE);
    return false;
#elif defined(MARKPASTE_HAVE_LAYERSHELLQT)
    if (!layerShellAvailable())
        return false;
    // Rounded strip corners: a surface with an alpha channel, StripWindow paints the background with its frame.
    // Before the window is created: the surface format is chosen when it is created.
    widget->setAttribute(Qt::WA_TranslucentBackground);
    auto *lw = layerWindow(widget);
    if (!lw)
        return false;
    using W = LayerShellQt::Window;
    lw->setLayer(W::LayerTop);
    lw->setExclusiveZone(0);
    lw->setKeyboardInteractivity(W::KeyboardInteractivityNone);
    lw->setScope(QStringLiteral("markpaste-strip"));
    setLayerScreen(lw, widget, screen);
    setPlacement(lw, placement);
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    Q_UNUSED(placement);
    return false;
#endif
}

bool movePanel(QWidget *widget, QScreen *screen, const PanelPlacement &placement)
{
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
    if (!layerShellAvailable() || !widget->windowHandle())
        return false;
    auto *lw = LayerShellQt::Window::get(widget->windowHandle());
    if (!lw)
        return false;
    const bool screenChanged = layerScreen(lw, widget) != screen;
    if (screenChanged) {
        // The surface output is set when it is created: recreate it
        const bool visible = widget->isVisible();
        widget->hide();
        setLayerScreen(lw, widget, screen);
        setPlacement(lw, placement);
        if (visible)
            widget->show();
    } else {
        setPlacement(lw, placement);
        // layer-shell anchors and margins reach the compositor only with a surface commit,
        // and Qt commits when it paints. Without a repaint the strip stayed in place
        // until something updated it (a tooltip, hover), 3–5 s (docs/problems.md).
        widget->update();
        widget->windowHandle()->requestUpdate();
    }
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    Q_UNUSED(placement);
    return false;
#endif
}

#ifdef Q_OS_WIN
namespace {
// The end of a window drag is WM_EXITSIZEMOVE: it is sent by the Windows modal loop
// in which the move after startSystemMove() runs
class MoveEndFilter : public QObject, public QAbstractNativeEventFilter
{
public:
    MoveEndFilter(QWidget *widget, std::function<void()> fn)
        : QObject(widget), m_widget(widget), m_fn(std::move(fn))
    {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
    ~MoveEndFilter() override
    {
        if (QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(this);
    }
    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *) override
    {
        if (type != "windows_generic_MSG")
            return false;
        const auto *msg = static_cast<const MSG *>(message);
        if (msg->message == WM_EXITSIZEMOVE && m_widget && m_widget->internalWinId()
            && msg->hwnd == reinterpret_cast<HWND>(m_widget->internalWinId()))
            QTimer::singleShot(0, this, m_fn);   // after leaving the move loop
        return false;
    }

private:
    QPointer<QWidget> m_widget;
    std::function<void()> m_fn;
};
} // namespace
#endif

#ifdef Q_OS_WIN
namespace {
// DWMWA_TRANSITIONS_FORCEDISABLED for every top-level window when shown: the attribute lives
// as long as the HWND, and Qt recreates it when window flags change
class NoAnimationFilter : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject *obj, QEvent *e) override
    {
        if (e->type() == QEvent::Show && obj->isWidgetType()) {
            auto *w = static_cast<QWidget *>(obj);
            if (w->isWindow() && w->internalWinId()) {
                const BOOL off = TRUE;
                DwmSetWindowAttribute(reinterpret_cast<HWND>(w->internalWinId()), DWMWA_TRANSITIONS_FORCEDISABLED,
                                      &off, sizeof(off));
            }
        }
        return false;
    }
};
} // namespace
#endif

void disableWindowAnimations()
{
#ifdef Q_OS_WIN
    QCoreApplication::instance()->installEventFilter(new NoAnimationFilter(QCoreApplication::instance()));
#endif
}

bool watchPanelMoves(QWidget *widget, std::function<void()> onMoveFinished)
{
#ifdef Q_OS_WIN
    new MoveEndFilter(widget, std::move(onMoveFinished));
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(onMoveFinished);
    return false;
#endif
}

void forceActivate(QWidget *widget)
{
#ifdef Q_OS_WIN
    const auto hwnd = reinterpret_cast<HWND>(widget->winId());
    HWND foreground = GetForegroundWindow();
    const DWORD ours = GetCurrentThreadId();
    const DWORD theirs = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    // A thread attached to the active window's thread may make its own window active
    const bool attached = theirs && theirs != ours && AttachThreadInput(ours, theirs, TRUE);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    if (attached)
        AttachThreadInput(ours, theirs, FALSE);
#endif
    widget->activateWindow();
}

} // namespace mp
