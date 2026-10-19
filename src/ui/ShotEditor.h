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

    void showShot(const QUuid &shotId);
    QUuid shotId() const { return m_canvas->shotId(); }
    void setStyle(const RenderStyle &style) { m_canvas->setStyle(style); }

signals:
    void newShotRequested();
    void doneRequested();

protected:
    void closeEvent(QCloseEvent *) override;

private:
    void updateTitle();
    void updateTools();
    void updateComments();

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
};

} // namespace mp
