#include "XdgWindowMenu.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPoint>
#include <QWindow>
#include <QtGlobal>

#if defined(MARKPASTE_HAVE_XDG_WINDOW_MENU) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#define MARKPASTE_XDG_WINDOW_MENU 1
#include "xdg-shell-client-protocol.h"

#include <QtGui/qguiapplication_platform.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow_p.h>
#endif

Q_DECLARE_LOGGING_CATEGORY(lcWayland)

namespace mp::lx {

bool canShowXdgWindowMenu()
{
#ifdef MARKPASTE_XDG_WINDOW_MENU
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))
           && qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
#else
    return false;   // Qt 6.4 has no public input serial (Ubuntu 24.04)
#endif
}

bool showXdgWindowMenu(QWindow *window, const QPoint &posInWindow)
{
#ifdef MARKPASTE_XDG_WINDOW_MENU
    if (!window || !canShowXdgWindowMenu())
        return false;
    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    auto *toplevel = native ? static_cast<xdg_toplevel *>(native->nativeResourceForWindow("xdg_toplevel", window))
                            : nullptr;
    wl_seat *seat = app->lastInputSeat();
    if (!toplevel || !seat) {
        qCWarning(lcWayland) << "window menu: no xdg_toplevel or seat";
        return false;
    }
    // Surface-local coordinates; a frameless window has no decoration offset
    xdg_toplevel_show_window_menu(toplevel, seat, app->lastInputSerial(), posInWindow.x(), posInWindow.y());
    wl_display_flush(app->display());
    return true;
#else
    Q_UNUSED(window);
    Q_UNUSED(posInWindow);
    return false;
#endif
}

bool requestXdgActivationToken(QWindow *window, std::function<void(const QString &)> done)
{
#ifdef MARKPASTE_XDG_WINDOW_MENU
    if (!window || !canShowXdgWindowMenu())
        return false;
    auto *wl = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!wl || !app)
        return false;
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = QObject::connect(wl, &QNativeInterface::Private::QWaylandWindow::xdgActivationTokenCreated, wl,
                             [conn, done = std::move(done)](const QString &token) {
                                 QObject::disconnect(*conn);
                                 done(token);
                             });
    wl->requestXdgActivationToken(app->lastInputSerial());
    return true;
#else
    Q_UNUSED(window);
    Q_UNUSED(done);
    return false;
#endif
}

} // namespace mp::lx
