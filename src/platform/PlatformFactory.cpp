#include "PlatformFactory.h"

#include "common/FallbackCapture.h"
#include "common/QtClipboard.h"
#include "common/QtScreenCapture.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSettings>

#ifdef Q_OS_LINUX
#include "linux/kde/KWinScreenshot.h"
#include "linux/portal/PortalBus.h"
#include "linux/portal/PortalGlobalShortcuts.h"
#include "linux/portal/PortalRemoteDesktop.h"
#include "linux/portal/PortalScreenshot.h"
#include "linux/wayland/DataControlClipboard.h"
#include "linux/wayland/WaylandConnection.h"
#endif

Q_LOGGING_CATEGORY(lcPlatform, "markpaste.platform")

namespace mp {

#ifdef Q_OS_LINUX
static void createLinuxWayland(Platform &p, const QString &appId)
{
    using namespace mp::lx;
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    p.environment = QStringLiteral("Wayland · %1").arg(desktop.isEmpty() ? QStringLiteral("unknown") : desktop);

    // Регистрация у порталов — первым же вызовом (ТЗ разд. 9).
    PortalBus &bus = PortalBus::instance();
    const Status reg = bus.registerApp(appId);
    if (!reg.ok)
        p.notes << QStringLiteral("Portal app id registration failed: %1. Permissions may be asked every time.").arg(reg.error);

    WaylandConnection *wl = WaylandConnection::instance();
    p.supportsLayerShell = wl && wl->hasGlobal("zwlr_layer_shell_v1");

    // Захват: KWin ScreenShot2 (быстро, без диалога) → портал Screenshot.
    std::unique_ptr<ICapture> portalShot;
    if (PortalScreenshot::isAvailable())
        portalShot = std::make_unique<PortalScreenshot>();
    if (KWinScreenshot::isPresent())
        p.capture = std::make_unique<FallbackCapture>(std::make_unique<KWinScreenshot>(), std::move(portalShot));
    else if (portalShot)
        p.capture = std::move(portalShot);
    else
        p.notes << QStringLiteral("No screenshot portal: install xdg-desktop-portal and a backend for your desktop.");

    // Глобальные клавиши
    if (PortalGlobalShortcuts::isAvailable())
        p.hotkeys = std::make_unique<PortalGlobalShortcuts>();
    else
        p.notes << QStringLiteral("No GlobalShortcuts portal: bind a key to `markpaste --capture` in your desktop settings.");

    // Буфер из фона
    if (auto *dc = DataControlClipboard::create())
        p.clipboard.reset(dc);
    else {
        p.clipboard = std::make_unique<QtClipboard>(false);
        p.notes << QStringLiteral("Compositor has no ext-data-control-v1: sequential paste is unavailable, "
                                  "use \"Copy set\" or drag-and-drop.");
    }

    // Эмуляция Ctrl+V. Токен восстановления хранится в настройках, чтобы диалог был один раз.
    auto rd = std::make_unique<PortalRemoteDesktop>();
    if (rd->isAvailable()) {
        QSettings settings;
        rd->setRestoreToken(settings.value(QStringLiteral("platform/remoteDesktopRestoreToken")).toString());
        QObject::connect(rd.get(), &PortalRemoteDesktop::restoreTokenChanged, [](const QString &token) {
            QSettings s;
            s.setValue(QStringLiteral("platform/remoteDesktopRestoreToken"), token);
        });
        p.injector = std::move(rd);
    } else {
        p.notes << QStringLiteral("No RemoteDesktop portal: sequential paste is unavailable.");
    }
}
#endif

Platform createPlatform(const QString &appId)
{
    Platform p;
    const QString qpa = QGuiApplication::platformName();
    const bool waylandSession = qEnvironmentVariable("XDG_SESSION_TYPE") == u"wayland"
                             || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    p.nativeWayland = qpa.startsWith(QStringLiteral("wayland"));
    p.runningUnderXWayland = waylandSession && !p.nativeWayland && qpa == u"xcb";

#ifdef Q_OS_LINUX
    if (p.nativeWayland) {
        createLinuxWayland(p, appId);
    } else {
        // X11 — по остаточному принципу (решение 0001). Пока — переносимые реализации Qt.
        p.environment = QStringLiteral("X11");
        p.capture = std::make_unique<QtScreenCapture>();
        p.clipboard = std::make_unique<QtClipboard>(true);   // в X11 владелец буфера может быть в фоне
        p.notes << QStringLiteral("X11: global hotkeys and Ctrl+V emulation are not implemented yet.");
        if (p.runningUnderXWayland)
            p.notes << QStringLiteral("Running under XWayland: global hotkeys and capture will not work. "
                                      "Do not force QT_QPA_PLATFORM=xcb.");
    }
#else
    Q_UNUSED(appId);
    p.environment = qpa;
    p.capture = std::make_unique<QtScreenCapture>();
    p.clipboard = std::make_unique<QtClipboard>(true);
    p.notes << QStringLiteral("This platform has only basic support so far.");
#endif

    qCInfo(lcPlatform) << "environment:" << p.environment
                       << "| capture:" << (p.capture ? p.capture->name() : QStringLiteral("—"))
                       << "| hotkeys:" << (p.hotkeys ? p.hotkeys->name() : QStringLiteral("—"))
                       << "| clipboard:" << (p.clipboard ? p.clipboard->name() : QStringLiteral("—"))
                       << "| input:" << (p.injector ? p.injector->name() : QStringLiteral("—"));
    for (const QString &n : std::as_const(p.notes))
        qCInfo(lcPlatform) << "note:" << n;
    return p;
}

} // namespace mp
