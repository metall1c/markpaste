#pragma once

#include "X11Keys.h"
#include "platform/Interfaces.h"

#include <QAbstractNativeEventFilter>

namespace mp::lx {

// Global hotkeys on X11 via XGrabKey on the root window (spec section 9).
//
// The shortcut is also grabbed with NumLock/CapsLock: X11 treats them as modifiers, and without
// these variants the key would not fire with NumLock on. If someone else has already
// grabbed the shortcut, the server answers BadAccess, and we report the key as taken.
//
// Key events arrive in Qt's connection; we take them with a native event filter.
// X11 autorepeat sends "released/pressed" pairs: a release during which the key
// is still down according to QueryKeymap is autorepeat and is skipped.
class X11Hotkeys : public IHotkeys, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    // nullptr if Qt does not run through xcb
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
