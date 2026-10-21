#pragma once

class QPoint;
class QWindow;

namespace mp::lx {

// The compositor's window menu (xdg_toplevel.show_window_menu) for a window of Qt's own Wayland connection.
// GNOME has "Always on Top" there: the only way to keep an ordinary window above others on Mutter,
// which has neither layer-shell nor a protocol for it (problems.md 2026-10-21).
//
// The request goes to Qt's xdg_toplevel, not through our WaylandConnection: the surface belongs to Qt's
// connection. The compositor checks the serial of the last input event, so call it while handling
// a mouse press in this window.
bool canShowXdgWindowMenu();
bool showXdgWindowMenu(QWindow *window, const QPoint &posInWindow);

} // namespace mp::lx
