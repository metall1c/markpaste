#include "PlatformFactory.h"

#include "common/FallbackCapture.h"
#include "common/QtClipboard.h"
#include "common/QtScreenCapture.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSettings>
#include <QSysInfo>

#ifdef Q_OS_LINUX
#include "linux/kde/KWinScreenshot.h"
#include "linux/portal/PortalBus.h"
#include "linux/portal/PortalClipboard.h"
#include "linux/portal/PortalGlobalShortcuts.h"
#include "linux/portal/PortalRemoteDesktop.h"
#include "linux/portal/PortalScreenshot.h"
#include "linux/wayland/DataControlClipboard.h"
#include "linux/wayland/VirtualKeyboardInjector.h"
#include "linux/wayland/WaylandConnection.h"
#endif
#ifdef Q_OS_WIN
#include "windows/DxgiCapture.h"
#include "windows/WinClipboard.h"
#include "windows/WinHotkeys.h"
#include "windows/WinInputInjector.h"
#endif
#ifdef MARKPASTE_HAVE_X11
#include "linux/x11/X11Hotkeys.h"
#include "linux/x11/X11InputInjector.h"
#endif

Q_LOGGING_CATEGORY(lcPlatform, "markpaste.platform")

namespace mp {

#ifdef Q_OS_LINUX
static void createLinuxWayland(Platform &p, const QString &appId)
{
    using namespace mp::lx;
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    p.environment = QStringLiteral("Wayland · %1").arg(desktop.isEmpty() ? QStringLiteral("unknown") : desktop);

    // Registering with the portals happens on the very first call (spec section 9).
    PortalBus &bus = PortalBus::instance();
    const Status reg = bus.registerApp(appId);
    if (!reg.ok)
        p.notes << QStringLiteral("Portal app id registration failed: %1. Permissions may be asked every time.").arg(reg.error);

    WaylandConnection *wl = WaylandConnection::instance();
    p.supportsLayerShell = wl && wl->hasGlobal("zwlr_layer_shell_v1");

    // Capture: KWin ScreenShot2 (fast, no dialog) → Screenshot portal.
    std::unique_ptr<ICapture> portalShot;
    if (PortalScreenshot::isAvailable())
        portalShot = std::make_unique<PortalScreenshot>();
    if (KWinScreenshot::isPresent())
        p.capture = std::make_unique<FallbackCapture>(std::make_unique<KWinScreenshot>(), std::move(portalShot));
    else if (portalShot)
        p.capture = std::move(portalShot);
    else
        p.notes << QStringLiteral("No screenshot portal: install xdg-desktop-portal and a backend for your desktop.");

    // Global hotkeys
    if (PortalGlobalShortcuts::isAvailable())
        p.hotkeys = std::make_unique<PortalGlobalShortcuts>();
    else
        p.notes << QStringLiteral("No GlobalShortcuts portal: bind keys to `markpaste --capture` (and `--capture-to-strip`) in your desktop settings.");

    // Ctrl+V emulation. The restore token is kept in settings so that the dialog appears once.
    auto rd = std::make_unique<PortalRemoteDesktop>();
    if (rd->isAvailable()) {
        QSettings settings;
        rd->setRestoreToken(settings.value(QStringLiteral("platform/remoteDesktopRestoreToken")).toString());
        QObject::connect(rd.get(), &PortalRemoteDesktop::restoreTokenChanged, [](const QString &token) {
            QSettings s;
            s.setValue(QStringLiteral("platform/remoteDesktopRestoreToken"), token);
        });
    }

    // Background clipboard: ext-data-control-v1 (KDE, Sway, Hyprland) → Clipboard portal inside
    // a RemoteDesktop session (GNOME) → focus only (QClipboard).
    // MARKPASTE_CLIPBOARD=portal forces the portal, for testing on KDE.
    const bool forcePortal = qEnvironmentVariable("MARKPASTE_CLIPBOARD") == u"portal";
    DataControlClipboard *dc = forcePortal ? nullptr : DataControlClipboard::create();
    if (dc) {
        p.clipboard.reset(dc);
    } else if (rd->isAvailable() && PortalClipboard::isAvailable()) {
        p.clipboard = std::make_unique<PortalClipboard>(rd.get());
        if (!forcePortal)
            p.notes << QStringLiteral("Compositor has no ext-data-control-v1: the clipboard goes through the portal "
                                      "while pasting.");
    } else {
        p.clipboard = std::make_unique<QtClipboard>(false);
        p.notes << QStringLiteral("Compositor has no ext-data-control-v1 and there is no Clipboard portal: "
                                  "sequential paste is unavailable, use \"Copy set\" or drag-and-drop.");
    }

    // Ctrl+V: virtual-keyboard-v1 (wlroots: Sway, Hyprland, no dialog) → RemoteDesktop portal
    // (KDE, GNOME). A portal clipboard lives only in a RemoteDesktop session, so then only the portal.
    // MARKPASTE_INPUT=portal forces the portal.
    const bool clipboardNeedsPortal = p.clipboard->needsInputSession();
    VirtualKeyboardInjector *vk = (clipboardNeedsPortal || qEnvironmentVariable("MARKPASTE_INPUT") == u"portal")
        ? nullptr
        : VirtualKeyboardInjector::create();
    if (vk)
        p.injector.reset(vk);
    else if (rd->isAvailable())
        p.injector = std::move(rd);
    else
        p.notes << QStringLiteral("No RemoteDesktop portal and no virtual-keyboard-v1: sequential paste is unavailable.");
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
        // X11 is lowest priority (decision 0001). For now, the portable Qt implementations.
        p.environment = QStringLiteral("X11");
        p.capture = std::make_unique<QtScreenCapture>();
        p.clipboard = std::make_unique<QtClipboard>(true);   // on X11 the clipboard owner may be in the background
#ifdef MARKPASTE_HAVE_X11
        if (!p.runningUnderXWayland) {
            p.hotkeys.reset(lx::X11Hotkeys::create());
            p.injector.reset(lx::X11InputInjector::create());
        }
#endif
        if (!p.hotkeys)
            p.notes << QStringLiteral("X11: global hotkeys are not available in this build.");
        if (!p.injector)
            p.notes << QStringLiteral("X11: Ctrl+V emulation (XTEST) is not available.");
        if (p.runningUnderXWayland)
            p.notes << QStringLiteral("Running under XWayland: global hotkeys and capture will not work. "
                                      "Do not force QT_QPA_PLATFORM=xcb.");
    }
#elif defined(Q_OS_WIN)
    Q_UNUSED(appId);
    // Windows gives everything needed directly, without permissions (spec section 9)
    p.environment = QStringLiteral("Windows %1").arg(QSysInfo::productVersion());
    // MARKPASTE_CAPTURE=gdi: no DXGI, for drivers where it misbehaves (and Wine without a screen)
    const bool dxgi = qEnvironmentVariable("MARKPASTE_CAPTURE") != u"gdi";
    // No FallbackCapture: DxgiCapture itself takes GDI for a monitor DXGI did not provide, on every
    // shot. After one failed shot (neither DXGI nor GDI gave a single monitor) FallbackCapture
    // would switch to QScreen::grabWindow for good, which is the same GDI but without DXGI and HDR
    // until the program restarts.
    p.capture = std::make_unique<win::DxgiCapture>(dxgi);
    p.hotkeys = std::make_unique<win::WinHotkeys>();
    p.clipboard = std::make_unique<win::WinClipboard>();
    p.injector = std::make_unique<win::WinInputInjector>();
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
