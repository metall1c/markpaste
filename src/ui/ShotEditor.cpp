#include "ShotEditor.h"

#include "PenPicker.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

namespace mp {

namespace {
QToolButton *toolButton(const QString &text, const QString &tip, bool checkable)
{
    auto *b = new QToolButton;
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(checkable);
    b->setAutoRaise(true);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
} // namespace

ShotEditor::ShotEditor(CaptureSetModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_canvas = new ShotCanvas(model);

    m_markTool = toolButton(tr("▭ Mark"), tr("Mark an area and comment it (R)"), true);
    m_arrowTool = toolButton(tr("➚ Arrow"), tr("Point with an arrow and comment it (A)"), true);
    m_penTool = toolButton(tr("✎ Pen"), tr("Draw freehand — circle or underline — and comment it (P)"), true);
    m_pointTool = toolButton(tr("● Point"), tr("Put a numbered point on the spot and comment it (D)"), true);
    m_blurTool = toolButton(tr("▦ Blur"), tr("Pixelate passwords and personal data (B)"), true);
    m_cropTool = toolButton(tr("⛶ Crop"), tr("Crop the shot (C)"), true);
    auto *group = new QButtonGroup(this);
    group->addButton(m_markTool);
    group->addButton(m_arrowTool);
    group->addButton(m_penTool);
    group->addButton(m_pointTool);
    group->addButton(m_blurTool);
    group->addButton(m_cropTool);
    m_markTool->setChecked(true);
    m_applyCrop = new QPushButton(tr("Apply crop"));
    m_applyCrop->setVisible(false);
    m_penPicker = new PenPicker;

    m_undo = toolButton(tr("↶"), tr("Undo (Ctrl+Z)"), false);
    m_redo = toolButton(tr("↷"), tr("Redo (Ctrl+Shift+Z)"), false);
    auto *deleteShot = toolButton(tr("Delete shot"), tr("Delete this shot from the set"), false);
    auto *newShot = new QPushButton(tr("New shot"));
    auto *done = new QPushButton(tr("Done"));
    done->setToolTip(tr("Close the shot (Ctrl+Enter)"));
    done->setDefault(true);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(8, 6, 8, 6);
    bar->addWidget(m_markTool);
    bar->addWidget(m_arrowTool);
    bar->addWidget(m_penTool);
    bar->addWidget(m_pointTool);
    bar->addWidget(m_blurTool);
    bar->addWidget(m_cropTool);
    bar->addWidget(m_applyCrop);
    bar->addSpacing(12);
    bar->addWidget(m_penPicker);
    bar->addSpacing(12);
    bar->addWidget(m_undo);
    bar->addWidget(m_redo);
    bar->addStretch();
    bar->addWidget(deleteShot);
    bar->addWidget(newShot);
    bar->addWidget(done);

    m_hint = new QLabel;
    m_hint->setForegroundRole(QPalette::PlaceholderText);
    m_hint->setContentsMargins(10, 2, 10, 6);

    // Список комментариев снимка: весь текст, который уйдёт в чат, виден сразу
    m_comments = new QListWidget;
    m_comments->setFixedWidth(240);
    m_comments->setWordWrap(true);
    m_comments->setFocusPolicy(Qt::NoFocus);
    m_comments->setToolTip(tr("Click a comment to edit it"));
    connect(m_comments, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        m_canvas->editMark(item->data(Qt::UserRole).toUuid());
    });

    auto *middle = new QHBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(m_canvas, 1);
    middle->addWidget(m_comments);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(bar);
    layout->addLayout(middle, 1);
    layout->addWidget(m_hint);

