#include "StripWindow.h"

#include "Controls.h"
#include "MoveGhost.h"
#include "platform/WindowRoles.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QWindow>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace mp {

namespace {
constexpr int kThumbWidth = 212;
constexpr int kThumbMaxHeight = 130;
} // namespace

// Trash button on a thumbnail: delete the shot with one click, without confirmation (the deletion goes
// through the model's undo stack and can be undone in the editor). It sits on top of the image, hence
// its own dark backing, like the comment counter's: the icon does not get lost on a light shot.
class ThumbDeleteButton : public QToolButton
{
public:
    explicit ThumbDeleteButton(const QString &text, QWidget *parent) : QToolButton(parent)
    {
        setText(text);
        setToolTip(text);
        setAccessibleName(text);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(24, 24);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(isDown() ? c.danger.darker(130) : underMouse() ? c.danger : QColor(0, 0, 0, 160));
        p.drawRoundedRect(QRectF(rect()), 6, 6);
        p.drawPixmap(QPointF((width() - 16) / 2.0, (height() - 16) / 2.0),
                     iconPixmap(Icon::Trash, Qt::white, 16, devicePixelRatioF()));
    }
};

// Shot thumbnail: a click opens it in the editor, a drag takes it outside as a file (STR-6).
class ShotThumb : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(ShotThumb)
public:
    ShotThumb(StripWindow *strip, const QUuid &id, int index, const QPixmap &pixmap, int comments, int marks,
              bool current)
        : m_strip(strip), m_id(id), m_index(index), m_pixmap(pixmap), m_comments(comments), m_marks(marks),
          m_current(current)
    {
        // Width follows the list (less the scroll bar, if any), height follows the shot
        setFixedHeight(std::max(48, int(pixmap.height() / pixmap.devicePixelRatio()) + 2));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("Click — edit · Drag — into a chat as a file or within the strip to reorder · Right click — menu"));
        // The thumbnail is deleted later (rebuild → deleteLater), so deleting from its own button is safe
        m_delete = new ThumbDeleteButton(tr("Delete shot"), this);
        connect(m_delete, &QToolButton::clicked, this, [this] { m_strip->m_model->removeShot(m_id); });
    }

protected:
    void resizeEvent(QResizeEvent *) override { m_delete->move(width() - 8 - m_delete->width(), 8); }

    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(rect()).adjusted(1, 1, -1, -1);
        QPainterPath clip;
        clip.addRoundedRect(frame, 8, 8);

        // Thumbnail in a rounded card; a narrow shot is centered on the card background
        p.save();
        p.setClipPath(clip);
        p.fillRect(frame, c.raised);
        const QSizeF ps = m_pixmap.deviceIndependentSize();
        p.drawPixmap(QPointF((width() - ps.width()) / 2, (height() - ps.height()) / 2), m_pixmap);
        p.restore();
        p.setBrush(Qt::NoBrush);
        if (m_current) {
            p.setPen(QPen(c.accent, 2));
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 8, 8);
        } else {
            p.setPen(QPen(c.border, 1));
            p.drawRoundedRect(frame.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        }

        // Shot letter in a circle, like a mark badge
        const QRectF badge(8, 8, 24, 24);
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(c.accentFill);
        p.drawEllipse(badge.adjusted(1, 1, -1, -1));
        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, QString(shotLetter(m_index)));

        // Number of comments
        if (m_marks > 0) {
            QFont small = smallerFont(font());
            small.setBold(true);
            const QString n = QString::number(m_comments);
            const qreal textW = QFontMetricsF(small).horizontalAdvance(n);
            const QRectF box(width() - 8 - (textW + 32), height() - 28, textW + 32, 20);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 160));
            p.drawRoundedRect(box, 6, 6);
            p.drawPixmap(QPointF(box.left() + 7, box.top() + 3), iconPixmap(Icon::Comment, Qt::white, 14, devicePixelRatioF()));
            p.setFont(small);
            p.setPen(Qt::white);
            p.drawText(box.adjusted(25, 0, -7, 0), Qt::AlignVCenter | Qt::AlignLeft, n);
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
            m_pressPos = e->position().toPoint();
        else if (e->button() == Qt::RightButton)
            showMenu(e->globalPosition().toPoint());
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!(e->buttons() & Qt::LeftButton) || m_pressPos.isNull())
            return;
        if ((e->position().toPoint() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            return;
        m_pressPos = {};
        m_strip->startDrag({m_id}, this);
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && !m_pressPos.isNull()) {
            m_pressPos = {};
            emit m_strip->shotActivated(m_id);
        }
    }

