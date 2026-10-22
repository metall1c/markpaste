#pragma once

#include "core/CaptureSetModel.h"
#include "platform/WindowRoles.h"
#include "render/ShotRenderer.h"

#include <QHash>
#include <QPixmap>
#include <QWidget>

#include <functional>

class QAction;
class QLabel;
class QMimeData;
class QVBoxLayout;
class QScrollArea;

namespace mp {

class ActionButton;
class CountPill;
class EmptyHint;
class FlatButton;
class MessageLabel;
class ShotThumb;

// The set strip (spec 5.3): thumbnails with a letter and the number of comments, buttons and a counter.
// On KDE/Sway it is a layer-shell panel at the screen edge that does not take keyboard focus:
// clicking "Paste" does not move focus away from the chat window. On Windows it is a frameless window without
// activation that we place at the edge ourselves. Otherwise an ordinary window.
class StripWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StripWindow(CaptureSetModel *model, QWidget *parent = nullptr);

    void setStyle(const RenderStyle &style);
    void setCurrentShot(const QUuid &shotId);
    // The primary button pastes the set (mode 1) or, where that is unavailable, copies it.
    // hotkey is the paste shortcut for the button label ("Alt+V"); empty means no label
    void setPasteMode(bool sequential, const QString &hotkey);
    // The capture shortcut for the hint in an empty set
    void setCaptureHotkey(const QString &hotkey);
    void showMessage(const QString &text, int timeoutMs = 4000);
    void setProgress(int done, int total);   // total = 0 means the paste is over

    // For dragging outside, the controller prepares the files (baking + temporary files)
    using DragProvider = std::function<QMimeData *(const QList<QUuid> &shots)>;
    void setDragProvider(DragProvider provider) { m_dragProvider = std::move(provider); }

    // Show at the screen edge (layer-shell if available) in the saved position
    void present();
    // Move the strip (mouse move and the debug-move-strip debug command)
    void moveTo(QScreen *screen, const PanelPlacement &placement);

    // Before a capture the strip must not get into the shot. Where the compositor places windows itself
    // (Wayland without layer-shell, GNOME) a hidden window comes back where the compositor wants (in the middle),
    // and loses "Always on Top": there the strip stays shown but fully transparent.
    void hideForCapture();
    void showAfterCapture();
    // "Hide to tray". Where the compositor places windows itself (GNOME) a hidden window comes back in the
    // middle of the screen, so there the strip is minimized instead and present() restores it in place
    // through xdg-activation (problems.md 2026-10-22).
    void hideToTray();
    // Visible and not minimized to the tray
    bool isShownOnScreen() const { return isVisible() && !m_minimized; }
    bool isMinimizedToTray() const { return m_minimized; }

signals:
    void shotActivated(const QUuid &shotId);
    void newShotRequested();
    void pasteRequested();
    void copyRequested();
    void clearRequested();
    void cancelPasteRequested();
    void settingsRequested();
    void previewRequested();        // PST-6
    void saveToFolderRequested();   // APP-6
    void aboutRequested();
    void hideRequested();   // "hide to tray"

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class ShotThumb;
    friend class StripHeader;
    // Dragging by the header (STR-5): on layer-shell the strip moves on release
    // true: the window manager moves the window (no layer-shell) and the drag is no longer ours
    bool beginMove(const QPoint &posInStrip);
    void updateMove(const QPoint &posInStrip);   // outline of the future position
    void finishMove(const QPoint &posInStrip);
    // Where the strip will go if the mouse is released at posInStrip
    void moveTarget(const QPoint &posInStrip, QScreen **screen, PanelPlacement *placement) const;
    QScreen *currentScreen() const;
    // Top-left corner of the strip at the given position, in global logical coordinates.
    // Qt does not know the position of a layer-shell surface, so we compute it from the edge and margins.
    QPoint topLeftFor(QScreen *screen, const PanelPlacement &placement) const;
    QPoint assumedTopLeft() const { return topLeftFor(currentScreen(), m_placement); }
    void unveil();              // undo hideForCapture()
    bool compositorPlaces() const;   // Wayland without layer-shell: the compositor places our windows
    void snapToEdge();          // move to m_placement
    QRect placementArea(QScreen *screen) const;   // what the edge and offset are measured from
    void onMovedByUser();       // the user released the dragged strip
    void loadPlacement();
    void savePlacement() const;
    void rebuild();
    QPixmap thumbnailFor(int index);
    void startDrag(const QList<QUuid> &shots, QWidget *source);

public:
    // Reordering by dragging within the strip (STR-3). Public for tests.
    static constexpr const char *kShotMime = "application/x-markpaste-shot";
    static QByteArray shotDragData(const QUuid &shotId);
    static QUuid draggedShot(const QMimeData *data);
    int dropIndexAt(int yInList) const;      // insert position 0…N by the coordinate in the list
    int dropLineY(int index) const;
    void moveShotTo(const QUuid &shotId, int insertIndex);

private:
    friend class ShotListHost;

    CaptureSetModel *m_model;
    RenderStyle m_style;
    QUuid m_current;
    CountPill *m_count;
    EmptyHint *m_empty;
    MessageLabel *m_message;
    QVBoxLayout *m_list;
    QScrollArea *m_scroll;
    ActionButton *m_paste;
    ActionButton *m_copy;
    ActionButton *m_newShot;
    FlatButton *m_clear;
    QAction *m_previewAction;
    QAction *m_saveAction;
    ActionButton *m_cancel;
    QWidget *m_dragHandle;
    bool m_pasteCopies = false;
    DragProvider m_dragProvider;
    bool m_roleApplied = false;
    bool m_layerShell = false;
    bool m_snapToEdge = false;   // a frameless window we place at the edge ourselves (Windows)
    PanelPlacement m_placement;
    QString m_screenName;
    QPoint m_movePress;
    bool m_moving = false;
    bool m_minimized = false;   // minimized by hideToTray(); the user may restore it from the dock
    bool m_veiled = false;   // transparent for a capture, see hideForCapture(); present() undoes it
    class MoveGhost *m_ghost = nullptr;

    struct CachedThumb {
        Shot shot;
        int index = -1;
        QPixmap pixmap;
    };
    QHash<QUuid, CachedThumb> m_thumbs;
};

} // namespace mp
