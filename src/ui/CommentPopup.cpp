#include "CommentPopup.h"

#include "core/CaptureSet.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace ps {

CommentPopup::CommentPopup(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setObjectName(QStringLiteral("CommentPopup"));
    setStyleSheet(QStringLiteral("#CommentPopup { border: 1px solid palette(mid); border-radius: 6px; background: palette(base); }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);
    auto *top = new QHBoxLayout;
    m_label = new QLabel;
    QFont f = m_label->font();
    f.setBold(true);
    m_label->setFont(f);
    m_counter = new QLabel;
    m_counter->setForegroundRole(QPalette::PlaceholderText);
    top->addWidget(m_label);
    top->addStretch();
    top->addWidget(m_counter);
    layout->addLayout(top);

    m_edit = new QPlainTextEdit;
    m_edit->setPlaceholderText(tr("Comment (Enter — save, Shift+Enter — new line)"));
    m_edit->setTabChangesFocus(true);
    m_edit->installEventFilter(this);
    m_edit->setFixedHeight(64);
    layout->addWidget(m_edit);
    connect(m_edit, &QPlainTextEdit::textChanged, this, &CommentPopup::updateCounter);

    setFixedWidth(300);
    hide();
}

void CommentPopup::open(const QUuid &markId, const QString &label, const QString &text, const QRect &anchor)
{
    m_markId = markId;
    m_label->setText(label);
    m_edit->setPlainText(text);
    m_edit->moveCursor(QTextCursor::End);
    adjustSize();

    // Под меткой; не влезает — над ней; всегда внутри холста.
    const QWidget *p = parentWidget();
    QPoint pos(anchor.left(), anchor.bottom() + 8);
    if (pos.y() + height() > p->height())
        pos.setY(anchor.top() - height() - 8);
    pos.setX(std::clamp(pos.x(), 4, std::max(4, p->width() - width() - 4)));
    pos.setY(std::clamp(pos.y(), 4, std::max(4, p->height() - height() - 4)));
    move(pos);
    show();
    raise();
    m_edit->setFocus();
    updateCounter();
}

void CommentPopup::commit()
{
    if (!isVisible())
        return;
    const QUuid id = m_markId;
    const QString text = m_edit->toPlainText().left(kMaxCommentLength);
    hide();
    m_markId = {};
    emit committed(id, text);
}

void CommentPopup::dismiss()
{
    hide();
    m_markId = {};
}

void CommentPopup::updateCounter()
{
    const int n = int(m_edit->toPlainText().size());
    if (n > kMaxCommentLength) {
        // Обрезаем сразу, чтобы человек видел лимит (EDT-5)
        QTextCursor c = m_edit->textCursor();
        m_edit->setPlainText(m_edit->toPlainText().left(kMaxCommentLength));
        c.movePosition(QTextCursor::End);
        m_edit->setTextCursor(c);
        return;
    }
    m_counter->setText(n > kMaxCommentLength - 100 ? QStringLiteral("%1/%2").arg(n).arg(kMaxCommentLength) : QString());
}

bool CommentPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            const bool enter = ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter;
            // Ctrl+Enter — «Готово» для всего окна: сохраняем и отдаём событие окну редактора
            if (enter && (ke->modifiers() & Qt::ControlModifier)) {
                commit();
                emit finishRequested();
                return true;
            }
            if (enter && !(ke->modifiers() & Qt::ShiftModifier)) {
                commit();
                return true;
            }
            if (ke->key() == Qt::Key_Escape) {
                dismiss();
                parentWidget()->setFocus();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut && isVisible()) {
            // Клик мимо поля — тоже сохранить: так не теряется набранное.
            commit();
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace ps
