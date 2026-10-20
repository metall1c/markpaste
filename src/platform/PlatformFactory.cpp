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
        p.notes << QStringLiteral("No GlobalShortcuts portal: bind keys to `markpaste --capture` (and `--capture-to-strip`) in your desktop settings.");

    // Эмуляция Ctrl+V. Токен восстановления хранится в настройках, чтобы диалог был один раз.
    auto rd = std::make_unique<PortalRemoteDesktop>();
    if (rd->isAvailable()) {
        QSettings settings;
        rd->setRestoreToken(settings.value(QStringLiteral("platform/remoteDesktopRestoreToken")).toString());
        QObject::connect(rd.get(), &PortalRemoteDesktop::restoreTokenChanged, [](const QString &token) {
            QSettings s;
            s.setValue(QStringLiteral("platform/remoteDesktopRestoreToken"), token);
        });
    }

    // Буфер из фона: ext-data-control-v1 (KDE, Sway, Hyprland) → портал Clipboard внутри
    // сессии RemoteDesktop (GNOME) → только с фокусом (QClipboard).
    // MARKPASTE_CLIPBOARD=portal — принудительно портал, для проверки на KDE.
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

    // Ctrl+V: virtual-keyboard-v1 (wlroots: Sway, Hyprland — без диалога) → портал RemoteDesktop
    // (KDE, GNOME). Буфер через портал живёт только в сессии RemoteDesktop, тогда — только портал.
    // MARKPASTE_INPUT=portal — принудительно портал.
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
        // X11 — по остаточному принципу (решение 0001). Пока — переносимые реализации Qt.
        p.environment = QStringLiteral("X11");
        p.capture = std::make_unique<QtScreenCapture>();
        p.clipboard = std::make_unique<QtClipboard>(true);   // в X11 владелец буфера может быть в фоне
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
    // Всё нужное Windows даёт напрямую, без разрешений (ТЗ разд. 9)
    p.environment = QStringLiteral("Windows %1").arg(QSysInfo::productVersion());
    // MARKPASTE_CAPTURE=gdi — без DXGI: для драйверов, где она сбоит (и Wine без экрана)
    const bool dxgi = qEnvironmentVariable("MARKPASTE_CAPTURE") != u"gdi";
    // Без FallbackCapture: GDI для монитора, который не отдала DXGI, DxgiCapture берёт сам, при каждом
    // снимке. FallbackCapture после одного неудачного снимка (ни DXGI, ни GDI не отдали ни одного
    // монитора) навсегда переходил бы на QScreen::grabWindow — тот же GDI, но уже без DXGI и HDR
    // до перезапуска программы.
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
