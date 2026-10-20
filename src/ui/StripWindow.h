#pragma once

#include "core/CaptureSetModel.h"
#include "platform/WindowRoles.h"
#include "render/ShotRenderer.h"

#include <QHash>
#include <QPixmap>
#include <QWidget>

#include <functional>

class QAction;
class QLabel;
class QMimeData;
class QVBoxLayout;
class QScrollArea;

namespace mp {

class ActionButton;
class CountPill;
class EmptyHint;
class FlatButton;
class MessageLabel;
class ShotThumb;

// Лента набора (ТЗ 5.3): миниатюры с буквой и числом комментариев, кнопки и счётчик.
// На KDE/Sway — панель layer-shell у края экрана, которая не забирает фокус клавиатуры:
// клик по «Вставить» не уводит фокус из окна чата. На Windows — окно без рамки и без
// активации, к краю его ставим сами. Иначе — обычное окно.
class StripWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StripWindow(CaptureSetModel *model, QWidget *parent = nullptr);

    void setStyle(const RenderStyle &style);
    void setCurrentShot(const QUuid &shotId);
    // Главная кнопка вставляет набор (режим 1) или, где это недоступно, копирует его.
    // hotkey — сочетание вставки для подписи на кнопке («Alt+V»), пустое — без подписи
    void setPasteMode(bool sequential, const QString &hotkey);
    // Сочетание захвата для подсказки в пустом наборе
    void setCaptureHotkey(const QString &hotkey);
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
    void previewRequested();        // PST-6
    void saveToFolderRequested();   // APP-6
    void aboutRequested();
    void hideRequested();   // «свернуть в трей»

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

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
    void snapToEdge();          // встать по m_placement
    QRect placementArea(QScreen *screen) const;   // от чего отсчитываются край и отступ
    void onMovedByUser();       // пользователь отпустил перетащенную ленту
    void loadPlacement();
    void savePlacement() const;
    void rebuild();
    QPixmap thumbnailFor(int index);
    void startDrag(const QList<QUuid> &shots, QWidget *source);

public:
    // Перестановка перетаскиванием внутри ленты (STR-3). Публичны для тестов.
    static constexpr const char *kShotMime = "application/x-markpaste-shot";
    static QByteArray shotDragData(const QUuid &shotId);
    static QUuid draggedShot(const QMimeData *data);
    int dropIndexAt(int yInList) const;      // место вставки 0…N по координате в списке
    int dropLineY(int index) const;
    void moveShotTo(const QUuid &shotId, int insertIndex);

private:
    friend class ShotListHost;

    CaptureSetModel *m_model;
    RenderStyle m_style;
    QUuid m_current;
    CountPill *m_count;
    EmptyHint *m_empty;
    MessageLabel *m_message;
    QVBoxLayout *m_list;
    QScrollArea *m_scroll;
    ActionButton *m_paste;
    ActionButton *m_copy;
    ActionButton *m_newShot;
    FlatButton *m_clear;
    QAction *m_previewAction;
    QAction *m_saveAction;
    ActionButton *m_cancel;
    QWidget *m_dragHandle;
    bool m_pasteCopies = false;
    DragProvider m_dragProvider;
    bool m_roleApplied = false;
    bool m_layerShell = false;
    bool m_snapToEdge = false;   // окно без рамки, к краю его ставим сами (Windows)
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
