#include "CommentPopup.h"

#include "Theme.h"
#include "core/CaptureSet.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace mp {

CommentPopup::CommentPopup(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);
    auto *top = new QHBoxLayout;
    m_label = new QLabel;
    QFont f = m_label->font();
    f.setBold(true);
    m_label->setFont(f);
    m_counter = new QLabel;
    top->addWidget(m_label);
    top->addStretch();
    top->addWidget(m_counter);
    layout->addLayout(top);

    m_edit = new QPlainTextEdit;
    m_edit->setPlaceholderText(tr("Comment"));
    m_edit->setTabChangesFocus(true);
    m_edit->installEventFilter(this);
    m_edit->setFixedHeight(64);
    layout->addWidget(m_edit);
    connect(m_edit, &QPlainTextEdit::textChanged, this, &CommentPopup::updateCounter);

    m_keys = new QLabel(tr("Enter — save · Shift+Enter — new line"));
    const QFont small = smallerFont(m_keys->font());
    m_keys->setFont(small);
    m_counter->setFont(small);
    layout->addWidget(m_keys);

    setFixedWidth(300);
    hide();
}

void CommentPopup::open(const QUuid &markId, const QString &label, const QString &text, const QRect &anchor)
{
    m_markId = markId;
    m_label->setText(label);
    applyColors();
    m_edit->setPlainText(text);
    m_edit->moveCursor(QTextCursor::End);
    adjustSize();

    // Below the mark; if it does not fit, above it; always inside the canvas.
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

void CommentPopup::applyColors()
{
    // Colors on every opening: the accent may have changed in settings, the theme in the system
    const UiColors c = UiColors::from(parentWidget()->palette());
    for (QLabel *l : {m_counter, m_keys}) {
        QPalette pal = l->palette();
        pal.setColor(QPalette::WindowText, c.muted);
        l->setPalette(pal);
    }
    m_edit->setStyleSheet(QStringLiteral("QPlainTextEdit { border: 1px solid %1; border-radius: 6px; padding: 2px 3px;"
                                         " background: palette(base); }")
                              .arg(c.accent.name()));
}

void CommentPopup::paintEvent(QPaintEvent *)
{
    const UiColors c = UiColors::from(palette());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c.border, 1));
    p.setBrush(c.raised);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}

void CommentPopup::commit()
{
    if (!isVisible())
        return;
    const QUuid id = m_markId;
    const QString text = m_edit->toPlainText().left(kMaxCommentLength);
    hide();
    m_markId = QUuid();
    emit committed(id, text);
}

void CommentPopup::dismiss()
{
    hide();
    m_markId = QUuid();
}

void CommentPopup::updateCounter()
{
    const int n = int(m_edit->toPlainText().size());
    if (n > kMaxCommentLength) {
        // Truncate right away so that the person sees the limit (EDT-5)
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
            // Ctrl+Enter is "Done" for the whole window: save and pass the event to the editor window
            if (enter && (ke->modifiers() & Qt::ControlModifier)) {
                commit();
                emit finishRequested();
                return true;
            }
            if (enter && !(ke->modifiers() & Qt::ShiftModifier)) {
                commit();
                emit submitted();
                return true;
            }
            if (ke->key() == Qt::Key_Escape) {
                dismiss();
                parentWidget()->setFocus();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut && isVisible()) {
            // A click outside the field saves too: this way nothing typed is lost.
            commit();
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace mp
