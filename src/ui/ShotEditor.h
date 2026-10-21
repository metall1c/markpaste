#pragma once

#include "ShotCanvas.h"

#include <QWidget>

class QAction;
class QLabel;
class QListWidget;

namespace mp {

class ActionButton;
class FlatButton;
class KeyHint;
class PenPicker;

// Shot editing window (spec section 4, steps 3–4). One shot at a time;
// switching between shots is done from the strip. Styling: docs/decisions/0019-ui-style.md.
class ShotEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ShotEditor(CaptureSetModel *model, QWidget *parent = nullptr);

    // fresh: the shot was just taken, Esc discards it (removes it from the set).
    // A shot opened from the strip for editing is only closed by Esc.
    void showShot(const QUuid &shotId, bool fresh = false);
    bool isFreshShot() const { return m_fresh; }
    QUuid shotId() const { return m_canvas->shotId(); }
    // Save the comment being typed. Needed before a paste: the window hides, and saving
    // on focus loss comes later than the set goes into the paste.
    void commitPendingEdits() { m_canvas->commitComment(); }
    void setStyle(const RenderStyle &style);
    // Pen for new marks (kept in settings between runs)
    void setPen(const MarkPen &pen);

signals:
    void newShotRequested();
    void doneRequested();
    void penChanged(const mp::MarkPen &pen);

protected:
    void closeEvent(QCloseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void changeEvent(QEvent *) override;

private:
    void updateTitle();
    void updateTools();
    void updateComments();
    void updateCommentSelection();
    void applyColors();
    void cancelShot();
    void updatePenDefaults(MarkShape shape);
    void setFrame(FrameShape frame);
    void updateFrameUi();

    CaptureSetModel *m_model;
    ShotCanvas *m_canvas;
    FlatButton *m_markTool;
    FlatButton *m_frameButton;              // frame shape: rectangle, rounded, ellipse
    QList<std::pair<FrameShape, QAction *>> m_frameActions;
    FlatButton *m_arrowTool;
    FlatButton *m_penTool;
    FlatButton *m_pointTool;
    FlatButton *m_blurTool;
    FlatButton *m_cropTool;
    ActionButton *m_applyCrop;
    PenPicker *m_penPicker;
    FlatButton *m_undo;
    FlatButton *m_redo;
    QLabel *m_hint;
    KeyHint *m_escHint;
    QLabel *m_commentsTitle;
    QLabel *m_commentsCount;
    QLabel *m_commentsFooter;
    QListWidget *m_comments;
    bool m_fresh = false;
    bool m_applyingColors = false;
};

} // namespace mp
