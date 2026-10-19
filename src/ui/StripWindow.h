#pragma once

#include "core/CaptureSetModel.h"
#include "platform/WindowRoles.h"
#include "render/ShotRenderer.h"

#include <QHash>
#include <QPixmap>
#include <QWidget>

#include <functional>

class QLabel;
class QMimeData;
class QPushButton;
class QVBoxLayout;
class QScrollArea;

namespace mp {

class ShotThumb;

// Лента набора (ТЗ 5.3): миниатюры с буквой и числом комментариев, кнопки и счётчик.
// На KDE/Sway — панель layer-shell у края экрана, которая не забирает фокус клавиатуры:
// клик по «Вставить» не уводит фокус из окна чата. Без layer-shell — обычное окно.
class StripWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StripWindow(CaptureSetModel *model, QWidget *parent = nullptr);

    void setStyle(const RenderStyle &style);
    void setCurrentShot(const QUuid &shotId);
    // Подписи кнопки вставки зависят от возможностей платформы
    void setPasteCaption(const QString &caption, const QString &tooltip);
    void showMessage(const QString &text, int timeoutMs = 4000);
    void setProgress(int done, int total);   // total = 0 — вставка закончилась

    // Для перетаскивания наружу файлы готовит контроллер (запекание + временные файлы)
    using DragProvider = std::function<QMimeData *(const QList<QUuid> &shots)>;
    void setDragProvider(DragProvider provider) { m_dragProvider = std::move(provider); }

    // Показать у края экрана (layer-shell, если есть) в сохранённом положении
    void present();
    // Переставить ленту (перенос мышью и отладочная команда debug-move-strip)
    void moveTo(QScreen *screen, const PanelPlacement &placement);

signals:
    void shotActivated(const QUuid &shotId);
    void newShotRequested();
    void pasteRequested();
    void copyRequested();
    void clearRequested();
    void cancelPasteRequested();
    void settingsRequested();
    void hideRequested();   // «свернуть в трей»

private:
    friend class ShotThumb;
    friend class StripHeader;
    // Перетаскивание за заголовок (STR-5): на layer-shell лента переезжает при отпускании
    void beginMove(const QPoint &posInStrip);
    void updateMove(const QPoint &posInStrip);   // контур будущего положения
    void finishMove(const QPoint &posInStrip);
    // Куда встанет лента, если отпустить мышь в точке posInStrip
    void moveTarget(const QPoint &posInStrip, QScreen **screen, PanelPlacement *placement) const;
    QScreen *currentScreen() const;
    // Левый верхний угол ленты при данном положении, в глобальных логических координатах.
    // Положение поверхности layer-shell Qt не знает — считаем сами из края и отступов.
    QPoint topLeftFor(QScreen *screen, const PanelPlacement &placement) const;
    QPoint assumedTopLeft() const { return topLeftFor(currentScreen(), m_placement); }
    void loadPlacement();
    void savePlacement() const;
    void rebuild();
    QPixmap thumbnailFor(int index);
    void startDrag(const QList<QUuid> &shots, QWidget *source);

    CaptureSetModel *m_model;
    RenderStyle m_style;
    QUuid m_current;
    QLabel *m_counter;
    QLabel *m_message;
    QVBoxLayout *m_list;
    QScrollArea *m_scroll;
    QPushButton *m_paste;
    QPushButton *m_copy;
    QPushButton *m_newShot;
    QPushButton *m_clear;
    QPushButton *m_cancel;
    QWidget *m_dragHandle;
    DragProvider m_dragProvider;
    bool m_roleApplied = false;
    bool m_layerShell = false;
    PanelPlacement m_placement;
    QString m_screenName;
    QPoint m_movePress;
    bool m_moving = false;
    class MoveGhost *m_ghost = nullptr;

    struct CachedThumb {
        Shot shot;
        int index = -1;
        QPixmap pixmap;
    };
    QHash<QUuid, CachedThumb> m_thumbs;
};

} // namespace mp
