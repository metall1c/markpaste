#include "ShotEditor.h"

#include "Controls.h"
#include "PenPicker.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QActionGroup>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <cmath>

namespace mp {

namespace {

enum CommentRole { IdRole = Qt::UserRole, LabelRole, BadgeColorRole };

// Comment card: the mark badge as on the image, and the text that goes to the chat.
// A mark without a comment is a dashed card: it does not get into the message text (ADR 0018).
class CommentDelegate : public QStyledItemDelegate
{
    Q_DECLARE_TR_FUNCTIONS(CommentDelegate)
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const int width = viewWidth(opt);
        const QString text = index.data(Qt::DisplayRole).toString();
        if (index.data(IdRole).toUuid().isNull()) {
            const QRectF r = QFontMetricsF(opt.font).boundingRect(QRectF(0, 0, width - 4, 1e6), Qt::TextWordWrap, text);
            return {width, int(std::ceil(r.height())) + 8};
        }
        return {width, int(std::ceil(contentHeight(opt, text, width))) + 2 * kPad + kSpacing};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const UiColors c = UiColors::from(opt.palette);
        const QString text = index.data(Qt::DisplayRole).toString();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (index.data(IdRole).toUuid().isNull()) {
            // "No marks yet" is plain text
            p->setPen(c.muted);
            p->drawText(QRectF(opt.rect).adjusted(2, 4, -2, 0), Qt::TextWordWrap, text);
            p->restore();
            return;
        }

        const QRectF card = QRectF(opt.rect).adjusted(0.5, 0.5, -0.5, -0.5 - kSpacing);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hover = opt.state & QStyle::State_MouseOver;
        if (text.isEmpty()) {
            p->setPen(selected ? QPen(c.accent, 1.5) : QPen(c.border, 1, Qt::DashLine));
            p->setBrush(hover ? c.hover : QColor(Qt::transparent));
        } else {
            p->setPen(QPen(selected ? c.accent : c.border, selected ? 1.5 : 1));
            p->setBrush(hover ? mixColors(c.card, c.text, 0.04) : c.card);
        }
        p->drawRoundedRect(card, 8, 8);

        // The A1 badge as on the shot, in the mark color
        const QRectF badge(card.left() + kPad, card.top() + kPad, kBadge, kBadge);
        const QColor badgeColor = index.data(BadgeColorRole).value<QColor>();
        p->setPen(Qt::NoPen);
        p->setBrush(badgeColor.isValid() ? badgeColor : c.accentFill);
        p->drawEllipse(badge);
        QFont bf = smallerFont(opt.font);
        bf.setBold(true);
        p->setFont(bf);
        p->setPen(c.onAccent);
        p->drawText(badge, Qt::AlignCenter, index.data(LabelRole).toString());

        const qreal x = badge.right() + kGap;
        const qreal w = card.right() - kPad - x;
        const QFontMetricsF fm(opt.font);
        qreal y = card.top() + kPad + std::max(0.0, (kBadge - fm.height()) / 2);
        if (text.isEmpty()) {
            QFont italic = opt.font;
            italic.setItalic(true);
            p->setFont(italic);
            p->setPen(c.muted);
            p->drawText(QRectF(x, y, w, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, tr("Add a comment"));
            const QFont small = smallerFont(opt.font);
            p->setFont(small);
            p->drawText(QRectF(x, y + fm.height() + 2, w, QFontMetricsF(small).height()), Qt::AlignLeft | Qt::AlignVCenter,
                        tr("without it the mark is not in the text"));
        } else {
            p->setFont(opt.font);
            p->setPen(c.text);
            p->drawText(QRectF(x, y, w, card.bottom() - y), Qt::TextWordWrap, text);
        }
        p->restore();
    }

private:
    static constexpr int kPad = 10;
    static constexpr int kBadge = 24;
    static constexpr int kGap = 10;
    static constexpr int kSpacing = 8;

    static int viewWidth(const QStyleOptionViewItem &opt)
    {
        if (const auto *view = qobject_cast<const QAbstractItemView *>(opt.widget))
            return view->viewport()->width();
        return opt.rect.width() > 0 ? opt.rect.width() : 260;
    }

