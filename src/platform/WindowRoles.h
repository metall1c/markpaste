#pragma once

#include <functional>

class QWidget;
class QScreen;
class QPoint;

namespace mp {

// Special window roles that are done differently on different systems (spec section 9).
// Call BEFORE the first show(). They return true if the role was assigned by the
// compositor (layer-shell); false means the window stays ordinary, and the caller
// makes it full screen or "on top" itself.

enum class PanelEdge { Left, Right };

// Panel position: at the left or right edge, at offset from the top of the screen
// (logical px; -1 means centered), with a margin from the edge.
struct PanelPlacement {
    PanelEdge edge = PanelEdge::Right;
    int offset = -1;
    int margin = 12;
};

bool layerShellAvailable();

// Capture overlay: above everything, over the whole given screen, takes the keyboard.
bool applyOverlayRole(QWidget *widget, QScreen *screen);

// A transparent full-screen layer above everything that accepts neither mouse nor keyboard.
// For hints like the outline of the strip being moved. false: no layer-shell.
bool applyGhostRole(QWidget *widget, QScreen *screen);

// Strip: a panel at the screen edge above windows. It does NOT take the keyboard, so clicking it
// does not move focus away from the chat window, and the "Paste" button can work in mode 1.
bool applyPanelRole(QWidget *widget, QScreen *screen, const PanelPlacement &placement);

// Move a panel that is already shown (STR-5). The user cannot drag a layer-shell surface
// with the mouse; the program sets the position. Changing the monitor recreates the surface.
// false: the panel is not layer-shell, the window manager moves it.
bool movePanel(QWidget *widget, QScreen *screen, const PanelPlacement &placement);

// A strip without layer-shell that the program itself puts at the screen edge (Windows): a frameless window
// the user drags by the strip header; when the drag ends, onMoveFinished is called
// once, not on every move. false: this is not possible; the window manager moves the window
// and the program does not touch its position.
bool watchPanelMoves(QWidget *widget, std::function<void()> onMoveFinished);

// The system window menu of an ordinary window (GNOME: "Always on Top", which a program cannot set itself
// on Wayland). Call from a mouse press handler of that window: the compositor checks the input event.
// false: not available (layer-shell, Windows, X11, Qt 6.4).
bool canShowWindowMenu(QWidget *widget);
bool showWindowMenu(QWidget *widget, const QPoint &posInWidget);

// Program windows appear and disappear without system animations. Before a capture the program hides
// its windows: on Windows a window fades for ~200 ms, and the half-transparent editor got into the shot.
// Call once after creating QApplication. Where there are no animations, does nothing.
void disableWindowAnimations();

// Give the window the keyboard right after it is shown. On Windows the system does not let a program become
// active if the last input event was not received by it, and our hook swallows Print Screen,
// so the event never reaches the program. Then the window only flashes in the taskbar, and Esc and Enter
// go to the previous window.
void forceActivate(QWidget *widget);

} // namespace mp