private:
    void showMenu(const QPoint &globalPos)
    {
        QMenu menu(this);
        CaptureSetModel *model = m_strip->m_model;
        const int count = int(model->set().shots.size());
        menu.addAction(tr("Edit"), this, [this] { emit m_strip->shotActivated(m_id); });
        QAction *up = menu.addAction(tr("Move up"), this, [model, this] { model->moveShot(m_id, m_index - 1); });
        QAction *down = menu.addAction(tr("Move down"), this, [model, this] { model->moveShot(m_id, m_index + 1); });
        up->setEnabled(m_index > 0);
        down->setEnabled(m_index < count - 1);
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, [model, this] { model->removeShot(m_id); });
        menu.exec(globalPos);
    }

    StripWindow *m_strip;
    QUuid m_id;
    int m_index;
    QPixmap m_pixmap;
    int m_comments;
    int m_marks;
    bool m_current;
    QPoint m_pressPos;
    ThumbDeleteButton *m_delete;
};

// The thumbnail list is a drop target within the strip (STR-3): a shot dropped between
// others takes that place, the letters are recomputed. Outside, the same shot goes as a file
// (STR-6): it is one drag, and the drop location decides.
class ShotListHost : public QWidget
{
public:
    explicit ShotListHost(StripWindow *strip) : m_strip(strip) { setAcceptDrops(true); }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override { dragMoveEvent(e); }
    void dragMoveEvent(QDragMoveEvent *e) override
    {
        if (StripWindow::draggedShot(e->mimeData()).isNull()) {
            e->ignore();
            return;
        }
        e->setDropAction(Qt::CopyAction);
        e->accept();
        setDropLine(m_strip->dropIndexAt(e->position().toPoint().y()));
    }
    void dragLeaveEvent(QDragLeaveEvent *) override { setDropLine(-1); }
    void dropEvent(QDropEvent *e) override
    {
        const QUuid id = StripWindow::draggedShot(e->mimeData());
        const int at = m_strip->dropIndexAt(e->position().toPoint().y());
        setDropLine(-1);
        if (id.isNull())
            return;
        e->setDropAction(Qt::CopyAction);
        e->accept();
        // Deferred: rebuilding the strip deletes the thumbnail, which is the source of the still running QDrag
        QTimer::singleShot(0, m_strip, [strip = m_strip, id, at] { strip->moveShotTo(id, at); });
    }
    void paintEvent(QPaintEvent *) override
    {
        if (m_dropIndex < 0)
            return;
        QPainter p(this);
        p.setPen(Qt::NoPen);
        p.setBrush(m_strip->m_style.accent);
        p.drawRoundedRect(QRectF(2, m_strip->dropLineY(m_dropIndex) - 2, width() - 4, 4), 2, 2);
    }

private:
    void setDropLine(int index)
    {
        if (index != m_dropIndex) {
            m_dropIndex = index;
            update();
        }
    }

    StripWindow *m_strip;
    int m_dropIndex = -1;
};

// "Drag the whole set" handle
class DragHandle : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(DragHandle)
public:
    explicit DragHandle(std::function<void()> onDrag)
        : m_onDrag(std::move(onDrag))
    {
        setFixedHeight(26);
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Drag into a chat: all shots as files, the text is copied to the clipboard"));
        setAccessibleName(text());
    }

    static QString text() { return tr("Drag the set into a chat"); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        const QFont f = smallerFont(font());
        p.setFont(f);
        const qreal textW = QFontMetricsF(f).horizontalAdvance(text());
        const qreal x = (width() - (14 + 6 + textW)) / 2;
        p.drawPixmap(QPointF(x, (height() - 14) / 2.0), iconPixmap(Icon::Grip, c.muted, 14, devicePixelRatioF()));
        p.setPen(c.muted);
        p.drawText(QRectF(x + 20, 0, textW + 1, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
    }
    void mousePressEvent(QMouseEvent *e) override { m_press = e->position().toPoint(); }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if ((e->buttons() & Qt::LeftButton) && !m_press.isNull()
            && (e->position().toPoint() - m_press).manhattanLength() >= QApplication::startDragDistance()) {
            m_press = {};
            m_onDrag();
        }
    }

private:
    std::function<void()> m_onDrag;
    QPoint m_press;
};