    static qreal contentHeight(const QStyleOptionViewItem &opt, const QString &text, int width)
    {
        const QFontMetricsF fm(opt.font);
        const qreal offset = std::max(0.0, (kBadge - fm.height()) / 2);
        qreal h;
        if (text.isEmpty()) {
            h = fm.height() + 2 + QFontMetricsF(smallerFont(opt.font)).height();
        } else {
            const qreal textW = width - 2 * kPad - kBadge - kGap;
            h = fm.boundingRect(QRectF(0, 0, textW, 1e6), Qt::TextWordWrap, text).height();
        }
        return std::max<qreal>(kBadge, offset + h);
    }
};

FlatButton *toolButton(Icon glyph, const QString &name, const QString &key, const QString &description)
{
    auto *b = new FlatButton(glyph, name);
    b->setToolTip(QStringLiteral("%1 (%2)\n%3").arg(name, key, description));
    b->setCheckable(true);
    return b;
}

} // namespace

ShotEditor::ShotEditor(CaptureSetModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_canvas = new ShotCanvas(model);

    m_markTool = toolButton(Icon::Mark, tr("Mark"), QStringLiteral("R"), tr("Outline an area and comment it"));
    m_arrowTool = toolButton(Icon::Arrow, tr("Arrow"), QStringLiteral("A"), tr("Point with an arrow and comment it"));
    m_penTool = toolButton(Icon::Pen, tr("Pen"), QStringLiteral("P"), tr("Draw freehand — circle or underline — and comment it"));
    m_pointTool = toolButton(Icon::Point, tr("Point"), QStringLiteral("D"), tr("Put a numbered point on the spot and comment it"));
    m_blurTool = toolButton(Icon::Blur, tr("Blur"), QStringLiteral("B"), tr("Pixelate passwords and personal data"));
    m_cropTool = toolButton(Icon::Crop, tr("Crop"), QStringLiteral("C"), tr("Crop the shot"));
    auto *group = new QButtonGroup(this);
    for (FlatButton *b : {m_markTool, m_arrowTool, m_penTool, m_pointTool, m_blurTool, m_cropTool})
        group->addButton(b);
    m_markTool->setChecked(true);

    // Frame shape is a menu on the "Mark" tool, as in Snapik. The choice applies to new marks and to
    // the selected frame; it is remembered together with the pen (docs/decisions/0020-frame-shape.md)
    m_frameButton = new FlatButton(Icon::ChevronDown, tr("Frame shape"));
    m_frameButton->setSide(36, 14);
    m_frameButton->setFixedWidth(18);
    m_frameButton->setPopupMode(QToolButton::InstantPopup);
    auto *frameMenu = new QMenu(m_frameButton);
    auto *frameGroup = new QActionGroup(frameMenu);
    for (auto [frame, name] : {std::pair{FrameShape::Rect, tr("Rectangle")},
                               std::pair{FrameShape::Rounded, tr("Rounded rectangle")},
                               std::pair{FrameShape::Ellipse, tr("Oval")}}) {
        QAction *a = frameMenu->addAction(name);
        a->setCheckable(true);
        frameGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, frame = frame] { setFrame(frame); });
        m_frameActions.append({frame, a});
    }
    m_frameButton->setMenu(frameMenu);

    m_applyCrop = new ActionButton(tr("Apply crop"));
    m_applyCrop->setGlyph(Icon::Check);
    m_applyCrop->setVisible(false);
    m_penPicker = new PenPicker;

    m_undo = new FlatButton(Icon::Undo, tr("Undo"));
    m_undo->setToolTip(tr("Undo (Ctrl+Z)"));
    m_redo = new FlatButton(Icon::Redo, tr("Redo"));
    m_redo->setToolTip(tr("Redo (Ctrl+Shift+Z)"));
    auto *deleteShot = new FlatButton(Icon::Trash, tr("Delete shot"));
    deleteShot->setToolTip(tr("Delete this shot from the set"));
    auto *newShot = new ActionButton(tr("New shot"));
    newShot->setGlyph(Icon::Camera);
    newShot->setToolTip(tr("Capture one more area"));
    auto *done = new ActionButton(tr("Done"), ActionButton::Kind::Primary);
    done->setGlyph(Icon::Check);
    done->setToolTip(tr("Close the shot (Ctrl+Enter)"));

    // Toolbar: mark tools | blur and crop | pen | history … actions
    auto *toolbar = new QWidget;
    toolbar->setFixedHeight(52);
    auto *bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(10, 8, 10, 8);
    bar->setSpacing(2);
    bar->addWidget(m_markTool);
    bar->addWidget(m_frameButton);
    for (FlatButton *b : {m_arrowTool, m_penTool, m_pointTool})
        bar->addWidget(b);
    bar->addWidget(new Rule(Qt::Vertical));
    bar->addWidget(m_blurTool);
    bar->addWidget(m_cropTool);
    bar->addSpacing(4);
    bar->addWidget(m_applyCrop);
    bar->addWidget(new Rule(Qt::Vertical));
    bar->addWidget(m_penPicker);
    bar->addWidget(new Rule(Qt::Vertical));
    bar->addWidget(m_undo);
    bar->addWidget(m_redo);
    bar->addStretch();
    bar->addWidget(deleteShot);
    bar->addSpacing(6);
    bar->addWidget(newShot);
    bar->addSpacing(6);
    bar->addWidget(done);

    // The shot's comments: all the text that goes to the chat is visible at once
    auto *side = new Panel(Qt::LeftEdge);
    side->setFixedWidth(300);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(13, 12, 12, 12);
    sideLayout->setSpacing(8);
    auto *sideHead = new QHBoxLayout;
    m_commentsTitle = new QLabel(tr("Comments"));
    QFont titleFont = m_commentsTitle->font();
    titleFont.setWeight(QFont::DemiBold);
    m_commentsTitle->setFont(titleFont);
    m_commentsCount = new QLabel;
    m_commentsCount->setFont(smallerFont(m_commentsCount->font()));
    sideHead->addWidget(m_commentsTitle);
    sideHead->addStretch();
    sideHead->addWidget(m_commentsCount);
    sideLayout->addLayout(sideHead);

    m_comments = new QListWidget;
    m_comments->setItemDelegate(new CommentDelegate(m_comments));
    m_comments->setFrameShape(QFrame::NoFrame);
    m_comments->setFocusPolicy(Qt::NoFocus);
    m_comments->setMouseTracking(true);
    m_comments->setSelectionMode(QAbstractItemView::SingleSelection);
    m_comments->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_comments->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_comments->viewport()->setAutoFillBackground(false);
    m_comments->setAutoFillBackground(false);
    m_comments->setToolTip(tr("Click a comment to edit it"));
    connect(m_comments, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QUuid id = item->data(IdRole).toUuid();
        if (!id.isNull())
            m_canvas->editMark(id);
    });
    sideLayout->addWidget(m_comments, 1);
    m_commentsFooter = new QLabel(tr("Click a comment or double-click a mark to edit it. Delete removes the selected mark."));
    m_commentsFooter->setWordWrap(true);
    m_commentsFooter->setFont(smallerFont(m_commentsFooter->font()));
    sideLayout->addWidget(m_commentsFooter);

    auto *middle = new QHBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(m_canvas, 1);
    middle->addWidget(side);

    // Hint line: what the tool does and the main keys
    auto *status = new QWidget;
    status->setFixedHeight(32);
    auto *statusLayout = new QHBoxLayout(status);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    statusLayout->setSpacing(14);
    m_hint = new QLabel;
    m_hint->setTextFormat(Qt::RichText);
    m_hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusLayout->addWidget(m_hint, 1);
    statusLayout->addWidget(new KeyHint(QStringLiteral("Ctrl+Z"), tr("undo")));
    statusLayout->addWidget(new KeyHint(QStringLiteral("Ctrl+Enter"), tr("done")));
    m_escHint = new KeyHint(QStringLiteral("Esc"), tr("close"));
    statusLayout->addWidget(m_escHint);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    layout->addWidget(new Rule(Qt::Horizontal));
    layout->addLayout(middle, 1);
    layout->addWidget(new Rule(Qt::Horizontal));
    layout->addWidget(status);

    connect(m_markTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Mark); });
    connect(m_arrowTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Arrow); });
    connect(m_penTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Pen); });
    connect(m_pointTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Point); });
    connect(m_blurTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Blur); });
    connect(m_cropTool, &QToolButton::clicked, this, [this] { m_canvas->setTool(ShotCanvas::Tool::Crop); });
    connect(m_applyCrop, &QPushButton::clicked, m_canvas, &ShotCanvas::applyCrop);
    connect(m_canvas, &ShotCanvas::toolChanged, this, &ShotEditor::updateTools);
    connect(m_canvas, &ShotCanvas::cropPendingChanged, this, &ShotEditor::updateTools);
    connect(m_canvas, &ShotCanvas::selectedMarkChanged, this, &ShotEditor::updateCommentSelection);

    // Pen: the choice in the panel applies to new marks and to the selected one; selecting a mark
    // shows its pen in the panel.
    connect(m_penPicker, &PenPicker::penChanged, this, [this](const MarkPen &pen) {
        m_canvas->setPen(pen);
        emit penChanged(pen);
    });
    connect(m_canvas, &ShotCanvas::markSelected, this, [this](const MarkPen &pen, MarkShape shape) {
        updatePenDefaults(shape);
        m_penPicker->setPen(pen);
        updateFrameUi();   // an ellipse was selected, so the next mark is an ellipse too, as with color
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

    // Unlimited undo within the set (EDT-8)
    auto *undoKey = new QShortcut(QKeySequence::Undo, this);
    connect(undoKey, &QShortcut::activated, this, [this, undo] {
        m_canvas->commitComment();
        undo->undo();
    });
    for (const QKeySequence &seq : {QKeySequence(QKeySequence::Redo), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)}) {
        auto *redoKey = new QShortcut(seq, this);
        connect(redoKey, &QShortcut::activated, undo, &QUndoStack::redo);
    }
    // Single-letter tool keys must not fire in the comment field.
    // M is the old frame key, kept next to R.
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
        m_fresh = false;   // the shot is accepted
        updateTools();
        emit doneRequested();
    };
    connect(done, &QPushButton::clicked, this, finish);
    // Ctrl+Enter works like "Done". In the comment field CommentPopup catches it
    // (otherwise Enter there would save only the comment).
    connect(m_canvas, &ShotCanvas::finishRequested, this, finish);
    for (const QKeySequence &seq : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto *sc = new QShortcut(seq, this);
        connect(sc, &QShortcut::activated, this, finish);
    }

    connect(m_model, &CaptureSetModel::changed, this, [this] {
        if (!m_model->shot(m_canvas->shotId())) {
            // The shot was deleted (with "Delete shot" or from the strip): close, as with Esc.
            // It used to open the last shot of the strip, which the user had not chosen (docs/problems.md).
            m_fresh = false;
            hide();
            if (!m_model->set().isEmpty())
                m_canvas->setShot(m_model->set().shots.last().id);   // so the window is not empty the next time it shows
            updateTools();
        }
        updateTitle();
        updateComments();
    });
    applyColors();
    updateTools();
}

