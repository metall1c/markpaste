#include "StripWindow.h"

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
constexpr int kThumbWidth = 168;
constexpr int kThumbMaxHeight = 120;
} // namespace

// Миниатюра снимка: клик — открыть в редакторе, перетаскивание — наружу как файл (STR-6).
class ShotThumb : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(ShotThumb)
public:
    ShotThumb(StripWindow *strip, const QUuid &id, int index, const QPixmap &pixmap, int comments, int marks,
              bool current)
        : m_strip(strip), m_id(id), m_index(index), m_pixmap(pixmap), m_comments(comments), m_marks(marks),
          m_current(current)
    {
        setFixedSize(kThumbWidth, int(pixmap.height() / pixmap.devicePixelRatio()) + 8);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("Click — edit · Drag — into a chat as a file or within the strip to reorder · Right click — menu"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(rect()).adjusted(2, 2, -2, -2);
        p.setPen(QPen(m_current ? m_strip->m_style.accent : palette().mid().color(), m_current ? 2 : 1));
        p.setBrush(palette().base());
        p.drawRoundedRect(frame, 6, 6);
        const QSizeF ps = m_pixmap.deviceIndependentSize();
        p.drawPixmap(QPointF((width() - ps.width()) / 2, 4), m_pixmap);

        // Буква снимка
        const QRectF badge(8, 8, 26, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(badgeFillColor(m_strip->m_style.accent));
        p.drawRoundedRect(badge, 5, 5);
        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, QString(shotLetter(m_index)));

        // Число комментариев
        if (m_marks > 0) {
            const QString n = QStringLiteral("💬 %1").arg(m_comments);
            const QRectF box(width() - 56, height() - 30, 48, 20);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 160));
            p.drawRoundedRect(box, 5, 5);
            p.setPen(Qt::white);
            f.setBold(false);
            p.setFont(f);
            p.drawText(box, Qt::AlignCenter, n);
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
};

// Список миниатюр — цель перетаскивания внутри ленты (STR-3): снимок, брошенный между
// другими, встаёт на это место, буквы пересчитываются. Наружу тот же снимок уходит файлом
// (STR-6) — это одно перетаскивание, решает место, где его отпустили.
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
        // Отложенно: перестройка ленты удалит миниатюру — источник ещё идущего QDrag
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