// Number of shots in the header; the full counter "3 shots · 4 comments" (STR-2) is in the tooltip
class CountPill : public QWidget
{
public:
    void setCount(int count, const QString &tooltip)
    {
        m_text = QString::number(count);
        setToolTip(tooltip);
        setAccessibleName(tooltip);
        updateGeometry();
        update();
    }
    QSize sizeHint() const override
    {
        QFont f = smallerFont(font());
        f.setBold(true);
        return {std::max(20, int(QFontMetricsF(f).horizontalAdvance(m_text)) + 12), 20};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal top = (height() - 20) / 2.0;
        const QRectF r(0.5, top + 0.5, width() - 1, 19);
        p.setPen(QPen(c.border, 1));
        p.setBrush(c.raised);
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        QFont f = smallerFont(font());
        f.setBold(true);
        p.setFont(f);
        p.setPen(c.text);
        p.drawText(r, Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
};

// Empty set: what to do to get a shot
class EmptyHint : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(EmptyHint)
public:
    EmptyHint() { setFixedHeight(150); }
    void setCaptureKey(const QString &key)
    {
        m_key = key;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(c.border, 1.5, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 10, 10);

        const QFontMetricsF fm(font());
        const QFont small = smallerFont(font());
        const QFontMetricsF sm(small);
        const QSizeF key = keyCapSize(sm, m_key);
        const QString after = tr("— capture an area");
        const qreal blockH = 28 + 8 + fm.height() + (m_key.isEmpty() ? 0 : 8 + key.height());
        qreal y = (height() - blockH) / 2;
        p.drawPixmap(QPointF((width() - 28) / 2.0, y), iconPixmap(Icon::Camera, c.muted, 28, devicePixelRatioF()));
        y += 28 + 8;
        QFont title = font();
        title.setWeight(QFont::DemiBold);
        p.setFont(title);
        p.setPen(c.text);
        p.drawText(QRectF(0, y, width(), fm.height()), Qt::AlignCenter, tr("The set is empty"));
        y += fm.height() + 8;
        if (!m_key.isEmpty()) {
            p.setFont(small);
            const qreal afterW = sm.horizontalAdvance(after);
            const qreal x = (width() - (key.width() + 6 + afterW)) / 2;
            drawKeyCap(p, QRectF(x, y, key.width(), key.height()), m_key, c.text, c.raised, c.border);
            p.setPen(c.muted);
            p.drawText(QRectF(x + key.width() + 6, y, afterW + 1, key.height()), Qt::AlignVCenter | Qt::AlignLeft, after);
        }
    }

private:
    QString m_key;
};

// Strip message ("Pasted", "Click into the chat window first") on a rounded plate
class MessageLabel : public QLabel
{
public:
    MessageLabel()
    {
        setWordWrap(true);
        setContentsMargins(10, 7, 10, 7);
    }

protected:
    void paintEvent(QPaintEvent *e) override
    {
        const UiColors c = UiColors::from(palette());
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(c.border, 1));
            p.setBrush(c.raised);
            p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        }
        QLabel::paintEvent(e);
    }
};

// The strip header is the drag handle
class StripHeader : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(StripHeader)
public:
    explicit StripHeader(StripWindow *strip)
        : m_strip(strip)
    {
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Drag to move the strip to another edge or screen"));
    }

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        // The window manager moves the window itself and takes the pointer: there is no release for us,
        // so the cursor is not switched to the "grabbing" one
        if (!m_strip->beginMove(mapTo(m_strip, e->position().toPoint())))
            setCursor(Qt::ClosedHandCursor);
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (e->buttons() & Qt::LeftButton)
            m_strip->updateMove(mapTo(m_strip, e->position().toPoint()));
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        setCursor(Qt::OpenHandCursor);
        // While the button is held, events come to us even outside the surface (implicit grab),
        // with coordinates relative to our window
        m_strip->finishMove(mapTo(m_strip, e->position().toPoint()));
    }

