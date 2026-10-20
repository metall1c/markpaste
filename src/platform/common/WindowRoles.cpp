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

#include <windows.h>
#endif

namespace mp {

bool layerShellAvailable()
{
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
    // LayerShellQt работает только поверх нативного Wayland-клиента Qt.
    // Наличие zwlr_layer_shell_v1 у композитора проверяет PlatformFactory.
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
#else
    return false;
#endif
}

#ifdef MARKPASTE_HAVE_LAYERSHELLQT
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
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
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
    lw->setScope(QStringLiteral("markpaste-overlay"));
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
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
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
    lw->setScope(QStringLiteral("markpaste-ghost"));
    lw->setScreen(screen);
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
#ifdef Q_OS_WIN
    Q_UNUSED(screen);
    Q_UNUSED(placement);
    // Панель без рамки, поверх окон и без активации: клик по ленте не забирает фокус у чата,
    // и «Вставить» работает в режиме 1 (ТЗ разд. 10). Положение задаёт StripWindow.
    // Скруглённые углы ленты: окно прозрачное, фон с рамкой рисует таблица стилей виджета
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
    auto *lw = layerWindow(widget);
    if (!lw)
        return false;
    using W = LayerShellQt::Window;
    lw->setLayer(W::LayerTop);
    lw->setExclusiveZone(0);
    lw->setKeyboardInteractivity(W::KeyboardInteractivityNone);
    lw->setScope(QStringLiteral("markpaste-strip"));
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
#ifdef MARKPASTE_HAVE_LAYERSHELLQT
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

#ifdef Q_OS_WIN
namespace {
// Конец перетаскивания окна — WM_EXITSIZEMOVE: его присылает модальный цикл Windows,
// в котором идёт перемещение после startSystemMove()
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
            QTimer::singleShot(0, this, m_fn);   // после выхода из цикла перемещения
        return false;
    }

private:
    QPointer<QWidget> m_widget;
    std::function<void()> m_fn;
};
} // namespace
#endif

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
    // Присоединённый к потоку активного окна поток может сделать активным своё окно
    const bool attached = theirs && theirs != ours && AttachThreadInput(ours, theirs, TRUE);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    if (attached)
        AttachThreadInput(ours, theirs, FALSE);
#endif
    widget->activateWindow();
}

} // namespace mp
