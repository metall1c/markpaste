#pragma once

#include "ShotCanvas.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QToolButton;
class QPushButton;

namespace mp {

class PenPicker;

// Окно редактирования снимка (ТЗ разд. 4, шаги 3–4). Один снимок за раз;
// переключение между снимками — из ленты.
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

private:
    void updateTitle();
    void updateTools();
    void updateComments();
    void cancelShot();
    void updatePenDefaults(MarkShape shape);

    CaptureSetModel *m_model;
    ShotCanvas *m_canvas;
    QToolButton *m_markTool;
    QToolButton *m_arrowTool;
    QToolButton *m_penTool;
    QToolButton *m_pointTool;
    QToolButton *m_blurTool;
    QToolButton *m_cropTool;
    QPushButton *m_applyCrop;
    PenPicker *m_penPicker;
    QToolButton *m_undo;
    QToolButton *m_redo;
    QLabel *m_hint;
    QListWidget *m_comments;
    bool m_fresh = false;
};

} // namespace mp
