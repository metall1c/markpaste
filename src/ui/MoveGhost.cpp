#include "MoveGhost.h"

#include "platform/WindowRoles.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWidget>

namespace mp {

class GhostLayer : public QWidget
{
public:
    explicit GhostLayer(QScreen *screen) : m_screen(screen) {}

    QScreen *screen() const { return m_screen; }

    void setTarget(const QRect &globalTarget, const QColor &accent)
    {
        const QRect local = globalTarget.intersects(m_screen->geometry())
                                ? globalTarget.translated(-m_screen->geometry().topLeft())
                                : QRect();
        if (local == m_rect && accent == m_accent)
            return;
        // Перерисовываем только старое и новое место контура
        const QRect dirty = m_rect.united(local).adjusted(-4, -4, 4, 4);
        m_rect = local;
        m_accent = accent;
        update(dirty);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_rect.isEmpty())
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor fill = m_accent;
        fill.setAlpha(40);
        p.setBrush(fill);
        p.setPen(QPen(m_accent, 2, Qt::DashLine));
        p.drawRoundedRect(QRectF(m_rect).adjusted(1, 1, -1, -1), 10, 10);
    }

private:
    QScreen *m_screen;
    QRect m_rect;
    QColor m_accent;
};

MoveGhost::MoveGhost(QObject *parent)
    : QObject(parent)
{
}

MoveGhost::~MoveGhost()
{
    for (const auto &l : std::as_const(m_layers))
        delete l.data();
}

void MoveGhost::show(const QRect &target, const QColor &accent)
{
    if (!m_available)
        return;
    if (!m_created) {
        m_created = true;
        for (QScreen *s : QGuiApplication::screens()) {
            auto *layer = new GhostLayer(s);
            if (!applyGhostRole(layer, s)) {
                delete layer;
                m_available = false;
                return;
            }
            m_layers << layer;
        }
    }
    for (const auto &l : std::as_const(m_layers)) {
        if (!l)
            continue;
        l->setTarget(target, accent);
        if (!l->isVisible())
            l->show();
    }
}

void MoveGhost::hide()
{
    // Слои удаляем: набор мониторов к следующему переносу мог измениться
    for (const auto &l : std::as_const(m_layers))
        if (l) {
            l->hide();
            l->deleteLater();
        }
    m_layers.clear();
    m_created = false;
}

} // namespace mp