void ShotEditor::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    // The system theme changed on the fly: hints need a new muted color
    if (e->type() == QEvent::PaletteChange && !m_applyingColors)
        applyColors();
}

void ShotEditor::applyColors()
{
    m_applyingColors = true;
    for (QWidget *w : {static_cast<QWidget *>(m_hint), static_cast<QWidget *>(m_commentsCount),
                       static_cast<QWidget *>(m_commentsFooter)})
        setMutedText(w);
    m_penPicker->setPen(m_penPicker->pen());   // color and line samples follow the new theme
    updateFrameUi();                          // frame shape menu icons in the new theme's color
    m_applyingColors = false;
    updateTools();
    update();
}

void ShotEditor::updateComments()
{
    m_comments->clear();
    m_commentsCount->clear();
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
    int withText = 0;
    for (int i = 0; i < shot->marks.size(); ++i) {
        const Mark &m = shot->marks[i];
        const QString text = m.comment.trimmed();
        const QString label = markLabel(si, i);
        auto *item = new QListWidgetItem(text);
        item->setData(IdRole, m.id);
        item->setData(LabelRole, label);
        item->setData(BadgeColorRole, badgeFillColor(markColor(m, m_canvas->style())));
        item->setData(Qt::AccessibleTextRole, text.isEmpty() ? label : label + QStringLiteral(" ") + text);
        m_comments->addItem(item);
        if (!text.isEmpty())
            ++withText;
    }
    m_commentsCount->setText(tr("in the text: %1 of %2").arg(withText).arg(shot->marks.size()));
    updateCommentSelection();
}