// Ручка «перетащить весь набор»
class DragHandle : public QLabel
{
    Q_DECLARE_TR_FUNCTIONS(DragHandle)
public:
    explicit DragHandle(std::function<void()> onDrag)
        : m_onDrag(std::move(onDrag))
    {
        setText(tr("⠿ Drag the whole set"));
        setAlignment(Qt::AlignCenter);
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Drag into a chat: all shots as files, the text is copied to the clipboard"));
        setForegroundRole(QPalette::PlaceholderText);
    }

protected:
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

// Кнопки заголовка: иконка из темы (на KDE — Breeze), без темы — нарисованная кодом
enum class HeaderGlyph { Gear, Minimize };

static QIcon drawnGlyph(HeaderGlyph glyph, const QColor &color)
{
    QIcon icon;
    for (int size : {16, 20, 24, 32, 48}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size;
        if (glyph == HeaderGlyph::Gear) {
            // Шестерёнка: 8 зубцов и кольцо
            QPainterPath gear;
            const QPointF c(s / 2, s / 2);
            const qreal rOuter = s * 0.46, rInner = s * 0.34;
            for (int i = 0; i < 16; ++i) {
                const qreal a0 = (i - 0.5) * M_PI / 8, a1 = (i + 0.5) * M_PI / 8;
                const qreal r = (i % 2 == 0) ? rOuter : rInner;
                const QPointF p0 = c + QPointF(std::cos(a0), std::sin(a0)) * r;
                const QPointF p1 = c + QPointF(std::cos(a1), std::sin(a1)) * r;
                if (i == 0)
                    gear.moveTo(p0);
                else
                    gear.lineTo(p0);
                gear.lineTo(p1);
            }
            gear.closeSubpath();
            gear.addEllipse(c, s * 0.15, s * 0.15);   // отверстие (правило even-odd)
            gear.setFillRule(Qt::OddEvenFill);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPath(gear);
        } else {
            p.setPen(QPen(color, std::max(1.5, s * 0.1), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(s * 0.22, s * 0.68), QPointF(s * 0.78, s * 0.68));
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

static QIcon themedIcon(const QStringList &names, HeaderGlyph fallback, const QColor &color)
{
    for (const QString &n : names)
        if (QIcon::hasThemeIcon(n))
            return QIcon::fromTheme(n);
    return drawnGlyph(fallback, color);
}

static QToolButton *headerButton(const QIcon &icon, const QString &tooltip)
{
    auto *b = new QToolButton;
    b->setIcon(icon);
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(30, 30);
    b->setAutoRaise(true);
    b->setToolTip(tooltip);
    b->setFocusPolicy(Qt::NoFocus);
    // Иначе кнопка наследует «руку» заголовка, за который ленту переносят
    b->setCursor(Qt::ArrowCursor);
    return b;
}

// Заголовок ленты — ручка перетаскивания
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
        setCursor(Qt::ClosedHandCursor);
        m_strip->beginMove(mapTo(m_strip, e->position().toPoint()));
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
        // Пока кнопка зажата, события идут нам даже за пределами поверхности (неявный захват),
        // координаты — относительно нашего окна
        m_strip->finishMove(mapTo(m_strip, e->position().toPoint()));
    }

private:
    StripWindow *m_strip;
};

StripWindow::StripWindow(CaptureSetModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setWindowTitle(tr("Markpaste — set"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setObjectName(QStringLiteral("Strip"));
    setStyleSheet(QStringLiteral("#Strip { background: palette(window); border: 1px solid palette(mid); border-radius: 10px; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *header = new StripHeader(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("⠿ <b>Markpaste</b>"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);   // тянем за весь заголовок
    auto *menuButton = headerButton(themedIcon({QStringLiteral("configure"), QStringLiteral("settings-configure")},
                                               HeaderGlyph::Gear, palette().color(QPalette::WindowText)),
                                    tr("Menu"));
    // Меню, а не сразу настройки: здесь же предпросмотр (PST-6) и сохранение в папку (APP-6)
    auto *menu = new QMenu(menuButton);
    m_previewAction = menu->addAction(tr("Preview message…"), this, &StripWindow::previewRequested);
    m_saveAction = menu->addAction(tr("Save to folder…"), this, &StripWindow::saveToFolderRequested);
    menu->addSeparator();
    menu->addAction(tr("Settings"), this, &StripWindow::settingsRequested);
    menuButton->setMenu(menu);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; }"));
    auto *hideButton = headerButton(themedIcon({QStringLiteral("window-minimize-symbolic"), QStringLiteral("window-minimize")},
                                               HeaderGlyph::Minimize, palette().color(QPalette::WindowText)),
                                    tr("Hide to tray"));
    connect(hideButton, &QToolButton::clicked, this, &StripWindow::hideRequested);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(menuButton);
    headerLayout->addWidget(hideButton);
    layout->addWidget(header);

    m_counter = new QLabel;
    m_counter->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_counter);

    auto *listHost = new ShotListHost(this);
    m_list = new QVBoxLayout(listHost);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(6);
    m_list->addStretch();
    m_scroll = new QScrollArea;
    m_scroll->setWidget(listHost);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFixedWidth(kThumbWidth + 14);
    layout->addWidget(m_scroll, 1);

    m_dragHandle = new DragHandle([this] {
        QList<QUuid> all;
        for (const Shot &s : m_model->set().shots)
            all << s.id;
        startDrag(all, m_dragHandle);
    });
    layout->addWidget(m_dragHandle);

    m_message = new QLabel;
    m_message->setWordWrap(true);
    m_message->setVisible(false);
    m_message->setStyleSheet(QStringLiteral("QLabel { background: palette(highlight); color: palette(highlighted-text);"
                                            " border-radius: 5px; padding: 4px 6px; }"));
    layout->addWidget(m_message);

    m_newShot = new QPushButton(tr("＋ New shot"));
    m_paste = new QPushButton(tr("Paste set"));
    m_paste->setDefault(true);
    m_copy = new QPushButton(tr("Copy set"));
    m_copy->setToolTip(tr("Put files and text into the clipboard, then press Ctrl+V in the chat"));
    m_clear = new QPushButton(tr("Clear"));
    m_cancel = new QPushButton(tr("Stop"));
    m_cancel->setVisible(false);
    for (QPushButton *b : {m_newShot, m_paste, m_copy, m_clear, m_cancel}) {
        b->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(b);
    }
    connect(m_newShot, &QPushButton::clicked, this, &StripWindow::newShotRequested);
    connect(m_paste, &QPushButton::clicked, this, &StripWindow::pasteRequested);
    connect(m_copy, &QPushButton::clicked, this, &StripWindow::copyRequested);
    connect(m_clear, &QPushButton::clicked, this, &StripWindow::clearRequested);
    connect(m_cancel, &QPushButton::clicked, this, &StripWindow::cancelPasteRequested);

    connect(m_model, &CaptureSetModel::changed, this, &StripWindow::rebuild);
    rebuild();
}

void StripWindow::setStyle(const RenderStyle &style)
{
    m_style = style;
    m_thumbs.clear();
    rebuild();
}

void StripWindow::setCurrentShot(const QUuid &shotId)
{
    m_current = shotId;
    rebuild();
}

void StripWindow::setPasteCaption(const QString &caption, const QString &tooltip)
{
    m_paste->setText(caption);
    m_paste->setToolTip(tooltip);
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
    // Миниатюра — запечённый снимок: видно метки и размытие так, как их увидит чат
    const QImage baked = renderShot(shot, index, m_style);
    const qreal dpr = devicePixelRatioF();
    QImage scaled = baked.scaled(QSize(int((kThumbWidth - 8) * dpr), int(kThumbMaxHeight * dpr)), Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    QPixmap pm = QPixmap::fromImage(scaled);
    pm.setDevicePixelRatio(dpr);
    m_thumbs.insert(shot.id, CachedThumb{shot, index, pm});
    return pm;
}

void StripWindow::rebuild()
{
    // Чистим старые миниатюры (последний элемент — растяжка). Удаляем отложенно: перестройку
    // часто вызывает сама миниатюра — щелчок, пункт её меню (меню в exec() — её дочерний объект
    // на стеке), перетаскивание. Прямой delete здесь ронял программу (docs/problems.md).
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

    // «3 снимка · 4 комментария» (STR-2)
    const int shots = int(set.shots.size());
    m_counter->setText(set.isEmpty() ? tr("The set is empty")
                                     : tr("%n shot(s)", nullptr, shots) + QStringLiteral(" · ")
                                           + tr("%n comment(s)", nullptr, set.totalComments()));
    const bool empty = set.isEmpty();
    m_paste->setEnabled(!empty);
    m_copy->setEnabled(!empty);
    m_clear->setEnabled(!empty);
    m_previewAction->setEnabled(!empty);
    m_saveAction->setEnabled(!empty);
    m_dragHandle->setVisible(shots > 1);
    m_newShot->setEnabled(!set.isFull());

    // Высота ленты — по содержимому, но не выше экрана
    int listHeight = 0;
    for (int i = 0; i < m_list->count() - 1; ++i)
        listHeight += m_list->itemAt(i)->widget()->height() + m_list->spacing();
    QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const int maxList = scr ? scr->availableGeometry().height() - 340 : 600;
    m_scroll->setFixedHeight(std::clamp(listHeight, 40, std::max(120, maxList)));
    adjustSize();

    // Лента выросла и вылезла за низ экрана — поднимаем её
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
    // pid — чтобы снимок из другого экземпляра программы не переставлял ничего здесь
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
    // Место вставки — перед первой миниатюрой, середина которой ниже курсора
    const int n = m_list->count() - 1;   // последний элемент — растяжка
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
    // Место вставки считается до удаления снимка со старого места
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
    // Один снимок можно бросить и обратно в ленту — тогда он переставляется (STR-3)
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
            // Обычное окно: на GNOME «поверх» и позиция недоступны (STR-1), двигает оконный менеджер
            setWindowFlag(Qt::WindowStaysOnTopHint);
            setWindowFlag(Qt::Tool);
        }
    }
    adjustSize();
    show();
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
    const QRect geo = screen->geometry();
    const int x = pl.edge == PanelEdge::Left ? geo.left() + pl.margin : geo.right() + 1 - pl.margin - width();
    const int y = pl.offset < 0 ? geo.top() + (geo.height() - height()) / 2 : geo.top() + pl.offset;
    return {x, y};
}

void StripWindow::beginMove(const QPoint &posInStrip)
{
    m_movePress = posInStrip;
    m_moving = false;
    if (!m_layerShell && windowHandle())
        windowHandle()->startSystemMove();   // обычное окно двигает оконный менеджер
}

void StripWindow::moveTarget(const QPoint &posInStrip, QScreen **screen, PanelPlacement *placement) const
{
    // Пока кнопка зажата, лента стоит на месте, поэтому её положение известно точно,
    // и точка на экране = положение ленты + координаты указателя в ней.
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
    // Переезд — один раз, при отпускании: во время движения Qt не знает, применил ли композитор
    // новые отступы, и координаты указателя «плыли» бы (docs/decisions/0004-*.md)
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
