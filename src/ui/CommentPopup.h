#pragma once

#include <QFrame>
#include <QUuid>

class QLabel;
class QPlainTextEdit;

namespace mp {

// Comment field next to a mark (EDT-1, EDT-5). A child widget of the canvas, not a separate
// window: on Wayland a program cannot place a window next to a mark itself.
// Enter saves, Shift+Enter inserts a line break, Esc closes without changes.
class CommentPopup : public QFrame
{
    Q_OBJECT
public:
    explicit CommentPopup(QWidget *parent);

    void open(const QUuid &markId, const QString &label, const QString &text, const QRect &anchor);
    QUuid markId() const { return m_markId; }
    void commit();
    void dismiss();

signals:
    void committed(const QUuid &markId, const QString &text);
    void finishRequested();   // Ctrl+Enter in the field: save and close the editor
    // Enter in the field: the comment is done. The canvas drops the selection, so the next choice of shape and color
    // applies to a new mark, not to this one. Saving by clicking outside the field does not do that:
    // clicking a color, the person wants to recolor exactly this mark.
    void submitted();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *) override;

private:
    void updateCounter();
    void applyColors();

    QUuid m_markId;
    QLabel *m_label;
    QLabel *m_counter;
    QLabel *m_keys;
    QPlainTextEdit *m_edit;
};

} // namespace mp