private:
    StripWindow *m_strip;
};

// Program icon in the strip header
class WindowIconView : public QWidget
{
public:
    WindowIconView() { setFixedSize(18, 18); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        window()->windowIcon().paint(&p, rect());
    }
};

StripWindow::StripWindow(CaptureSetModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setWindowTitle(tr("Markpaste — set"));
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *header = new StripHeader(this);
    header->setFixedHeight(32);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 0, 0, 0);
    headerLayout->setSpacing(6);
    auto *icon = new WindowIconView;
    auto *title = new QLabel(QStringLiteral("Markpaste"));
    QFont titleFont = title->font();
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    m_count = new CountPill;
    for (QWidget *w : {static_cast<QWidget *>(icon), static_cast<QWidget *>(title), static_cast<QWidget *>(m_count)})
        w->setAttribute(Qt::WA_TransparentForMouseEvents);   // the whole header is draggable
    m_count->setAttribute(Qt::WA_TransparentForMouseEvents, false);   // the counter tooltip

    // "Clear" (STR-2) is the trash icon in the header, next to the menu
    m_clear = new FlatButton(Icon::Trash, tr("Clear"));
    m_clear->setToolTip(tr("Clear the set"));
    m_clear->setSide(28, 18);
    auto *menuButton = new FlatButton(Icon::More, tr("Menu"));
    menuButton->setSide(28, 18);
    // A menu rather than settings right away: preview (PST-6) and save to folder (APP-6) are here too
    auto *menu = new QMenu(menuButton);
    m_previewAction = menu->addAction(tr("Preview message…"), this, &StripWindow::previewRequested);
    m_saveAction = menu->addAction(tr("Save to folder…"), this, &StripWindow::saveToFolderRequested);
    menu->addSeparator();
    menu->addAction(tr("Settings"), this, &StripWindow::settingsRequested);
    menu->addAction(tr("About Markpaste"), this, &StripWindow::aboutRequested);
    menuButton->setMenu(menu);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    auto *hideButton = new FlatButton(Icon::Minimize, tr("Hide to tray"));
    hideButton->setSide(28, 16);
    connect(hideButton, &QToolButton::clicked, this, &StripWindow::hideRequested);
    headerLayout->addWidget(icon);
    headerLayout->addWidget(title);
    headerLayout->addWidget(m_count);
    headerLayout->addStretch();
    headerLayout->addWidget(m_clear);
    headerLayout->addWidget(menuButton);
    headerLayout->addWidget(hideButton);
    layout->addWidget(header);

    m_empty = new EmptyHint;
    m_empty->setFixedWidth(kThumbWidth);
    layout->addWidget(m_empty);

    auto *listHost = new ShotListHost(this);
    m_list = new QVBoxLayout(listHost);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(8);
    m_list->addStretch();
    m_scroll = new QScrollArea;
    m_scroll->setWidget(listHost);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFixedWidth(kThumbWidth);
    // The list background is the strip background: for a transparent window (rounded corners) it is painted in paintEvent
    m_scroll->viewport()->setAutoFillBackground(false);
    listHost->setAutoFillBackground(false);
    layout->addWidget(m_scroll, 1);

    m_dragHandle = new DragHandle([this] {
        QList<QUuid> all;
        for (const Shot &s : m_model->set().shots)
            all << s.id;
        startDrag(all, m_dragHandle);
    });
    layout->addWidget(m_dragHandle);

    m_message = new MessageLabel;
    // Not wider than the list: otherwise a long message widened the strip, and on Windows it went off the screen edge
    m_message->setMaximumWidth(kThumbWidth);
    m_message->setVisible(false);
    layout->addWidget(m_message);

    // The primary button is "Paste set"; in an empty set "New shot" takes its place
    m_paste = new ActionButton(tr("Paste set"), ActionButton::Kind::Primary);
    m_paste->setButtonHeight(40);
    m_newShot = new ActionButton(tr("New shot"));
    m_newShot->setGlyph(Icon::Camera);
    m_copy = new ActionButton(QString());
    m_copy->setGlyph(Icon::Copy);
    m_copy->setAccessibleName(tr("Copy set"));
    m_copy->setToolTip(tr("Copy set: files and text go to the clipboard, then press Ctrl+V in the chat"));
    m_cancel = new ActionButton(tr("Stop"));
    m_cancel->setVisible(false);
    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    row->addWidget(m_newShot, 1);
    row->addWidget(m_copy);
    layout->addWidget(m_paste);
    layout->addLayout(row);
    layout->addWidget(m_cancel);
    connect(m_newShot, &QPushButton::clicked, this, &StripWindow::newShotRequested);
    connect(m_paste, &QPushButton::clicked, this, &StripWindow::pasteRequested);
    connect(m_copy, &QPushButton::clicked, this, &StripWindow::copyRequested);
    connect(m_clear, &QToolButton::clicked, this, &StripWindow::clearRequested);
    connect(m_cancel, &QPushButton::clicked, this, &StripWindow::cancelPasteRequested);

    connect(m_model, &CaptureSetModel::changed, this, &StripWindow::rebuild);
    rebuild();
}