void ShotEditor::updateCommentSelection()
{
    const QUuid selected = m_canvas->selectedMark();
    for (int i = 0; i < m_comments->count(); ++i) {
        QListWidgetItem *item = m_comments->item(i);
        const bool match = !selected.isNull() && item->data(IdRole).toUuid() == selected;
        item->setSelected(match);
        if (match)
            m_comments->scrollToItem(item);
    }
}

void ShotEditor::setStyle(const RenderStyle &style)
{
    setUiAccent(style.accent);
    m_canvas->setStyle(style);
    updatePenDefaults(MarkShape::Rect);
    applyColors();
    updateComments();   // card badges use the mark colors, which may come from the accent
}

void ShotEditor::setPen(const MarkPen &pen)
{
    m_penPicker->setPen(pen);
    m_canvas->setPen(pen);
    updateFrameUi();
}

void ShotEditor::setFrame(FrameShape frame)
{
    MarkPen pen = m_penPicker->pen();
    pen.frame = frame;
    m_penPicker->setPen(pen);
    m_canvas->setPen(pen);      // and the selected frame, as one edit in the history
    m_canvas->setTool(ShotCanvas::Tool::Mark);
    updateFrameUi();
    emit penChanged(pen);
}

void ShotEditor::updateFrameUi()
{
    const FrameShape current = m_penPicker->pen().frame;
    m_markTool->setGlyph(current == FrameShape::Rounded   ? Icon::MarkRounded
                         : current == FrameShape::Ellipse ? Icon::MarkEllipse
                                                          : Icon::Mark);
    const QColor color = UiColors::from(palette()).text;
    const qreal dpr = devicePixelRatioF();
    for (const auto &[frame, action] : m_frameActions) {
        action->setChecked(frame == current);
        action->setIcon(QIcon(iconPixmap(frame == FrameShape::Rounded   ? Icon::FrameRounded
                                         : frame == FrameShape::Ellipse ? Icon::FrameEllipse
                                                                        : Icon::FrameRect,
                                         color, 20, dpr)));
    }
}

