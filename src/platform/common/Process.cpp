#include "platform/Process.h"

#include <QByteArray>

#include <csignal>

namespace mp {

namespace {
bool setPortalTheme = false;
}

void prepareProcess()
{
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif
#if defined(MARKPASTE_STATIC_QT) && defined(Q_OS_LINUX)
    // The static Qt has no plasma-integration or gtk3 theme: Qt picks its built-in "kde" or
    // "gnome" theme, which has no native dialogs, and QFileDialog falls back to Qt's own widget
    // dialog. Qt takes the portal theme (FileChooser) by itself only in flatpak and snap, so ask
    // for it; it keeps the desktop theme as its base for palette, fonts and icons (ADR 0028).
    // A theme chosen by the user (qt6ct and the like) stays.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME")) {
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
        setPortalTheme = true;
    }
#endif
}

void finishProcessSetup()
{
    if (setPortalTheme)
        qunsetenv("QT_QPA_PLATFORMTHEME");
}

} // namespace mp
