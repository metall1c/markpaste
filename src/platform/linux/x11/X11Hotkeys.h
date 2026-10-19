#pragma once

#include "X11Keys.h"
#include "platform/Interfaces.h"

#include <QAbstractNativeEventFilter>

namespace mp::lx {

// Глобальные клавиши на X11 через XGrabKey на корневом окне (ТЗ разд. 9).
//
// Сочетание захватывается и с NumLock/CapsLock: X11 считает их модификаторами, и без
// этих вариантов клавиша не срабатывала бы при включённом NumLock. Если сочетание уже
// захватил кто-то другой, сервер отвечает BadAccess — сообщаем, что клавиша занята.
//
// События клавиш приходят в подключение Qt; забираем их фильтром нативных событий.
// Автоповтор X11 присылает пары «отпущена/нажата»: отпускание, при котором клавиша
// по QueryKeymap ещё нажата, — автоповтор, его пропускаем.
class X11Hotkeys : public IHotkeys, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    // nullptr, если Qt работает не через xcb
    static X11Hotkeys *create(QObject *parent = nullptr);
    ~X11Hotkeys() override;

    QFuture<BindResult> bind(Action action, const QString &preferred) override;
    QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) override;
    QString name() const override { return QStringLiteral("X11 XGrabKey"); }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    explicit X11Hotkeys(QObject *parent);
    struct Binding {
        X11Trigger trigger;
        QList<uint8_t> keycodes;
        bool down = false;
    };
    BindResult grab(Action action, const QString &preferred);
    void ungrab(const Binding &b);

    QHash<Action, Binding> m_bindings;
};

} // namespace mp::lx