void StripWindow::setStyle(const RenderStyle &style)
{
    m_style = style;
    setUiAccent(style.accent);
    m_thumbs.clear();
    rebuild();
    update();
}

void StripWindow::setCurrentShot(const QUuid &shotId)
{
    m_current = shotId;
    rebuild();
}

void StripWindow::setPasteMode(bool sequential, const QString &hotkey)
{
    m_pasteCopies = !sequential;
    if (sequential) {
        m_paste->setText(tr("Paste set"));
        m_paste->setToolTip(tr("Pastes all shots and the text into the active chat window"));
        m_paste->setKeyHint(hotkey);
    } else {
        m_paste->setText(tr("Copy set"));
        m_paste->setToolTip(tr("Copies files and text — then press Ctrl+V in the chat"));
        m_paste->setKeyHint({});
    }
    rebuild();   // a separate "Copy" button is not needed if the primary one already copies
}

void StripWindow::setCaptureHotkey(const QString &hotkey)
{
    m_empty->setCaptureKey(hotkey);
}

void StripWindow::showMessage(const QString &text, int timeoutMs)
{
    m_message->setText(text);
    m_message->setVisible(true);
    adjustSize();
    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, m_message, [this, text] {
            if (m_message->text() == text) {
                m_message->setVisible(false);
                adjustSize();
            }
        });
    }
}

void StripWindow::setProgress(int done, int total)
{
    const bool running = total > 0 && done < total;
    m_cancel->setVisible(running);
    m_paste->setEnabled(!running);
    m_copy->setEnabled(!running);
    if (total > 0)
        showMessage(running ? tr("Pasting %1 of %2…").arg(done + 1).arg(total) : tr("Pasted"), running ? 0 : 2500);
}

