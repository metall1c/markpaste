#pragma once

#include "CaptureSet.h"

#include <QObject>
#include <QUndoStack>

#include <functional>

namespace mp {

// Владелец набора и единственная точка его изменения.
//
// Отмена устроена на снимках состояния: каждая команда хранит CaptureSet «до» и «после».
// QByteArray и QList разделяются неявно, поэтому снимок стоит копии списков, а не кадров.
// Так отмена работает для любых операций, включая удаление и перестановку снимков (EDT-8),
// и не требует писать обратную операцию к каждой правке.
class CaptureSetModel : public QObject
{
    Q_OBJECT
public:
    explicit CaptureSetModel(QObject *parent = nullptr);

    const CaptureSet &set() const { return m_set; }
    const Shot *shot(const QUuid &shotId) const;
    QUndoStack *undoStack() { return &m_undo; }

    // Снимки. addShot возвращает пустой QUuid, если набор заполнен (CAP-8).
    QUuid addShot(Shot shot);
    bool removeShot(const QUuid &shotId);
    bool moveShot(const QUuid &shotId, int newIndex);
    void clear();                                   // очистка набора — тоже отменяется

    // Метки
    QUuid addMark(const QUuid &shotId, const QRect &rect, const QString &comment = {}, const MarkPen &pen = {});
    bool setMarkRect(const QUuid &shotId, const QUuid &markId, const QRect &rect);
    // Стрелка, карандаш и точка (EDT-9). Точки обрезаются по видимой области, линия карандаша
    // упрощается; rect метки становится охватывающим прямоугольником.
    QUuid addLinearMark(const QUuid &shotId, MarkShape shape, const QList<QPoint> &points,
                        const QString &comment = {}, const MarkPen &pen = {});
    bool setMarkPoints(const QUuid &shotId, const QUuid &markId, const QList<QPoint> &points);
    bool setMarkComment(const QUuid &shotId, const QUuid &markId, const QString &comment);
    bool setMarkPen(const QUuid &shotId, const QUuid &markId, const MarkPen &pen);
    bool removeMark(const QUuid &shotId, const QUuid &markId);

    // Размытие
    QUuid addRedaction(const QUuid &shotId, const QRect &rect);
    bool setRedactionRect(const QUuid &shotId, const QUuid &redactionId, const QRect &rect);
    bool removeRedaction(const QUuid &shotId, const QUuid &redactionId);

    // Обрезка (EDT-7): метки и размытия вне новой области удаляются, частично
    // попавшие — обрезаются по ней (стрелка и карандаш — по линии, а не по рамке). Сколько удалится — заранее говорит marksOutsideCrop.
    bool setCrop(const QUuid &shotId, const QRect &crop);
    int marksOutsideCrop(const QUuid &shotId, const QRect &crop) const;

    // Без истории: только для загрузки сохранённой сессии.
    void resetWithoutHistory(const CaptureSet &set);

signals:
    void changed();

private:
    friend class StateCommand;
    using Mutation = std::function<bool(CaptureSet &)>;
    bool apply(const QString &text, const Mutation &mutate);
    void setState(const CaptureSet &set);

    static QRect normalizedInside(const QRect &rect, const QRect &bounds);
    // Вписывает метку любой формы в bounds. false — от метки ничего не осталось.
    static bool fitMark(Mark &mark, const QRect &bounds, qreal dpr);

    CaptureSet m_set;
    QUndoStack m_undo;
};

} // namespace mp
