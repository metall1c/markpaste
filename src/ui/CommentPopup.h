#pragma once

#include <QFrame>
#include <QUuid>

class QLabel;
class QPlainTextEdit;

namespace mp {

// Поле комментария рядом с меткой (EDT-1, EDT-5). Дочерний виджет холста, а не отдельное
// окно: на Wayland программа не может сама поставить окно рядом с меткой.
// Enter — сохранить, Shift+Enter — перенос строки, Esc — закрыть без изменений.
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
    void finishRequested();   // Ctrl+Enter в поле: сохранить и закрыть редактор

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateCounter();

    QUuid m_markId;
    QLabel *m_label;
    QLabel *m_counter;
    QPlainTextEdit *m_edit;
};

} // namespace mp
