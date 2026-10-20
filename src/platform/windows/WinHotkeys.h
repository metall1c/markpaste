#pragma once

#include "KeyboardHook.h"
#include "WinKeys.h"
#include "platform/Interfaces.h"

#include <QTimer>

#include <optional>

#include <windows.h>

namespace mp::win {

// Глобальные клавиши на Windows (ТЗ разд. 9).
//
// Обычные сочетания — RegisterHotKey на скрытом окне сообщений: система сама разбирает
// модификаторы, не присылает автоповтор (MOD_NOREPEAT) и сообщает, если сочетание занято.
// Print Screen — через хук WH_KEYBOARD_LL (KeyboardHook): в Windows 11 эту клавишу забирают
// «Ножницы», и RegisterHotKey её не получает.
//
// RegisterHotKey не сообщает об отпускании. Его ждёт вставка (отпустить Alt, прежде чем
// нажимать Ctrl+V), поэтому после срабатывания опрашиваем клавиатуру, пока клавиша и
// модификаторы сочетания не отпущены.
class WinHotkeys : public IHotkeys
{
    Q_OBJECT
public:
    explicit WinHotkeys(QObject *parent = nullptr);
    ~WinHotkeys() override;

    QFuture<BindResult> bind(Action action, const QString &preferred) override;
    QFuture<QHash<Action, BindResult>> bindAll(const QHash<Action, QString> &preferred) override;
    QString name() const override { return QStringLiteral("RegisterHotKey"); }

    // Для окна сообщений
    void onHotkey(int id);

private:
    struct Binding {
        WinTrigger trigger;
        int id = 0;
        bool viaHook = false;
        bool waitingRelease = false;
    };
    BindResult grab(Action action, const QString &preferred);
    void ungrab(const Binding &b);
    void updateHook();
    void pollReleases();
    void activate(int id, bool watchRelease);
    std::optional<Action> actionFor(int id) const;

    HWND m_window = nullptr;
    QHash<Action, Binding> m_bindings;
    KeyboardHook m_hook;
    QTimer m_releasePoll;
};

} // namespace mp::win