    connect(m_markTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Mark); });
    connect(m_arrowTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Arrow); });
    connect(m_penTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Pen); });
    connect(m_pointTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Point); });
    connect(m_blurTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Blur); });
    connect(m_cropTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Crop); });
    connect(m_applyCrop, &QPushButton::clicked, m_canvas, &ShotCanvas::applyCrop);
    connect(m_canvas, &ShotCanvas::toolChanged, this, &ShotEditor::updateTools);
    connect(m_canvas, &ShotCanvas::cropPendingChanged, this, &ShotEditor::updateTools);

    // Перо: выбор в панели — для новых меток и для выделенной; выделение метки
    // показывает в панели её перо.
    connect(m_penPicker, &PenPicker::penChanged, this, [this](const MarkPen &pen) {
        m_canvas->setPen(pen);
        emit penChanged(pen);
    });
    connect(m_canvas, &ShotCanvas::markSelected, this, [this](const MarkPen &pen, MarkShape shape) {
        updatePenDefaults(shape);
        m_penPicker->setPen(pen);
    });
    connect(m_canvas, &ShotCanvas::toolChanged, this, [this](ShotCanvas::Tool tool) {
        updatePenDefaults(tool == ShotCanvas::Tool::Arrow ? MarkShape::Arrow
                          : tool == ShotCanvas::Tool::Pen ? MarkShape::Freehand
                          : tool == ShotCanvas::Tool::Point ? MarkShape::Point
                                                          : MarkShape::Rect);
    });

    QUndoStack *undo = model->undoStack();
    connect(m_undo, &QToolButton::clicked, undo, &QUndoStack::undo);
    connect(m_redo, &QToolButton::clicked, undo, &QUndoStack::redo);
    connect(undo, &QUndoStack::canUndoChanged, m_undo, &QToolButton::setEnabled);
    connect(undo, &QUndoStack::canRedoChanged, m_redo, &QToolButton::setEnabled);
    m_undo->setEnabled(undo->canUndo());
    m_redo->setEnabled(undo->canRedo());

    // Отмена без ограничения глубины, в пределах набора (EDT-8)
    auto *undoKey = new QShortcut(QKeySequence::Undo, this);
    connect(undoKey, &QShortcut::activated, this, [this, undo] {
        m_canvas->commitComment();
        undo->undo();
    });
    for (const QKeySequence &seq : {QKeySequence(QKeySequence::Redo), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)}) {
        auto *redoKey = new QShortcut(seq, this);
        connect(redoKey, &QShortcut::activated, undo, &QUndoStack::redo);
    }
    // Однобуквенные клавиши инструментов не должны срабатывать в поле комментария.
    // M — прежняя клавиша рамки, оставлена рядом с R.
    using Tool = ShotCanvas::Tool;
    for (auto [key, tool] : {std::pair{Qt::Key_R, Tool::Mark}, std::pair{Qt::Key_M, Tool::Mark},
                             std::pair{Qt::Key_A, Tool::Arrow}, std::pair{Qt::Key_P, Tool::Pen}, std::pair{Qt::Key_D, Tool::Point},
                             std::pair{Qt::Key_B, Tool::Blur}, std::pair{Qt::Key_C, Tool::Crop}}) {
        auto *sc = new QShortcut(QKeySequence(key), m_canvas, nullptr, nullptr, Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, [this, tool] { m_canvas->setTool(tool); });
    }

    connect(deleteShot, &QToolButton::clicked, this, [this] {
        m_model->removeShot(m_canvas->shotId());
    });
    connect(newShot, &QPushButton::clicked, this, &ShotEditor::newShotRequested);
    auto finish = [this] {
        m_canvas->commitComment();
        m_fresh = false;   // снимок принят
        updateTools();
        emit doneRequested();
    };
    connect(done, &QPushButton::clicked, this, finish);
    // Ctrl+Enter — как «Готово». В поле комментария его ловит CommentPopup
    // (иначе Enter там сохранил бы только комментарий).
    connect(m_canvas, &ShotCanvas::finishRequested, this, finish);
    for (const QKeySequence &seq : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto *sc = new QShortcut(seq, this);
        connect(sc, &QShortcut::activated, this, finish);
    }

    connect(m_model, &CaptureSetModel::changed, this, [this] {
        if (!m_model->shot(m_canvas->shotId())) {
            // Снимок удалён: показываем последний оставшийся или закрываемся.
            // Оставшийся уже принят — Esc не должен его удалять как только что снятый.
            m_fresh = false;
            if (m_model->set().isEmpty())
                hide();
            else
                m_canvas->setShot(m_model->set().shots.last().id);
            updateTools();
        }
        updateTitle();
        updateComments();
    });
    updateTools();
}

void ShotEditor::updateComments()
{
    m_comments->clear();
    const int si = m_model->set().shotIndex(m_canvas->shotId());
    const Shot *shot = m_model->shot(m_canvas->shotId());
    if (!shot || si < 0)
        return;
    if (shot->marks.isEmpty()) {
        auto *empty = new QListWidgetItem(tr("No marks yet.\nDrag over the shot to add one."));
        empty->setFlags(Qt::NoItemFlags);
        m_comments->addItem(empty);
        return;
    }
    for (int i = 0; i < shot->marks.size(); ++i) {
        const Mark &m = shot->marks[i];
        const QString text = m.comment.trimmed().isEmpty() ? tr("(no comment)") : m.comment.trimmed();
        auto *item = new QListWidgetItem(markLabel(si, i) + QStringLiteral("  ") + text);
        item->setData(Qt::UserRole, m.id);
        if (m.comment.trimmed().isEmpty())
            item->setForeground(palette().placeholderText());
        m_comments->addItem(item);
    }
}

void ShotEditor::setStyle(const RenderStyle &style)
{
    m_canvas->setStyle(style);
    updatePenDefaults(MarkShape::Rect);
}