QPixmap StripWindow::thumbnailFor(int index)
{
    const Shot &shot = m_model->set().shots[index];
    auto it = m_thumbs.find(shot.id);
    if (it != m_thumbs.end() && it->index == index && it->shot == shot)
        return it->pixmap;
    // The thumbnail is the baked shot: marks and blur look exactly as the chat will see them
    const QImage baked = renderShot(shot, index, m_style);
    const qreal dpr = devicePixelRatioF();
    QImage scaled = baked.scaled(QSize(int((kThumbWidth - 2) * dpr), int(kThumbMaxHeight * dpr)), Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    QPixmap pm = QPixmap::fromImage(scaled);
    pm.setDevicePixelRatio(dpr);
    m_thumbs.insert(shot.id, CachedThumb{shot, index, pm});
    return pm;
}

void StripWindow::rebuild()
{
    // Remove the old thumbnails (the last item is the stretch). Deleted later: a rebuild
    // is often triggered by the thumbnail itself: a click, an item of its menu (the menu in exec() is its child object
    // on the stack), a drag. A direct delete here crashed the program (docs/problems.md).
    while (m_list->count() > 1) {
        QLayoutItem *item = m_list->takeAt(0);
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    const CaptureSet &set = m_model->set();
    QSet<QUuid> alive;
    for (int i = 0; i < set.shots.size(); ++i) {
        const Shot &s = set.shots[i];
        alive.insert(s.id);
        auto *thumb = new ShotThumb(this, s.id, i, thumbnailFor(i), s.commentCount(), int(s.marks.size()),
                                    s.id == m_current);
        m_list->insertWidget(i, thumb);
    }
    for (auto it = m_thumbs.begin(); it != m_thumbs.end();)
        it = alive.contains(it.key()) ? std::next(it) : m_thumbs.erase(it);

    // "3 shots · 4 comments" (STR-2) is the tooltip of the shot count in the header
    const int shots = int(set.shots.size());
    const bool empty = set.isEmpty();
    m_count->setCount(shots, tr("%n shot(s)", nullptr, shots) + QStringLiteral(" · ")
                                 + tr("%n comment(s)", nullptr, set.totalComments()));
    m_count->setVisible(!empty);
    m_clear->setVisible(!empty);
    m_empty->setVisible(empty);
    m_scroll->setVisible(!empty);
    const bool pasting = !m_cancel->isHidden();
    m_paste->setVisible(!empty);
    m_paste->setEnabled(!pasting);
    m_copy->setVisible(!empty && !m_pasteCopies);
    m_copy->setEnabled(!pasting);
    m_newShot->setKind(empty ? ActionButton::Kind::Primary : ActionButton::Kind::Secondary);
    m_newShot->setButtonHeight(empty ? 40 : 36);
    m_previewAction->setEnabled(!empty);
    m_saveAction->setEnabled(!empty);
    m_dragHandle->setVisible(shots > 1);
    m_newShot->setEnabled(!set.isFull());

    // Strip height follows the content, but no taller than the screen
    int listHeight = 0;
    for (int i = 0; i < m_list->count() - 1; ++i)
        listHeight += m_list->itemAt(i)->widget()->height() + m_list->spacing();
    QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const int maxList = scr ? scr->availableGeometry().height() - 340 : 600;
    m_scroll->setFixedHeight(std::clamp(listHeight, 40, std::max(120, maxList)));
    adjustSize();

    if (m_snapToEdge && isVisible())
        snapToEdge();   // centered on the edge: the center moved; at the bottom: do not go off the screen

    // The strip grew and went below the screen bottom: lift it
    if (m_layerShell && m_placement.offset >= 0 && isVisible()) {
        const int maxOffset = std::max(0, currentScreen()->geometry().height() - height());
        if (m_placement.offset > maxOffset) {
            m_placement.offset = maxOffset;
            movePanel(this, currentScreen(), m_placement);
        }
    }
}

QByteArray StripWindow::shotDragData(const QUuid &shotId)
{
    // pid, so that a shot from another program instance does not reorder anything here
    return QByteArray::number(QCoreApplication::applicationPid()) + ' ' + shotId.toByteArray(QUuid::WithoutBraces);
}

QUuid StripWindow::draggedShot(const QMimeData *data)
{
    if (!data || !data->hasFormat(QString::fromLatin1(kShotMime)))
        return {};
    const QList<QByteArray> parts = data->data(QString::fromLatin1(kShotMime)).split(' ');
    if (parts.size() != 2 || parts[0].toLongLong() != QCoreApplication::applicationPid())
        return {};
    return QUuid::fromString(QLatin1String(parts[1]));
}

int StripWindow::dropIndexAt(int y) const
{
    // The insert position is before the first thumbnail whose middle is below the cursor
    const int n = m_list->count() - 1;   // the last item is the stretch
    for (int i = 0; i < n; ++i) {
        const QWidget *w = m_list->itemAt(i)->widget();
        if (y < w->geometry().center().y())
            return i;
    }
    return n;
}

int StripWindow::dropLineY(int index) const
{
    const int n = m_list->count() - 1;
    if (n <= 0)
        return 0;
    if (index < n) {
        const QRect g = m_list->itemAt(index)->widget()->geometry();
        return index == 0 ? g.top() + 2 : g.top() - m_list->spacing() / 2;
    }
    return m_list->itemAt(n - 1)->widget()->geometry().bottom() + 1;
}

void StripWindow::moveShotTo(const QUuid &shotId, int insertIndex)
{
    const int from = m_model->set().shotIndex(shotId);
    if (from < 0)
        return;
    // The insert position is computed before the shot is removed from its old place
    const int to = insertIndex > from ? insertIndex - 1 : insertIndex;
    if (to != from)
        m_model->moveShot(shotId, to);
}

void StripWindow::startDrag(const QList<QUuid> &shots, QWidget *source)
{
    if (!m_dragProvider || shots.isEmpty())
        return;
    QMimeData *data = m_dragProvider(shots);
    if (!data)
        return;
    // A single shot can be dropped back into the strip, then it is reordered (STR-3)
    if (shots.size() == 1)
        data->setData(QString::fromLatin1(kShotMime), shotDragData(shots.first()));
    auto *drag = new QDrag(source);
    drag->setMimeData(data);
    if (shots.size() == 1) {
        const int i = m_model->set().shotIndex(shots.first());
        if (i >= 0)
            drag->setPixmap(thumbnailFor(i).scaledToWidth(96, Qt::SmoothTransformation));
    }
    drag->exec(Qt::CopyAction);
}

void StripWindow::present()
{
    if (!m_roleApplied) {
        m_roleApplied = true;
        loadPlacement();
        m_layerShell = applyPanelRole(this, currentScreen(), m_placement);
        if (!m_layerShell) {
            // An ordinary window: on GNOME "on top" and position are unavailable (STR-1), the window manager moves it.
            // Frameless: the system title bar would be a second header above ours (problems.md 2026-10-21);
            // our header moves the window through startSystemMove(). Transparent for the rounded corners.
            setWindowFlag(Qt::WindowStaysOnTopHint);
            setWindowFlag(Qt::Tool);
            setWindowFlag(Qt::FramelessWindowHint);
            setAttribute(Qt::WA_TranslucentBackground);
            m_snapToEdge = watchPanelMoves(this, [this] { onMovedByUser(); });
        }
    }
    adjustSize();
    if (m_snapToEdge)
        snapToEdge();
    show();
}

void StripWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // A window we place ourselves (Windows) grew because of a message or the "Stop" button: snap again
    // so as not to go below the work area under the taskbar
    if (m_snapToEdge && isVisible())
        snapToEdge();
}

void StripWindow::paintEvent(QPaintEvent *)
{
    // A transparent window (layer-shell, Windows, a frameless one on GNOME/X11): we paint the background with rounded corners and a border ourselves.
    // For an opaque one (an ordinary window with a system frame) Qt paints the background.
    if (!testAttribute(Qt::WA_TranslucentBackground))
        return;
    const UiColors c = UiColors::from(palette());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c.border, 1));
    p.setBrush(c.surface);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
}

