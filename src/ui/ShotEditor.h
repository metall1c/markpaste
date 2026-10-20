#pragma once

#include "ShotCanvas.h"

#include <QWidget>

class QLabel;
class QListWidget;

namespace mp {

class ActionButton;
class FlatButton;
class KeyHint;
class PenPicker;

// Окно редактирования снимка (ТЗ разд. 4, шаги 3–4). Один снимок за раз;
// переключение между снимками — из ленты. Оформление — docs/decisions/0019-ui-style.md.
class ShotEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ShotEditor(CaptureSetModel *model, QWidget *parent = nullptr);

    // fresh — снимок только что снят: Esc его отменяет (удаляет из набора).
    // Снимок, открытый из ленты для правки, Esc только закрывает.
    void showShot(const QUuid &shotId, bool fresh = false);
    bool isFreshShot() const { return m_fresh; }
    QUuid shotId() const { return m_canvas->shotId(); }
    // Сохранить набираемый комментарий. Нужно перед вставкой: окно прячется, а сохранение
    // по потере фокуса приходит позже, чем набор уходит во вставку.
    void commitPendingEdits() { m_canvas->commitComment(); }
    void setStyle(const RenderStyle &style);
    // Перо новых меток (сохраняется в настройках между запусками)
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

    CaptureSetModel *m_model;
    ShotCanvas *m_canvas;
    FlatButton *m_markTool;
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
