#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>

class QScreen;

namespace mp {

class GhostLayer;

// Outline of the strip being moved (STR-5). One transparent layer per monitor,
// above everything and without input. Draws a rectangle where the strip will end up after
// the mouse is released. Works only with layer-shell; without it the window manager moves the strip.
class MoveGhost : public QObject
{
    Q_OBJECT
public:
    explicit MoveGhost(QObject *parent = nullptr);
    ~MoveGhost() override;

    // target is in global logical desktop coordinates
    void show(const QRect &target, const QColor &accent);
    void hide();

private:
    QList<QPointer<GhostLayer>> m_layers;
    bool m_created = false;
    bool m_available = true;
};

} // namespace mp