void StripWindow::snapToEdge()
{
    QScreen *screen = currentScreen();
    PanelPlacement pl = m_placement;
    if (pl.offset >= 0)   // the strip grew: do not let it go below the screen bottom
        pl.offset = std::min(pl.offset, std::max(0, placementArea(screen).height() - height()));
    move(topLeftFor(screen, pl));
}

QRect StripWindow::placementArea(QScreen *screen) const
{
    // A window we place ourselves (Windows) stays within the work area: otherwise a strip at the bottom of the screen
    // covered the taskbar with the clock and the tray icon. The compositor moves a layer-shell surface away from panels,
    // so its position is computed from the whole screen.
    return m_snapToEdge ? screen->availableGeometry() : screen->geometry();
}

void StripWindow::onMovedByUser()
{
    // Where it was dropped is where it goes, but flush with the nearest edge (STR-5)
    const QPoint center = frameGeometry().center();
    QScreen *target = QGuiApplication::screenAt(center);
    if (!target)
        target = currentScreen();
    const QRect geo = placementArea(target);
    PanelPlacement pl = m_placement;
    pl.edge = center.x() < geo.center().x() ? PanelEdge::Left : PanelEdge::Right;
    pl.offset = std::clamp(y() - geo.top(), 0, std::max(0, geo.height() - height()));
    moveTo(target, pl);
    snapToEdge();
}