void ShotEditor::updatePenDefaults(MarkShape shape)
{
    m_penPicker->setDefaults(m_canvas->style().accent, defaultMarkWidth(shape, m_canvas->style()));
}

void ShotEditor::showShot(const QUuid &shotId, bool fresh)
{
    m_fresh = fresh;
    m_canvas->setShot(shotId);
    m_canvas->setTool(ShotCanvas::Tool::Mark);   // every shot starts with the frame tool
    updateTitle();
    updateComments();
    updateTools();   // the Esc hint depends on whether the shot is fresh

    // Size follows the shot (plus the comment panel, tools and hint), but no more than 85 % of the screen
    const QSize hint = m_canvas->sizeHint() + QSize(300, 52 + 32 + 2);
    QScreen *screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
    const QSize avail = screen->availableGeometry().size() * 0.85;
    resize(hint.boundedTo(avail).expandedTo(QSize(720, 420)));
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

    QString name, hint;
    switch (tool) {
    case ShotCanvas::Tool::Mark:
        name = tr("Mark");
        hint = tr("drag over an area — a comment field opens");
        break;
    case ShotCanvas::Tool::Arrow:
        name = tr("Arrow");
        hint = tr("drag from the tail to the tip; drag an end to adjust it");
        break;
    case ShotCanvas::Tool::Pen:
        name = tr("Pen");
        hint = tr("draw around or under the spot");
        break;
    case ShotCanvas::Tool::Point:
        name = tr("Point");
        hint = tr("click the spot — a numbered point appears");
        break;
    case ShotCanvas::Tool::Blur:
        name = tr("Blur");
        hint = tr("drag over passwords and personal data — the chat gets a pixelated copy");
        break;
    case ShotCanvas::Tool::Crop:
        name = tr("Crop");
        hint = tr("drag the area to keep · Enter — apply · Esc — cancel");
        break;
    }
    const UiColors c = UiColors::from(palette());
    m_hint->setText(QStringLiteral("<span style=\"color:%1\"><b>%2</b></span> · %3")
                        .arg(c.text.name(), name.toHtmlEscaped(), hint.toHtmlEscaped()));
    m_escHint->setLabel(m_fresh ? tr("discard the shot") : tr("close"));
}

void ShotEditor::keyPressEvent(QKeyEvent *e)
{
    // Esc gets here only if there is nothing to close inside the window: the comment field,
    // the crop frame and the selection are handled by the canvas and the field themselves.
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
        // A shot from the strip: just close, the edits are saved
        m_canvas->commitComment();
        emit doneRequested();
        return;
    }
    // A fresh shot: discard it. If there is already work on it, ask.
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
