#pragma once

#include "ShotCanvas.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QToolButton;
class QPushButton;

namespace mp {

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
    void setStyle(const RenderStyle &style) { m_canvas->setStyle(style); }

signals:
    void newShotRequested();
    void doneRequested();

protected:
    void closeEvent(QCloseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    void updateTitle();
    void updateTools();
    void updateComments();
    void cancelShot();

    CaptureSetModel *m_model;
    ShotCanvas *m_canvas;
    QToolButton *m_markTool;
    QToolButton *m_blurTool;
    QToolButton *m_cropTool;
    QPushButton *m_applyCrop;
    QToolButton *m_undo;
    QToolButton *m_redo;
    QLabel *m_hint;
    QListWidget *m_comments;
    bool m_fresh = false;
};

} // namespace mp