QScreen *StripWindow::currentScreen() const
{
    for (QScreen *s : QGuiApplication::screens())
        if (s->name() == m_screenName)
            return s;
    QScreen *s = QGuiApplication::screenAt(QCursor::pos());
    return s ? s : QGuiApplication::primaryScreen();
}

QPoint StripWindow::topLeftFor(QScreen *screen, const PanelPlacement &pl) const
{
    const QRect geo = placementArea(screen);
    const int x = pl.edge == PanelEdge::Left ? geo.left() + pl.margin : geo.right() + 1 - pl.margin - width();
    const int y = pl.offset < 0 ? geo.top() + (geo.height() - height()) / 2 : geo.top() + pl.offset;
    return {x, y};
}

bool StripWindow::beginMove(const QPoint &posInStrip)
{
    m_movePress = posInStrip;
    m_moving = false;
    if (!m_layerShell && windowHandle())
        return windowHandle()->startSystemMove();   // an ordinary window is moved by the window manager
    return false;
}

void StripWindow::moveTarget(const QPoint &posInStrip, QScreen **screen, PanelPlacement *placement) const
{
    // While the button is held, the strip stays in place, so its position is known exactly,
    // and the point on the screen = the strip position + the pointer coordinates in it.
    const QPoint global = assumedTopLeft() + posInStrip;
    QScreen *target = QGuiApplication::screenAt(global);
    if (!target)
        target = currentScreen();
    const QRect geo = target->geometry();
    PanelPlacement pl = m_placement;
    pl.edge = global.x() < geo.center().x() ? PanelEdge::Left : PanelEdge::Right;
    pl.offset = std::clamp(global.y() - m_movePress.y() - geo.top(), 0, std::max(0, geo.height() - height()));
    *screen = target;
    *placement = pl;
}

void StripWindow::updateMove(const QPoint &posInStrip)
{
    if (!m_layerShell)
        return;
    if (!m_moving && (posInStrip - m_movePress).manhattanLength() < QApplication::startDragDistance())
        return;
    m_moving = true;
    QScreen *screen = nullptr;
    PanelPlacement pl;
    moveTarget(posInStrip, &screen, &pl);
    if (!m_ghost)
        m_ghost = new MoveGhost(this);
    m_ghost->show(QRect(topLeftFor(screen, pl), size()), m_style.accent);
}

void StripWindow::finishMove(const QPoint &posInStrip)
{
    if (m_ghost)
        m_ghost->hide();
    const bool moved = m_moving;
    m_moving = false;
    if (!m_layerShell || !moved)
        return;
    // The move happens once, on release: during the motion Qt does not know whether the compositor has applied
    // the new margins, and the pointer coordinates would "drift" (docs/decisions/0004-*.md)
    QScreen *target = nullptr;
    PanelPlacement pl;
    moveTarget(posInStrip, &target, &pl);
    moveTo(target, pl);
}

void StripWindow::moveTo(QScreen *screen, const PanelPlacement &placement)
{
    m_placement = placement;
    m_screenName = screen->name();
    if (m_layerShell)
        movePanel(this, screen, placement);
    savePlacement();
}

void StripWindow::loadPlacement()
{
    QSettings s;
    m_placement.edge = s.value(QStringLiteral("strip/edge")).toString() == u"left" ? PanelEdge::Left : PanelEdge::Right;
    m_placement.offset = s.value(QStringLiteral("strip/offset"), -1).toInt();
    m_screenName = s.value(QStringLiteral("strip/screen")).toString();
}

void StripWindow::savePlacement() const
{
    QSettings s;
    s.setValue(QStringLiteral("strip/edge"), m_placement.edge == PanelEdge::Left ? QStringLiteral("left") : QStringLiteral("right"));
    s.setValue(QStringLiteral("strip/offset"), m_placement.offset);
    s.setValue(QStringLiteral("strip/screen"), m_screenName);
}

} // namespace mp
