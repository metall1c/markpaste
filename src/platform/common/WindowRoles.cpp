#include "platform/WindowRoles.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#ifdef POINTSHOT_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

namespace ps {

bool layerShellAvailable()
{
#ifdef POINTSHOT_HAVE_LAYERSHELLQT
    // LayerShellQt работает только поверх нативного Wayland-клиента Qt.
    // Наличие zwlr_layer_shell_v1 у композитора проверяет PlatformFactory.
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
#else
    return false;
#endif
}

#ifdef POINTSHOT_HAVE_LAYERSHELLQT
static LayerShellQt::Window *layerWindow(QWidget *widget)
{
    // Роль поверхности назначается при создании платформенного окна,
    // поэтому QWindow нужен до show(), но после этого вызова — никаких create() заново.
    widget->setAttribute(Qt::WA_NativeWindow);
    widget->createWinId();
    return LayerShellQt::Window::get(widget->windowHandle());
}
#endif

bool applyOverlayRole(QWidget *widget, QScreen *screen)
{
#ifdef POINTSHOT_HAVE_LAYERSHELLQT
    if (!layerShellAvailable())
        return false;
    auto *lw = layerWindow(widget);
    if (!lw)
        return false;
    lw->setLayer(LayerShellQt::Window::LayerOverlay);
    lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                 | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    lw->setExclusiveZone(-1);   // не сдвигаться из-за панелей
    lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    lw->setScope(QStringLiteral("pointshot-overlay"));
    lw->setScreen(screen);
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
#ifdef POINTSHOT_HAVE_LAYERSHELLQT
    if (!layerShellAvailable())
        return false;
    // Ввод проходит насквозь: на Wayland Qt ставит поверхности пустую область ввода
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
    lw->setScope(QStringLiteral("pointshot-ghost"));
    lw->setScreen(screen);
    widget->setGeometry(screen->geometry());
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    return false;
#endif
}

#ifdef POINTSHOT_HAVE_LAYERSHELLQT
static void setPlacement(LayerShellQt::Window *lw, const PanelPlacement &pl)
{
    using W = LayerShellQt::Window;
    const W::Anchor side = pl.edge == PanelEdge::Left ? W::AnchorLeft : W::AnchorRight;
    if (pl.offset < 0) {
        lw->setAnchors(W::Anchors(side));   // одна сторона — по центру вдоль неё
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
#ifdef POINTSHOT_HAVE_LAYERSHELLQT
    if (!layerShellAvailable())
        return false;
    auto *lw = layerWindow(widget);
    if (!lw)
        return false;
    using W = LayerShellQt::Window;
    lw->setLayer(W::LayerTop);
    lw->setExclusiveZone(0);
    lw->setKeyboardInteractivity(W::KeyboardInteractivityNone);
    lw->setScope(QStringLiteral("pointshot-strip"));
    lw->setScreen(screen);
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
#ifdef POINTSHOT_HAVE_LAYERSHELLQT
    if (!layerShellAvailable() || !widget->windowHandle())
        return false;
    auto *lw = LayerShellQt::Window::get(widget->windowHandle());
    if (!lw)
        return false;
    const bool screenChanged = lw->screen() != screen;
    if (screenChanged) {
        // Выход (output) поверхности задаётся при её создании: пересоздаём
        const bool visible = widget->isVisible();
        widget->hide();
        lw->setScreen(screen);
        setPlacement(lw, placement);
        if (visible)
            widget->show();
    } else {
        setPlacement(lw, placement);
        // Якоря и отступы layer-shell уходят композитору только с commit поверхности,
        // а commit Qt делает при отрисовке. Без перерисовки лента стояла на месте,
        // пока её что-нибудь не обновит (подсказка, наведение) — 3–5 с (docs/problems.md).
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

} // namespace ps