void ShotEditor::setPen(const MarkPen &pen)
{
    m_penPicker->setPen(pen);
    m_canvas->setPen(pen);
}

void ShotEditor::updatePenDefaults(MarkShape shape)
{
    m_penPicker->setDefaults(m_canvas->style().accent, defaultMarkWidth(shape, m_canvas->style()));
}

void ShotEditor::showShot(const QUuid &shotId, bool fresh)
{
    m_fresh = fresh;
    m_canvas->setShot(shotId);
    m_canvas->setTool(ShotCanvas::Tool::Mark);   // каждый снимок начинается с рамки
    updateTitle();
    updateComments();
    updateTools();   // подсказка про Esc зависит от того, свежий ли снимок

    // Размер — по снимку, но не больше 85 % экрана
    const QSize hint = m_canvas->sizeHint() + QSize(240, 90);
    QScreen *screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
    const QSize avail = screen->availableGeometry().size() * 0.85;
    resize(hint.boundedTo(avail).expandedTo(QSize(560, 360)));
    show();
    raise();
    activateWindow();
    m_canvas->setFocus();
}

void ShotEditor::updateTitle()
{
    const int i = m_model->set().shotIndex(m_canvas->shotId());
    setWindowTitle(i < 0 ? QStringLiteral("Markpaste")
                         : tr("Shot %1 — Markpaste").arg(shotLetter(i)));
}

void ShotEditor::updateTools()
{
    const auto tool = m_canvas->tool();
    m_markTool->setChecked(tool == ShotCanvas::Tool::Mark);
    m_arrowTool->setChecked(tool == ShotCanvas::Tool::Arrow);
    m_penTool->setChecked(tool == ShotCanvas::Tool::Pen);
    m_pointTool->setChecked(tool == ShotCanvas::Tool::Point);
    m_blurTool->setChecked(tool == ShotCanvas::Tool::Blur);
    m_cropTool->setChecked(tool == ShotCanvas::Tool::Crop);
    m_applyCrop->setVisible(tool == ShotCanvas::Tool::Crop && m_canvas->hasPendingCrop());
    switch (tool) {
    case ShotCanvas::Tool::Mark:
        m_hint->setText(m_fresh ? tr("Drag to mark · Double-click — edit comment · Ctrl+Z — undo · Ctrl+Enter — done · Esc — discard the shot")
                        : tr("Drag to mark · Double-click — edit comment · Delete — remove · Ctrl+Z — undo · Ctrl+Enter or Esc — close"));
        break;
    case ShotCanvas::Tool::Arrow:
        m_hint->setText(tr("Drag from the tail to the tip · Drag an end to adjust · Double-click — edit comment"));
        break;
    case ShotCanvas::Tool::Pen:
        m_hint->setText(tr("Draw around or under the spot · Double-click a line — edit comment"));
        break;
    case ShotCanvas::Tool::Point:
        m_hint->setText(tr("Click the spot — a numbered point appears · Drag a point to move it · Double-click — edit comment"));
        break;
    case ShotCanvas::Tool::Blur:
        m_hint->setText(tr("Drag over passwords, card numbers, names — the chat gets a pixelated copy"));
        break;
    case ShotCanvas::Tool::Crop:
        m_hint->setText(tr("Drag the area to keep · Enter — apply · Esc — cancel"));
        break;
    }
}

void ShotEditor::keyPressEvent(QKeyEvent *e)
{
    // Сюда Esc доходит, только если внутри окна нечего закрывать: поле комментария,
    // рамку обрезки и выделение холст и поле обрабатывают сами.
    if (e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier) {
        cancelShot();
        return;
    }
    QWidget::keyPressEvent(e);
}

void ShotEditor::cancelShot()
{
    const QUuid id = m_canvas->shotId();
    const Shot *shot = m_model->shot(id);
    if (!m_fresh || !shot) {
        // Снимок из ленты — только закрыть, правки сохранены
        m_canvas->commitComment();
        emit doneRequested();
        return;
    }
    // Свежий снимок: отменить. Если на нём уже есть работа — спросить.
    if (!shot->marks.isEmpty() || !shot->redactions.isEmpty() || !shot->crop.isNull()) {
        const int index = m_model->set().shotIndex(id);
        const auto answer = QMessageBox::question(
            this, tr("Cancel shot"),
            tr("Discard shot %1 with its marks?").arg(shotLetter(index)),
            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Discard)
            return;
    }
    m_fresh = false;
    hide();
    m_model->removeShot(id);
}

void ShotEditor::closeEvent(QCloseEvent *e)
{
    m_canvas->commitComment();
    e->accept();
}

} // namespace mp
