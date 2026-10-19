#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>

class QScreen;

namespace mp {

class GhostLayer;

// Контур переносимой ленты (STR-5). По прозрачному слою на каждый монитор,
// поверх всего и без ввода. Рисует прямоугольник там, где лента окажется после
// отпускания мыши. Работает только с layer-shell; без него ленту двигает оконный менеджер.
class MoveGhost : public QObject
{
    Q_OBJECT
public:
    explicit MoveGhost(QObject *parent = nullptr);
    ~MoveGhost() override;

    // target — в глобальных логических координатах рабочего стола
    void show(const QRect &target, const QColor &accent);
    void hide();

private:
    QList<QPointer<GhostLayer>> m_layers;
    bool m_created = false;
    bool m_available = true;
};

} // namespace mp
